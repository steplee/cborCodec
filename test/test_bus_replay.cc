#include <gtest/gtest.h>

#include <fstream>

#include "cborCodec/cbor_parser.hpp"
#include "cborCodec/cbor_encoder.hpp"

#include "to_json_visitor.hpp"


namespace {

	struct ApplicationMetadata {
		std::string helloMessage;

		inline void encodeCbor(CborEncoder& ce) const {
			ce.begin_map(1);
			ce.push_value("helloMessage");
			ce.push_value(helloMessage);
		}
	};

	enum class MessageTag : uint8_t {
		eMsg1, eMsg2, eEnd
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

		inline void encodeCbor(CborEncoder& ce) const {
			ce.begin_map(6);
			ce.push_key_value((uint8_t)CommonFieldTag::eMessageTag, (uint8_t)MessageTag::eMsg1);
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

		inline void encodeCbor(CborEncoder& ce) const {
			ce.begin_map(7);
			ce.push_key_value((uint8_t)CommonFieldTag::eMessageTag, (uint8_t)MessageTag::eMsg1);
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






	//
	// Decode the application log format.
	// It consists of a map with `ApplicationMetadata`, then an indefinite map with the captured streamed messages.
	//
	// Note that to easily support combinator style parsing (breaking up the specific type decoders into seperate code regions),
	// we can make a couple of assumptions.
	// If `curMsgTag` is valid (not eEnd) and if `state.depth > curMsgDepthBegin` then we are currently parsing a message with the
	// given tag, and all of the visit functions can dispatch to that type decoder.
	//

	class ReplayVisitor {

		private:

		ApplicationMetadata appMeta;
		std::vector<MessageVariant> msgs;

		MessageTag curMsgTag = MessageTag::eEnd;
		int curMsgDepthBegin = -1;

		bool didStartMsgs;
		bool didEndMsgs;
		int msgStartDepth; // if we ever return past this after `didStartMsgs`, set `didEndMsgs` to false.
		bool inMsgs() const {
			return didStartMsgs and !didEndMsgs;
		}

		public:

		template <class T>
		inline void visit_value(State state, T&& t) {

			int seq = state.sequenceIdx;
			int depth = state.depth;
			auto mode = state.mode;

			if constexpr (std::is_same_v<T, BeginMap>) {
			} else if constexpr (std::is_same_v<T, BeginArray>) {
			} else if constexpr (std::is_same_v<T, EndMap>) {
			} else if constexpr (std::is_same_v<T, EndArray>) {
			}

		}


		inline std::vector<MessageVariant> getMsgs() {
			return msgs;
		}

		private:
		std::string os;
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

	encoder.begin_array(kIndefiniteLength);
	for (auto& msg : msgs) {
		if (std::holds_alternative<Message1>(msg)) encoder.push_value(std::get<Message1>(msg));
		else if (std::holds_alternative<Message2>(msg)) encoder.push_value(std::get<Message2>(msg));
		else assert(false);
	}

	encoder.end_indefinite();

	std::vector<uint8_t> encoded = encoder.finish();
	std::cout << " - encoded size " << encoded.size() << "\n";

	{
		ReplayVisitor v;
		BufferCborParser<ReplayVisitor> p(v, BinStreamBuffer{encoded.data(), encoded.size()});
		p.parse();

		std::vector<MessageVariant> msgs1;
	}

}
