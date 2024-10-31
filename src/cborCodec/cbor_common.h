#pragma once

#include <cstdint>
#include <cassert>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>
#include <cstring>

// #define cborPrintf(...) printf( __VA_ARGS__ );
#define cborPrintf(...) {}

namespace cbor {

    constexpr size_t kIndefiniteLength = std::numeric_limits<size_t>::max();
    constexpr size_t kInvalidLength    = std::numeric_limits<size_t>::max() - 1;

    using byte                         = uint8_t;

    // using TextStringView               = std::basic_string_view<char>;
    // using ByteStringView               = std::basic_string_view<uint8_t>;

    struct True { };
    struct False { };
    struct Null { };

    namespace {
        template <class T> T maybeSwapBytes(bool littleEndian, const T& v) {
            return v;
        }
    }

	//
	// Base class of `TextBuffer`, `ByteBuffer`, and `TypedArrayBuffer`.
	//
	// The history of these types is that they used to just be sized pointers to the input buffer.
	// But this only works when the input *is* a buffer, but does not work when the input is a file.
	//
	// When the input is a buffer, it's most efficient to just use a pointer+size, because no malloc and no copies are needed.
	// For file inputs, we could keep a window of multiple blocks, and in practice there may never be an issue.
	// But for a completely correct implementation (supporting unrealistic but possible very large strings or typed arrays), we must
	// have this buffer class own it's data for file inputs.
	//
	// So this class supports both scenarios. If `isView` is false then we own the pointer and the destructor frees it.
	// Otherwise we just have a very efficient view into the input buffer (whose lifetime is ASSUMED to outlive the parser's, and is const).
	//
	// Note that those classes subclass this one which makes std::variant<> easy to use while not duplicating code.
	//
	struct DataBuffer {
		// std::vector<uint8_t> buf;
		const byte* buf = 0;
		std::size_t len = 0;
		bool isView = true;

		inline DataBuffer();

		DataBuffer(const DataBuffer& o) = delete;
		DataBuffer operator=(const DataBuffer& o) = delete;

		inline DataBuffer(DataBuffer&& o) : buf(o.buf), len(o.len), isView(o.isView) {
			o.isView = false;
			o.buf = nullptr;
			o.len = 0;
		}
		inline DataBuffer& operator=(DataBuffer&& o) {
			isView = o.isView;
			buf = o.buf;
			len = o.len;
			o.isView = false;
			o.buf = nullptr;
			o.len = 0;
			return *this;
		}

		// view
		inline DataBuffer(const uint8_t* data, std::size_t len) : buf(data), len(len), isView(true) {
		}

		// allocate
		inline DataBuffer(std::size_t len) : len(len), isView(false), buf(new byte[len]) {
		}

		inline ~DataBuffer() {
			if (isView) {
			} else if (buf) {
				delete[] buf;
			}
			buf = 0;
			len = 0;
		}

		inline std::size_t size() const { return len; }

	};
	struct TextBuffer : public DataBuffer {
		using DataBuffer::DataBuffer;
		inline TextBuffer(DataBuffer&& db) : DataBuffer(std::move(db)) {}
		inline TextBuffer(const std::string& s) : DataBuffer((const uint8_t*)s.data(), s.length()) {}
		inline TextBuffer(const char* s) : DataBuffer((const uint8_t*)s, strlen(s)) {}

		std::string_view asStringView() const {
			return std::string_view((const char*)buf, len);
		}
		inline char operator[](std::size_t i) const { return (char )buf[i]; }
	};
	struct ByteBuffer : public DataBuffer {
		inline byte operator[](std::size_t i) const { return buf[i]; }
		inline std::vector<uint8_t> clone() const {
			return std::vector<uint8_t>((const uint8_t*)buf, (const uint8_t*)buf + size());
		}
	};

    struct TypedArrayBuffer : public DataBuffer {


        enum Type {
            eInt8,
            eUInt8,
            eInt16,
            eUInt16,
            eInt32,
            eUInt32,
            eInt64,
            eUInt64,
            eFloat32,
            eFloat64,
        } type;

		inline TypedArrayBuffer(DataBuffer&& db, TypedArrayBuffer::Type type, uint8_t endianness)
			:
			   type(type)
			  ,	DataBuffer(std::move(db))
			  , endianness(endianness)
		{
		}

		inline TypedArrayBuffer(const float* data, size_t len) : DataBuffer((const uint8_t*)data, sizeof(std::remove_pointer_t<decltype(data)>)*len), type(Type::eFloat32) {}
		inline TypedArrayBuffer(const double* data, size_t len) : DataBuffer((const uint8_t*)data, sizeof(std::remove_pointer_t<decltype(data)>)*len), type(Type::eFloat64) {}
		inline TypedArrayBuffer(const uint8_t* data, size_t len) : DataBuffer((const uint8_t*)data, sizeof(std::remove_pointer_t<decltype(data)>)*len), type(Type::eUInt8) {}
		inline TypedArrayBuffer(const int32_t* data, size_t len) : DataBuffer((const uint8_t*)data, sizeof(std::remove_pointer_t<decltype(data)>)*len), type(Type::eInt32) {}
		inline TypedArrayBuffer(const int64_t* data, size_t len) : DataBuffer((const uint8_t*)data, sizeof(std::remove_pointer_t<decltype(data)>)*len), type(Type::eInt64) {}
		inline TypedArrayBuffer(const uint64_t* data, size_t len) : DataBuffer((const uint8_t*)data, sizeof(std::remove_pointer_t<decltype(data)>)*len), type(Type::eUInt64) {}

        uint8_t endianness : 1; // 0 big, 1 little


