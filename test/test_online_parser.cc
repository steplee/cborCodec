#include <gtest/gtest.h>

#include "cborCodec/cbor_parser.hpp"
#include "json_printer.hpp"

using namespace cbor;


TEST(Parser, Simple) {

	std::vector<uint8_t> data {
		// 1
		0b000'00001,

		// "Hello"
		0b011'00101,
			(uint8_t)'h',
			(uint8_t)'e',
			(uint8_t)'l',
			(uint8_t)'l',
			(uint8_t)'o',

		// Indef string -- not allowed!
		// 0b011'11111, 0b111'11111,
		// Indef byte string -- not allowed!
		// 0b010'11111, 0b111'11111,

		// Map with two kv pairs.
		0b101'00010,
			0b000'00001,
			0b000'00010,
			0b000'00011,
			0b000'00100,


		// Map with one kv pair.
		0b101'00001,
			0b000'00001,
			0b000'00010,


		// Indef Array
		0b100'11111,
			0b000'00001,
			0b000'00010,
			0b111'11111,
			
		// Map with map with an array
		0b101'00001,
			0b011'00101,
				(uint8_t)'o',
				(uint8_t)'u',
				(uint8_t)'t',
				(uint8_t)'e',
				(uint8_t)'r',

			0b101'00001,
				0b000'00001,
				0b100'11111,
					0b000'00001,
					0b111'11111,

		// Typed array
		(0b110'00000 | 24), // Tag with one following byte for tag number
		0b010'01110, // (0: int not float) | (1: signed) | (1: little endian) | (10: 32 bit) ===> int32_t
			0b010'01000, // byte string of length 8 (two int32_t elements)
				1, 0, 0, 0, // = 1
				0, 1, 0, 0, // = 255


	};

	using namespace cbor;
	CborParser p(BinStreamBuffer{data.data(), data.size()});

	JsonPrinter jp(p);
	printf(" - json:\n%s\n", jp.os.c_str());

}

TEST(Parser, ConsumeMapButStop) {

	std::vector<uint8_t> data {
		// Map with map with an array
		0b101'00001,
			0b011'00101,
				(uint8_t)'o',
				(uint8_t)'u',
				(uint8_t)'t',
				(uint8_t)'e',
				(uint8_t)'r',

			0b101'00001,
				0b000'00001,
				0b100'11111,
					0b000'00001,
					0b111'11111,

			0b011'00110,
				(uint8_t)'o',
				(uint8_t)'u',
				(uint8_t)'t',
				(uint8_t)'e',
				(uint8_t)'r',
				(uint8_t)'2',

			0b101'00001,
				0b000'00001,
				0b100'11111,
					0b000'00001,
					0b111'11111,
	};

	using namespace cbor;

	int nProcessedWithoutStop = 0;
	int nProcessedWithStop = 0;

	{
		CborParser p(BinStreamBuffer{data.data(), data.size()});

		p.next();
		p.consumeMap(2, [&](Item&& k, Item&& v) {
				nProcessedWithoutStop++;
		});
	}
	{
		CborParser p(BinStreamBuffer{data.data(), data.size()});

		p.next();
		p.consumeMap(2, [&](Item&& k, Item&& v) {
				nProcessedWithStop++;
				return false;
		});
	}

	EXPECT_EQ(nProcessedWithoutStop, 2);
	EXPECT_EQ(nProcessedWithStop, 1);

}
