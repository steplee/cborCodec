#include "cborCodec/cbor_encoder.hpp"
#include "cborCodec/cbor_parser.hpp"

#include "cstdio"

using namespace cbor;


void check_encoder() {
	CborEncoder e;

	e.push_value(uint64_t{1});
	e.push_value(uint64_t{30});
	e.push_value(.5f);

	e.begin_array(1);
	e.push_value(uint64_t{1});

	e.push_value(int64_t{-500});

	e.begin_map(1);
	e.push_value(uint64_t{1lu<<41lu});
	e.push_value(uint64_t{1});
	
	int32_t taData[4] = {
		1,2,3,4
	};
	e.push_typed_array(taData, 4);

	auto vec = e.strm.finish();

	printf(" - Encoded: [\n");
	for (uint8_t b : vec) {
		printf("%d%d%d'%d%d%d%d%d = %d\n",
				(b >> 7) & 1,
				(b >> 6) & 1,
				(b >> 5) & 1,
				(b >> 4) & 1,
				(b >> 3) & 1,
				(b >> 2) & 1,
				(b >> 1) & 1,
				(b >> 0) & 1,
				b
				);
	}
	printf("], size %ld\n", vec.size());
}



struct MyVisitor {

	template <class T>
	inline void visit_value(Mode mode, size_t seqIdx, T&& t) {
		printDepth();
		if constexpr (std::is_integral_v<T>) printf("val: %ld\n", (int64_t)t);
		else if constexpr (std::is_same_v<T,float>) printf("val: %f\n", (float)t);
		else if constexpr (std::is_same_v<T,double>) printf("val: %lf\n", (double)t);
		else if constexpr (std::is_same_v<T, TextStringView>) printf("val: %s\n", std::string{t}.c_str());
		else if constexpr (std::is_same_v<T, ByteStringView>) printf("bytes of size %ld\n", t.size());
		else if constexpr (std::is_same_v<T, TypedArrayView>) {
			if (t.type == TypedArrayView::eInt32) {
				printf("typed int32 array of size %ld, endian: %d:\n", t.elementLength(), t.endianness);
				depth++;
				// std::vector<int32_t> vec = t.toVector<int32_t>();
				// std::vector<int32_t> vec = t.toVector<int32_t>();
				std::vector<int32_t> vec = reinterpret_cast<TypedArrayView*>(&t)->toVector<int32_t>();
				for (int i=0; i<t.elementLength(); i++) {
					printDepth();
					printf("%d\n", vec[i]);
				}
				depth--;
			} else if (t.type == TypedArrayView::eFloat64) {
				printf("typed float64 array of size %ld, endian: %d:\n", t.elementLength(), t.endianness);
				depth++;
				std::vector<double> vec = reinterpret_cast<TypedArrayView*>(&t)->toVector<double>();
				for (int i=0; i<t.elementLength(); i++) {
					printDepth();
					printf("%lf\n", vec[i]);
				}
				depth--;
			} else
				printf("typed array of size %ld\n", t.elementLength());
		}
		else printf("val: <unhandled>\n");
	}

	inline void visit_begin_array() {
		printDepth();
		printf("begin array.\n");
		depth++;
	}
	inline void visit_end_array() {
		printDepth();
		printf("end array.\n");
		depth--;
	}

	inline void visit_begin_map() {
		printDepth();
		printf("begin map.\n");
		depth++;
	}
	inline void visit_end_map() {
		printDepth();
		printf("end map.\n");
		depth--;
	}

	inline void printDepth() {
		for (int i=0; i<depth*4; i++) printf(" ");
	}

	int depth = 0;
};

void check_codec() {
	CborEncoder e;

	e.push_value(uint64_t{1});
	e.push_value(uint64_t{30});
	e.push_value(9990.5f);

	e.begin_array(1);
	e.push_value(uint64_t{1});

	e.push_value(int64_t{-500});

	e.begin_map(1);
	e.push_value(uint64_t{1lu<<41lu});
	e.push_value(uint64_t{1});

	int32_t taData[4] = {
		1,2,3,4
	};
	e.push_typed_array(taData, 4);

	double taDataD[4] = {
		1,2,3,4
	};
	e.push_typed_array(taDataD, 4);

	// Push an indef array
	e.begin_array(kIndefiniteLength);
	e.push_value(std::string{"yes"});
	// e.push_value(uint64_t{1});
	e.end_indefinite();

	auto vec = e.strm.finish();

	MyVisitor v;
	CborParser p(v, vec.data(), vec.size());
	p.parse();

}


int main() {

	check_encoder();
	check_codec();


	return 0;
}
