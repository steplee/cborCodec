
#include "cbor_parser.hpp"
#include "cbor_encoder.hpp"

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

		Kind kind;



		inline Node() : kind(Kind::Invalid), scalar({}) {}
		inline static Node fromInt(int64_t x) {
			Node out;
			out.scalar.int64 = x;
			out.kind = Kind::Int64;
			return out;
		}
		inline static Node fromUint(uint64_t x) {
			Node out;
			out.scalar.uint64 = x;
			out.kind = Kind::Uint64;
			return out;
		}
		inline static Node fromByte(uint8_t x) {
			Node out;
			out.scalar.byte = x;
			out.kind = Kind::Byte;
			return out;
		}
		inline static Node fromFloat(float x) {
			Node out;
			out.scalar.f32 = x;
			out.kind = Kind::F32;
			return out;
		}
		inline static Node fromDouble(double x) {
			Node out;
			out.scalar.f64 = x;
			out.kind = Kind::F64;
			return out;
		}
		inline static Node fromBool(bool x) {
			Node out;
			out.scalar.boolean = x;
			out.kind = Kind::Boolean;
			return out;
		}
		inline static Node fromMap(std::vector<std::pair<Node,Node>>&& map) {
			Node out;
			out.map = std::move(map);
			out.kind = Kind::Map;
			return out;
		}
		inline static Node fromVec(std::vector<Node>&& vec) {
			Node out;
			out.vec = std::move(vec);
			out.kind = Kind::Vec;
			return out;
		}
		inline static Node fromText(std::string_view s) {
			Node out;
			out.text = TextBuffer{s};
			out.kind = Kind::Text;
			return out;
		}
		inline static Node fromTextBuffer(TextBuffer&& tb) {
			Node out;
			out.text = std::move(tb);
			out.kind = Kind::Text;
			return out;
		}
		inline static Node fromBytes(const uint8_t* data, size_t len) {
			Node out;
			out.bytes = std::move(ByteBuffer{data,len});
			out.kind = Kind::Bytes;
			return out;
		}
		inline static Node fromBytes(ByteBuffer&& bb) {
			Node out;
			out.bytes = std::move(bb);
			out.kind = Kind::Bytes;
			return out;
		}
		inline static Node fromTypedArray(TypedArrayBuffer&& tab) {
			Node out;
			out.tab = std::move(tab);
			out.kind = Kind::TypedArray;
			return out;
		}



		inline bool isInvalid() const { return kind == Kind::Invalid; }
		inline bool isMap() const { return kind == Kind::Map; }
		inline bool isVec() const { return kind == Kind::Vec; }
		inline bool isText() const { return kind == Kind::Text; }
		inline bool isTypedArray() const { return kind == Kind::TypedArray; }

		inline const TypedArrayBuffer& asTypedArray() const {
			assert(isTypedArray());
			return tab;
		}

		inline int64_t asInt() const {
			assert(!isMap() and !isVec());
			assert(!isInvalid());
			if (kind == Kind::Byte) return scalar.byte;
			if (kind == Kind::Int64) return scalar.int64;
			if (kind == Kind::Uint64) return scalar.uint64;
			if (kind == Kind::F32) return scalar.f32;
			if (kind == Kind::F64) return scalar.f64;
			if (kind == Kind::Boolean) return scalar.boolean;
			assert(false);
		}

		inline uint64_t asUint() const {
			assert(!isMap() and !isVec());
			assert(!isInvalid());
			if (kind == Kind::Byte) return scalar.byte;
			if (kind == Kind::Int64) return scalar.int64;
			if (kind == Kind::Uint64) return scalar.uint64;
			if (kind == Kind::F32) return scalar.f32;
			if (kind == Kind::F64) return scalar.f64;
			if (kind == Kind::Boolean) return scalar.boolean;
			assert(false);
		}

		inline bool asBool() const {
			assert(!isMap() and !isVec());
			assert(!isInvalid());
			if (kind == Kind::Boolean) return scalar.boolean;
			if (kind == Kind::Byte) return scalar.byte;
			if (kind == Kind::Int64) return scalar.int64;
			if (kind == Kind::Uint64) return scalar.uint64;
			if (kind == Kind::F32) return scalar.f32;
			if (kind == Kind::F64) return scalar.f64;
			assert(false);
		}

		inline float asFloat32() const {
			assert(!isMap() and !isVec());
			assert(!isInvalid());
			if (kind == Kind::Byte) return scalar.byte;
			if (kind == Kind::Int64) return scalar.int64;
			if (kind == Kind::Uint64) return scalar.uint64;
			if (kind == Kind::F32) return scalar.f32;
			if (kind == Kind::F64) return scalar.f64;
			if (kind == Kind::Boolean) return scalar.boolean;
			assert(false);
		}
		inline double asFloat64() const {
			assert(!isMap() and !isVec());
			assert(!isInvalid());
			if (kind == Kind::Byte) return scalar.byte;
			if (kind == Kind::Int64) return scalar.int64;
			if (kind == Kind::Uint64) return scalar.uint64;
			if (kind == Kind::F32) return scalar.f32;
			if (kind == Kind::F64) return scalar.f64;
			if (kind == Kind::Boolean) return scalar.boolean;
			assert(false);
		}
		inline std::string_view asStringView() const {
			assert(isText());
			return text.asStringView();
		}

		inline size_t size() const {
			assert(isMap() or isVec());
			return isMap() ? map.size() : vec.size();
		}

		inline const Node& operator[](size_t i) const {
			if (isVec()) {
				assert(i < vec.size());
				return vec[i];
			}
			if (isMap()) {
				assert(false && "todo: indexing a map with an integer is not supported yet.");
			}
			throw std::runtime_error("can only index a Node with an integer if it is a map or vec");
		}

		// TODO: Allow matching against a Node, not just a strview
		inline const Node& operator[](std::string_view key) const {
			assert(isMap());
			for (uint32_t i=0; i<map.size(); i++) {
				assert(map[i].first.isText());
				if (map[i].first.text.asStringView() == key) return map[i].second;
			}
			throw std::runtime_error("missing key");
		}

		inline size_t find(std::string_view key) const {
			assert(isMap());
			for (uint32_t i=0; i<map.size(); i++) {
				assert(map[i].first.isText());
				if (map[i].first.text.asStringView() == key) return i;
			}
			return kInvalidLength;
		}

		inline bool has(std::string_view key) const {
			return find(key) != kInvalidLength;
		}


	};


	Node parseMap(CborParser& p, BeginMap&& begin);
	Node parseArray(CborParser& p, BeginArray&& begin);
	inline Node parseOne(CborParser& p, Item&& v) {
		/*
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
		*/
		if (v.is<uint8_t>()) return Node::fromByte(v.expect<uint8_t>());
		if (v.is<int64_t>()) return Node::fromInt(v.expect<int64_t>());
		if (v.is<uint64_t>()) return Node::fromUint(v.expect<uint64_t>());
		if (v.is<float>()) return Node::fromFloat(v.expect<float>());
		if (v.is<double>()) return Node::fromFloat(v.expect<double>());
		if (v.is<bool>()) return Node::fromFloat(v.expect<bool>());
		if (v.is<TextBuffer>()) return Node::fromTextBuffer(std::move(v.expect<TextBuffer>()));
		if (v.is<ByteBuffer>()) return Node::fromBytes(std::move(v.expect<ByteBuffer>()));
		if (v.is<TypedArrayBuffer>()) return Node::fromTypedArray(std::move(v.expect<TypedArrayBuffer>()));

		if (v.is<BeginMap>()) {
			return parseMap(p, std::move(v.expect<BeginMap>()));
		}
		if (v.is<BeginArray>()) {
			return parseArray(p, std::move(v.expect<BeginArray>()));
		}
		assert(false);
	}

	inline Node parseMap(CborParser& p, BeginMap&& begin) {
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

	inline Node parseArray(CborParser& p, BeginArray&& begin) {
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
	
	inline void encodeOne(CborEncoder& ce, const Node& node) {
		assert(!node.isInvalid());

		if (node.isMap()) {
			ce.begin_map(node.size());
			for (const auto &kv : node.map) {
				encodeOne(ce, kv.first);
				encodeOne(ce, kv.second);
			}
		}
		else if (node.isVec()) {
			ce.begin_array(node.size());
			for (const auto &v : node.vec) {
				encodeOne(ce, v);
			}
		}

		// Invalid, Byte, Int64, Uint64, F32, F64, Boolean, Text, Bytes, TypedArray, Map, Vec
		else if (node.kind == Kind::Byte) ce.push_value(node.scalar.byte);
		else if (node.kind == Kind::Int64) ce.push_value(node.scalar.int64);
		else if (node.kind == Kind::Uint64) ce.push_value(node.scalar.uint64);
		else if (node.kind == Kind::F32) ce.push_value(node.scalar.f32);
		else if (node.kind == Kind::F64) ce.push_value(node.scalar.f64);
		else if (node.kind == Kind::Boolean) ce.push_value(node.scalar.boolean);
		else if (node.kind == Kind::Text) ce.push_value(node.text);
		else if (node.kind == Kind::Bytes) ce.push_value(node.bytes);
		else if (node.kind == Kind::TypedArray) ce.push_value(node.tab);

		else {
			throw std::runtime_error("impossible: failed to match node kind.");
		}
	}




	inline Node parseTree(CborParser&& p) {
		auto it = p.next();
		return parseOne(p, std::move(it));
	}

	inline void encodeTree(CborEncoder& ce, const Node& root) {
		encodeOne(ce, root);
	}


}
