#pragma once

#include <cassert>
#include <cstdint>
#include <deque>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#include <arpa/inet.h>
#include <cstring>

#include "cbor_common.h"

//
// A cbor encoder.
//
// This thing maintains no state and thus has no debugging assertions or error handling or anything like that.
// It's all up to the user to use properly!
//
// FIXME: THIS ASSUMES THAT THE HOST IS LITTLE-ENDIAN.
//
// WARNING: Beware the implicit casts.
//

namespace cbor {

    namespace {
	}


	struct OutBinStream_Vector {

		std::vector<uint8_t> data;
		size_t cursor = 0;

		inline OutBinStream_Vector() {
			data.resize(4096);
		}

		inline void write(const uint8_t* bytes, size_t len) {
			if (cursor + len >= data.size()) data.resize(data.size() * 2);
			memcpy(data.data() + cursor, bytes, len);
			cursor += len;
		}

		inline std::vector<uint8_t> finish() {
			data.resize(cursor);
			return std::move(data);
		}


	};

	struct CborEncoder {
		OutBinStream_Vector strm;

		inline std::vector<uint8_t> finish() {
			return strm.finish();
		}

		/*
		template<class T>
		void push_value(const std::enable_if_t<std::is_integral_v<T>,T> s);

		template<class T>
		void push_value(const std::enable_if_t<std::is_floating_point_v<T>,T> s);
		*/

		/*
		void push_value(int8_t v);
		void push_value(int16_t v);
		void push_value(int32_t v);
		void push_value(int64_t v);
		void push_value(uint8_t v);
		void push_value(uint16_t v);
		void push_value(uint32_t v);
		void push_value(uint64_t v);
		*/
		void push_value(int64_t v);
		void push_value(uint64_t v);

		void push_value(float v);
		void push_value(double v);

		// Text string.
		void push_value(const std::string& s);
		void push_value(const char* s, size_t len);

		// Byte string.
		void push_value(const uint8_t* bytes, size_t len);

		void begin_array(size_t size);
		void begin_map(size_t sizeInPairs);
		void end_indefinite();

		void push_typed_array(const float* vs, size_t len);
		void push_typed_array(const double* vs, size_t len);
		void push_typed_array(const uint8_t* vs, size_t len);
		void push_typed_array(const int32_t* vs, size_t len);
		void push_typed_array(const int64_t* vs, size_t len);
		void push_typed_array(const uint64_t* vs, size_t len);

		private:


		void push_pos_integer(byte majorType, uint64_t v);
		void push_neg_integer(byte majorType, int64_t v);

		template <class T>
		inline void write(const T& t) {
			static_assert(std::is_fundamental<T>::value, "bad write<T> call -- only primitives allowed.");
			strm.write((const uint8_t*)&t, sizeof(T));
		}
		inline void write(const uint8_t* d, size_t len) {
			strm.write(d, len);
		}

	};

	inline void CborEncoder::push_value(int64_t v) {
		if (v >= 0)
			push_pos_integer(0b000, v);
		else
			push_neg_integer(0b001, v);
	}
	inline void CborEncoder::push_value(uint64_t v) {
		push_pos_integer(0b000, v);
	}

	// This ought to be two functions (one for lengths, one for normal integers...)
	inline void CborEncoder::push_pos_integer(byte majorType, uint64_t v) {

		byte m = majorType << 5;

		if (majorType != 0 and majorType != 1 and v == kIndefiniteLength) {
			write(byte{(majorType << 5) | 0b11111});
		}

		else if (v < 24) {
			write(byte{m | static_cast<uint8_t>(v)});
		} else if (v < (1lu << 8lu)) {
			write(byte{m | static_cast<uint8_t>(24)});
			write((static_cast<uint8_t>(v)));
		} else if (v < (1lu << 16lu)) {
			write(byte{m | static_cast<uint8_t>(25)});
			write(htons(static_cast<uint16_t>(v)));
		} else if (v < (1lu << 32lu)) {
			write(byte{m | static_cast<uint8_t>(26)});
			write(htonl(static_cast<uint32_t>(v)));
		} else {
			write(byte{m | static_cast<uint8_t>(27)});
			write(htonll(static_cast<uint64_t>(v)));
		}
	}

	// I think...
	inline void CborEncoder::push_neg_integer(byte majorType, int64_t v0) {
		uint64_t v = -v0 - 1;
		push_pos_integer(majorType, v);
	}

	void CborEncoder::push_value(float v) {
		write(byte{(0b111 << 5) | 26});
		v = hton(v);
		write(v);
	}
	void CborEncoder::push_value(double v) {
		write(byte{(0b111 << 5) | 27});
		v = hton(v);
		write(v);
	}

	void CborEncoder::begin_array(size_t size) {
		push_pos_integer(0b100, size);
	}
	void CborEncoder::begin_map(size_t size) {
		push_pos_integer(0b101, size);
	}
	void CborEncoder::end_indefinite() {
		write(byte{0b111'11111});
	}

	void CborEncoder::push_value(const std::string& s) {
		push_pos_integer(0b011, s.length());
		write((const uint8_t*)s.data(), s.length());
	}
	void CborEncoder::push_value(const char* s, size_t len) {
		push_pos_integer(0b011, len);
		write((const uint8_t*)s, len);
	}
	void CborEncoder::push_value(const uint8_t* s, size_t len) {
		push_pos_integer(0b010, len);
		write(s, len);
	}

	// WARNING: Once again, we assume the host machine is little-endian.

	void CborEncoder::push_typed_array(const float* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b11101 });
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(float) * len);
	}
	void CborEncoder::push_typed_array(const double* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b11110 });
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(double) * len);
	}

	void CborEncoder::push_typed_array(const uint8_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b00100 }); // integral, unsigned, little-endian, size=8bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(uint8_t) * len);
	}
	void CborEncoder::push_typed_array(const int32_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b01110 }); // integral, signed, little-endian, size=32bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(int32_t) * len);
	}
	void CborEncoder::push_typed_array(const int64_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b01111 }); // integral, signed, little-endian, size=64bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(int64_t) * len);
	}
	void CborEncoder::push_typed_array(const uint64_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b00111 }); // integral, unsigned, little-endian, size=64bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(uint64_t) * len);
	}


}
