#include <gtest/gtest.h>

#include <fstream>
#include <cassert>

#include "cborCodec/cbor_parser.hpp"
#include "cborCodec/cbor_encoder.hpp"

// Not a great API -- it needs some thought.


namespace {

	using namespace cbor;

	struct ApplicationMetadata {
		std::string helloMessage;

		inline void encodeCbor(CborEncoder& ce) const {
			ce.begin_map(1);
			ce.push_value("helloMessage");
			ce.push_value(helloMessage);
		}
	};

	enum class MessageTag : uint8_t {
		eMessage1, eMessage2, eEnd
	};

	enum class CommonFieldTag : uint8_t {
		eMessageTag = 0,
		eTstamp = 1,
		eHtstamp = 2,
		eEnd = 8,
	};

	enum class Message1Tags : uint8_t {
		eField1 = (uint8_t)CommonFieldTag::eEnd,
		eField2 = 9,
		eField3 = 10,
		eField4 = 11,
	};
	enum class Message2Tags : uint8_t {
		eField0 = (uint8_t)CommonFieldTag::eEnd,
		eField1,
		eField2,
		eField3,
		eField4,
	};

	struct Message1 {
		int64_t tstamp;
		int64_t field1;
		double field2;
		float field3;
		std::string field4;

		inline constexpr static MessageTag getMessageTag() { return MessageTag::eMessage1; }
		inline void encodeCbor(CborEncoder& ce) const {
			ce.begin_map(6);
			ce.push_key_value((uint8_t)CommonFieldTag::eMessageTag, (uint8_t)MessageTag::eMessage1);
			ce.push_key_value((uint8_t)CommonFieldTag::eTstamp, tstamp);
			ce.push_key_value((uint8_t)Message1Tags::eField1, field1);
			ce.push_key_value((uint8_t)Message1Tags::eField2, field2);
			ce.push_key_value((uint8_t)Message1Tags::eField3, field3);
			ce.push_key_value((uint8_t)Message1Tags::eField4, field4);
		}
	};
	struct Message2 {
		int64_t tstamp;
		double field0[8];
		int64_t field1;
		double field2;
		float field3;
		std::string field4;

		inline constexpr static MessageTag getMessageTag() { return MessageTag::eMessage2; }
		inline void encodeCbor(CborEncoder& ce) const {
			ce.begin_map(7);
			ce.push_key_value((uint8_t)CommonFieldTag::eMessageTag, (uint8_t)MessageTag::eMessage2);
			ce.push_key_value((uint8_t)CommonFieldTag::eTstamp, tstamp);

			ce.push_value((uint8_t)Message2Tags::eField0);
			ce.push_typed_array(field0, 8);

			ce.push_key_value((uint8_t)Message2Tags::eField1, field1);
			ce.push_key_value((uint8_t)Message2Tags::eField2, field2);
			ce.push_key_value((uint8_t)Message2Tags::eField3, field3);
			ce.push_key_value((uint8_t)Message2Tags::eField4, field4);
		}
	};

	using MessageVariant = std::variant<
		Message1, Message2>;

	template <class T>
	inline bool isField(const Item& v, const T& t) {
		// static_assert(std::is_integral_v<T>);
		assert(std::holds_alternative<uint8_t>(v.value));
		return std::get<uint8_t>(v.value) == static_cast<uint8_t>(t);
	}

	template <class T>
	inline T convert(const Item& i) {
		const auto& v = i.value;

		// Allow implict int -> int conversions and int -> float conversions (but not float -> int)

		if constexpr (std::is_same_v<T,int64_t>) {
			if (std::holds_alternative<uint8_t>(v)) return std::get<uint8_t>(v);
			if (std::holds_alternative<uint64_t>(v)) return std::get<uint64_t>(v);
			// if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
		}
		else if constexpr (std::is_same_v<T,uint64_t>) {
			if (std::holds_alternative<uint8_t>(v)) return std::get<uint8_t>(v);
			// if (std::holds_alternative<uint64_t>(v)) return std::get<uint64_t>(v);
			if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
		}
		else if constexpr (std::is_same_v<T,float>) {
			if (std::holds_alternative<double>(v)) return std::get<double>(v);
			if (std::holds_alternative<uint8_t>(v)) return std::get<uint8_t>(v);
			if (std::holds_alternative<uint64_t>(v)) return std::get<uint64_t>(v);
			if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
		}
		else if constexpr (std::is_same_v<T,double>) {
			if (std::holds_alternative<float>(v)) return std::get<float>(v);
			if (std::holds_alternative<uint8_t>(v)) return std::get<uint8_t>(v);
			if (std::holds_alternative<uint64_t>(v)) return std::get<uint64_t>(v);
			if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
		}
		
		if constexpr (std::is_same_v<T,std::string>) {
			assert(std::holds_alternative<TextBuffer>(v));
			return std::string { std::get<TextBuffer>(v).asStringView() };
		}

		else if constexpr (std::is_pointer_v<T>) {
		}

		else {
			assert(std::holds_alternative<T>(v));
			return std::get<T>(v);
		}
	}

