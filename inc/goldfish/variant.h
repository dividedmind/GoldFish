#pragma once

#include <algorithm>
#include <assert.h>
#include <memory>
#include <type_traits>
#include <utility>

namespace goldfish
{
	struct bad_variant_access {};

	namespace details
	{
		template <class...> struct conjunction {};
		template <> struct conjunction<> { enum { value = true }; };
		template <class Head, class... Tail> struct conjunction<Head, Tail...> { enum { value = Head::value && conjunction<Tail...>::value }; };

		template <class...> struct disjunction {};
		template <> struct disjunction<> { enum { value = false }; };
		template <class Head, class... Tail> struct disjunction<Head, Tail...> { enum { value = Head::value || disjunction<Tail...>::value }; };

		template <size_t...> struct largest {};
		template <size_t x> struct largest<x> { enum { value = x }; };
		template <size_t x, size_t y> struct largest<x, y> { enum { value = x > y ? x : y }; };
		template <size_t x, size_t y, size_t... z> struct largest<x, y, z...> { enum { value = largest<x, largest<y, z...>::value>::value }; };

		template <class T, class dummy = uint8_t> struct with_padding
		{
			T data;
			uint8_t padding_for_variant;
		};
		template <class T> struct with_padding<T, decltype(std::declval<T>().padding_for_variant)> { T data;  };
		
		template <class U, class... Args> struct index_of { };
		template <class U, class... Tail> struct index_of<U, U, Tail...> { enum { value = 0 }; };
		template <class U, class Head, class... Tail> struct index_of<U, Head, Tail...> { enum { value = 1 + index_of<U, Tail...>::value }; };

		template <size_t, class... Args> struct nth_type { };
		template <size_t N, class Head, class... Tail> struct nth_type<N, Head, Tail...> { using type = typename nth_type<N - 1, Tail...>::type; };
		template <class Head, class... Tail> struct nth_type<0, Head, Tail...> { using type = Head; };
		template <size_t N, class... Tail> using nth_type_t = typename nth_type<N, Tail...>::type;

		template <class T, class... Types> using is_one_of = disjunction<std::is_same<T, Types>...>;

		template <class... types> class variant_base
		{
		public:		
			uint8_t which() const { return *(reinterpret_cast<const char*>(&m_data) + sizeof(m_data) - 1); }
			template <size_t N> auto& as() &
			{
				if (which() != N)
					throw bad_variant_access();
				return as_unchecked<N>();
			}
			template <size_t N> auto& as() const &
			{
				if (which() != N)
					throw bad_variant_access();
				return as_unchecked<N>();
			}
			template <size_t N> auto&& as() &&
			{
				if (which() != N)
					throw bad_variant_access();
				return std::move(*this).as_unchecked<N>();
			}
			template <size_t N> auto& as_unchecked() & noexcept
			{
				assert(which() == N);
				return reinterpret_cast<nth_type_t<N, types...>&>(m_data);
			}
			template <size_t N> auto& as_unchecked() const & noexcept
			{
				assert(which() == N);
				return reinterpret_cast<const nth_type_t<N, types...>&>(m_data);
			}
			template <size_t N> auto&& as_unchecked() && noexcept
			{
				assert(which() == N);
				return std::move(reinterpret_cast<nth_type_t<N, types...>&>(m_data));
			}

			template <class T> auto& as() & { return as<index_of<T, types...>::value>(); }
			template <class T> auto& as() const & { return as<index_of<T, types...>::value>(); }
			template <class T> auto&& as() && { return std::move(*this).as<index_of<T, types...>::value>(); }
			template <class T> auto& as_unchecked() & noexcept { return as_unchecked<index_of<T, types...>::value>(); }
			template <class T> auto& as_unchecked() const & noexcept { return as_unchecked<index_of<T, types...>::value>(); }
			template <class T> auto&& as_unchecked() && noexcept { return std::move(*this).as_unchecked<index_of<T, types...>::value>(); }

		private:
			template <class Return, size_t N, class TLambda> static Return eval(variant_base<types...>& v, TLambda&& l) { return l(v.as_unchecked<N>()); }
			template <class Return, size_t N, class TLambda> static Return eval(variant_base<types...>&& v, TLambda&& l) { return l(std::move(v).as_unchecked<N>()); }
			template <class Return, size_t N, class TLambda> static Return eval(const variant_base<types...>& v, TLambda&& l) { return l(v.as_unchecked<N>()); }
			template <class TLambda, std::size_t... Is> decltype(auto) apply_helper(TLambda&& l, std::index_sequence<Is...>) &
			{
				using Return = decltype(l(std::declval<nth_type_t<0, types...>&>()));
				using Fn = Return(*)(variant_base<types...>&, TLambda&&);
				static Fn fns[] = { eval<Return, Is, TLambda>... };
				return fns[which()](*this, std::forward<TLambda>(l));
			}
			template <class TLambda, std::size_t... Is> decltype(auto) apply_helper(TLambda&& l, std::index_sequence<Is...>) const &
			{
				using Return = decltype(l(std::declval<nth_type_t<0, types...>&>()));
				using Fn = Return(*)(const variant_base<types...>&, TLambda&&);
				static Fn fns[] = { eval<Return, Is, TLambda>... };
				return fns[which()](*this, std::forward<TLambda>(l));
			}
			template <class TLambda, std::size_t... Is> decltype(auto) apply_helper(TLambda&& l, std::index_sequence<Is...>) &&
			{
				using Return = decltype(l(std::declval<nth_type_t<0, types...>&&>()));
				using Fn = Return(*)(variant_base<types...>&&, TLambda&&);
				static Fn fns[] = { eval<Return, Is, TLambda>... };
				return fns[which()](std::move(*this), std::forward<TLambda>(l));
			}
		public:
			template <class TLambda> decltype(auto) visit(TLambda&& l) &
			{
				return apply_helper(std::forward<TLambda>(l), std::index_sequence_for<types...>{});
			}
			template <class TLambda> decltype(auto) visit(TLambda&& l) const &
			{
				return apply_helper(std::forward<TLambda>(l), std::index_sequence_for<types...>{});
			}
			template <class TLambda> decltype(auto) visit(TLambda&& l) &&
			{
				return std::move(*this).apply_helper(std::forward<TLambda>(l), std::index_sequence_for<types...>{});
			}

			template <class T> bool is() const { return which() == index_of<T, types...>::value; }

		protected:
			void set_which(uint8_t i) { *(reinterpret_cast<char*>(&m_data) + sizeof(m_data) - 1) = i; }
			std::aligned_storage_t<largest<sizeof(with_padding<types>)...>::value, largest<alignof(with_padding<types>)...>::value> m_data;
		};

