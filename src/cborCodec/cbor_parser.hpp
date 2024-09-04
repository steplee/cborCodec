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
#include <variant>

#include <arpa/inet.h>
#include <cstring>

#include "cbor_common.h"

//
// The visitor based approach worked well. It was even not difficult to use for a cbor -> json converter.
// But converting to a stronger typed domain was uh... less than ergonomic.
//
// So I will experiment with a parser that lazily decodes. Hopefully this can provide an API that makes
// it easier to construct objects than the low-level visitor approach.
//

namespace cbor {

        struct BinStreamBuffer {
            inline BinStreamBuffer(const uint8_t* data, size_t len)
                : data(data)
                , len(len) {
            }
            inline BinStreamBuffer() : data(0), len(0) {}

            const uint8_t* data = 0;
            size_t len = 0;
            size_t cursor_ = 0;

			inline size_t cursor() const { return cursor_; }

            inline bool hasMore(size_t n = 1) const {
                return cursor_ + n <= len;
            }
            inline uint8_t nextByte() {
                assert(hasMore());
                return data[cursor_++];
            }
            template <class V> inline V nextValue() {
				/*
                auto out = nextBytes(sizeof(V));
                return *reinterpret_cast<const V*>(out); // NOTE: Must I memcpy to align to boundary?
				*/
				V v;
				memcpy(&v, data+cursor_, sizeof(V));
				cursor_ += sizeof(V);
                return v;
            }

			/*
            inline const uint8_t* nextBytes(size_t n) {
                assert(hasMore(n));
                auto out = data + cursor_;
                cursor_ += n;
                return out;
            }
			*/
			inline DataBuffer nextBytes(size_t n) {
                assert(hasMore(n));
                auto ptr = data + cursor_;
                cursor_ += n;
                return DataBuffer(ptr, n); // view.
			}

			inline bool valid() const { return data != 0; }
        };

        struct BinStreamFile {
            inline BinStreamFile(const std::string& path) : ifs(path, std::ios_base::in | std::ios_base::binary), isSet(true) {}
            inline BinStreamFile(std::ifstream&& ifs) : ifs(std::move(ifs)), isSet(true) {}
            inline BinStreamFile() : isSet(false) {}

			inline bool valid() const { return isSet; }

            inline bool hasMore(size_t n = 1) const {
                return !ifs.eof();
            }
            inline uint8_t nextByte() {
                assert(hasMore());
				uint8_t out;
                ifs.read((char*)&out, 1);
				return out;
            }
            template <class V> inline V nextValue() {
				V v;
				ifs.read((char*)&v, sizeof(V));
                return v;
            }
            inline DataBuffer nextBytes(size_t n) {
                assert(hasMore(n));
                DataBuffer db(n); // allocate + own.
				ifs.read((char*)&db.buf, n);
                return db;
            }

			private:

			std::ifstream ifs;
			bool isSet;

		};

        enum class Mode : uint8_t { root, array, map };
        struct State {
            Mode mode = Mode::root;
            size_t byteIdx=0;     // This is not update, it's frozen at start.
            size_t sequenceIdx=0; // This is updated *in-place*
            size_t len= kIndefiniteLength;         // If the mode is array or map, how many elements are there? (or kIndefiniteLength)
			int depth = 0;

            inline bool operator==(const Mode& m) {
                return mode == m;
            }
        };

		struct BeginArray { size_t size; };
		struct BeginMap { size_t size; };
		struct EndArray {};
		struct EndMap {};
		struct End {};

	using Value = std::variant<
		uint8_t, int64_t, uint64_t,
		float, double,
		bool,
		// TextStringView,
		// ByteStringView,
		// TypedArrayView,
		TextBuffer, ByteBuffer, TypedArrayBuffer,
		BeginArray, BeginMap, EndArray, EndMap,
		Null,
		End
		>;

	struct Item {
		std::vector<State>& stack;
		Value value;

		inline operator Value&() { return value; }
		inline const State& state() const { return stack.back(); }

		template <class T> inline T& expect() { return std::get<T>(value); }

		inline std::string toString() const {
			char buf[32] = {0};
			bool done = false;
			if (std::holds_alternative<uint8_t>(value)) sprintf(buf, "%ld", (int64_t)std::get<uint8_t>(value)), done = true;
			else if (std::holds_alternative<bool>(value)) sprintf(buf, std::get<bool>(value) ? "true" : "false"), done = true;
			else if (std::holds_alternative<uint64_t>(value)) sprintf(buf, "%lu", std::get<uint64_t>(value)), done = true;
			else if (std::holds_alternative<int64_t>(value)) sprintf(buf, "%ld", std::get<int64_t>(value)), done = true;
			else if (std::holds_alternative<float>(value)) sprintf(buf, "%f", std::get<float>(value)), done = true;
			else if (std::holds_alternative<double>(value)) sprintf(buf, "%lf", std::get<double>(value)), done = true;
			else if (std::holds_alternative<Null>(value)) sprintf(buf, "null"), done = true;
			else if (std::holds_alternative<End>(value)) sprintf(buf, "end"), done = true;
			else if (std::holds_alternative<BeginArray>(value)) sprintf(buf, "["), done = true;
			else if (std::holds_alternative<EndArray>(value)) sprintf(buf, "]"), done = true;
			else if (std::holds_alternative<BeginMap>(value)) sprintf(buf, "{"), done = true;
			else if (std::holds_alternative<EndMap>(value)) sprintf(buf, "}"), done = true;

			if (done) { return std::string{buf}; }

			if (std::holds_alternative<TextBuffer>(value)) return "\"" + std::string{std::get<TextBuffer>(value).asStringView()} + "\"";
			if (std::holds_alternative<ByteBuffer>(value)) printf("bstr{len=%ld}", std::get<ByteBuffer>(value).size());
			if (std::holds_alternative<TypedArrayBuffer>(value)) printf("tav{len=%ld}", std::get<TypedArrayBuffer>(value).size());

			return std::string{"<unknown>"};
		}

