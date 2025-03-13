// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <sstream>

#include "log.h"
#include "serialization.h"
#include "nodedef.h"
#include "noise.h"

class TestCompression : public TestBase {
public:
	TestCompression() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestCompression"; }

	void runTests(IGameDef *gamedef);

	void testRLECompression();
	void testZlibCompression();
	void testZlibLargeData();
	void testZstdLargeData();
	void testZlibLimit();
	void _testZlibLimit(u32 size, u32 limit);
};

static TestCompression g_test_instance;

void TestCompression::runTests(IGameDef *gamedef)
{
	TEST(testRLECompression);
	TEST(testZlibCompression);
	TEST(testZlibLargeData);
	TEST(testZstdLargeData);
	TEST(testZlibLimit);
}

////////////////////////////////////////////////////////////////////////////////

void TestCompression::testRLECompression()
{
	Buffer<u8> fromdata(4);
	fromdata[0]=1;
	fromdata[1]=5;
	fromdata[2]=5;
	fromdata[3]=1;

	std::ostringstream os(std::ios_base::binary);
	compress(fromdata, os, 0);

	std::string str_out = os.str();

	infostream << "str_out.size()="<<str_out.size()<<std::endl;
	infostream << "TestCompress: 1,5,5,1 -> ";
	for (char i : str_out)
		infostream << (u32) i << ",";
	infostream << std::endl;

	UASSERT(str_out.size() == 10);

	UASSERT(str_out[0] == 0);
	UASSERT(str_out[1] == 0);
	UASSERT(str_out[2] == 0);
	UASSERT(str_out[3] == 4);
	UASSERT(str_out[4] == 0);
	UASSERT(str_out[5] == 1);
	UASSERT(str_out[6] == 1);
	UASSERT(str_out[7] == 5);
	UASSERT(str_out[8] == 0);
	UASSERT(str_out[9] == 1);

	std::istringstream is(str_out, std::ios_base::binary);
	std::ostringstream os2(std::ios_base::binary);

	decompress(is, os2, 0);
	std::string str_out2 = os2.str();

	infostream << "decompress: ";
	for (char i : str_out2)
		infostream << (u32) i << ",";
	infostream << std::endl;

	UASSERTEQ(size_t, str_out2.size(), fromdata.getSize());

	for (u32 i = 0; i < str_out2.size(); i++)
		UASSERT(str_out2[i] == fromdata[i]);
}

void TestCompression::testZlibCompression()
{
	Buffer<u8> fromdata(4);
	fromdata[0]=1;
	fromdata[1]=5;
	fromdata[2]=5;
	fromdata[3]=1;

	std::ostringstream os(std::ios_base::binary);
	compressZlib(*fromdata, fromdata.getSize(), os);

	std::string str_out = os.str();

	infostream << "str_out.size()=" << str_out.size() <<std::endl;
	infostream << "TestCompress: 1,5,5,1 -> ";
	for (char i : str_out)
		infostream << (u32) i << ",";
	infostream << std::endl;

	std::istringstream is(str_out, std::ios_base::binary);
	std::ostringstream os2(std::ios_base::binary);

	decompressZlib(is, os2);
	std::string str_out2 = os2.str();

	infostream << "decompress: ";
	for (char i : str_out2)
		infostream << (u32) i << ",";
	infostream << std::endl;

	UASSERTEQ(size_t, str_out2.size(), fromdata.getSize());

	for (u32 i = 0; i < str_out2.size(); i++)
		UASSERT(str_out2[i] == fromdata[i]);
}

