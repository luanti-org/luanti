// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include "util/string.h"
#include "util/serialize.h"
#include <cmath>

class TestSerialization : public TestBase {
public:
	TestSerialization() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSerialization"; }

	void runTests(IGameDef *gamedef);
	void buildTestStrings();

	void testSerializeString();
	void testSerializeLongString();
	void testSerializeJsonString();
	void testDeSerializeString();
	void testDeSerializeLongString();
	void testStreamRead();
	void testStreamWrite();
	void testFloatFormat();

	std::string teststring2;
	std::wstring teststring2_w;
	std::string teststring2_w_encoded;

	static const u8 test_serialized_data[12 * 11 - 2];
};

static TestSerialization g_test_instance;

void TestSerialization::runTests(IGameDef *gamedef)
{
	buildTestStrings();

	TEST(testSerializeString);
	TEST(testDeSerializeString);
	TEST(testSerializeLongString);
	TEST(testDeSerializeLongString);
	TEST(testSerializeJsonString);
	TEST(testStreamRead);
	TEST(testStreamWrite);
	TEST(testFloatFormat);
}

////////////////////////////////////////////////////////////////////////////////

// To be used like this:
//   mkstr("Some\0string\0with\0embedded\0nuls")
// since std::string("...") doesn't work as expected in that case.
template<size_t N> std::string mkstr(const char (&s)[N])
{
	return std::string(s, N - 1);
}

void TestSerialization::buildTestStrings()
{
	std::ostringstream tmp_os;
	std::wostringstream tmp_os_w;
	std::ostringstream tmp_os_w_encoded;
	for (int i = 0; i < 256; i++) {
		tmp_os << (char)i;
		tmp_os_w << (wchar_t)i;
		tmp_os_w_encoded << (char)0 << (char)i;
	}
	teststring2 = tmp_os.str();
	teststring2_w = tmp_os_w.str();
	teststring2_w_encoded = tmp_os_w_encoded.str();
}

void TestSerialization::testSerializeString()
{
	// Test blank string
	UASSERT(serializeString16("") == mkstr("\0\0"));

	// Test basic string
	UASSERT(serializeString16("Hello world!") == mkstr("\0\14Hello world!"));

	// Test character range
	UASSERT(serializeString16(teststring2) == mkstr("\1\0") + teststring2);
}

void TestSerialization::testDeSerializeString()
{
	// Test deserialize
	{
		std::istringstream is(serializeString16(teststring2), std::ios::binary);
		UASSERT(deSerializeString16(is) == teststring2);
		UASSERT(!is.eof());
		is.get();
		UASSERT(is.eof());
	}

	// Test deserialize an incomplete length specifier
	{
		std::istringstream is(mkstr("\x53"), std::ios::binary);
		EXCEPTION_CHECK(SerializationError, deSerializeString16(is));
	}

	// Test deserialize a string with incomplete data
	{
		std::istringstream is(mkstr("\x00\x55 abcdefg"), std::ios::binary);
		EXCEPTION_CHECK(SerializationError, deSerializeString16(is));
	}
}

void TestSerialization::testSerializeLongString()
{
	// Test blank string
	UASSERT(serializeString32("") == mkstr("\0\0\0\0"));

	// Test basic string
	UASSERT(serializeString32("Hello world!") == mkstr("\0\0\0\14Hello world!"));

	// Test character range
	UASSERT(serializeString32(teststring2) == mkstr("\0\0\1\0") + teststring2);
}

void TestSerialization::testDeSerializeLongString()
{
	// Test deserialize
	{
		std::istringstream is(serializeString32(teststring2), std::ios::binary);
		UASSERT(deSerializeString32(is) == teststring2);
		UASSERT(!is.eof());
		is.get();
		UASSERT(is.eof());
	}

	// Test deserialize an incomplete length specifier
	{
		std::istringstream is(mkstr("\x53"), std::ios::binary);
		EXCEPTION_CHECK(SerializationError, deSerializeString32(is));
	}

	// Test deserialize a string with incomplete data
	{
		std::istringstream is(mkstr("\x00\x00\x00\x05 abc"), std::ios::binary);
		EXCEPTION_CHECK(SerializationError, deSerializeString32(is));
	}

	// Test deserialize a string with a length too large
	{
		std::istringstream is(mkstr("\xFF\xFF\xFF\xFF blah"), std::ios::binary);
		EXCEPTION_CHECK(SerializationError, deSerializeString32(is));
	}
}


