#include "cborCodec/cbor_parser.hpp"

namespace {

	using namespace cbor;

struct ToJsonVisitor {


	// A string is used rather than an ostream because we need to remove the trailing ',' in maps and keys,
	// and the api does not provide a surface for this. Nor could it since CBOR supports indefinite length collections,
	// terminated by a stop token.
	
	// std::ostream os;

	//
	// One-function API.
	// For example if `mode == Mode::map`, then `seqIdx % 2` tells us key or value.
	//

	inline ToJsonVisitor() {
		os = "[";
		os.reserve(1 << 20);
	}

	template <class T>
	inline void visit_value(State state, size_t seqIdx, T&& t) {
		auto mode = state.mode;

		if (os.size() + 32 > os.capacity()) {
			os.reserve(os.capacity() * 2);
		}

		if ((mode == Mode::root or mode == Mode::array) and seqIdx > 0) {
			os += ",";
		}
		if ((mode == Mode::map) and seqIdx > 0 and seqIdx % 2 == 0) {
			if (os.back() != ',')
				os += ",";
		}

		if constexpr (std::is_same_v<T, TextStringView>) {
			std::string tt;
			tt.reserve(t.size());
			char prev = 0;
			for (int i=0; i<t.size(); i++) {
				if (t[i] == '\r') {
					tt.push_back('\\');
					tt.push_back('r');
				} else if (t[i] == '\n') {
					tt.push_back('\\');
					tt.push_back('n');
				} else if (t[i] == '\f') {
					tt.push_back('\\');
					tt.push_back('f');
				} else if (t[i] == '\b') {
					tt.push_back('\\');
					tt.push_back('b');
				} else if (t[i] == '\\') {
					tt.push_back('\\');
					tt.push_back('\\');
				} else if (t[i] == '\t') {
					tt.push_back('\\');
					tt.push_back('t');
				} else if (t[i] < 0x1f) {
					// skip.
				} else
					tt.push_back(t[i]);
			}
			os += "\"" + tt + "\"";
		}
		else if constexpr (std::is_same_v<T, ByteStringView>) {
			throw std::runtime_error("Cannot have bytes in json.");
		}
		else if constexpr (std::is_same_v<T, True>) {
			os += "true";
		}
		else if constexpr (std::is_same_v<T, False>) {
			os += "false";
		}
		else if constexpr (std::is_same_v<T, Null>) {
			os += "null";
		}
		else if constexpr (std::is_same_v<T, TypedArrayView>) {
			if (t.type == TypedArrayView::eInt32) {
				// printf("typed int32 array of size %ld, endian: %d:\n", t.elementLength(), t.endianness);
				os += "[";
				for (int i=0; i<t.elementLength(); i++) {
					os += std::to_string(reinterpret_cast<TypedArrayView*>(&t)->accessAs<int32_t>(i));
					if (i < t.elementLength()-1) os += ",";
				}
				os += "]";
			} else
				printf("typed array of size %ld\n", t.elementLength());
		}
		else {
			std::ostringstream oss;

			// Object keys must strings.
			if ((mode == Mode::map) and seqIdx % 2 == 0) oss << "\"";

			oss << t;

			if ((mode == Mode::map) and seqIdx % 2 == 0) oss << "\"";

			os += oss.str();
			// throw std::runtime_error("Unhandled in ToJsonVisitor::visit_value");
		}

		if ((mode == Mode::map) and seqIdx % 2 == 1 and seqIdx > 0) {
			os += ",";
		}
		if ((mode == Mode::map) and seqIdx % 2 == 0) {
			os += ":";
		}

	}

	//
	// TODO: This is hacky. I should provide mode/seqIdx also here!
	//

	inline void visit_begin_array() {
		if (os.back() != ':' and os.back() != ',' and os.back() != '[' and os.back() != '{')
			os += ",";

		os += "[";
	}
	inline void visit_end_array() {
		if (os.back() == ',') os.pop_back();
		os += "]";
	}

	inline void visit_begin_map() {
		if (os.back() != ':' and os.back() != ',' and os.back() != '[' and os.back() != '{')
			os += ",";

		os += "{";
	}
	inline void visit_end_map() {
		if (os.back() == ',') os.pop_back();
		os += "}";
	}

	inline std::string finish() {
		os += "]";
		return os;
	}

	private:
	std::string os;

};

}
