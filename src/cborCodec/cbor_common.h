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
