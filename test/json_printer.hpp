#include "cborCodec/cbor_online_parser.hpp"

namespace {
using namespace cbor;
struct JsonPrinter {

	OnlineCborParser &p;

	std::string os;

	inline JsonPrinter(OnlineCborParser& p_) : p(p_) {
		visitRoot();
	}

	inline void visitRoot() {
		// Similar to `visitArray`
		int i = 0;
		os += "[";
		while (p.hasMore()) {
			if (i != 0) os += ",";
			visitScalar(p.next());
			i++;
		}
		os += "]";
	}

	inline void visitArray(const Item& it) {
		int i = 0;
		os += "[";
		p.consumeArray(std::get<BeginArray>(it.value).size, [&i,this](Item&& v) {
				if (i != 0) os += ",";
				visitScalar(v);
				i++;
		});
		os += "]";
	}

	inline void visitMap(const Item& it) {
		int i = 0;
		os += "{";
		p.consumeMap(std::get<BeginMap>(it.value).size, [&i,this](Item&& k, Item&& v) {
				if (i != 0) os += ",";
				auto ks = k.toString();
				if (ks[0] == '"') os += ks;
				else os += "\"" + ks + "\"";
				os += ":";

				visitScalar(v);
				i++;
		});
		os += "}";
	}


	inline void visitScalar(const Item& v) {
		if (std::holds_alternative<BeginMap>(v.value)) {
			visitMap(v);
		} else if (std::holds_alternative<BeginArray>(v.value)) {
			visitArray(v);

		} else if (std::holds_alternative<EndMap>(v.value)) {
			assert(false);
		} else if (std::holds_alternative<EndArray>(v.value)) {
			assert(false);
		}

		else if (std::holds_alternative<TypedArrayBuffer>(v.value)) {
			os += "<NO TYPED ARRAYS IN JSON>";
		}
		else if (std::holds_alternative<ByteBuffer>(v.value)) {
			os += "<NO BYTE STRINGS IN JSON>";
		} else {
			os += v.toString();
		}
	}
};
}