void TestCompression::testZlibLargeData()
{
	infostream << "Test: Testing zlib wrappers with a large amount "
		"of pseudorandom data" << std::endl;

	u32 size = 50000;
	infostream << "Test: Input size of large compressZlib is "
		<< size << std::endl;

	std::string data_in;
	data_in.resize(size);
	PseudoRandom pseudorandom(9420);
	for (u32 i = 0; i < size; i++)
		data_in[i] = pseudorandom.range(0, 255);

	std::ostringstream os_compressed(std::ios::binary);
	compressZlib(data_in, os_compressed);
	infostream << "Test: Output size of large compressZlib is "
		<< os_compressed.str().size()<<std::endl;

	std::istringstream is_compressed(os_compressed.str(), std::ios::binary);
	std::ostringstream os_decompressed(std::ios::binary);
	decompressZlib(is_compressed, os_decompressed);
	infostream << "Test: Output size of large decompressZlib is "
		<< os_decompressed.str().size() << std::endl;

	std::string str_decompressed = os_decompressed.str();
	UASSERTEQ(size_t, str_decompressed.size(), data_in.size());

	for (u32 i = 0; i < size && i < str_decompressed.size(); i++) {
		UTEST(str_decompressed[i] == data_in[i],
				"index out[%i]=%i differs from in[%i]=%i",
				i, str_decompressed[i], i, data_in[i]);
	}
}

void TestCompression::testZstdLargeData()
{
	infostream << "Test: Testing zstd wrappers with a large amount "
		"of pseudorandom data" << std::endl;

	u32 size = 500000;
	infostream << "Test: Input size of large compressZstd is "
		<< size << std::endl;

	std::string data_in;
	data_in.resize(size);
	PseudoRandom pseudorandom(9420);
	for (u32 i = 0; i < size; i++)
		data_in[i] = pseudorandom.range(0, 255);

	std::ostringstream os_compressed(std::ios::binary);
	compressZstd(data_in, os_compressed, 0);
	infostream << "Test: Output size of large compressZstd is "
		<< os_compressed.str().size()<<std::endl;

	std::istringstream is_compressed(os_compressed.str(), std::ios::binary);
	std::ostringstream os_decompressed(std::ios::binary);
	decompressZstd(is_compressed, os_decompressed);
	infostream << "Test: Output size of large decompressZstd is "
		<< os_decompressed.str().size() << std::endl;

	std::string str_decompressed = os_decompressed.str();
	UASSERTEQ(size_t, str_decompressed.size(), data_in.size());

	for (u32 i = 0; i < size && i < str_decompressed.size(); i++) {
		UTEST(str_decompressed[i] == data_in[i],
				"index out[%i]=%i differs from in[%i]=%i",
				i, str_decompressed[i], i, data_in[i]);
	}
}

void TestCompression::testZlibLimit()
{
	// edge cases
	_testZlibLimit(1024, 1023);
	_testZlibLimit(1024, 1024);
	_testZlibLimit(1024, 1025);

	// test around buffer borders
	u32 bufsize = 16384; // as in implementation
	for (int s = -1; s <= 1; s++)
	{
		for (int l = -1; l <= 1; l++)
		{
			_testZlibLimit(bufsize + s, bufsize + l);
		}
	}
	// span multiple buffers
	_testZlibLimit(35000, 22000);
	_testZlibLimit(22000, 35000);
}

void TestCompression::_testZlibLimit(u32 size, u32 limit)
{
	infostream << "Test: Testing zlib wrappers with a decompression "
		"memory limit of " << limit << std::endl;

	infostream << "Test: Input size of compressZlib for limit is "
		<< size << std::endl;

	// how much data we expect to get
	u32 expected = size < limit ? size : limit;

	// create recognizable data
	std::string data_in;
	data_in.resize(size);
	for (u32 i = 0; i < size; i++)
		data_in[i] = (u8)(i % 256);

	std::ostringstream os_compressed(std::ios::binary);
	compressZlib(data_in, os_compressed);
	infostream << "Test: Output size of compressZlib for limit is "
		<< os_compressed.str().size()<<std::endl;

	std::istringstream is_compressed(os_compressed.str(), std::ios::binary);
	std::ostringstream os_decompressed(std::ios::binary);
	decompressZlib(is_compressed, os_decompressed, limit);
	infostream << "Test: Output size of decompressZlib with limit is "
		<< os_decompressed.str().size() << std::endl;

	std::string str_decompressed = os_decompressed.str();
	UASSERTEQ(size_t, str_decompressed.size(), expected);

	for (u32 i = 0; i < size && i < str_decompressed.size(); i++) {
		UTEST(str_decompressed[i] == data_in[i],
				"index out[%i]=%i differs from in[%i]=%i",
				i, str_decompressed[i], i, data_in[i]);
	}
}