void TestSerialization::testSerializeJsonString()
{
	std::istringstream is(std::ios::binary);
	const auto reset_is = [&] (const std::string &s) {
		is.clear();
		is.str(s);
	};
	const auto assert_at_eof = [] (std::istream &is) {
		is.get();
		UASSERT(is.eof());
	};

	// Test blank string
	UASSERTEQ(std::string, serializeJsonString(""), "\"\"");
	reset_is("\"\"");
	UASSERTEQ(std::string, deSerializeJsonString(is), "");
	assert_at_eof(is);

	// Test basic string
	UASSERTEQ(std::string, serializeJsonString("Hello world!"), "\"Hello world!\"");
	reset_is("\"Hello world!\"");
	UASSERTEQ(std::string, deSerializeJsonString(is), "Hello world!");
	assert_at_eof(is);

	// Test optional serialization
	const std::pair<const char*, const char*> test_pairs[] = {
		{ "abc", "abc" },
		{ "x y z", "\"x y z\"" },
		{ "\"", "\"\\\"\"" },
	};
	for (auto it : test_pairs) {
		UASSERTEQ(std::string, serializeJsonStringIfNeeded(it.first), it.second);
		reset_is(it.second);
		UASSERTEQ(std::string, deSerializeJsonStringIfNeeded(is), it.first);
		assert_at_eof(is);
	}

	// Test all byte values
	const std::string bs = "\\"; // MSVC fails when directly using "\\\\"
	const std::string expected = mkstr("\"") +
		"\\u0000\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007" +
		"\\b\\t\\n\\u000b\\f\\r\\u000e\\u000f" +
		"\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017" +
		"\\u0018\\u0019\\u001a\\u001b\\u001c\\u001d\\u001e\\u001f" +
		" !\\\"" + teststring2.substr(0x23, 0x5c-0x23) +
		bs + bs + teststring2.substr(0x5d, 0x7f-0x5d) + "\\u007f" +
		"\\u0080\\u0081\\u0082\\u0083\\u0084\\u0085\\u0086\\u0087" +
		"\\u0088\\u0089\\u008a\\u008b\\u008c\\u008d\\u008e\\u008f" +
		"\\u0090\\u0091\\u0092\\u0093\\u0094\\u0095\\u0096\\u0097" +
		"\\u0098\\u0099\\u009a\\u009b\\u009c\\u009d\\u009e\\u009f" +
		"\\u00a0\\u00a1\\u00a2\\u00a3\\u00a4\\u00a5\\u00a6\\u00a7" +
		"\\u00a8\\u00a9\\u00aa\\u00ab\\u00ac\\u00ad\\u00ae\\u00af" +
		"\\u00b0\\u00b1\\u00b2\\u00b3\\u00b4\\u00b5\\u00b6\\u00b7" +
		"\\u00b8\\u00b9\\u00ba\\u00bb\\u00bc\\u00bd\\u00be\\u00bf" +
		"\\u00c0\\u00c1\\u00c2\\u00c3\\u00c4\\u00c5\\u00c6\\u00c7" +
		"\\u00c8\\u00c9\\u00ca\\u00cb\\u00cc\\u00cd\\u00ce\\u00cf" +
		"\\u00d0\\u00d1\\u00d2\\u00d3\\u00d4\\u00d5\\u00d6\\u00d7" +
		"\\u00d8\\u00d9\\u00da\\u00db\\u00dc\\u00dd\\u00de\\u00df" +
		"\\u00e0\\u00e1\\u00e2\\u00e3\\u00e4\\u00e5\\u00e6\\u00e7" +
		"\\u00e8\\u00e9\\u00ea\\u00eb\\u00ec\\u00ed\\u00ee\\u00ef" +
		"\\u00f0\\u00f1\\u00f2\\u00f3\\u00f4\\u00f5\\u00f6\\u00f7" +
		"\\u00f8\\u00f9\\u00fa\\u00fb\\u00fc\\u00fd\\u00fe\\u00ff" +
		"\"";
	std::string serialized = serializeJsonString(teststring2);
	UASSERTEQ(std::string, serialized, expected);

	reset_is(serialized);
	UASSERTEQ(std::string, deSerializeJsonString(is), teststring2);
	UASSERT(!is.eof()); // should have stopped at " so eof must not be set yet
	assert_at_eof(is);

	// Test that deserialization leaves rest of stream alone
	std::string tmp;
	reset_is("\"foo\"bar");
	UASSERTEQ(std::string, deSerializeJsonString(is), "foo");
	std::getline(is, tmp, '\0');
	UASSERTEQ(std::string, tmp, "bar");

	reset_is("\"x y z\"bar");
	UASSERTEQ(std::string, deSerializeJsonStringIfNeeded(is), "x y z");
	std::getline(is, tmp, '\0');
	UASSERTEQ(std::string, tmp, "bar");

	reset_is("foo bar");
	UASSERTEQ(std::string, deSerializeJsonStringIfNeeded(is), "foo");
	std::getline(is, tmp, '\0');
	UASSERTEQ(std::string, tmp, " bar");
}