		template <bool no_destructor, class... types> class variant_destructor_impl {};
		template <class... types> class variant_destructor_impl<true /*no_destructor*/, types...> : public variant_base<types...>
		{
		protected:
			void destroy() {}
		};
		template <class... types> class variant_destructor_impl<false /*no_destructor*/, types...> : public variant_base<types...>
		{
		public:
			~variant_destructor_impl() { destroy(); }

		protected:
			void destroy() noexcept
			{
				visit([](auto& x) noexcept
				{
					using T = std::decay_t<decltype(x)>;
					x.~T();
				});
			}
		};
		template <class... types> using variant_destructor = variant_destructor_impl<conjunction<std::is_trivially_destructible<types>...>::value, types...>;

		template <bool no_copy, class... types> class variant_copy_impl {};
		template <class... types> class variant_copy_impl<true /*no_copy*/, types...> : public variant_destructor<types...>
		{
		};
		template <class... types> class variant_copy_impl<false /*no_copy*/, types...> : public variant_destructor<types...>
		{
		public:
			variant_copy_impl() = default;
			variant_copy_impl(variant_copy_impl&& rhs) noexcept(conjunction<std::is_nothrow_move_constructible<types>...>::value)
			{
				if (conjunction<std::is_trivially_copy_constructible<types>...>::value)
				{
					m_data = rhs.m_data;
				}
				else
				{
					rhs.visit([&](auto& x)
					{
						using T = std::decay_t<decltype(x)>;
						new (&m_data) T(std::move(x));
					});
				}
				set_which(rhs.which());
			}
			variant_copy_impl(const variant_copy_impl& rhs) noexcept(conjunction<std::is_nothrow_copy_constructible<types>...>::value)
			{
				if (conjunction<std::is_trivially_copy_constructible<types>...>::value)
				{
					m_data = rhs.m_data;
				}
				else
				{
					rhs.visit([&](auto& x)
					{
						using T = std::decay_t<decltype(x)>;
						new (&m_data) T(x);
					});
				}
				set_which(rhs.which());
			}
		};
		template <class... types> using variant_copy = variant_copy_impl<
			conjunction<std::is_trivially_copy_constructible<types>...>::value &&
			conjunction<std::is_trivially_copy_assignable<types>...>::value,
			types...>;

