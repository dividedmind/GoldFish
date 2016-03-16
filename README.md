# GoldFish
## A fast JSON and CBOR streaming library, without using memory

## Why GoldFish?
GoldFish can parse and generate very large [JSON](http://json.org) or [CBOR](http://cbor.io) documents.
It has some similarities to a [SAX](https://en.wikipedia.org/wiki/Simple_API_for_XML) parser, but doesn't use an event driven API, instead the user of the GoldFish interface is in control.
GoldFish intends to be the easiest and fastest JSON and CBOR streaming parser and serializer to use (even though not necessarily the fastest/easiest DOM parser/generator).

## Quick tutorial
### Converting a JSON stream to a CBOR stream
~~~~~~~~~~cpp
#include <goldfish/stream.h>
#include <goldfish/json_reader.h>
#include <goldfish/cbor_writer.h>

int main()
{
	using namespace goldfish;

	// Read the string literal as a stream and parse it as a JSON document
	// This doesn't really do any work, the stream will be read as we parse the document
	auto document = json::read(stream::read_string_literal("{\"a\":[1,2,3],\"b\":3.0}"));

	// Generate a stream on a vector, a CBOR writer around that stream and write
	// the JSON document to it
	// Note that all the streams need to be flushed to ensure that there any potentially
	// buffered data is serialized.
	stream::vector_writer output_stream;
	cbor::write(stream::ref(output_stream)).write(document);
	output_stream.flush();

	// output_stream.data contains the CBOR document
}
~~~~~~~~~~

### Parsing a JSON document with a schema
SAX parsers are notoriously more complicated to use than DOM parser. The order of the fields in a JSON object matters for a SAX parser.
Defining a schema (which is simply an ordering of the expected key names in the object) helps keep the code simple.
Note that the example below is O(1) in memory (meaning the amount of memory used does not depend on the size of the document)

~~~~~~~~~~cpp
#include <goldfish/stream.h>
#include <goldfish/json_reader.h>
#include <goldfish/schema.h>

int main()
{
	using namespace goldfish;

	static const schema s{ "a", "b", "c" };
	auto document = apply_schema(json::read(stream::read_string_literal("{\"a\":1,\"c\":3.0}")).as_map(), s);

	assert(document.read_value("a")->as_uint() == 1);
	assert(document.read_value("b") == nullopt);
	assert(document.read_value("c")->as_double() == 3.0);
	seek_to_end(document);
}
~~~~~~~~~~

### Generating a JSON or CBOR document
You can get a JSON or CBOR writer by calling json::create_writer or cbor::create_writer on an output stream.

~~~~~~~~~~cpp
int main()
{
	using namespace goldfish;
	
	stream::string_writer output_stream;
	auto map = json::create_writer(stream::ref(output_stream)).start_map();
	map.write("A", 1);
	map.write("B", "text");
	{
		const char binary_buffer[] = "Hello world!";
		auto stream = map.start_binary("C", sizeof(binary_buffer) - 1);
		stream.write_buffer(const_buffer_ref{ reinterpret_cast<const uint8_t*>(binary_buffer), sizeof(binary_buffer) - 1 });
		stream.flush();
	}
	map.flush();
	output_stream.flush();
	assert(output_stream.data.size() == 41);
	assert(output_stream.data == "{\"A\":1,\"B\":\"text\",\"C\":\"SGVsbG8gd29ybGQh\"}");
}
~~~~~~~~~~

Note how similar the code is to generate a CBOR document. The only change is the creation of the writer (cbor::create_writer instead of json::create_writer) and the type of output_stream (vector<uint8_t> is better suited to storing the binary data than std::string).
CBOR leads to some significant reduction in document size (the document above is 41 bytes in JSON but only 27 in CBOR format). Because CBOR supports binary data natively, there is also performance benefits (no need to encode the data in base64).

~~~~~~~~~~cpp
int main()
{
	using namespace goldfish;

	stream::vector_writer output_stream;
	auto map = cbor::create_writer(stream::ref(output_stream)).start_map();
	map.write("A", 1);
	map.write("B", "text");
	{
		const char binary_buffer[] = "Hello world!";
		auto stream = map.start_binary("C", sizeof(binary_buffer) - 1);
		stream.write_buffer(const_buffer_ref{ reinterpret_cast<const uint8_t*>(binary_buffer), sizeof(binary_buffer) - 1 });
		stream.flush();
	}
	map.flush();
	output_stream.flush();
	test(output_stream.data.size() == 27);
	test(output_stream.data == std::vector<uint8_t>{
		0xbf,                               // start map marker
		0x61,0x41,                          // key: "A"
		0x01,                               // value : uint 1
		0x61,0x42,                          // key : "B"
		0x64,0x74,0x65,0x78,0x74,           // value : "text"
		0x61,0x43,                          // key : "C"
		0x4c,0x48,0x65,0x6c,0x6c,0x6f,0x20,
		0x77,0x6f,0x72,0x6c,0x64,0x21,      // value : binary blob "Hello world!"
		0xff                                // end of map
	});
}
~~~~~~~~~~

## Documentation
### Streams
Goldfish parses documents from read streams and serializes documents to write streams.

Goldfish comes with a few readers: a reader over an in memory buffer (see stream::read_buffer_ref) or over a file (see stream::file_reader). It also provides a buffering (see stream::buffer). You might find yourself in a position where you want to implement your own stream, for example, as a network stream on top of your favorite network library.
Not to worry, the interface for a read stream is fairly straightforward, with a single read_buffer API:
~~~~~~~~~~cpp
struct read_stream
{
	// Copies some bytes from the stream to the "buffer"
	// Returns the number of bytes copied.
	// If the API returns something else than buffer.size(), the end of stream was reached.
	// Can throw on IO error.
	//
	// buffer_ref is an object that contains a pointer to the buffer (buffer.data() is the pointer)
	// as well as the number of bytes in the buffer (buffer.size())
	size_t read_buffer(buffer_ref buffer);
}
~~~~~~~~~~