        inline size_t elementSize() const {
            switch (type) {
            case eInt8:
            case eUInt8: return 1;
            case eInt16:
            case eUInt16: return 2;
            case eInt32:
            case eUInt32:
            case eFloat32: return 4;
            case eInt64:
            case eUInt64:
            case eFloat64: return 8;
            };
            assert(false);
        }

        inline size_t elementLength() const {
            return len / elementSize();
        }

        //
        // TODO: Allow some implicit conversions...
        //
        template <class T> inline T accessAs(int i) const {

            assert(endianness == 1 and "Only little-endian arrays are supported.");

            T t;
            memcpy(&t, buf + elementSize() * i, elementSize());

            if constexpr (std::is_same_v<T, int8_t>) {
                assert(type == eInt8);
                return maybeSwapBytes(endianness, t);
            } else if constexpr (std::is_same_v<T, uint8_t>) {
                assert(type == eUInt8);
                return maybeSwapBytes(endianness, t);
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                assert(type == eUInt16);
                return maybeSwapBytes(endianness, t);
            } else if constexpr (std::is_same_v<T, int16_t>) {
                assert(type == eInt16);
                return maybeSwapBytes(endianness, t);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                assert(type == eUInt32);
                return maybeSwapBytes(endianness, t);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                assert(type == eInt32);
                return maybeSwapBytes(endianness, t);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                assert(type == eUInt64);
                return maybeSwapBytes(endianness, t);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                assert(type == eInt64);
                return maybeSwapBytes(endianness, t);

                // FIXME: Does endianess switch for float32/float64????
            } else if constexpr (std::is_same_v<T, float>) {
                assert(type == eFloat32);
                // return maybeSwapBytes(endianness, t);
                return t;
            } else if constexpr (std::is_same_v<T, double>) {
                assert(type == eFloat64);
                // return maybeSwapBytes(endianness, t);
                return t;
            }

            assert(false);
        }

        template <class It> inline void copyTo(It begin, It end) const {
            uint32_t i = 0;
            while (begin != end) {

                assert(i < elementLength());

                *begin = accessAs<std::remove_reference_t<decltype(*begin)>>(i++);
                begin++;
            }
        }

        template <class T> inline std::vector<T> toVector() const {
            std::vector<T> vec(elementLength());
            copyTo(vec.begin(), vec.end());
            return vec;
        }
    };

    // WARNING: TEST THIS.
    // FIXME: MISLEADING NAME: this hton should be conditional -- this swaps unconditionally (assumes WE are little endian)

    inline uint64_t htonll(uint64_t v) {
        uint64_t o = 0;
// #pragma unroll
        for (int i = 0; i < 8; i++) { o |= ((v >> static_cast<uint64_t>(8 * (8 - i - 1))) & 0b1111'1111) << (8 * i); }
        return o;
    }

    inline uint64_t ntohll(uint64_t v) {
        uint64_t o = 0;
// #pragma unroll
        for (int i = 0; i < 8; i++) { o |= ((v >> static_cast<uint64_t>(8 * (8 - i - 1))) & 0b1111'1111) << (8 * i); }
        return o;
    }

    inline uint16_t ntoh(const uint16_t& v) {
        return ntohs(v);
    }
    inline uint32_t ntoh(const uint32_t& v) {
        return ntohl(v);
    }
    inline uint64_t ntoh(const uint64_t& v) {
        return ntohll(v);
    }

    inline int16_t ntoh(const int16_t& v) {
        const uint16_t& vv = *reinterpret_cast<const uint16_t*>(&v);
        uint16_t vvv       = ntoh(vv);
        return *reinterpret_cast<int16_t*>(&vvv);
    }
    inline int32_t ntoh(const int32_t& v) {
        const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
        uint32_t vvv       = ntoh(vv);
        return *reinterpret_cast<int32_t*>(&vvv);
    }
    inline int64_t ntoh(const int64_t& v) {
        const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
        uint64_t vvv       = ntoh(vv);
        return *reinterpret_cast<int64_t*>(&vvv);
    }

    inline float ntoh(const float& v) {
        const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
        uint32_t vvv       = ntoh(vv);
        float *out = reinterpret_cast<float*>(&vvv);
		return *out;
    }
    inline double ntoh(const double& v) {
        const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
        uint64_t vvv       = ntoh(vv);
        double *out = reinterpret_cast<double*>(&vvv);
		return *out;
    }

    inline uint16_t hton(const uint16_t& v) {
        return htons(v);
    }
    inline uint32_t hton(const uint32_t& v) {
        return htonl(v);
    }
    inline uint64_t hton(const uint64_t& v) {
        return htonll(v);
    }

    inline int16_t hton(const int16_t& v) {
        const uint16_t& vv = *reinterpret_cast<const uint16_t*>(&v);
        uint16_t vvv       = hton(vv);
        return *reinterpret_cast<int16_t*>(&vvv);
    }
    inline int32_t hton(const int32_t& v) {
        const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
        uint32_t vvv       = hton(vv);
        return *reinterpret_cast<int32_t*>(&vvv);
    }
    inline int64_t hton(const int64_t& v) {
        const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
        uint64_t vvv       = hton(vv);
        return *reinterpret_cast<int64_t*>(&vvv);
    }

    inline float hton(const float& v) {
        const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
        uint32_t vvv       = hton(vv);
        float *out = reinterpret_cast<float*>(&vvv);
		return *out;
    }
    inline double hton(const double& v) {
        const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
        uint64_t vvv       = hton(vv);
        double *out = reinterpret_cast<double*>(&vvv);
		return *out;
    }

}
