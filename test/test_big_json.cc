#include <gtest/gtest.h>

#include <fstream>

#include "json_printer.hpp"
#include "timing.hpp"

// NOTE: You must run `python3 -m pysrc.getTestData` PRIOR to running this test.

namespace {
std::vector<uint8_t> read_file_bytes(const std::string& path) {
	std::ifstream ifs(path);
	ifs.seekg(0, std::ios_base::end);
	size_t len = ifs.tellg();
	assert(len < (1<<30));
	std::vector<uint8_t> data(len);
	ifs.seekg(0, std::ios_base::beg);
	ifs.read((char*)data.data(), len);
	ifs.close();
	return data;
}
std::string read_file_string(const std::string& path) {
	std::ifstream ifs(path);
	ifs.seekg(0, std::ios_base::end);
	size_t len = ifs.tellg();
	std::string data;
	data.resize(len);
	assert(len < (1<<30));
	ifs.seekg(0, std::ios_base::beg);
	ifs.read((char*)data.data(), len);
	ifs.close();
	return data;
}

bool files_are_same(const std::string& pathA, const std::string& pathB) {
	std::string a, b;
	a = read_file_string(pathA);
	b = read_file_string(pathB);
	return a == b;
}
}


// FIXME: These tests do not make assertions. The actual json output is malformed (jq complains).
//        But they do test that we can parse a big cbor file correctly.

TEST(Parser, BigJson_BufferInput) {
	std::string cborInputPath = "/tmp/big.cbor";
	std::string cborOutputPath = "/tmp/big.fromCbor.buffer.json";

	auto t0 = getMicros();
	auto data = read_file_bytes(cborInputPath);
	std::ofstream ofs(cborOutputPath);

	std::cout << " - input size: " << static_cast<double>(data.size()) / (1<<20) << "MB\n";

	auto t1 = getMicros();
	OnlineCborParser p(BinStreamBuffer{data.data(), data.size()});
	JsonPrinter jp(p);
	auto t2 = getMicros();

	std::cout << " - [buffer input] 'big.cbor' parse took: " << (t2-t1) * 1e-3 << "ms\n";
	std::cout << " - [buffer input] 'big.cbor' parse took including read: " << (t2-t0) * 1e-3 << "ms\n";

	ofs << jp.os;

}
