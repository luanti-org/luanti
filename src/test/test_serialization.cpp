// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "util/serialize.h"
#include "exceptions.h"

#include "catch.h"

#include <cmath>
#include <cstring>
#include <ios>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace {

// To be used like this:
//   mkstr("Some\0string\0with\0embedded\0nuls")
// since std::string("...") doesn't work as expected in that case.
template<size_t N> std::string mkstr(const char (&s)[N])
{
	return std::string(s, N - 1);
}

// A string containing every possible byte value exactly once, in order.
const std::string &allBytesString()
{
	static const std::string s = [] {
		std::string tmp;
		tmp.reserve(256);
		for (int i = 0; i < 256; i++)
			tmp.push_back((char)i);
		return tmp;
	}();
	return s;
}

const u8 test_serialized_data[12 * 11 - 2] = {
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

} // namespace

TEST_CASE("String16 serialization", "[serialization]") {
	const std::string &all_bytes = allBytesString();

	SECTION("blank string") {
		CHECK(serializeString16("") == mkstr("\0\0"));
	}

	SECTION("basic string") {
		CHECK(serializeString16("Hello world!") == mkstr("\0\14Hello world!"));
	}

	SECTION("character range") {
		CHECK(serializeString16(all_bytes) == mkstr("\1\0") + all_bytes);
	}
}

TEST_CASE("String16 deserialization", "[serialization]") {
	const std::string &all_bytes = allBytesString();

	SECTION("round trip") {
		std::istringstream is(serializeString16(all_bytes), std::ios::binary);
		CHECK(deSerializeString16(is) == all_bytes);
		CHECK(!is.eof());
		is.get();
		CHECK(is.eof());
	}

	SECTION("primitive reader functions throw at end of stream") {
		std::istringstream is(mkstr("\x00\x01\x02\x03"), std::ios::binary);
		CHECK(readU16(is) == 0x0001);
		CHECK(canRead(is));
		CHECK(readU16(is) == 0x0203);
		// Yet no EOF set, thus use `canRead`.
		CHECK(is.good());
		CHECK(!is.eof());
		CHECK(!canRead(is));

		// Out of bounds
		CHECK_THROWS_AS(readU8(is), SerializationError);
		CHECK(is.eof());
		CHECK(!canRead(is));
	}

	SECTION("incomplete length specifier") {
		std::istringstream is(mkstr("\x53"), std::ios::binary);
		CHECK_THROWS_AS(deSerializeString16(is), SerializationError);
	}

	SECTION("incomplete data") {
		std::istringstream is(mkstr("\x00\x55 abcdefg"), std::ios::binary);
		CHECK_THROWS_AS(deSerializeString16(is), SerializationError);
	}
}

TEST_CASE("String32 serialization", "[serialization]") {
	const std::string &all_bytes = allBytesString();

	SECTION("blank string") {
		CHECK(serializeString32("") == mkstr("\0\0\0\0"));
	}

	SECTION("basic string") {
		CHECK(serializeString32("Hello world!") == mkstr("\0\0\0\14Hello world!"));
	}

	SECTION("character range") {
		CHECK(serializeString32(all_bytes) == mkstr("\0\0\1\0") + all_bytes);
	}
}

TEST_CASE("String32 deserialization", "[serialization]") {
	const std::string &all_bytes = allBytesString();

	SECTION("round trip") {
		std::istringstream is(serializeString32(all_bytes), std::ios::binary);
		CHECK(deSerializeString32(is) == all_bytes);
		CHECK(!is.eof());
		is.get();
		CHECK(is.eof());
	}

	SECTION("incomplete length specifier") {
		std::istringstream is(mkstr("\x53"), std::ios::binary);
		CHECK_THROWS_AS(deSerializeString32(is), SerializationError);
	}

	SECTION("incomplete data") {
		std::istringstream is(mkstr("\x00\x00\x00\x05 abc"), std::ios::binary);
		CHECK_THROWS_AS(deSerializeString32(is), SerializationError);
	}

	SECTION("length too large") {
		std::istringstream is(mkstr("\xFF\xFF\xFF\xFF blah"), std::ios::binary);
		CHECK_THROWS_AS(deSerializeString32(is), SerializationError);
	}
}

