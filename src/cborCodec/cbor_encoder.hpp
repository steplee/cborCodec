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
		auto kInvalidCursor = std::numeric_limits<size_t>::max();
	}


	struct OutBinStreamBuffer {
		std::vector<uint8_t> data;
		size_t cursor = kInvalidCursor;

		inline OutBinStreamBuffer() {
			cursor = kInvalidCursor;
		}
		inline OutBinStreamBuffer(size_t size) {
			data.resize(size);
			cursor = 0;
		}

		inline void write(const uint8_t* bytes, size_t len) {
			// if (cursor + len >= data.size()) data.resize(data.size() * 2);
			if (cursor + len >= data.size()) {
				if (data.size() * 2 >= cursor + len)
					data.resize(data.size() * 2);
				else {
					size_t targetSize = data.size() * 2;
					while (cursor + len >= targetSize) {
						targetSize *= 2;
					}
					data.resize(targetSize);
				}
			}

			memcpy(data.data() + cursor, bytes, len);
			cursor += len;
		}

		inline std::vector<uint8_t> finish() {
            if (cursor != kInvalidCursor) data.resize(cursor);
			// return std::move(data);
			return data;
		}

		inline bool valid() const { return cursor != kInvalidCursor; }
	};


	struct OutBinStreamFile {
		std::ofstream ofs;

		inline OutBinStreamFile() {
		}
		inline OutBinStreamFile(const std::string &path) : ofs(path, std::ios_base::out | std::ios_base::binary) {
		}
		inline OutBinStreamFile(std::ofstream &&o) : ofs(std::move(o)) {
		}

		inline void write(const uint8_t* bytes, size_t len) {
			ofs.write((const char*)bytes,len);
		}

		inline void finish() {
			ofs.flush();
		}

		inline bool valid() const { return ofs.good(); }
	};

	struct CborEncoder {
		private:

		// Only one will be active.
		// NOTE: This used to be templated, but it's simpler to support custom type encoders
		//       by having it non-templated.
		OutBinStreamBuffer strm1;
		OutBinStreamFile strm2;

		public:

		inline CborEncoder(std::ofstream&& ofs) : strm2(std::move(ofs)) {}
		inline CborEncoder(const std::string& path) : strm2(path) {}
		inline CborEncoder() : strm1(1<<10) {}

		inline std::vector<uint8_t> finish() {
			if (strm1.valid()) return strm1.finish();
			else strm2.finish();
			return {};
		}

		template <class K, class V>
		inline void push_key_value(const K& k, const V& v) {
			push_value(k);
			push_value(v);
		}

		void push_value(uint8_t v);
		void push_value(int64_t v);
		void push_value(uint64_t v);

		void push_value(float v);
		void push_value(double v);


		void push_value(True);
		void push_value(False);
		void push_value(Null);

		// Text string.
		// void push_value(const std::string& s); // Already captured by TextBuffer
		void push_value(const char* s, size_t len);
		void push_value(const TextBuffer& tb);

		// Byte string.
		void push_value(const uint8_t* bytes, size_t len);
		void push_value(const ByteBuffer& bb);

		void begin_array(size_t size);
		void begin_map(size_t sizeInPairs);
		void end_indefinite();

		void push_typed_array(const float* vs, size_t len);
		void push_typed_array(const double* vs, size_t len);
		void push_typed_array(const uint8_t* vs, size_t len);
		void push_typed_array(const int32_t* vs, size_t len);
		void push_typed_array(const int64_t* vs, size_t len);
		void push_typed_array(const uint64_t* vs, size_t len);
		void push_value(const TypedArrayBuffer& tab);

		template <typename Bool, typename T=std::enable_if_t<std::is_same<Bool, bool>{}>>
		inline void push_value(Bool) {
			assert(false && "Do not use push_value(bool). Use push_value(True) or push_value(False).");
		}
		
		template <typename T, typename V=std::enable_if_t<std::is_member_function_pointer_v<decltype(&T::encodeCbor)>>>
		inline void push_value(const T& t) {
			t.encodeCbor(*this);
		}

		// private:


		void push_pos_integer(byte majorType, uint64_t v);
		void push_neg_integer(byte majorType, int64_t v);

		template <class T>
		inline void write(const T& t) {
			static_assert(std::is_fundamental<T>::value, "bad write<T> call -- only primitives allowed.");
			if (strm1.valid()) strm1.write((const uint8_t*)&t, sizeof(T));
			else strm2.write((const uint8_t*)&t, sizeof(T));
		}
		inline void write(const uint8_t* d, size_t len) {
			if (strm1.valid()) strm1.write(d, len);
			else strm2.write(d, len);
		}

	};

	inline void CborEncoder::push_value(uint8_t v) {
		push_pos_integer(0b000, v);
	}
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
			write(static_cast<byte>((majorType << 5) | 0b11111));
		}

		else if (v < 24) {
			write(static_cast<byte>(m | static_cast<uint8_t>(v)));
		} else if (v < (1lu << 8lu)) {
			write(static_cast<byte>(m | static_cast<uint8_t>(24)));
			write((static_cast<uint8_t>(v)));
		} else if (v < (1lu << 16lu)) {
			write(static_cast<byte>(m | static_cast<uint8_t>(25)));
			write(htons(static_cast<uint16_t>(v)));
		} else if (v < (1lu << 32lu)) {
			write(static_cast<byte>(m | static_cast<uint8_t>(26)));
			write(htonl(static_cast<uint32_t>(v)));
		} else {
			write(static_cast<byte>(m | static_cast<uint8_t>(27)));
			write(htonll(static_cast<uint64_t>(v)));
		}
	}

	// I think...
	inline void CborEncoder::push_neg_integer(byte majorType, int64_t v0) {
		uint64_t v = -v0 - 1;
		push_pos_integer(majorType, v);
	}

	inline void CborEncoder::push_value(False v) {
		write(static_cast<byte>((0b111 << 5) | 20));
	}
	inline void CborEncoder::push_value(True v) {
		write(static_cast<byte>((0b111 << 5) | 21));
	}
	inline void CborEncoder::push_value(Null v) {
		write(static_cast<byte>((0b111 << 5) | 22));
	}

	inline void CborEncoder::push_value(float v) {
		write(static_cast<byte>((0b111 << 5) | 26));
		v = hton(v);
		write(v);
	}
	inline void CborEncoder::push_value(double v) {
		write(static_cast<byte>((0b111 << 5) | 27));
		v = hton(v);
		write(v);
	}

	inline void CborEncoder::begin_array(size_t size) {
		push_pos_integer(0b100, size);
	}
	inline void CborEncoder::begin_map(size_t size) {
		push_pos_integer(0b101, size);
	}
	inline void CborEncoder::end_indefinite() {
		write(byte{0b111'11111});
	}

	/*
	inline void CborEncoder::push_value(const std::string& s) {
		push_pos_integer(0b011, s.length());
		write((const uint8_t*)s.data(), s.length());
	}
	*/
	inline void CborEncoder::push_value(const char* s, size_t len) {
		push_pos_integer(0b011, len);
		write((const uint8_t*)s, len);
	}
	inline void CborEncoder::push_value(const uint8_t* s, size_t len) {
		push_pos_integer(0b010, len);
		write(s, len);
	}

	inline void CborEncoder::push_value(const TextBuffer& tb) {
		push_pos_integer(0b011, tb.len);
		write((const uint8_t*)tb.buf, tb.len);
	}
	inline void CborEncoder::push_value(const ByteBuffer& bb) {
		push_pos_integer(0b010, bb.len);
		write((const uint8_t*)bb.buf, bb.len);
	}

	// WARNING: Once again, we assume the host machine is little-endian.

	inline void CborEncoder::push_value(const TypedArrayBuffer& tab) {
		switch (tab.type) {
			case TypedArrayBuffer::eUInt8:
				push_typed_array((const uint8_t*)tab.buf, tab.elementLength());
				break;
			case TypedArrayBuffer::eFloat32:
				push_typed_array((const float*)tab.buf, tab.elementLength());
				break;
			case TypedArrayBuffer::eFloat64:
				push_typed_array((const double*)tab.buf, tab.elementLength());
				break;
			case TypedArrayBuffer::eInt32:
				push_typed_array((const int32_t*)tab.buf, tab.elementLength());
				break;
			case TypedArrayBuffer::eInt64:
				push_typed_array((const int64_t*)tab.buf, tab.elementLength());
				break;
			case TypedArrayBuffer::eUInt64:
				push_typed_array((const uint64_t*)tab.buf, tab.elementLength());
				break;
			default:
				assert(false && "unsupported");
				break;
		}
	}

	inline void CborEncoder::push_typed_array(const float* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b11101 });
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(float) * len);
	}
	inline void CborEncoder::push_typed_array(const double* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b11110 });
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(double) * len);
	}

	inline void CborEncoder::push_typed_array(const uint8_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b00100 }); // integral, unsigned, little-endian, size=8bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(uint8_t) * len);
	}
	inline void CborEncoder::push_typed_array(const int32_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b01110 }); // integral, signed, little-endian, size=32bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(int32_t) * len);
	}
	inline void CborEncoder::push_typed_array(const int64_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b01111 }); // integral, signed, little-endian, size=64bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(int64_t) * len);
	}
	inline void CborEncoder::push_typed_array(const uint64_t* vs, size_t len) {
		push_pos_integer(0b110, uint64_t { 0b010'00000 | 0b00111 }); // integral, unsigned, little-endian, size=64bit
		push_value(reinterpret_cast<const uint8_t*>(vs), sizeof(uint64_t) * len);
	}


}