	template <class T>
	inline void convert1(const Item& i, T& t) {
		const auto& v = i.value;

		if constexpr (std::is_pointer_v<T> or std::is_array_v<T>) {

			// NOTE: Only allow typed arrays -- not arrays with type tags every field.
			assert(std::holds_alternative<TypedArrayBuffer>(v));
			const auto &tav = std::get<TypedArrayBuffer>(v);

			for (uint32_t i=0; i<tav.elementLength(); i++) {
				if constexpr(std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,double>)
					t[i] = tav.accessAs<double>(i);
				else if constexpr(std::is_array_v<T> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>,double>)
					t[i] = tav.accessAs<double>(i);
				else if constexpr(std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,float>)
					t[i] = tav.accessAs<float>(i);
				else if constexpr(std::is_array_v<T> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>,float>)
					t[i] = tav.accessAs<float>(i);
				else if constexpr(std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,uint8_t>)
					t[i] = tav.accessAs<uint8_t>(i);
				else if constexpr(std::is_array_v<T> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>,uint8_t>)
					t[i] = tav.accessAs<uint8_t>(i);
				else if constexpr(std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,int64_t>)
					t[i] = tav.accessAs<int64_t>(i);
				else if constexpr(std::is_array_v<T> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<T>>,int64_t>)
					t[i] = tav.accessAs<int64_t>(i);
				else
					assert(false);
			}
		}

		// Allow implict int -> int conversions and int -> float conversions (but not float -> int)
		else if constexpr (std::is_integral_v<T>) {
			if (std::holds_alternative<uint8_t>(v)) t = std::get<uint8_t>(v);
			else if (std::holds_alternative<uint64_t>(v)) t = std::get<uint64_t>(v);
			else if (std::holds_alternative<int64_t>(v)) t = std::get<int64_t>(v);
			else assert(false);
		}

		else if constexpr (std::is_floating_point_v<T>) {
			if (std::holds_alternative<float>(v)) t = std::get<float>(v);
			else if (std::holds_alternative<double>(v)) t = std::get<double>(v);
			else if (std::holds_alternative<uint8_t>(v)) t = std::get<uint8_t>(v);
			else if (std::holds_alternative<uint64_t>(v)) t = std::get<uint64_t>(v);
			else if (std::holds_alternative<int64_t>(v)) t = std::get<int64_t>(v);
			else assert(false);
		}

		// Todo: also handle eigen types.

	}



	Message1 parseMessage1(CborParser& p, size_t numel) {
	// Message1 parseMessage1(CborParser& p) {
		// auto bm = p.next();
		// assert(std::holds_alternative<BeginMap>(bm.value));
		// size_t numel = std::get<BeginMap>(bm.value).size;

		Message1 m;
		p.consumeMap(numel, [&m](Item&& k, Item&& v) {
			if (isField(k, Message1Tags::eField1)) m.field1 = convert<decltype(m.field1)>(v);
			if (isField(k, Message1Tags::eField2)) m.field2 = convert<decltype(m.field2)>(v);
			if (isField(k, Message1Tags::eField3)) m.field3 = convert<decltype(m.field3)>(v);
			if (isField(k, Message1Tags::eField4)) m.field4 = convert<decltype(m.field4)>(v);
		});
		return m;
	}
	Message2 parseMessage2(CborParser& p, size_t numel) {
	// Message2 parseMessage2(CborParser& p) {
		// auto bm = p.next();
		// assert(std::holds_alternative<BeginMap>(bm.value));
		// size_t numel = std::get<BeginMap>(bm.value).size;

		Message2 m;
		p.consumeMap(numel, [&m](Item&& k, Item&& v) {
			if (isField(k, Message2Tags::eField0)) convert1(v, m.field0);
			if (isField(k, Message2Tags::eField1)) convert1(v, m.field1);
			if (isField(k, Message2Tags::eField2)) convert1(v, m.field2);
			if (isField(k, Message2Tags::eField3)) convert1(v, m.field3);
			if (isField(k, Message2Tags::eField4)) convert1(v, m.field4);
		});
		return m;
	}