TEST_CASE("JSON string serialization", "[serialization]") {
	const std::string &all_bytes = allBytesString();

	std::istringstream is(std::ios::binary);
	const auto reset_is = [&] (const std::string &s) {
		is.clear();
		is.str(s);
	};
	const auto check_at_eof = [] (std::istream &is) {
		is.get();
		CHECK(is.eof());
	};

	SECTION("blank string") {
		CHECK(serializeJsonString("") == "\"\"");
		reset_is("\"\"");
		CHECK(deSerializeJsonString(is) == "");
		check_at_eof(is);
	}

	SECTION("basic string") {
		CHECK(serializeJsonString("Hello world!") == "\"Hello world!\"");
		reset_is("\"Hello world!\"");
		CHECK(deSerializeJsonString(is) == "Hello world!");
		check_at_eof(is);
	}

	SECTION("optional serialization") {
		const std::pair<const char*, const char*> test_pairs[] = {
			{ "abc", "abc" },
			{ "x y z", "\"x y z\"" },
			{ "\"", "\"\\\"\"" },
		};
		for (auto it : test_pairs) {
			CHECK(serializeJsonStringIfNeeded(it.first) == it.second);
			reset_is(it.second);
			CHECK(deSerializeJsonStringIfNeeded(is) == it.first);
			check_at_eof(is);
		}
	}

	SECTION("all byte values") {
		const std::string bs = "\\"; // MSVC fails when directly using "\\\\"
		const std::string expected = mkstr("\"") +
			"\\u0000\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007" +
			"\\b\\t\\n\\u000b\\f\\r\\u000e\\u000f" +
			"\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017" +
			"\\u0018\\u0019\\u001a\\u001b\\u001c\\u001d\\u001e\\u001f" +
			" !\\\"" + all_bytes.substr(0x23, 0x5c-0x23) +
			bs + bs + all_bytes.substr(0x5d, 0x7f-0x5d) + "\\u007f" +
			all_bytes.substr(0x80, 0xff-0x7f) + "\"";
		std::string serialized = serializeJsonString(all_bytes);
		CHECK(serialized == expected);

		reset_is(serialized);
		CHECK(deSerializeJsonString(is) == all_bytes);
		CHECK(!is.eof()); // should have stopped at " so eof must not be set yet
		check_at_eof(is);
	}

	SECTION("deserialization leaves rest of stream alone") {
		std::string tmp;
		reset_is("\"foo\"bar");
		CHECK(deSerializeJsonString(is) == "foo");
		std::getline(is, tmp, '\0');
		CHECK(tmp == "bar");

		reset_is("\"x y z\"bar");
		CHECK(deSerializeJsonStringIfNeeded(is) == "x y z");
		std::getline(is, tmp, '\0');
		CHECK(tmp == "bar");

		reset_is("foo bar");
		CHECK(deSerializeJsonStringIfNeeded(is) == "foo");
		std::getline(is, tmp, '\0');
		CHECK(tmp == " bar");
	}
}

TEST_CASE("Stream read", "[serialization]") {
	std::string datastr(
		(const char *)test_serialized_data,
		sizeof(test_serialized_data));
	std::istringstream is(datastr, std::ios_base::binary);

	CHECK(readU8(is) == 0x11);
	CHECK(readU16(is) == 0x2233);
	CHECK(readU32(is) == 0x44556677);
	CHECK(readU64(is) == 0x8899AABBCCDDEEFFLL);

	CHECK(readS8(is) == -128);
	CHECK(readS16(is) == 30000);
	CHECK(readS32(is) == -6);
	CHECK(readS64(is) == -43);

	CHECK(readF1000(is) == 53.534f);
	CHECK(readF1000(is) == -300000.32f);
	CHECK(readF1000(is) == F1000_MIN);
	CHECK(readF1000(is) == F1000_MAX);

	CHECK(deSerializeString16(is) == "foobar!");

	CHECK(readV2S16(is) == v2s16(500, 500));
	CHECK(readV3S16(is) == v3s16(4207, 604, -30));
	CHECK(readV2S32(is) == v2s32(1920, 1080));
	CHECK(readV3S32(is) == v3s32(-400, 6400054, 290549855));

	CHECK(readV3F1000(is) == v3f(500, 10024.2f, -192.54f));
	CHECK(readARGB8(is) == video::SColor(255, 128, 50, 128));

	CHECK(deSerializeString32(is) == "some longer string here");

	CHECK(is.rdbuf()->in_avail() == 2);
	CHECK(readU16(is) == 0xF00D);
	CHECK(is.rdbuf()->in_avail() == 0);
}

