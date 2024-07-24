#pragma once

#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#ifdef CBOR_CODEC_DEBUG
#define cborPrintf(...) printf(__VA_ARGS__);
#else
#define cborPrintf(...) {}
#endif


namespace cbor {

		constexpr size_t kIndefiniteLength = std::numeric_limits<size_t>::max();
		constexpr size_t kInvalidLength    = std::numeric_limits<size_t>::max() - 1;

		using byte                         = uint8_t;

    using TextStringView = std::basic_string_view<char>;
    using ByteStringView = std::basic_string_view<uint8_t>;

	struct True {};
	struct False {};
	struct Null {};


	namespace {
        template <class T> T maybeSwapBytes(bool littleEndian, const T& v) {
            return v;
        }
	}

    struct TypedArrayView {
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

        uint8_t endianness : 1; // 0 big, 1 little

        const uint8_t* data = 0;
        size_t byteLength   = 0;

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
            return byteLength / elementSize();
        }

        //
        // TODO: Allow some implicit conversions...
        //
        template <class T> inline T accessAs(int i) const {

            assert(endianness == 1 and "Only little-endian arrays are supported.");

            T t;
            memcpy(&t, data + elementSize() * i, elementSize());

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
            int i = 0;
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
			#pragma unroll
            for (int i = 0; i < 8; i++) { o |= ((v >> static_cast<uint64_t>(8 * (8 - i - 1))) & 0b1111'1111) << (8 * i); }
            return o;
        }
	
        inline uint64_t ntohll(uint64_t v) {
            uint64_t o = 0;
#pragma unroll
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
			uint16_t vvv = ntoh(vv);
			return *reinterpret_cast<int16_t*>(&vvv);
		}
        inline int32_t ntoh(const int32_t& v) {
			const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
			uint32_t vvv = ntoh(vv);
			return *reinterpret_cast<int32_t*>(&vvv);
		}
        inline int64_t ntoh(const int64_t& v) {
			const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
			uint64_t vvv = ntoh(vv);
			return *reinterpret_cast<int64_t*>(&vvv);
		}

		inline float ntoh(const float& v) {
			const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
			uint32_t vvv = ntoh(vv);
			return *reinterpret_cast<float*>(&vvv);
		}
		inline double ntoh(const double& v) {
			const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
			uint64_t vvv = ntoh(vv);
			return *reinterpret_cast<double*>(&vvv);
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
			uint16_t vvv = hton(vv);
			return *reinterpret_cast<int16_t*>(&vvv);
		}
        inline int32_t hton(const int32_t& v) {
			const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
			uint32_t vvv = hton(vv);
			return *reinterpret_cast<int32_t*>(&vvv);
		}
        inline int64_t hton(const int64_t& v) {
			const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
			uint64_t vvv = hton(vv);
			return *reinterpret_cast<int64_t*>(&vvv);
		}

		inline float hton(const float& v) {
			const uint32_t& vv = *reinterpret_cast<const uint32_t*>(&v);
			uint32_t vvv = hton(vv);
			return *reinterpret_cast<float*>(&vvv);
		}
		inline double hton(const double& v) {
			const uint64_t& vv = *reinterpret_cast<const uint64_t*>(&v);
			uint64_t vvv = hton(vv);
			return *reinterpret_cast<double*>(&vvv);
		}

}