	struct Replay {
		CborParser &p;
		std::vector<MessageVariant> msgs;
		inline Replay(CborParser& p_) : p(p_) {
		}

		inline void run() {
			visitRoot();
		}

		inline void visitRoot() {
			// Similar to `visitArray`
			int i = 0;
			while (p.hasMore()) {
				auto it = p.next();
				assert(std::holds_alternative<BeginMap>(it.value));
				visitRootItem(std::move(std::get<BeginMap>(it.value)));
				i++;
			}
		}

		inline void visitRootItem(BeginMap&& bm) {
			auto k0 = p.next();
			assert(std::holds_alternative<TextBuffer>(k0.value) and std::get<TextBuffer>(k0.value).asStringView() == "metadata");
			auto meta = p.next();
			assert(std::holds_alternative<BeginMap>(meta.value));
			p.consumeMap(std::get<BeginMap>(meta.value).size, [](Item&& k, Item&& v) {});
			// visitMetadata(std::move(std::get<BeginMap>(meta.value)));

			auto k1 = p.next();
			assert(std::holds_alternative<TextBuffer>(k1.value));
			printf("%s\n", std::string{std::get<TextBuffer>(k1.value).asStringView()}.c_str());
			assert(std::get<TextBuffer>(k1.value).asStringView() == "messages");
			auto messages = p.next();
			assert(std::holds_alternative<BeginMap>(messages.value));
			visitMessages(std::move(std::get<BeginMap>(messages.value)));
		}

		inline void visitMetadata(BeginMap&& bm) {
		}

		inline void visitMessages(BeginMap&& ba) {
			int i = 0;
			p.consumeMap(ba.size, [this,&i](Item&& k, Item&& v) {

					assert(std::holds_alternative<uint8_t>(k.value));
					auto msgTag = (MessageTag) std::get<uint8_t>(k.value);

					assert(std::holds_alternative<BeginMap>(v.value));
					auto size =  std::get<BeginMap>(v.value).size;

					if (msgTag == MessageTag::eMessage1) {
						Message1 m = parseMessage1(p, size);
						msgs.push_back(m);
					} else if (msgTag == MessageTag::eMessage2) {
						Message2 m = parseMessage2(p, size);
						msgs.push_back(m);
					} else {
						assert(false);
					}
					i++;
			});
		}


	};

}

TEST(BusMessageVisitor, One) {

	CborEncoder encoder;


	std::vector<MessageVariant> msgs;
	msgs.push_back(Message1{
			.tstamp = 1,
			.field1 = 100000,
			.field2 = 1,
			.field3 = 1,
			.field4 = "the first message",
	});
	msgs.push_back(Message2{
			.tstamp = 2,
			.field0 = {101,102,103,104, 105,106,107,108},
			.field1 = 100000,
			.field2 = 2,
			.field3 = 2,
			.field4 = "the second message",
	});

	encoder.begin_map(2);

	encoder.push_value(std::string{"metadata"});
	encoder.push_value(ApplicationMetadata{.helloMessage="Hello World"});

	encoder.push_value(std::string{"messages"});
	encoder.begin_map(kIndefiniteLength);
	for (auto& msg : msgs) {
		if (std::holds_alternative<Message1>(msg)) {
			encoder.push_value((uint8_t)std::get<Message1>(msg).getMessageTag());
			encoder.push_value(std::get<Message1>(msg));
		}
		else if (std::holds_alternative<Message2>(msg)) {
			encoder.push_value((uint8_t)std::get<Message2>(msg).getMessageTag());
			encoder.push_value(std::get<Message2>(msg));
		}
		else assert(false);
	}

	encoder.end_indefinite();

	std::vector<uint8_t> encoded = encoder.finish();
	std::cout << " - encoded size " << encoded.size() << "\n";

	{
		CborParser p(BinStreamBuffer{encoded.data(), encoded.size()});
		Replay v(p);
		v.run();

		std::vector<MessageVariant> msgs1 = v.msgs;
		EXPECT_EQ(msgs1.size(), 2);
		EXPECT_TRUE(std::holds_alternative<Message1>(msgs1[0]));
		EXPECT_TRUE(std::holds_alternative<Message2>(msgs1[1]));
	}

}