		inline void print() const {
			if (std::holds_alternative<uint8_t>(value)) printf("byte{%ld}", (int64_t)std::get<uint8_t>(value));
			else if (std::holds_alternative<uint64_t>(value)) printf("ulong{%lu}", std::get<uint64_t>(value));
			else if (std::holds_alternative<int64_t>(value)) printf("long{%ld}", std::get<int64_t>(value));
			else if (std::holds_alternative<float>(value)) printf("f32{%f}", std::get<float>(value));
			else if (std::holds_alternative<double>(value)) printf("f64{%lf}", std::get<double>(value));
			else if (std::holds_alternative<TextBuffer>(value)) printf("str{%s}", std::string{std::get<TextBuffer>(value).asStringView()}.c_str());
			else if (std::holds_alternative<ByteBuffer>(value)) printf("bstr{%ld}", std::get<ByteBuffer>(value).size());
			else if (std::holds_alternative<Null>(value)) printf("null");
			else if (std::holds_alternative<End>(value)) printf("end");
			else if (std::holds_alternative<TypedArrayBuffer>(value)) printf("tav{%ld}", std::get<TypedArrayBuffer>(value).size());
			else if (std::holds_alternative<BeginArray>(value)) printf("beginArray{");
			else if (std::holds_alternative<EndArray>(value)) printf("}endArray");
			else if (std::holds_alternative<BeginMap>(value)) printf("beginMap{");
			else if (std::holds_alternative<EndMap>(value)) printf("}endMap");
			else printf("<unknown>");
		}
		inline void print(const char* before, const char* after) const {
			printf("%s", before);
			print();
			printf("%s", after);
		}
	};

	struct CborParser {
		private:
		BinStreamBuffer strm1;
		BinStreamFile   strm2;
		std::vector<State> stack = { State{Mode::root}, };

		public:

        inline CborParser(BinStreamBuffer&& strm) : strm1(std::move(strm)) {}
        inline CborParser(BinStreamFile  && strm) : strm2(std::move(strm)) {}

		Item next();

		inline void advance() {
			auto &state = stack.back();
			state.sequenceIdx++;
			if (state.sequenceIdx >= state.len) {
				stack.pop_back();
				advance();
			}
		}

		inline Item makeItem(Value&& v) {
			advance();
			return Item { stack , std::move(v) };
		}

		template <class F>
		void consumeMap(size_t size, F&& f);
		template <class F>
		void consumeArray(size_t size, F&& f);

		inline bool hasMore() const {
			return strm1.hasMore();
		}
		
	};


	inline Item CborParser::next() {

		if (!strm1.hasMore()) {
			return makeItem(End{});
		}
		auto depth = stack.back().depth;
		auto seq = stack.back().sequenceIdx;
		auto len = stack.back().len;
		// printf(" - NEXT (d %d|%ld) (s %ld / %ld)\n", depth, stack.size(), seq, len);

        byte b              = strm1.nextByte();
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
                    v = (strm1.template nextValue<uint8_t>());
                else if (additionalInfo == 25)
                    v = ntohs(strm1.template nextValue<uint16_t>());
                else if (additionalInfo == 26)
                    v = ntohl(strm1.template nextValue<uint32_t>());
                else if (additionalInfo == 27)
                    v = ntohll(strm1.template nextValue<uint64_t>());
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
            uint64_t v = (get_uint_for_value(additionalInfo));
			if (v < 256) return makeItem((uint8_t)v);
			return makeItem(v);
        }

        else if (majorType == 1) {
            // vtor.visit_uint(-1 - static_cast<int64_t>(get_uint_for_value(additionalInfo)));
            int64_t v = (-1 - static_cast<int64_t>(get_uint_for_value(additionalInfo)));
			return makeItem(v);
        }
		
        else if (majorType == 2) {
            size_t str_len = get_uint_for_length(additionalInfo);
            if (str_len == kIndefiniteLength) {
                throw std::runtime_error("Indefinite length strings are NOT supported by this decoder.");
            }
            // assert(str_len > 0);
			auto db = strm1.nextBytes(str_len);
            return makeItem(ByteBuffer { std::move(db) });
        }

