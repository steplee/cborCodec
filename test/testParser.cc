#include "cbor_parser.hpp"

#include "cstdio"

using namespace cbor;

struct MyVisitor {

	/*
	template <class T>
	inline void visit_root_value(T&& t) {
		printDepth();
		if constexpr (std::is_integral_v<T>) printf("root val: %ld\n", (int64_t)t);
		else printf("root val: <unhandled>\n");
	}

	template <class T>
	inline void visit_map_key(T&& t) {
		printDepth();
		if constexpr (std::is_integral_v<T>) printf("map key: %ld\n", (int64_t)t);
		else printf("map key: <unhandled>\n");
	}

	template <class T>
	inline void visit_map_value(T&& t) {
		printDepth();
		if constexpr (std::is_integral_v<T>) printf("map val: %ld\n", (int64_t)t);
		else printf("map val: <unhandled>\n");
	}

	template <class T>
	inline void visit_array_value(T&& t) {
		printDepth();
		if constexpr (std::is_integral_v<T>) printf("arr val: %ld\n", (int64_t)t);
		else printf("arr val: <unhandled>\n");
	}
	*/


	//
	// One-function API.
	// For example if `mode == Mode::map`, then `seqIdx % 2` tells us key or value.
	//

	template <class T>
	inline void visit_value(Mode mode, size_t seqIdx, T&& t) {
		printDepth();
		if constexpr (std::is_integral_v<T>) printf("val: %ld\n", (int64_t)t);
		else if constexpr (std::is_same_v<T, TextStringView>) printf("val: %s\n", std::string{t}.c_str());
		else if constexpr (std::is_same_v<T, ByteStringView>) printf("bytes of size %ld\n", t.size());
		else if constexpr (std::is_same_v<T, TypedArrayView>) {
			if (t.type == TypedArrayView::eInt32) {
				printf("typed int32 array of size %ld, endian: %d:\n", t.elementLength(), t.endianness);
				depth++;
				// std::vector<int32_t> vec = t.toVector<int32_t>();
				// std::vector<int32_t> vec = t.toVector<int32_t>();
				std::vector<int32_t> vec = reinterpret_cast<TypedArrayView*>(&t)->toVector<int32_t>();
				for (int i=0; i<t.elementLength(); i++) {
					printDepth();
					printf("%d\n", vec[i]);
				}
				depth--;
			} else
				printf("typed array of size %ld\n", t.elementLength());
		}
		else printf("val: <unhandled>\n");
	}

	inline void visit_begin_array() {
		printDepth();
		printf("begin array.\n");
		depth++;
	}
	inline void visit_end_array() {
		printDepth();
		printf("end array.\n");
		depth--;
	}

	inline void visit_begin_map() {
		printDepth();
		printf("begin map.\n");
		depth++;
	}
	inline void visit_end_map() {
		printDepth();
		printf("end map.\n");
		depth--;
	}

	inline void printDepth() {
		for (int i=0; i<depth*4; i++) printf(" ");
	}

	int depth = 0;
};


int main() {

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

	MyVisitor v;
	CborParser p(v, data.data(), data.size());
	p.parse();

	return 0;
}