		template <class T, size_t index, class... types> struct add_constructors {};
		template <class T, size_t index> struct add_constructors<T, index> : T { };
		template <class T, size_t index, class head, class... types> struct add_constructors<T, index, head, types...> : add_constructors<T, index+1, types...>
		{
		private:
			using base = add_constructors<T, index + 1, types...>;
		public:
			using base::base;
			add_constructors() = default;
			add_constructors(const head& data)
			{
				if (std::is_trivially_copy_constructible<head>::value)
					memcpy(&m_data, &data, sizeof(head));
				else
					new (&m_data) head(data);
				set_which(index);
			}
			add_constructors(head&& data)
			{
				if (std::is_trivially_copy_constructible<head>::value)
					memcpy(&m_data, &data, sizeof(head));
				else
					new (&m_data) head(std::move(data));
				set_which(index);
			}
		};
		template <class... types> using variant_with_constructors = add_constructors<variant_copy<types...>, 0, types...>;
	}

	template <class... types>
	class variant : public details::variant_with_constructors<types...>
	{
		using base = details::variant_with_constructors<types...>;
	public:
		static_assert(sizeof...(types) < 255, "too many arguments, index 255 is reserved for invalid state");

		using base::base;

		variant()
			: details::variant_with_constructors<types...>(details::nth_type_t<0, types...>())
		{}

		variant(const variant&) = default;
		variant(variant&&) = default;

		variant& operator = (variant&& rhs)
		{
			rhs.visit([&](auto& x)
			{
				*this = std::move(x);
			});
			return *this;
		}
		variant& operator = (const variant& rhs)
		{
			rhs.visit([&](auto& x)
			{
				*this = x;
			});
			return *this;
		}
		template <class U> std::enable_if_t<details::is_one_of<std::decay_t<U>, types...>::value, variant>& operator = (U&& u)
		{
			if (details::conjunction<std::is_trivially_destructible<types>...>::value && std::is_trivially_copyable<std::decay_t<U>>::value)
			{
				safe_destroy_construct<U>(std::forward<U>(u));
			}
			else
			{
				if (which() == details::index_of<std::decay_t<U>, types...>::value)
					as_unchecked<std::decay_t<U>>() = std::forward<U>(u);
				else
					safe_destroy_construct<U>(std::forward<U>(u));
			}
			return *this;
		}

		friend bool operator == (const variant& lhs, const variant& rhs)
		{
			return lhs.which() == rhs.which() && lhs.visit([&](auto&& x)
			{
				return x == rhs.as_unchecked<std::decay_t<decltype(x)>>();
			});
		}
		friend bool operator < (const variant& lhs, const variant& rhs)
		{
			if (lhs.which() == rhs.which())
			{
				return lhs.visit([&](auto&& x)
				{
					return x < rhs.as_unchecked<std::decay_t<decltype(x)>>();
				});
			}
			else
			{
				return lhs.which() < rhs.which();
			}
		}
		
		struct invalid_state
		{
			static void set(std::aligned_storage_t<sizeof(base), alignof(base)>& data)
			{
				reinterpret_cast<uint8_t*>(&data)[sizeof(base) - 1] = sizeof...(types);
			}
			static bool is(const std::aligned_storage_t<sizeof(base), alignof(base)>& data)
			{
				return reinterpret_cast<const uint8_t*>(&data)[sizeof(base) - 1] == sizeof...(types);
			}
		};
	private:
		template <class U> std::enable_if_t<std::is_nothrow_move_constructible<U>::value, void> safe_destroy_construct(std::decay_t<U>&& u) noexcept
		{
			destroy();
			new (&m_data) std::decay_t<U>(std::move(u));
			set_which(details::index_of<std::decay_t<U>, types...>::value);
		}
		template <class U> std::enable_if_t<std::is_nothrow_copy_constructible<U>::value, void> safe_destroy_construct(const std::decay_t<U>& u) noexcept
		{
			destroy();
			new (&m_data) std::decay_t<U>(u);
			set_which(details::index_of<std::decay_t<U>, types...>::value);
		}
		template <class U> std::enable_if_t<std::is_nothrow_move_constructible<U>::value && !std::is_nothrow_copy_constructible<U>::value, void> safe_destroy_construct(const std::remove_reference_t<U>& u)
		{
			safe_destroy_construct<U>(U(u));
		}
	};
}