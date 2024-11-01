#include <gtest/gtest.h>

#include "cborCodec/cbor_tree_parser.hpp"
#include "cborCodec/cbor_encoder.hpp"

#include "json_printer.hpp"

using namespace cbor;

	static std::string toString(const Node& e, int depth=0) {
		std::string pre = "";
		for (int i=0; i<depth; i++) pre += " ";
		std::string s = "";
		switch (e.kind) {
			case Kind::Invalid:
				s = "<invalid>";
				break;
			case Kind::Byte:
				s = "(byte)";
				break;
			case Kind::Int64:
				s = "(int64)";
				break;
			case Kind::Uint64:
				s = "(uint64)";
				break;
			case Kind::F32:
				s = "(float)";
				break;
			case Kind::F64:
				s = "(double)";
				break;
			case Kind::Boolean:
				s = "(bool)";
				break;
			case Kind::Text:
				s = std::string{e.text.asStringView()};
				break;
			case Kind::Bytes:
				s = "(bytes " + std::to_string(e.bytes.len) + ")";
				break;
			case Kind::Vec:
				s = "[\n";
				for (auto& v : e.vec) {
					s += toString(v, depth + 1);
				}
				s += "\n" + pre + "]\n";
				break;
			case Kind::Map: {
				s = "{\n";
				for (auto& kv : e.map) {
					s += toString(kv.first, depth + 1);
					s += " -> ";
					s += toString(kv.second, depth + 1);
					s += ",\n" + pre;
				}
				s += "\n" + pre + "}\n";
				break;
			}
		}
		return pre + s;
	}

TEST(TreeParser, Simple) {

	CborEncoder encoder;
	encoder.begin_map(4);

	encoder.push_value("key1");
	encoder.push_value("val1");

	encoder.push_value("key2");
	encoder.begin_array(2);
	encoder.push_value("elem1");
	encoder.push_value("elem2");

	encoder.push_value("key3");
	encoder.begin_array(2);
	encoder.push_value("elem1");
	encoder.begin_array(1);
	encoder.push_value("innerElem1");

	encoder.push_value("key4");
	encoder.push_value("val4");

	auto data = encoder.finish();

	{
		CborParser cp(BinStreamBuffer{data.data(), data.size()});

		Node e = parseOne(cp, cp.next());
		printf("e.kind: %d\n", e.kind);
		printf("%s\n", toString(e).c_str());

		auto& key1_value = e["key1"];
		printf("key1_value: %s\n", toString(key1_value).c_str());

		auto& ie = e["key3"][1];
		printf("innerElem: %s\n", toString(ie).c_str());

		CborEncoder encoder2;
		encodeTree(encoder2, e);
		auto data2 = encoder2.finish();


		// Repeating the test with the representation that was re-encoded.
		{
			CborParser cp(BinStreamBuffer{data2.data(), data2.size()});

			Node e = parseOne(cp, cp.next());
			printf("e.kind: %d\n", e.kind);
			printf("%s\n", toString(e).c_str());

			auto& key1_value = e["key1"];
			printf("key1_value: %s\n", toString(key1_value).c_str());

			auto& ie = e["key3"][1];
			printf("innerElem: %s\n", toString(ie).c_str());
		}
	}
}

TEST(TreeParser, Bytes) {

	uint8_t ptr[8] = {1,2,3,4,5,6,7,8};

	Node n = Node::fromBytes(ByteBuffer{ptr,8});


	CborEncoder encoder2;
	encodeTree(encoder2, n);
	auto data2 = encoder2.finish();

	CborParser cp(BinStreamBuffer{data2.data(), data2.size()});
	Node e = parseOne(cp, cp.next());
	printf("%s\n", toString(e).c_str());
}

/*
TEST(TreeParser, ConsumeInnerMap) {

	CborEncoder encoder;
	encoder.begin_map(2);

	encoder.push_value("key0");
	encoder.push_value("val0");

	encoder.push_value("map1");
	encoder.begin_map(1);
	encoder.push_value("innerKey");
	encoder.push_value("innerVal");
	
	auto data = encoder.finish();

	{
		CborParser p(BinStreamBuffer{data.data(), data.size()});
		p.next();
		int i = 0;
		p.consumeMap(2, [&](Item&& k, Item&& v) {
			if (i == 1) {
				p.consumeMap(1, [&](Item&& k, Item&& v) {
					printf("inner:: %s: %s\n", k.toString().c_str(), v.toString().c_str());
				});
			}
			i++;
		});
	}

}
*/
