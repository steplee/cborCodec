#include <gtest/gtest.h>

#include <fstream>
#include <cassert>

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


	struct FieldKey {
		size_t i = kIndefiniteLength;
		std::string_view s = {};
		inline bool operator==(size_t k) const { return i == k; }
		inline bool operator==(std::string_view k) const { return s == k; }
		inline FieldKey() : i(kIndefiniteLength), s() {}
		inline FieldKey(size_t k) : i(k) {}
		inline FieldKey(std::string_view k) : s(k) {}
	};



	struct MessageParser {
		private:

			int baseDepth;
			FieldKey k;
			MessageTag m;

		public:

			inline MessageParser() {
			}

			inline void begin(int baseDepth) {
				this->baseDepth = baseDepth;
			}

			template <class T>
			inline void visit_value(State state, T&& t) {
				auto seq = state.sequenceIdx;
				auto depth = state.depth;
				auto mode = state.mode;
				assert(depth >= baseDepth);

				if (depth == baseDepth) {
					assert(mode == Mode::map);
					if (seq % 2 == 0) {
						if constexpr(std::is_same_v<T,TextStringView>) k = { (TextStringView) t };
						else if constexpr(std::is_integral_v<T>) k = { (size_t) t };
						else throw std::runtime_error("unsupported key.");
					} else {
						if (k == (size_t)CommonFieldTag::eMessageTag) {
							if constexpr (std::is_integral_v<T>)
								m = (MessageTag)(size_t)t;
							else
								throw std::runtime_error("unsupported value.");
						}
						if (m == MessageTag::eMessage1) {
							if (k == (size_t)Message1Tags::eField1) {
								// This sucks. It's not feasible.
							}
						}
					}
				}
			}

	};


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

		struct ReplayState {
			int beginDepth = 0;
			enum Mode {
				Root, Header, MessageList, Message
			} mode = Root;
			MessageTag messageTag = (MessageTag)0; // valid only if `mode == Message`.
		};

		std::vector<ReplayState> replayState = { ReplayState{} };

		// When rmode == MessageList
		std::string_view curMessageKey;
		size_t curMessageKeyInt = kIndefiniteLength;

		// When rmode == Message
		std::string_view curFieldKey;
		size_t curFieldKeyInt = kIndefiniteLength;

		ApplicationMetadata appMeta;
		std::vector<MessageVariant> msgs;

		MessageTag curMsgTag = MessageTag::eEnd;
		int curMsgDepthBegin = -1;

		/*
		bool didStartMsgs;
		bool didEndMsgs;
		int msgStartDepth; // if we ever return past this after `didStartMsgs`, set `didEndMsgs` to false.
		bool inMsgs() const {
			return didStartMsgs and !didEndMsgs;
		}
		*/

		public:

		template <class T>
		inline void visit_value(State state, T&& t) {

			int seq = state.sequenceIdx;
			int depth = state.depth;
			auto mode = state.mode;

			const auto& rstate0 = replayState.back();

			// confusing because beginarray/beginmap come BEFORE `stateStack` is modified...
			if (depth < rstate0.beginDepth and !std::is_same_v<T,BeginArray> and !std::is_same_v<T,BeginMap>) {
				const char *a = "root";
				if (replayState.back().mode == ReplayState::Mode::Header) a = "header";
				if (replayState.back().mode == ReplayState::Mode::MessageList) a = "messageList";
				if (replayState.back().mode == ReplayState::Mode::Message) a = "message";
				replayState.pop_back();
				const char *b = "root";
				if (replayState.back().mode == ReplayState::Mode::Header) b = "header";
				if (replayState.back().mode == ReplayState::Mode::MessageList) b = "messageList";
				if (replayState.back().mode == ReplayState::Mode::Message) b = "message";
				printf("(d %d) exit :: %s -> %s\n", depth, a,b);
			}

			const auto& rstate = replayState.back();
			auto rmode = rstate.mode;

			if constexpr (std::is_same_v<T, BeginMap>) {
				if (rmode == ReplayState::Mode::MessageList) {
					printf("begin map (rmode messageList) (d %d) (s %d)\n", depth,seq);
					// assert(mode == Mode::map);
					assert(rstate.beginDepth == depth);
					assert(seq % 2 == 1);
					replayState.push_back(ReplayState{depth+1, ReplayState::Mode::Message});
					msgParser.begin(depth+1);
					return;
				}
			} else if constexpr (std::is_same_v<T, BeginArray>) {
			} else if constexpr (std::is_same_v<T, EndMap>) {
			} else if constexpr (std::is_same_v<T, EndArray>) {
			} else {

				/*
				if constexpr(std::is_same_v<T, TextStringView>) {
					printf("text: %s (d %d)\n", std::string{t}.c_str(), state.depth);
				}
				*/

				if (rmode == ReplayState::Mode::MessageList) {
					if (seq % 2 == 0) {
						if constexpr (std::is_same_v<T,TextStringView>) curMessageKey = t;
						else if constexpr (std::is_integral_v<T>) curMessageKeyInt = (size_t)t;
						else throw std::runtime_error("Message keys must be strings or ints.");
					} else {
					// else we are entering BeginMap{}
					}
				}

				else if (rmode == ReplayState::Mode::Message) {

					// TODO: Call into combinator etc.


					/*
					if (depth == rbeginDepth) {
						if (seq % 2 == 0) {
							if constexpr (std::is_same_v<T,TextStringView>) curFieldKey = t;
							else if constexpr (std::is_integral_v<T>) curFieldKeyInt = (size_t)t;
							else throw std::runtime_error("Field keys must be strings or ints.");
						} else {
						}
					} else {
					}
					*/
				}

				else if (mode == Mode::map and rmode == ReplayState::Mode::Root and depth == 1 and seq % 2 == 0) {
					if constexpr(std::is_same_v<T, TextStringView>) {
						std::string_view key { t };
						printf("begin string '%s' (rmode messageList) (d %d) (s %d)\n", std::string{t}.c_str(),depth,seq);
						if (key == "metadata") {
							printf("enter header\n");
							replayState.push_back(ReplayState{depth+1, ReplayState::Mode::Header});
						} else if (key == "messages") {
							printf("enter messages\n");
							replayState.push_back(ReplayState{depth+1, ReplayState::Mode::MessageList});
						}
						return;
					}
				}

				if (rmode == ReplayState::Mode::Message) {
					msgParser.visit_value(state, std::move(t));
				}
			}
		}


		inline std::vector<MessageVariant> getMsgs() {
			throw std::runtime_error(".");
			// return msgParser.msgs;
		}

		private:
			MessageParser msgParser;
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

	encoder.push_value(std::string{"messages"});
	encoder.begin_array(kIndefiniteLength);
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
		ReplayVisitor v;
		BufferCborParser<ReplayVisitor> p(v, BinStreamBuffer{encoded.data(), encoded.size()});
		p.parse();

		std::vector<MessageVariant> msgs1;
	}

}
