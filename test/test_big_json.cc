#include <gtest/gtest.h>

#include <fstream>

#include "to_json_visitor.hpp"
#include "timing.hpp"


TEST(Parser, BigJson_BufferInput) {


	std::string cborInputPath = "/tmp/big.cbor";
	std::string cborOutputPath = "/tmp/big.fromCbor.buffer.json";

	std::ifstream ifs(cborInputPath);
	std::ofstream ofs(cborOutputPath);

	ifs.seekg(0, std::ios_base::end);
	size_t len = ifs.tellg();
	std::vector<uint8_t> data(len);
	ifs.seekg(0, std::ios_base::beg);
	ifs.read((char*)data.data(), len);
	ifs.close();

	std::cout << " - input size: " << len << "\n";

	ToJsonVisitor v;


	auto t0 = getMicros();
	BufferCborParser<ToJsonVisitor> p(v, BinStreamBuffer{data.data(), data.size()});
	p.parse();
	auto t1 = getMicros();

	std::cout << " - [buffer input] 'big.cbor' parse took: " << (t1-t0) * 1e-3 << "ms\n";

	ofs << v.finish();

}

TEST(Parser, BigJson_FileInput) {


	std::string cborInputPath = "/tmp/big.cbor";
	std::string cborOutputPath = "/tmp/big.fromCbor.file.json";

	std::ifstream ifs(cborInputPath);
	std::ofstream ofs(cborOutputPath);

	ToJsonVisitor v;

	auto t0 = getMicros();
	FileCborParser<ToJsonVisitor> p(v, BinStreamFile{ifs});
	p.parse();
	auto t1 = getMicros();

	std::cout << " - [file input] 'big.cbor' parse took: " << (t1-t0) * 1e-3 << "ms\n";

	ofs << v.finish();

}

