


#include <gtest/gtest.h>

#include <fstream>

#include "cborCodec/cbor_encoder.hpp"
#include "json_printer.hpp"


TEST(EncoderParser, TestCodeDecodeSimple) {

	CborEncoder encoder;

	encoder.begin_map(5);

	encoder.push_value("key0");
	encoder.push_value("val0");

	encoder.push_value("key1");
	encoder.push_value("val1");

	encoder.push_value("key2");
	encoder.push_value(1l);

	encoder.push_value("key3");
	encoder.push_value(-10000000l);

	encoder.push_value("key4");
	encoder.begin_array(1);
	encoder.begin_array(1);
	encoder.begin_array(1);
	encoder.push_value("deep string");

	auto data = encoder.finish();


	{
		CborParser p(BinStreamBuffer{data.data(), data.size()});
		JsonPrinter jp(p);

		// std::cout << " - json:\n" << v.finish() << "\n";
		EXPECT_EQ(jp.os,
			R"([{"key0":"val0","key1":"val1","key2":1,"key3":-10000000,"key4":[[["deep string"]]]}])"
		);
	}

}
