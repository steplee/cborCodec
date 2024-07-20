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

		constexpr size_t kIndefiniteLength = std::numeric_limits<size_t>::max();
		constexpr size_t kInvalidLength    = std::numeric_limits<size_t>::max() - 1;

		using byte                         = uint8_t;

    using TextStringView = std::basic_string_view<char>;
    using ByteStringView = std::basic_string_view<uint8_t>;

}