        else if (majorType == 3) {
            size_t str_len = get_uint_for_length(additionalInfo);
            if (str_len == kIndefiniteLength) {
                throw std::runtime_error("Indefinite length strings are NOT supported by this decoder.");
            }
            // assert(str_len > 0);
			auto db = strm1.nextBytes(str_len);
            // vtor.visit_text_string(TextStringView{head, str_len});
            // cborPrintf(" - Advance with text string view : \"%s\"\n", std::string{TextStringView{head, str_len}}.c_str());
            return makeItem(TextBuffer { std::move(db) });
        }

        else if (majorType == 4) {
            size_t len = get_uint_for_length(additionalInfo);
            return makeItem(BeginArray { len });
        }

        else if (majorType == 5) {
            size_t len = get_uint_for_length(additionalInfo);
            return makeItem(BeginMap { len });
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
                TypedArrayBuffer::Type type;
                if (floating and ll == 0)
                    assert(false && "invalid typed array type");
                else if (floating and ll == 0)
                    assert(false && "float16 not supported");
                else if (floating and ll == 3)
                    assert(false && "float128 not supported");
                else if (floating and ll == 1)
                    type = TypedArrayBuffer::eFloat32;
                else if (floating and ll == 2)
                    type = TypedArrayBuffer::eFloat64;
                else if (ll == 0 and !signing)
                    type = TypedArrayBuffer::eUInt8;
                else if (ll == 0 and signing)
                    type = TypedArrayBuffer::eInt8;
                else if (ll == 1 and !signing)
                    type = TypedArrayBuffer::eUInt16;
                else if (ll == 1 and signing)
                    type = TypedArrayBuffer::eInt16;
                else if (ll == 2 and !signing)
                    type = TypedArrayBuffer::eUInt32;
                else if (ll == 2 and signing)
                    type = TypedArrayBuffer::eInt32;
                else if (ll == 3 and !signing)
                    type = TypedArrayBuffer::eUInt64;
                else if (ll == 3 and signing)
                    type = TypedArrayBuffer::eInt64;
				else
					throw std::runtime_error("invalid typed array type.");

                byte byteStringHead = strm1.nextByte();
                if (byteStringHead >> 5 != 0b010) { throw std::runtime_error("while parsing typed array, expected byte string."); }
                byte byteStringAdditionalInfo = byteStringHead & 0b11111;
                size_t blen                   = get_uint_for_length(byteStringAdditionalInfo);

				auto db = strm1.nextBytes(blen);
				TypedArrayBuffer tav(std::move(db), type, endian);
                return makeItem(std::move(tav));
            } else {
                assert(false && "unsupported tag data encountered.");
            }
        }

        else if (majorType == 7) {
            if (additionalInfo == 20) {
                return makeItem(false);
			} else if (additionalInfo == 21) {
                return makeItem(true);
			} else if (additionalInfo == 22) {
                return makeItem(Null{});
			} else if (additionalInfo < 24) {
                return makeItem(additionalInfo);
            } else if (additionalInfo == 24) {
                byte sval = strm1.nextByte();
                return makeItem(sval);
            } else if (additionalInfo == 25) {
                assert(false && "half floats are not supported.");
            } else if (additionalInfo == 26) {
                float v = strm1.template nextValue<float>();
				v = ntoh(v);
                return makeItem(v);
            } else if (additionalInfo == 27) {
                double v = strm1.template nextValue<double>();
				v = ntoh(v);
                return makeItem(v);
            } else if (additionalInfo == 28 or additionalInfo == 29 or additionalInfo == 30) {
                assert(false && "Not a valid code in Jul 2024");
            } else if (additionalInfo == 31) {
				/*
                if (stack.back().mode == Mode::array) {
					printf(" - end indef array\n");
                    // vtor.visit_end_array();
                    stack.pop_back();
                    // advance();
                    return makeItem(EndArray {});
                } else if (stack.back().mode == Mode::map) {
					printf(" - end indef map\n");
                    // vtor.visit_end_map();
                    stack.pop_back();
                    // advance();
                    return makeItem(EndMap {});
                } else {
                    assert(false && "expected indef length thing.");
                }
				*/
				return makeItem(End{});
            }
            throw std::runtime_error("what.");
		}

        assert(false && "impossible");
	}

	template <class F>
	inline void CborParser::consumeMap(size_t size, F&& f) {
		// printf(" - begin map %d\n", (int)size);
		for (size_t i=0; i<size; i++) {
			auto key = this->next();
			// if (std::holds_alternative<EndMap>(key.value)) break;
			if (std::holds_alternative<End>(key.value)) break;

			auto val = this->next();
			f(std::move(key), std::move(val));
		}
		// f(makeItem(EndMap{}),makeItem(EndMap{}));
	}

	template <class F>
	inline void CborParser::consumeArray(size_t size, F&& f) {
		for (size_t i=0; i<size; i++) {

			auto val = this->next();
			// if (std::holds_alternative<EndArray>(val.value)) break;
			if (std::holds_alternative<End>(val.value)) break;
			f(std::move(val));
		}
		// f(makeItem(EndArray{}));
	}


}