void TestSerialization::testStreamRead()
{
	std::string datastr(
		(const char *)test_serialized_data,
		sizeof(test_serialized_data));
	std::istringstream is(datastr, std::ios_base::binary);

	UASSERT(readU8(is) == 0x11);
	UASSERT(readU16(is) == 0x2233);
	UASSERT(readU32(is) == 0x44556677);
	UASSERT(readU64(is) == 0x8899AABBCCDDEEFFLL);

	UASSERT(readS8(is) == -128);
	UASSERT(readS16(is) == 30000);
	UASSERT(readS32(is) == -6);
	UASSERT(readS64(is) == -43);

	UASSERT(readF1000(is) == 53.534f);
	UASSERT(readF1000(is) == -300000.32f);
	UASSERT(readF1000(is) == F1000_MIN);
	UASSERT(readF1000(is) == F1000_MAX);

	UASSERT(deSerializeString16(is) == "foobar!");

	UASSERT(readV2S16(is) == v2s16(500, 500));
	UASSERT(readV3S16(is) == v3s16(4207, 604, -30));
	UASSERT(readV2S32(is) == v2s32(1920, 1080));
	UASSERT(readV3S32(is) == v3s32(-400, 6400054, 290549855));

	UASSERT(readV3F1000(is) == v3f(500, 10024.2f, -192.54f));
	UASSERT(readARGB8(is) == video::SColor(255, 128, 50, 128));

	UASSERT(deSerializeString32(is) == "some longer string here");

	UASSERT(is.rdbuf()->in_avail() == 2);
	UASSERT(readU16(is) == 0xF00D);
	UASSERT(is.rdbuf()->in_avail() == 0);
}


void TestSerialization::testStreamWrite()
{
	std::ostringstream os(std::ios_base::binary);
	std::string data;

	writeU8(os, 0x11);
	writeU16(os, 0x2233);
	writeU32(os, 0x44556677);
	writeU64(os, 0x8899AABBCCDDEEFFLL);

	writeS8(os, -128);
	writeS16(os, 30000);
	writeS32(os, -6);
	writeS64(os, -43);

	writeF1000(os, 53.53467f);
	writeF1000(os, -300000.32f);
	writeF1000(os, F1000_MIN);
	writeF1000(os, F1000_MAX);

	os << serializeString16("foobar!");

	data = os.str();
	UASSERT(data.size() < sizeof(test_serialized_data));
	UASSERT(!memcmp(&data[0], test_serialized_data, data.size()));

	writeV2S16(os, v2s16(500, 500));
	writeV3S16(os, v3s16(4207, 604, -30));
	writeV2S32(os, v2s32(1920, 1080));
	writeV3S32(os, v3s32(-400, 6400054, 290549855));

	writeV3F1000(os, v3f(500, 10024.2f, -192.54f));
	writeARGB8(os, video::SColor(255, 128, 50, 128));

	os << serializeString32("some longer string here");

	writeU16(os, 0xF00D);

	data = os.str();
	UASSERT(data.size() == sizeof(test_serialized_data));
	UASSERT(!memcmp(&data[0], test_serialized_data, sizeof(test_serialized_data)));
}


