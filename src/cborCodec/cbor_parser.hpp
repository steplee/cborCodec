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
// A cbor parser.
//
// Looking at [RFC8949](https://www.rfc-editor.org/rfc/rfc8949.html#name-major-types) I was worried this
// might be difficult at first. But it's a really well designed and easy to implement format.
//
// This class handles all parsing logic. But to allow efficient and full decoding support, the
// user must provide a Visitor type that may need to manage some state itself.
//
// The API is currently that the Visitor is notified of begin/end array/map events, as well as all values.
// The `visit_value` provides the `Mode` (either root/array/map), the `sequenceIdx` and of course the value.
// `sequenceIdx` tells us which array element we are on if Mode is `array`.
// If Mode is `map` then `sequenceIdx % 2` tells whether the value is a key or value.
// So it's up to the visitor to actually aggregate the key-value pairs.
//
// I do not support indefinite length text/byte strings, which will never come up.
// Indefinite arrays and maps are supported!
//
// TODO: Write tests (including on random large files and for negative int decoding, and the `ntohll` function, ...)
//
// TODO: Think through a client helper hpp file that allows creating combinators and putting them together...
//
// FIXME: THIS ASSUMES THAT THE HOST IS LITTLE-ENDIAN.
//
//

namespace cbor {

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

        template <class It> inline void copyTo(It&& begin, It&& end) const {
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

    namespace {

        enum class Mode : uint8_t { root, array, map };

        struct BeginArray {
            size_t len;
        };
        struct BeginMap {
            size_t len;
        };
        struct EndIndefiniteArray { };
        struct EndIndefiniteMap { };
        struct EndSizedArray { };
        struct EndSizedMap { };

        struct State {
            Mode mode;
            size_t byteIdx;     // This is not update, it's frozen at start.
            size_t sequenceIdx; // This is updated *in-place*
            size_t len;         // If the mode is array or map, how many elements are there? (or kIndefiniteLength)

            inline bool operator==(const Mode& m) {
                return mode == m;
            }
        };

        struct BinStream_Span {
            inline BinStream_Span(const uint8_t* data, size_t len)
                : data(data)
                , len(len) {
            }

            const uint8_t* data;
            size_t len;
            size_t cursor = 0;

            inline bool hasMore(size_t n = 1) const {
                return cursor + n <= len;
            }
            inline uint8_t nextByte() {
                assert(hasMore());
                return data[cursor++];
            }
            template <class V> inline V nextValue() {
                auto out = nextBytes(sizeof(V));
                return *reinterpret_cast<const V*>(out); // NOTE: Must I memcpy to align to boundary?
            }
            inline const uint8_t* nextBytes(size_t n) {
                assert(hasMore(n));
                auto out = data + cursor;
                cursor += n;
                return out;
            }
        };

        //
        // When we resize the buffer, we invalidate all pointers.
        // This is NOT managed by the code -- any old references may be invalid and this is UNSAFE.
        //
        // The only way to truly handle this bug-free would be either to store the entire file in RAM, or
        // track pointer lifetimes etc., essentially creating a GC, memory pool, smart ptr, or something like that.
        // On the other hand we can provide long-term access, that is techincally buggy on large files if the user
        // access things from a long time ago, but in most use-cases should work.
        //
        // Use a deque so that we can push/pop new/old blocks with finer granularity. Imagine we just used one block
        // and we read 5 bytes which pushed us over the 16M buffer size. Then _all_ references would immediately be broken.
        // But by using multiple blocks -- this is helped because we only invalidate smaller+older regions.
        //
        struct Buffer {
            static constexpr size_t blockSize = 4 * (1 << 20); // 4M
            static constexpr size_t maxBlocks = 4;

            std::deque<std::vector<uint8_t>> blocks;
            size_t cursor = 0;

            inline Buffer() {
                pushNewBlock();
            }

            inline void pushNewBlock() {
                std::vector<uint8_t> block(blockSize);
                if (blocks.size() >= maxBlocks) blocks.pop_front();
                blocks.push_back(std::move(block));
                cursor = 0;
            }

            uint8_t* alloc(size_t n) {

                if (n >= blockSize) { throw std::runtime_error("alloc(n) n must be less than blockSize."); }

                if (cursor + n >= blockSize) { pushNewBlock(); }

                auto& block  = blocks.back();

                uint8_t* out = &block[cursor];
                cursor += n;
                return out;
            }
        };

        struct BinStream_File {

            inline BinStream_File(std::ifstream& ifs)
                : ifs(ifs) {
            }

            inline bool hasMore() const {
                return !ifs.eof();
            }
            inline uint8_t nextByte() {
                assert(hasMore());
                uint8_t v;
                ifs.read((char*)&v, 1);
                return v;
            }
            template <class V> inline V nextValue() {
                auto out = nextBytes(sizeof(V));
                return *reinterpret_cast<const V*>(out); // NOTE: Must I memcpy to align to boundary?
            }
            inline const uint8_t* nextBytes(size_t n) {
                // assert(hasMore(n));
                uint8_t* out = buf.alloc(n);
                ifs.read((char*)out, n);
                assert(ifs.gcount() == n);
                return out;
            }

            Buffer buf;
            std::ifstream& ifs;
        };

    }

    template <class Visitor> class CborParser {

    public:
        CborParser(Visitor& vtor, const uint8_t* data, size_t len);

        void parse();

    private:
        inline void pushState(const Mode& mode, size_t len = kInvalidLength) {
            stateStack.push_back(State { mode, strm.cursor, 0, len });
        }

        template <class V> void advance(V&& value);

        // Called after processing every item (for arrays and maps, this after they **close**, not open)
        // This can be called in `advance` and may recursively call `advance` (e.g. to cascade the closing of arrays)
        void post_item();

        void parse_root();
        void parse_item();
        void parse_object();
        void parse_array();

        // uint64_t parse_uint();
        // int64_t parse_nint();
        // ByteStringView parse_byte_string();
        // TextStringView parse_text_string();

        Visitor& vtor;
        BinStream_Span strm;
        // const uint8_t *data;
        // size_t len;

        std::vector<State> stateStack;
    };

    template <class Visitor>
    CborParser<Visitor>::CborParser(Visitor& vtor, const uint8_t* data, size_t len)
        // : vtor(vtor), data(data), len(len)
        : vtor(vtor)
        , strm(data, len) {
    }

    template <class Visitor> void CborParser<Visitor>::post_item() {
        auto& state = stateStack.back();

        state.sequenceIdx++;

        assert(state.sequenceIdx <= state.len);

        if (state.mode == Mode::array or state.mode == Mode::map) {
            cborPrintf(" - post_item [%d/%lu]\n", state.sequenceIdx, state.len);
            if (state.sequenceIdx == state.len) {
                if (state.mode == Mode::array) advance(EndSizedArray {});
                if (state.mode == Mode::map) advance(EndSizedMap {});
            }
        } else {
            cborPrintf(" - post_item [scalar]\n", state.sequenceIdx, state.len);
            assert(state.len == kInvalidLength);
        }
    }

    template <class Visitor> template <class V> void CborParser<Visitor>::advance(V&& value) {

        if constexpr (std::is_same_v<V, BeginArray>) {
            stateStack.push_back(State { Mode::array, strm.cursor, 0, value.len });
            vtor.visit_begin_array();
        }

        else if constexpr (std::is_same_v<V, BeginMap>) {
            stateStack.push_back(State { Mode::map, strm.cursor, 0, value.len });
            vtor.visit_begin_map();
        }

        else if constexpr (std::is_same_v<V, EndIndefiniteArray>) {
            assert(stateStack.back().mode == Mode::array);
            assert(stateStack.back().len == kIndefiniteLength);
            stateStack.pop_back();
            vtor.visit_end_array();
            post_item();
        }

        else if constexpr (std::is_same_v<V, EndIndefiniteMap>) {
            assert(stateStack.back().mode == Mode::map);
            assert(stateStack.back().len == kIndefiniteLength);
            stateStack.pop_back();
            vtor.visit_end_map();
            post_item();
        }

        else if constexpr (std::is_same_v<V, EndSizedArray>) {
            assert(stateStack.back().mode == Mode::array);
            assert(stateStack.back().len == stateStack.back().sequenceIdx);
            stateStack.pop_back();
            vtor.visit_end_array();
            post_item();
        }

        else if constexpr (std::is_same_v<V, EndSizedMap>) {
            assert(stateStack.back().mode == Mode::map);
            assert(stateStack.back().len == stateStack.back().sequenceIdx);
            stateStack.pop_back();
            vtor.visit_end_map();
            post_item();
        }

        else {

            /*
            int oldSeq = stateStack.back().sequenceIdx;

            if (stateStack.back().mode == Mode::map) {
                    if (oldSeq % 2 == 0)
                            vtor.visit_map_key(std::move(value));
                    else
                            vtor.visit_map_value(std::move(value));
                    post_item();
            } else if (stateStack.back().mode == Mode::array) {
                    vtor.visit_array_value(std::move(value));
                    post_item();
            } else if (stateStack.back().mode == Mode::root) {
                    vtor.visit_root_value(std::move(value));
                    post_item();
            }
            */

            auto& state = stateStack.back();
            vtor.visit_value(state.mode, state.sequenceIdx, std::move(value));
            post_item();
        }
    }

    template <class Visitor> void CborParser<Visitor>::parse_item() {
        byte b              = strm.nextByte();
        byte majorType      = b >> 5;
        byte additionalInfo = b & 0b11111;
        cborPrintf(" - parse_item() [stackDepth: %d] [curState: %s] [m %d | addInfo %d]\n", stateStack.size(),
               stateStack.back().mode == Mode::array ? "array"
               : stateStack.back().mode == Mode::map ? "map"
                                                     : "root",
               majorType, additionalInfo);

        auto get_uint_for_length = [this](uint8_t additionalInfo) {
            if (additionalInfo < 24) {
                return (size_t)additionalInfo;
            } else if (additionalInfo >= 24 and additionalInfo <= 27) {
                size_t v;
                if (additionalInfo == 24)
                    v = (strm.nextValue<uint8_t>());
                else if (additionalInfo == 25)
                    v = ntohs(strm.nextValue<uint16_t>());
                else if (additionalInfo == 26)
                    v = ntohl(strm.nextValue<uint32_t>());
                else if (additionalInfo == 27)
                    v = ntohll(strm.nextValue<uint64_t>());
                return v;
            } else if (additionalInfo >= 28 and additionalInfo <= 30) {
                assert(false);
            } else if (additionalInfo == 31) {
                return kIndefiniteLength;
            }
            throw std::runtime_error("what.");
        };

        auto get_uint_for_value = [&](uint8_t additionalInfo) {
            auto out = get_uint_for_length(additionalInfo);
            assert(out != kIndefiniteLength && "invalid value with additionalInfo 31.");
            return out;
        };

        if (majorType == 0) {
            // vtor.visit_uint(get_uint_for_value(additionalInfo));
            advance(get_uint_for_value(additionalInfo));
        }

        else if (majorType == 1) {
            // vtor.visit_uint(-1 - static_cast<int64_t>(get_uint_for_value(additionalInfo)));
            advance(-1 - static_cast<int64_t>(get_uint_for_value(additionalInfo)));
        }

        else if (majorType == 2) {
            size_t str_len = get_uint_for_length(additionalInfo);
            if (str_len == kIndefiniteLength) {
                throw std::runtime_error("Indefinite length strings are NOT supported by this decoder.");
            }
            assert(str_len > 0);
            const byte* head = strm.nextBytes(str_len);
            // vtor.visit_byte_string(ByteStringView{head, str_len});
            advance(ByteStringView { head, str_len });
        }

        else if (majorType == 3) {
            size_t str_len = get_uint_for_length(additionalInfo);
            if (str_len == kIndefiniteLength) {
                throw std::runtime_error("Indefinite length strings are NOT supported by this decoder.");
            }
            assert(str_len > 0);
            const char* head = (const char*)strm.nextBytes(str_len);
            // vtor.visit_text_string(TextStringView{head, str_len});
            // cborPrintf(" - Advance with text string view : \"%s\"\n", std::string{TextStringView{head, str_len}}.c_str());
            advance(TextStringView { head, str_len });
        }

        else if (majorType == 4) {
            size_t len = get_uint_for_length(additionalInfo);
            // vtor.visit_begin_array();
            advance(BeginArray { len });
            // pushState(Mode::array, len);
        }

        else if (majorType == 5) {
            size_t len1 = get_uint_for_length(additionalInfo);
            size_t len  = len1 == kIndefiniteLength ? len1 : len1 * 2;
            advance(BeginMap { len });
            // pushState(Mode::map, len);
        }

        else if (majorType == 6) {

            size_t tag = get_uint_for_value(additionalInfo);

            // This is a typed array.
            if (tag >= 0b010'00000 and tag <= 0b010'11111) {

                // uint8_t taType = tag & 0b11111;
                uint8_t floating = (tag & 0b10000) != 0;
                uint8_t signing  = (tag & 0b01000) != 0;
                uint8_t endian   = (tag & 0b00100) != 0;
                uint8_t ll       = tag & 0b00011;
                TypedArrayView::Type type;
                if (floating and ll == 0)
                    assert(false && "invalid typed array type");
                else if (floating and ll == 1)
                    assert(false && "float16 not supported");
                else if (floating and ll == 2)
                    type = TypedArrayView::eFloat32;
                else if (floating and ll == 3)
                    type = TypedArrayView::eFloat64;
                else if (ll == 0 and !signing)
                    type = TypedArrayView::eUInt8;
                else if (ll == 0 and signing)
                    type = TypedArrayView::eInt8;
                else if (ll == 1 and !signing)
                    type = TypedArrayView::eUInt16;
                else if (ll == 1 and signing)
                    type = TypedArrayView::eInt16;
                else if (ll == 2 and !signing)
                    type = TypedArrayView::eUInt32;
                else if (ll == 2 and signing)
                    type = TypedArrayView::eInt32;
                else if (ll == 3 and !signing)
                    type = TypedArrayView::eUInt64;
                else if (ll == 3 and signing)
                    type = TypedArrayView::eInt64;

                byte byteStringHead = strm.nextByte();
                if (byteStringHead >> 5 != 0b010) { throw std::runtime_error("while parsing typed array, expected byte string."); }
                byte byteStringAdditionalInfo = byteStringHead & 0b11111;
                size_t blen                   = get_uint_for_length(byteStringAdditionalInfo);

                TypedArrayView tav { .type = type, .endianness = endian, .data = strm.nextBytes(blen), .byteLength = blen };

                advance(std::move(tav));
            } else {
                assert(false && "unsupported tag data encountered.");
            }
        }

        else if (majorType == 7) {
            if (additionalInfo < 24) {
                // vtor.visit_simple_value(additionalInfo);
                advance(additionalInfo);
            } else if (additionalInfo == 24) {
                byte sval = strm.nextByte();
                // vtor.visit_simple_value(sval);
                advance(sval);
            } else if (additionalInfo == 25) {
                assert(false && "half floats are not supported.");
            } else if (additionalInfo == 26) {
                float v = strm.nextValue<float>();
                // vtor.visit_float(v);
                advance(v);
            } else if (additionalInfo == 27) {
                double v = strm.nextValue<double>();
                // vtor.visit_double(v);
                advance(v);
            } else if (additionalInfo == 28 or additionalInfo == 29 or additionalInfo == 30) {
                assert(false && "Not a valid code in Jul 2024");
            } else if (additionalInfo == 31) {
                if (stateStack.back().mode == Mode::array) {
                    // vtor.visit_end_array();
                    // stateStack.pop_back();
                    // advance();
                    advance(EndIndefiniteArray {});
                } else if (stateStack.back().mode == Mode::map) {
                    // vtor.visit_end_map();
                    // stateStack.pop_back();
                    // advance();
                    advance(EndIndefiniteMap {});
                } else {
                    assert(false && "expected indef length thing.");
                }
            }

        } else {
            assert(false && "impossible");
        }
    }

    template <class Visitor> void CborParser<Visitor>::parse() {
        assert(stateStack.empty());
        pushState(Mode::root);
        parse_root();

        // There's no `visit_end_root` so we stop at len=1
        while (stateStack.size() > 1) { post_item(); }
    }

    template <class Visitor> void CborParser<Visitor>::parse_root() {
        while (strm.hasMore()) parse_item();
    }

    /*
    template <class Visitor>
    uint64_t CborParser<Visitor>::parse_uint() {
            throw std::runtime_error("todo");
    }
    template <class Visitor>
    int64_t CborParser<Visitor>::parse_nint() {
            throw std::runtime_error("todo");
    }
    template <class Visitor>
    ByteString CborParser<Visitor>::parse_byte_string() {
            throw std::runtime_error("todo");
    }
    template <class Visitor>
    std::string_view CborParser<Visitor>::parse_text_string() {
            throw std::runtime_error("todo");
    }
    */

}