TEST_CASE("Stream write", "[serialization]") {
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
	REQUIRE(data.size() < sizeof(test_serialized_data));
	CHECK(memcmp(&data[0], test_serialized_data, data.size()) == 0);

	writeV2S16(os, v2s16(500, 500));
	writeV3S16(os, v3s16(4207, 604, -30));
	writeV2S32(os, v2s32(1920, 1080));
	writeV3S32(os, v3s32(-400, 6400054, 290549855));

	writeV3F1000(os, v3f(500, 10024.2f, -192.54f));
	writeARGB8(os, video::SColor(255, 128, 50, 128));

	os << serializeString32("some longer string here");

	writeU16(os, 0xF00D);

	data = os.str();
	REQUIRE(data.size() == sizeof(test_serialized_data));
	CHECK(memcmp(&data[0], test_serialized_data, sizeof(test_serialized_data)) == 0);
}

TEST_CASE("Float format", "[serialization]") {
	SECTION("precision of float calculations on this platform") {
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
			u32 i = f32Tou32Slow(v.first);
			INFO("f32Tou32Slow(" << v.first << ") expected 0x" << std::hex
				<< v.second << ", actual 0x" << i);
			CHECK(std::abs((s64)v.second - i) <= 32);

			f32 fs = u32Tof32Slow(v.second);
			INFO("u32Tof32Slow(0x" << std::hex << v.second << ") expected "
				<< v.first << ", actual " << fs);
			CHECK(std::fabs(v.first - fs) <= std::fabs(v.first * 0.000005f));
		}
	}

	SECTION("exact comparison against IEEE 754 implementation") {
		if (getFloatSerializationType() == FLOATTYPE_SLOW) {
			// conversion using memcpy is not possible
			SKIP("Platform does not use IEEE 754 floats");
		}

		// The code below compares the IEEE conversion functions with a
		// known good IEC559/IEEE754 implementation. This test needs
		// IEC559 compliance in the compiler.
#if defined(__GNUC__) && (!defined(__STDC_IEC_559__) || defined(__FAST_MATH__))
		// GNU C++ lies about its IEC559 support when -ffast-math is active.
		// https://gcc.gnu.org/bugzilla//show_bug.cgi?id=84949
		bool is_iec559 = false;
#else
		bool is_iec559 = std::numeric_limits<f32>::is_iec559;
#endif
		if (!is_iec559)
			SKIP("Compiler does not guarantee IEC559 compliance");

		const auto test_single = [] (u32 i) {
			f32 fm;
			memcpy(&fm, &i, 4);
			f32 fs = u32Tof32Slow(i);
			u32 back = f32Tou32Slow(fs);
			// Fast path: this is called millions of times, so avoid the
			// overhead of the Catch2 assertion macros unless something is off.
			if (fs == fm && back == i)
				return;
			INFO("bit pattern 0x" << std::hex << i);
			REQUIRE(fs == fm);
			REQUIRE(back == i);
		};

		// Use step of prime 277 to speed things up from 3 minutes to a few seconds
		// Test from 0 to 0x7F800000UL (positive)
		for (u32 i = 0x00000000UL; i <= 0x7F800000UL; i += 277)
			test_single(i);

		// Ensure +inf and -inf are tested
		test_single(0x7F800000UL);
		test_single(0xFF800000UL);

		// Test from 0x80000000UL to 0xFF800000UL (negative)
		for (u32 i = 0x80000000UL; i <= 0xFF800000UL; i += 277)
			test_single(i);
	}
}
