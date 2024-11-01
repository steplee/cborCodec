
#include "cbor_parser.hpp"

//
// An even higher level of abstraction.
// We decode to a tree, from which the user can access items.
// Buffers are not copied, but viewed.
//

namespace cbor {

	enum class Kind {
		Invalid, Byte, Int64, Uint64, F32, F64, Boolean, Text, Bytes, TypedArray, Map, Vec
	};

	union Union {
		uint8_t byte;
		int64_t int64;
		uint64_t uint64;
		float f32;
		double f64;
		bool boolean;
	};

	struct Node {
		Union scalar;

		TextBuffer text;
		ByteBuffer bytes;
		TypedArrayBuffer tab;

		std::vector<Node> vec;
		std::vector<std::pair<Node,Node>> map;

		Kind kind = Kind::Invalid;
	};

	Node parseMap(CborParser& p, BeginMap&& begin);
	Node parseArray(CborParser& p, BeginArray&& begin);

	Node parseOne(CborParser& p, Item&& v) {
		Node out;
		if (v.is<uint8_t>()) {
			out.kind = Kind::Byte;
			out.scalar.byte = v.expect<uint8_t>();
			return out;
		}
		if (v.is<int64_t>()) {
			out.kind = Kind::Int64;
			out.scalar.int64 = v.expect<int64_t>();
			return out;
		}
		if (v.is<uint64_t>()) {
			out.kind = Kind::Uint64;
			out.scalar.uint64 = v.expect<uint64_t>();
			return out;
		}
		if (v.is<float>()) {
			out.kind = Kind::F32;
			out.scalar.f32 = v.expect<float>();
			return out;
		}
		if (v.is<double>()) {
			out.kind = Kind::F64;
			out.scalar.f64 = v.expect<double>();
			return out;
		}
		if (v.is<bool>()) {
			out.kind = Kind::Boolean;
			out.scalar.boolean = v.expect<bool>();
			return out;
		}
		if (v.is<TextBuffer>()) {
			out.kind = Kind::Text;
			out.text = std::move(v.expect<TextBuffer>());
			return out;
		}
		if (v.is<ByteBuffer>()) {
			out.kind = Kind::Bytes;
			out.bytes = std::move(v.expect<ByteBuffer>());
			return out;
		}
		if (v.is<TypedArrayBuffer>()) {
			out.kind = Kind::TypedArray;
			out.tab = std::move(v.expect<TypedArrayBuffer>());
			return out;
		}

		if (v.is<BeginMap>()) {
			return parseMap(p, std::move(v.expect<BeginMap>()));
		}
		if (v.is<BeginArray>()) {
			return parseArray(p, std::move(v.expect<BeginArray>()));
		}
		assert(false);
	}

	Node parseMap(CborParser& p, BeginMap&& begin) {
		auto len = begin.size;

		Node out;
		out.kind = Kind::Map;
		if (len != kInvalidLength) out.map.reserve(len);

		p.consumeMap(len, [&](Item&& k, Item&& v) {
			// TextBuffer key { k.expect<TextBuffer>() };
			Node key { parseOne(p, std::move(k)) };
			Node value { parseOne(p, std::move(v)) };
			// out.map.emplace_back(std::make_pair(std::move(key), std::move(value)));
			out.map.push_back(std::make_pair(std::move(key), std::move(value)));
		});

		return out;
	}

	Node parseArray(CborParser& p, BeginArray&& begin) {
		auto len = begin.size;

		Node out;
		out.kind = Kind::Vec;
		if (len != kInvalidLength) out.vec.reserve(len);

		p.consumeArray(len, [&](Item&& v) {
			Node value { parseOne(p, std::move(v)) };
			// out.map.emplace_back(std::make_pair(std::move(key), std::move(value)));
			out.vec.push_back(std::move(value));
		});

		return out;
	}


	/*
	using Node = std::variant<
		uint8_t, int64_t, uint64_t,
		float, double,
		bool,
		TextBuffer, ByteBuffer, TypedArrayBuffer,
		std::vector<Node>,
		std::vector<std::pair<Node,Node>>,
		>;

	Node parseTop(CborParser&& p) {
		return parseMap(p, p.next());
	}

	Node parseOne(CborParser&& p, Item&& v) {
		if (v.is<uint8_t>()) return v.expect<uint8_t>();
		if (v.is<int64_t>()) return v.expect<int64_t>();
		if (v.is<uint64_t>()) return v.expect<uint64_t>();
		if (v.is<float>()) return v.expect<float>();
		if (v.is<double>()) return v.expect<double>();
		if (v.is<bool>()) return v.expect<bool>();
		if (v.is<TextBuffer>()) return v.expect<TextBuffer>();
		if (v.is<ByteBuffer>()) return v.expect<ByteBuffer>();
		if (v.is<TypedArrayBuffer>()) return v.expect<TypedArrayBuffer>();
		if (v.is<BeginMap>()) {
			return parseMap(p, v.expect<BeginMap>());
		}
		if (v.is<BeginArray>()) {
			return parseArray(p, v.expect<BeginArray>());
		}
		assert(false);
	}

	Node parseMap(CborParser&& p, BeginMap&& begin) {
		// auto top = p.next();
		// auto len = top.expect<BeginMap>().size;
		auto len = begin.size;

		std::vector<std::pair<Node,Node>> out;
		if (len != kInvalidLength) out.reserve(len);

		p.consumeMap(len, [&](Item&& k, Item&& v) {
			TextBuffer key { k.expect<TextBuffer>() };
			Node value { parseOne(p) }
			out.emplace_back(std::make_pair(std::move(key), std::move(value)));
		});

		return out;
	}

	Node parseArray(CborParser&& p, BeginArray&& begin) {
		auto len = begin.size;

		std::vector<std::pair<Node,Node>> out;
		if (len != kInvalidLength) out.reserve(len);

		p.consumeArray(len, [&](Item&& v) {
			Node value { parseOne(v) };
			out.emplace_back(std::move(value));
		});

		return out;
	}
	*/


}