void TestSerialization::testFloatFormat()
{
	FloatType type = getFloatSerializationType();
	u32 i;
	f32 fs, fm;

	// Check precision of float calculations on this platform
	const std::unordered_map<f32, u32> float_results = {
		{  0.0f, 0x00000000UL },
		{  1.0f, 0x3F800000UL },
		{ -1.0f, 0xBF800000UL },
		{  0.1f, 0x3DCCCCCDUL },
		{ -0.1f, 0xBDCCCCCDUL },
		{ 1945329.25f, 0x49ED778AUL },
		{ -23298764.f, 0xCBB1C166UL },
		{  0.5f, 0x3F000000UL },
		{ -0.5f, 0xBF000000UL }
	};
	for (const auto &v : float_results) {
		i = f32Tou32Slow(v.first);
		if (std::abs((s64)v.second - i) > 32) {
			printf("Inaccurate float values on %.9g, expected 0x%X, actual 0x%X\n",
				v.first, v.second, i);
			UASSERT(false);
		}

		fs = u32Tof32Slow(v.second);
		if (std::fabs(v.first - fs) > std::fabs(v.first * 0.000005f)) {
			printf("Inaccurate float values on 0x%X, expected %.9g, actual 0x%.9g\n",
				v.second, v.first, fs);
			UASSERT(false);
		}
	}

	if (type == FLOATTYPE_SLOW) {
		// conversion using memcpy is not possible
		// Skip exact float comparison checks below
		return;
	}

	// The code below compares the IEEE conversion functions with a
	// known good IEC559/IEEE754 implementation. This test neeeds
	// IEC559 compliance in the compiler.
#if defined(__GNUC__) && (!defined(__STDC_IEC_559__) || defined(__FAST_MATH__))
	// GNU C++ lies about its IEC559 support when -ffast-math is active.
	// https://gcc.gnu.org/bugzilla//show_bug.cgi?id=84949
	bool is_iec559 = false;
#else
	bool is_iec559 = std::numeric_limits<f32>::is_iec559;
#endif
	if (!is_iec559)
		return;

	auto test_single = [&fs, &fm](const u32 &i) -> bool {
		memcpy(&fm, &i, 4);
		fs = u32Tof32Slow(i);
		if (fm != fs) {
			printf("u32Tof32Slow failed on 0x%X, expected %.9g, actual %.9g\n",
				i, fm, fs);
			return false;
		}
		if (f32Tou32Slow(fs) != i) {
			printf("f32Tou32Slow failed on %.9g, expected 0x%X, actual 0x%X\n",
				fs, i, f32Tou32Slow(fs));
			return false;
		}
		return true;
	};

	// Use step of prime 277 to speed things up from 3 minutes to a few seconds
	// Test from 0 to 0xFF800000UL (positive)
	for (i = 0x00000000UL; i <= 0x7F800000UL; i += 277)
		UASSERT(test_single(i));

	// Ensure +inf and -inf are tested
	UASSERT(test_single(0x7F800000UL));
	UASSERT(test_single(0xFF800000UL));

	// Test from 0x80000000UL to 0xFF800000UL (negative)
	for (i = 0x80000000UL; i <= 0xFF800000UL; i += 277)
		UASSERT(test_single(i));
}

const u8 TestSerialization::test_serialized_data[12 * 11 - 2] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc,
	0xdd, 0xee, 0xff, 0x80, 0x75, 0x30, 0xff, 0xff, 0xff, 0xfa, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xd5, 0x00, 0x00, 0xd1, 0x1e, 0xee, 0x1e,
	0x5b, 0xc0, 0x80, 0x00, 0x02, 0x80, 0x7F, 0xFF, 0xFD, 0x80, 0x00, 0x07,
	0x66, 0x6f, 0x6f, 0x62, 0x61, 0x72, 0x21, 0x01, 0xf4, 0x01, 0xf4, 0x10,
	0x6f, 0x02, 0x5c, 0xff, 0xe2, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x04,
	0x38, 0xff, 0xff, 0xfe, 0x70, 0x00, 0x61, 0xa8, 0x36, 0x11, 0x51, 0x70,
	0x5f, 0x00, 0x07, 0xa1, 0x20, 0x00, 0x98, 0xf5, 0x08, 0xff,
	0xfd, 0x0f, 0xe4, 0xff, 0x80, 0x32, 0x80, 0x00, 0x00, 0x00, 0x17, 0x73,
	0x6f, 0x6d, 0x65, 0x20, 0x6c, 0x6f, 0x6e, 0x67, 0x65, 0x72, 0x20, 0x73,
	0x74, 0x72, 0x69, 0x6e, 0x67, 0x20, 0x68, 0x65, 0x72, 0x65, 0xF0, 0x0D,
};
