#include <goldfish/debug_checks_reader.h>
#include <goldfish/json_reader.h>
#include "unit_test.h"

namespace goldfish
{
	TEST_CASE(reading_parent_before_stream_end)
	{
		auto document = json::read(stream::read_string_literal("[\"hello\"]"), debug_check::throw_on_error{}).as<tags::array>();

		auto string = document.read()->as<tags::text_string>();
		test(stream::read<char>(string) == 'h');
		test(stream::seek(string, 1) == 1);
		expect_exception<debug_check::library_missused>([&] { document.read(); });
	}
	TEST_CASE(reading_parent_after_reading_all_ok)
	{
		auto document = json::read(stream::read_string_literal("[\"hello\"]"), debug_check::throw_on_error{}).as<tags::array>();

		auto string = document.read()->as<tags::text_string>();
		test(stream::read_all_as_string(string) == "hello");
		test(document.read() == nullopt);
	}
	TEST_CASE(reading_parent_after_seeking_to_exactly_end_throws)
	{
		auto document = json::read(stream::read_string_literal("[\"hello\"]"), debug_check::throw_on_error{}).as<tags::array>();

		auto string = document.read()->as<tags::text_string>();
		test(stream::seek(string, 5) == 5);
		expect_exception<debug_check::library_missused>([&] { document.read(); });
	}
	TEST_CASE(reading_parent_after_seeking_past_end_ok)
	{
		auto document = json::read(stream::read_string_literal("[\"hello\"]"), debug_check::throw_on_error{}).as<tags::array>();

		auto string = document.read()->as<tags::text_string>();
		test(stream::seek(string, 6) == 5);
		test(document.read() == nullopt);
	}

	TEST_CASE(reading_parent_before_end_of_array_throws)
	{
		auto document = json::read(stream::read_string_literal("[[1, 2]]"), debug_check::throw_on_error{}).as<tags::array>();

		auto array = document.read()->as<tags::array>();
		test(array.read()->as<uint64_t>() == 1);
		expect_exception<debug_check::library_missused>([&] { document.read(); });
	}
	TEST_CASE(reading_parent_at_exactly_end_of_array_throws)
	{
		auto document = json::read(stream::read_string_literal("[[1, 2]]"), debug_check::throw_on_error{}).as<tags::array>();

		auto array = document.read()->as<tags::array>();
		test(array.read()->as<uint64_t>() == 1);
		test(array.read()->as<uint64_t>() == 2);
		expect_exception<debug_check::library_missused>([&] { document.read(); });
	}
	TEST_CASE(reading_parent_passed_end_of_array_ok)
	{
		auto document = json::read(stream::read_string_literal("[[1, 2]]"), debug_check::throw_on_error{}).as<tags::array>();

		auto array = document.read()->as<tags::array>();
		test(array.read()->as<uint64_t>() == 1);
		test(array.read()->as<uint64_t>() == 2);
		test(array.read() == nullopt);
		test(document.read() == nullopt);
	}

	TEST_CASE(reading_parent_before_end_of_map_throws)
	{
		auto document = json::read(stream::read_string_literal("[{\"a\":1, \"b\":2}]"), debug_check::throw_on_error{}).as<tags::array>();

		auto map = document.read()->as<tags::map>();
		test(stream::read_all_as_string(map.read_key()->as<tags::text_string>()) == "a");
		expect_exception<debug_check::library_missused>([&] { document.read(); });
	}
	TEST_CASE(reading_parent_at_exactly_end_of_map_throws)
	{
		auto document = json::read(stream::read_string_literal("[{\"a\":1, \"b\":2}]"), debug_check::throw_on_error{}).as<tags::array>();

		auto map = document.read()->as<tags::map>();
		test(stream::read_all_as_string(map.read_key()->as<tags::text_string>()) == "a");
		test(map.read_value().as<uint64_t>() == 1);
		test(stream::read_all_as_string(map.read_key()->as<tags::text_string>()) == "b");
		test(map.read_value().as<uint64_t>() == 2);
		expect_exception<debug_check::library_missused>([&] { document.read(); });
	}
	TEST_CASE(reading_parent_passed_end_of_map_ok)
	{
		auto document = json::read(stream::read_string_literal("[{\"a\":1, \"b\":2}]"), debug_check::throw_on_error{}).as<tags::array>();

		auto map = document.read()->as<tags::map>();
		test(stream::read_all_as_string(map.read_key()->as<tags::text_string>()) == "a");
		test(map.read_value().as<uint64_t>() == 1);
		test(stream::read_all_as_string(map.read_key()->as<tags::text_string>()) == "b");
		test(map.read_value().as<uint64_t>() == 2);
		test(map.read_key() == nullopt);

		test(document.read() == nullopt);
	}
	TEST_CASE(reading_value_before_finishing_key_in_map)
	{
		auto document = json::read(stream::read_string_literal("[{\"a\":1, \"b\":2}]"), debug_check::throw_on_error{}).as<tags::array>();

		auto map = document.read()->as<tags::map>();
		map.read_key();
		expect_exception<debug_check::library_missused>([&] { map.read_value(); });
	}
	TEST_CASE(reading_key_before_finishing_value_in_map)
	{
		auto document = json::read(stream::read_string_literal("[{\"a\":\"1\", \"b\":2}]"), debug_check::throw_on_error{}).as<tags::array>();

		auto map = document.read()->as<tags::map>();
		test(stream::read_all_as_string(map.read_key()->as<tags::text_string>()) == "a");
		map.read_value();
		expect_exception<debug_check::library_missused>([&] { map.read_key(); });
	}
	TEST_CASE(reading_value_instead_of_key_in_map)
	{
		auto document = json::read(stream::read_string_literal("[{\"a\":1, \"b\":2}]"), debug_check::throw_on_error{}).as<tags::array>();

		auto map = document.read()->as<tags::map>();
		expect_exception<debug_check::library_missused>([&] { map.read_value(); });
	}
	TEST_CASE(reading_key_instead_of_value_in_map)
	{
		auto document = json::read(stream::read_string_literal("[{\"a\":1, \"b\":2}]"), debug_check::throw_on_error{}).as<tags::array>();

		auto map = document.read()->as<tags::map>();
		test(stream::read_all_as_string(map.read_key()->as<tags::text_string>()) == "a");
		expect_exception<debug_check::library_missused>([&] { map.read_key(); });
	}
}