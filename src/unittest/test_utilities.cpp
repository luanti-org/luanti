// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <cmath>
#include <limits>
#include "util/enriched_string.h"
#include "util/numeric.h"
#include "util/string.h"
#include "util/base64.h"
#include "util/colorize.h"

class TestUtilities : public TestBase {
public:
	TestUtilities() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestUtilities"; }

	void runTests(IGameDef *gamedef);

	void testAngleWrapAround();
	void testWrapDegrees_0_360_v3f();
	void testLowercase();
	void testTrim();
	void testIsYes();
	void testRemoveStringEnd();
	void testUrlEncode();
	void testUrlDecode();
	void testPadString();
	void testStartsWith();
	void testStrEqual();
	void testStrToIntConversion();
	void testStringReplace();
	void testStringAllowed();
	void testAsciiPrintableHelper();
	void testUTF8();
	void testRemoveEscapes();
	void testWrapRows();
	void testEnrichedString();
	void testIsNumber();
	void testIsPowerOfTwo();
	void testMyround();
	void testStringJoin();
	void testEulerConversion();
	void testBase64();
	void testSanitizeDirName();
	void testIsBlockInSight();
	void testColorizeURL();
	void testSanitizeUntrusted();
	void testReadSeed();
	void testMyDoubleStringConversions();
};

static TestUtilities g_test_instance;

void TestUtilities::runTests(IGameDef *gamedef)
{
	TEST(testAngleWrapAround);
	TEST(testWrapDegrees_0_360_v3f);
	TEST(testLowercase);
	TEST(testTrim);
	TEST(testIsYes);
	TEST(testRemoveStringEnd);
	TEST(testUrlEncode);
	TEST(testUrlDecode);
	TEST(testPadString);
	TEST(testStartsWith);
	TEST(testStrEqual);
	TEST(testStrToIntConversion);
	TEST(testStringReplace);
	TEST(testStringAllowed);
	TEST(testAsciiPrintableHelper);
	TEST(testUTF8);
	TEST(testRemoveEscapes);
	TEST(testWrapRows);
	TEST(testEnrichedString);
	TEST(testIsNumber);
	TEST(testIsPowerOfTwo);
	TEST(testMyround);
	TEST(testStringJoin);
	TEST(testEulerConversion);
	TEST(testBase64);
	TEST(testSanitizeDirName);
	TEST(testIsBlockInSight);
	TEST(testColorizeURL);
	TEST(testSanitizeUntrusted);
	TEST(testReadSeed);
	TEST(testMyDoubleStringConversions);
}

////////////////////////////////////////////////////////////////////////////////

inline float ref_WrapDegrees180(float f)
{
	// This is a slower alternative to the wrapDegrees_180() function;
	// used as a reference for testing
	float value = fmodf(f + 180, 360);
	if (value < 0)
		value += 360;
	return value - 180;
}


inline float ref_WrapDegrees_0_360(float f)
{
	// This is a slower alternative to the wrapDegrees_0_360() function;
	// used as a reference for testing
	float value = fmodf(f, 360);
	if (value < 0)
		value += 360;
	return value < 0 ? value + 360 : value;
}


void TestUtilities::testAngleWrapAround() {
	UASSERT(fabs(modulo360f(100.0) - 100.0) < 0.001);
	UASSERT(fabs(modulo360f(720.5) - 0.5) < 0.001);
	UASSERT(fabs(modulo360f(-0.5) - (-0.5)) < 0.001);
	UASSERT(fabs(modulo360f(-365.5) - (-5.5)) < 0.001);

	for (float f = -720; f <= -360; f += 0.25) {
		UASSERT(std::fabs(modulo360f(f) - modulo360f(f + 360)) < 0.001);
	}

	for (float f = -1440; f <= 1440; f += 0.25) {
		UASSERT(std::fabs(modulo360f(f) - fmodf(f, 360)) < 0.001);
		UASSERT(std::fabs(wrapDegrees_180(f) - ref_WrapDegrees180(f)) < 0.001);
		UASSERT(std::fabs(wrapDegrees_0_360(f) - ref_WrapDegrees_0_360(f)) < 0.001);
		UASSERT(wrapDegrees_0_360(
				std::fabs(wrapDegrees_180(f) - wrapDegrees_0_360(f))) < 0.001);
	}

}

void TestUtilities::testWrapDegrees_0_360_v3f()
{
	// only x test with little step
	for (float x = -720.f; x <= 720; x += 0.05) {
		v3f r = wrapDegrees_0_360_v3f(v3f(x, 0, 0));
		UASSERT(r.X >= 0.0f && r.X < 360.0f)
		UASSERT(r.Y == 0.0f)
		UASSERT(r.Z == 0.0f)
	}

	// only y test with little step
	for (float y = -720.f; y <= 720; y += 0.05) {
		v3f r = wrapDegrees_0_360_v3f(v3f(0, y, 0));
		UASSERT(r.X == 0.0f)
		UASSERT(r.Y >= 0.0f && r.Y < 360.0f)
		UASSERT(r.Z == 0.0f)
	}

	// only z test with little step
	for (float z = -720.f; z <= 720; z += 0.05) {
		v3f r = wrapDegrees_0_360_v3f(v3f(0, 0, z));
		UASSERT(r.X == 0.0f)
		UASSERT(r.Y == 0.0f)
		UASSERT(r.Z >= 0.0f && r.Z < 360.0f)
	}

	// test the whole coordinate translation
	for (float x = -720.f; x <= 720; x += 2.5) {
		for (float y = -720.f; y <= 720; y += 2.5) {
			for (float z = -720.f; z <= 720; z += 2.5) {
				v3f r = wrapDegrees_0_360_v3f(v3f(x, y, z));
				UASSERT(r.X >= 0.0f && r.X < 360.0f)
				UASSERT(r.Y >= 0.0f && r.Y < 360.0f)
				UASSERT(r.Z >= 0.0f && r.Z < 360.0f)
			}
		}
	}
}


void TestUtilities::testLowercase()
{
	UASSERTEQ(auto, lowercase("Foo bAR"), "foo bar");
	UASSERTEQ(auto, lowercase(u8"eeeeeeaaaaaaaaaaaààààà"), u8"eeeeeeaaaaaaaaaaaààààà");
	// intentionally won't handle Unicode, regardless of locale
	UASSERTEQ(auto, lowercase(u8"ÜÜ"), u8"ÜÜ");
	UASSERTEQ(auto, lowercase("MINETEST-powa"), "minetest-powa");
}


void TestUtilities::testTrim()
{
	UASSERT(trim("") == "");
	UASSERT(trim("dirt_with_grass") == "dirt_with_grass");
	UASSERT(trim("\n \t\r  Foo bAR  \r\n\t\t  ") == "Foo bAR");
	UASSERT(trim("\n \t\r    \r\n\t\t  ") == "");
	UASSERT(trim("  a") == "a");
	UASSERT(trim("a   ") == "a");
}


void TestUtilities::testIsYes()
{
	UASSERT(is_yes("YeS") == true);
	UASSERT(is_yes("") == false);
	UASSERT(is_yes("FAlse") == false);
	UASSERT(is_yes("-1") == true);
	UASSERT(is_yes("0") == false);
	UASSERT(is_yes("1") == true);
	UASSERT(is_yes("2") == true);
}


void TestUtilities::testRemoveStringEnd()
{
	const char *ends[] = {"abc", "c", "bc", "", NULL};
	UASSERT(removeStringEnd("abc", ends) == "");
	UASSERT(removeStringEnd("bc", ends) == "b");
	UASSERT(removeStringEnd("12c", ends) == "12");
	UASSERT(removeStringEnd("foo", ends) == "");
}


void TestUtilities::testUrlEncode()
{
	UASSERT(urlencode("\"Aardvarks lurk, OK?\"")
			== "%22Aardvarks%20lurk%2C%20OK%3F%22");
}


void TestUtilities::testUrlDecode()
{
	UASSERT(urldecode("%22Aardvarks%20lurk%2C%20OK%3F%22")
			== "\"Aardvarks lurk, OK?\"");
}


void TestUtilities::testPadString()
{
	UASSERT(padStringRight("hello", 8) == "hello   ");
}

void TestUtilities::testStartsWith()
{
	std::string the("the");
	UASSERT(str_starts_with(std::string(), "") == true);
	UASSERT(str_starts_with(std::string("the sharp pickaxe"),
		std::string()) == true);
	UASSERT(str_starts_with(std::string("the sharp pickaxe"),
		std::string_view(the)) == true);
	UASSERT(str_starts_with(std::string("the sharp pickaxe"),
		std::string("The")) == false);
	UASSERT(str_starts_with(std::string("the sharp pickaxe"),
		std::string("The"), true) == true);
	UASSERT(str_starts_with(std::string("T"), "The") == false);
}

void TestUtilities::testStrEqual()
{
	std::string foo("foo");
	UASSERT(str_equal(foo, std::string_view(foo)));
	UASSERT(!str_equal(foo, std::string("bar")));
	UASSERT(str_equal(std::string_view(foo), std::string_view(foo)));
	UASSERT(str_equal(std::wstring(L"FOO"), std::wstring(L"foo"), true));
	UASSERT(str_equal(utf8_to_wide("abc"), utf8_to_wide("abc")));
	UASSERT(str_equal(utf8_to_wide("ABC"), utf8_to_wide("abc"), true));
}


void TestUtilities::testStrToIntConversion()
{
	UASSERT(mystoi("123", 0, 1000) == 123);
	UASSERT(mystoi("123", 0, 10) == 10);
}


void TestUtilities::testStringReplace()
{
	std::string test_str;
	test_str = "Hello there";
	str_replace(test_str, "there", "world");
	UASSERT(test_str == "Hello world");
	test_str = "ThisAisAaAtest";
	str_replace(test_str, 'A', ' ');
	UASSERT(test_str == "This is a test");
}


void TestUtilities::testStringAllowed()
{
	UASSERT(string_allowed("hello", "abcdefghijklmno") == true);
	UASSERT(string_allowed("123", "abcdefghijklmno") == false);
	UASSERT(string_allowed_blacklist("hello", "123") == true);
	UASSERT(string_allowed_blacklist("hello123", "123") == false);
}

void TestUtilities::testAsciiPrintableHelper()
{
	UASSERT(IS_ASCII_PRINTABLE_CHAR('e') == true);
	UASSERT(IS_ASCII_PRINTABLE_CHAR('\0') == false);

	// Ensures that there is no cutting off going on...
	// If there were, 331 would be cut to 75 in this example
	// and 73 is a valid ASCII char.
	int ch = 331;
	UASSERT(IS_ASCII_PRINTABLE_CHAR(ch) == false);
}

void TestUtilities::testUTF8()
{
	UASSERT(utf8_to_wide(u8"¤") == L"¤");

	UASSERTEQ(std::string, wide_to_utf8(L"¤"), u8"¤");

	UASSERTEQ(std::string, wide_to_utf8(utf8_to_wide("")), "");
	UASSERTEQ(std::string, wide_to_utf8(utf8_to_wide("the shovel dug a crumbly node!")),
		"the shovel dug a crumbly node!");

	UASSERTEQ(std::string, wide_to_utf8(utf8_to_wide(u8"-ä-")),
		u8"-ä-");
	UASSERTEQ(std::string, wide_to_utf8(utf8_to_wide(u8"-\U0002000b-")),
		u8"-\U0002000b-");
	if constexpr (sizeof(wchar_t) == 4) {
		const auto *literal = U"-\U0002000b-";
		UASSERT(utf8_to_wide(u8"-\U0002000b-") == reinterpret_cast<const wchar_t*>(literal));
	}

	// try to check that the conversion function does not accidentally keep
	// its internal state across invocations.
	// \xC4\x81 is UTF-8 for \u0101
	static_cast<void>(utf8_to_wide("\xC4"));
	UASSERT(utf8_to_wide("\x81") != L"\u0101");
}

void TestUtilities::testRemoveEscapes()
{
	UASSERT(unescape_enriched<wchar_t>(
		L"abc\x1bXdef") == L"abcdef");
	UASSERT(unescape_enriched<wchar_t>(
		L"abc\x1b(escaped)def") == L"abcdef");
	UASSERT(unescape_enriched<wchar_t>(
		L"abc\x1b((escaped with parenthesis\\))def") == L"abcdef");
	UASSERTEQ(auto, unescape_enriched("abc\x1b(not this\\\\)def"),
		"abcdef");
	UASSERT(unescape_enriched<wchar_t>(
		L"abc\x1b(incomplete") == L"abc");
	UASSERT(unescape_enriched<wchar_t>(
		L"escape at the end\x1b") == L"escape at the end");
	// Nested escapes not supported
	UASSERT(unescape_enriched<wchar_t>(
		L"abc\x1b(outer \x1b(inner escape)escape)def") == L"abcescape)def");
	// Multiple
	UASSERTEQ(auto, unescape_enriched("one\x1bX two \x1b(four)three"),
		"one two three");
}

void TestUtilities::testWrapRows()
{
	UASSERT(wrap_rows("12345678",4) == "1234\n5678");
	// test that wrap_rows doesn't wrap inside multibyte sequences
	{
		const unsigned char s[] = {
			0x2f, 0x68, 0x6f, 0x6d, 0x65, 0x2f, 0x72, 0x61, 0x70, 0x74, 0x6f,
			0x72, 0x2f, 0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82, 0x2f,
			0x6d, 0x69, 0x6e, 0x65, 0x74, 0x65, 0x73, 0x74, 0x2f, 0x62, 0x69,
			0x6e, 0x2f, 0x2e, 0x2e, 0};
		std::string str((char *)s);
		UASSERT(utf8_to_wide(wrap_rows(str, 20)) != L"<invalid UTF-8 string>");
	};
	{
		const unsigned char s[] = {
			0x74, 0x65, 0x73, 0x74, 0x20, 0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81,
			0xd1, 0x82, 0x20, 0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82,
			0x20, 0xd1, 0x82, 0xd0, 0xb5, 0xd1, 0x81, 0xd1, 0x82, 0};
		std::string str((char *)s);
		UASSERT(utf8_to_wide(wrap_rows(str, 8)) != L"<invalid UTF-8 string>");
	}
}

void TestUtilities::testEnrichedString()
{
	EnrichedString str(L"Test bar");
	video::SColor color(0xFF, 0, 0, 0xFF);

	UASSERT(str.substr(1, 3).getString() == L"est");
	str += L" BUZZ";
	UASSERT(str.substr(9, std::string::npos).getString() == L"BUZZ");
	str.setDefaultColor(color); // Blue foreground
	UASSERT(str.getColors()[5] == color);
	// Green background, then white and yellow text
	str = L"\x1b(b@#0F0)Regular \x1b(c@#FF0)yellow";
	UASSERT(str.getColors()[2] == 0xFFFFFFFF);
	str.setDefaultColor(color); // Blue foreground
	UASSERT(str.getColors()[13] == 0xFFFFFF00); // Still yellow text
	UASSERT(str.getBackground() == 0xFF00FF00); // Green background
}

void TestUtilities::testIsNumber()
{
	UASSERT(is_number("123") == true);
	UASSERT(is_number("") == false);
	UASSERT(is_number("123a") == false);
}


void TestUtilities::testIsPowerOfTwo()
{
	UASSERT(is_power_of_two(0) == false);
	UASSERT(is_power_of_two(1) == true);
	UASSERT(is_power_of_two(2) == true);
	UASSERT(is_power_of_two(3) == false);
	for (int exponent = 2; exponent <= 31; ++exponent) {
		UASSERT(is_power_of_two((1U << exponent) - 1) == false);
		UASSERT(is_power_of_two((1U << exponent)) == true);
		UASSERT(is_power_of_two((1U << exponent) + 1) == false);
	}
	UASSERT(is_power_of_two(U32_MAX) == false);
}

void TestUtilities::testMyround()
{
	UASSERT(myround(4.6f) == 5);
	UASSERT(myround(1.2f) == 1);
	UASSERT(myround(-3.1f) == -3);
	UASSERT(myround(-6.5f) == -7);
}

void TestUtilities::testStringJoin()
{
	std::vector<std::string> input;
	UASSERT(str_join(input, ",") == "");

	input.emplace_back("one");
	UASSERT(str_join(input, ",") == "one");

	input.emplace_back("two");
	UASSERT(str_join(input, ",") == "one,two");

	input.emplace_back("three");
	UASSERT(str_join(input, ",") == "one,two,three");

	input[1] = "";
	UASSERT(str_join(input, ",") == "one,,three");

	input[1] = "two";
	UASSERT(str_join(input, " and ") == "one and two and three");
}


static bool within(const f32 value1, const f32 value2, const f32 precision)
{
	return std::fabs(value1 - value2) <= precision;
}

static bool within(const v3f &v1, const v3f &v2, const f32 precision)
{
	return within(v1.X, v2.X, precision) && within(v1.Y, v2.Y, precision)
		&& within(v1.Z, v2.Z, precision);
}

static bool within(const core::matrix4 &m1, const core::matrix4 &m2,
		const f32 precision)
{
	const f32 *M1 = m1.pointer();
	const f32 *M2 = m2.pointer();
	for (int i = 0; i < 16; i++)
		if (! within(M1[i], M2[i], precision))
			return false;
	return true;
}

static bool roundTripsDeg(const v3f &v, const f32 precision)
{
	core::matrix4 m;
	setPitchYawRoll(m, v);
	return within(v, getPitchYawRoll(m), precision);
}

void TestUtilities::testEulerConversion()
{
	// This test may fail on non-IEEE systems.
	// Low tolerance is 4 ulp(1.0) for binary floats with 24 bit mantissa.
	// (ulp = unit in the last place; ulp(1.0) = 2^-23).
	const f32 tolL = 4.76837158203125e-7f;
	// High tolerance is 2 ulp(180.0), needed for numbers in degrees.
	// ulp(180.0) = 2^-16
	const f32 tolH = 3.0517578125e-5f;
	v3f v1, v2;
	core::matrix4 m1, m2;
	const f32 *M1 = m1.pointer();
	const f32 *M2 = m2.pointer();

	// Check that the radians version and the degrees version
	// produce the same results. Check also that the conversion
	// works both ways for these values.
	v1 = v3f(M_PI/3.0, M_PI/5.0, M_PI/4.0);
	v2 = v3f(60.0f, 36.0f, 45.0f);
	setPitchYawRollRad(m1, v1);
	setPitchYawRoll(m2, v2);
	UASSERT(within(m1, m2, tolL));
	UASSERT(within(getPitchYawRollRad(m1), v1, tolL));
	UASSERT(within(getPitchYawRoll(m2), v2, tolH));

	// Check the rotation matrix produced.
	UASSERT(within(M1[0], 0.932004869f, tolL));
	UASSERT(within(M1[1], 0.353553385f, tolL));
	UASSERT(within(M1[2], 0.0797927827f, tolL));
	UASSERT(within(M1[4], -0.21211791f, tolL));
	UASSERT(within(M1[5], 0.353553355f, tolL));
	UASSERT(within(M1[6], 0.911046684f, tolL));
	UASSERT(within(M1[8], 0.293892622f, tolL));
	UASSERT(within(M1[9], -0.866025448f, tolL));
	UASSERT(within(M1[10], 0.404508471f, tolL));

	// Check that the matrix is still homogeneous with no translation
	UASSERT(M1[3] == 0.0f);
	UASSERT(M1[7] == 0.0f);
	UASSERT(M1[11] == 0.0f);
	UASSERT(M1[12] == 0.0f);
	UASSERT(M1[13] == 0.0f);
	UASSERT(M1[14] == 0.0f);
	UASSERT(M1[15] == 1.0f);
	UASSERT(M2[3] == 0.0f);
	UASSERT(M2[7] == 0.0f);
	UASSERT(M2[11] == 0.0f);
	UASSERT(M2[12] == 0.0f);
	UASSERT(M2[13] == 0.0f);
	UASSERT(M2[14] == 0.0f);
	UASSERT(M2[15] == 1.0f);

	// Compare to Irrlicht's results. To be comparable, the
	// angles must come in a different order and the matrix
	// elements to compare are different too.
	m2.setRotationRadians(v3f(v1.Z, v1.X, v1.Y));
	UASSERT(within(M1[0], M2[5], tolL));
	UASSERT(within(M1[1], M2[6], tolL));
	UASSERT(within(M1[2], M2[4], tolL));

	UASSERT(within(M1[4], M2[9], tolL));
	UASSERT(within(M1[5], M2[10], tolL));
	UASSERT(within(M1[6], M2[8], tolL));

	UASSERT(within(M1[8], M2[1], tolL));
	UASSERT(within(M1[9], M2[2], tolL));
	UASSERT(within(M1[10], M2[0], tolL));

	// Check that Eulers that produce near gimbal-lock still round-trip
	UASSERT(roundTripsDeg(v3f(89.9999f, 17.f, 0.f), tolH));
	UASSERT(roundTripsDeg(v3f(89.9999f, 0.f, 19.f), tolH));
	UASSERT(roundTripsDeg(v3f(89.9999f, 17.f, 19.f), tolH));

	// Check that Eulers at an angle > 90 degrees may not round-trip...
	v1 = v3f(90.00001f, 1.f, 1.f);
	setPitchYawRoll(m1, v1);
	v2 = getPitchYawRoll(m1);
	//UASSERT(within(v1, v2, tolL)); // this is typically false
	// ... however the rotation matrix is the same for both
	setPitchYawRoll(m2, v2);
	UASSERT(within(m1, m2, tolL));
}

void TestUtilities::testBase64()
{
	// Test character set
	UASSERT(base64_is_valid("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/") == true);
	UASSERT(base64_is_valid("/+9876543210"
		"zyxwvutsrqponmlkjihgfedcba"
		"ZYXWVUTSRQPONMLKJIHGFEDCBA") == true);
	UASSERT(base64_is_valid("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+.") == false);
	UASSERT(base64_is_valid("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789 /") == false);

	// Test empty string
	UASSERT(base64_is_valid("") == true);

	// Test different lengths, with and without padding,
	// with correct and incorrect padding
	UASSERT(base64_is_valid("A") == false);
	UASSERT(base64_is_valid("AA") == true);
	UASSERT(base64_is_valid("AAA") == true);
	UASSERT(base64_is_valid("AAAA") == true);
	UASSERT(base64_is_valid("AAAAA") == false);
	UASSERT(base64_is_valid("AAAAAA") == true);
	UASSERT(base64_is_valid("AAAAAAA") == true);
	UASSERT(base64_is_valid("AAAAAAAA") == true);
	UASSERT(base64_is_valid("A===") == false);
	UASSERT(base64_is_valid("AA==") == true);
	UASSERT(base64_is_valid("AAA=") == true);
	UASSERT(base64_is_valid("AAAA") == true);
	UASSERT(base64_is_valid("AAAA====") == false);
	UASSERT(base64_is_valid("AAAAA===") == false);
	UASSERT(base64_is_valid("AAAAAA==") == true);
	UASSERT(base64_is_valid("AAAAAAA=") == true);
	UASSERT(base64_is_valid("AAAAAAA==") == false);
	UASSERT(base64_is_valid("AAAAAAA===") == false);
	UASSERT(base64_is_valid("AAAAAAA====") == false);
	UASSERT(base64_is_valid("AAAAAAAA") == true);
	UASSERT(base64_is_valid("AAAAAAAA=") == false);
	UASSERT(base64_is_valid("AAAAAAAA==") == false);
	UASSERT(base64_is_valid("AAAAAAAA===") == false);
	UASSERT(base64_is_valid("AAAAAAAA====") == false);

	// Test if canonical encoding
	// Last character limitations, length % 4 == 3
	UASSERT(base64_is_valid("AAB") == false);
	UASSERT(base64_is_valid("AAE") == true);
	UASSERT(base64_is_valid("AAQ") == true);
	UASSERT(base64_is_valid("AAB=") == false);
	UASSERT(base64_is_valid("AAE=") == true);
	UASSERT(base64_is_valid("AAQ=") == true);
	UASSERT(base64_is_valid("AAAAAAB=") == false);
	UASSERT(base64_is_valid("AAAAAAE=") == true);
	UASSERT(base64_is_valid("AAAAAAQ=") == true);
	// Last character limitations, length % 4 == 2
	UASSERT(base64_is_valid("AB") == false);
	UASSERT(base64_is_valid("AE") == false);
	UASSERT(base64_is_valid("AQ") == true);
	UASSERT(base64_is_valid("AB==") == false);
	UASSERT(base64_is_valid("AE==") == false);
	UASSERT(base64_is_valid("AQ==") == true);
	UASSERT(base64_is_valid("AAAAAB==") == false);
	UASSERT(base64_is_valid("AAAAAE==") == false);
	UASSERT(base64_is_valid("AAAAAQ==") == true);

	// Extraneous character present
	UASSERT(base64_is_valid(".") == false);
	UASSERT(base64_is_valid("A.") == false);
	UASSERT(base64_is_valid("AA.") == false);
	UASSERT(base64_is_valid("AAA.") == false);
	UASSERT(base64_is_valid("AAAA.") == false);
	UASSERT(base64_is_valid("AAAAA.") == false);
	UASSERT(base64_is_valid("A.A") == false);
	UASSERT(base64_is_valid("AA.A") == false);
	UASSERT(base64_is_valid("AAA.A") == false);
	UASSERT(base64_is_valid("AAAA.A") == false);
	UASSERT(base64_is_valid("AAAAA.A") == false);
	UASSERT(base64_is_valid("\xE1""AAA") == false);

	// Padding in wrong position
	UASSERT(base64_is_valid("A=A") == false);
	UASSERT(base64_is_valid("AA=A") == false);
	UASSERT(base64_is_valid("AAA=A") == false);
	UASSERT(base64_is_valid("AAAA=A") == false);
	UASSERT(base64_is_valid("AAAAA=A") == false);
}


void TestUtilities::testSanitizeDirName()
{
	UASSERTEQ(auto, sanitizeDirName("a", "~"), "a");
	UASSERTEQ(auto, sanitizeDirName("  ", "~"), "__");
	UASSERTEQ(auto, sanitizeDirName(" a ", "~"), "_a_");
	UASSERTEQ(auto, sanitizeDirName("COM1", "~"), "~COM1");
	UASSERTEQ(auto, sanitizeDirName("COM1", ":"), "_COM1");
	UASSERTEQ(auto, sanitizeDirName(u8"cOm\u00B2", "~"), u8"~cOm\u00B2");
	UASSERTEQ(auto, sanitizeDirName("cOnIn$", "~"), "~cOnIn$");
	UASSERTEQ(auto, sanitizeDirName(" cOnIn$ ", "~"), "_cOnIn$_");
}

template <typename F, typename C>
C apply_all(const C &co, F functor)
{
	C ret;
	for (auto it = co.begin(); it != co.end(); it++)
		ret.push_back(functor(*it));
	return ret;
}

void TestUtilities::testIsBlockInSight()
{
	const std::vector<v3s16> testdata1 = {
		{0, 1 * (int)BS, 0}, // camera_pos
		{1, 0, 0},           // camera_dir

		{ 2, 0, 0},
		{-2, 0, 0},
		{0, 0,  3},
		{0, 0, -3},
		{0, 0, 0},
		{6, 0, 0}
	};
	auto test1 = [] (const std::vector<v3s16> &data) {
		float range = BS * MAP_BLOCKSIZE * 4;
		float fov = 72 * core::DEGTORAD;
		v3f cam_pos = v3f::from(data[0]), cam_dir = v3f::from(data[1]);
		UASSERT( isBlockInSight(data[2], cam_pos, cam_dir, fov, range));
		UASSERT(!isBlockInSight(data[3], cam_pos, cam_dir, fov, range));
		UASSERT(!isBlockInSight(data[4], cam_pos, cam_dir, fov, range));
		UASSERT(!isBlockInSight(data[5], cam_pos, cam_dir, fov, range));

		// camera block must be visible
		UASSERT(isBlockInSight(data[6], cam_pos, cam_dir, fov, range));

		// out of range is never visible
		UASSERT(!isBlockInSight(data[7], cam_pos, cam_dir, fov, range));
	};
	// XZ rotations
	for (int j = 0; j < 4; j++) {
		auto tmpdata = apply_all(testdata1, [&] (v3s16 v) -> v3s16 {
			v.rotateXZBy(j*90);
			return v;
		});
		test1(tmpdata);
	}
	// just two for XY
	for (int j = 0; j < 2; j++) {
		auto tmpdata = apply_all(testdata1, [&] (v3s16 v) -> v3s16 {
			v.rotateXYBy(90+j*180);
			return v;
		});
		test1(tmpdata);
	}

	{
		float range = BS * MAP_BLOCKSIZE * 2;
		float fov = 72 * core::DEGTORAD;
		v3f cam_pos(-(MAP_BLOCKSIZE - 1) * BS, 0, 0), cam_dir(1, 0, 0);
		// we're looking at X+ but are so close to block (-1,0,0) that it
		// should still be considered visible
		UASSERT(isBlockInSight({-1, 0, 0}, cam_pos, cam_dir, fov, range));
	}
}

void TestUtilities::testColorizeURL()
{
#ifdef HAVE_COLORIZE_URL
	#define RED COLOR_CODE("#faa")
	#define GREY COLOR_CODE("#aaa")
	#define WHITE COLOR_CODE("#fff")

	std::string result = colorize_url("http://example.com/");
	UASSERTEQ(auto, result, (GREY "http://" WHITE "example.com" GREY "/"));

	result = colorize_url(u8"https://u:p@wikipedi\u0430.org:1234/heIIoll?a=b#c");
	UASSERTEQ(auto, result,
		(GREY "https://u:p@" WHITE "wikipedi" RED "%d0%b0" WHITE ".org" GREY ":1234/heIIoll?a=b#c"));
#else
	warningstream << "Test skipped." << std::endl;
#endif
}

void TestUtilities::testSanitizeUntrusted()
{
	std::string_view t1{u8"Anästhesieausrüstung"};
	UASSERTEQ(auto, sanitize_untrusted(t1), t1);

	std::string_view t2{"stop\x00here", 9};
	UASSERTEQ(auto, sanitize_untrusted(t2), "stop");

	UASSERTEQ(auto, sanitize_untrusted("\x01\x08\x13\x1dhello\r\n\tworld"), "hello\n\tworld");

	std::string_view t3{"some \x1b(T@whatever)text\x1b" "E here"};
	UASSERTEQ(auto, sanitize_untrusted(t3), t3);
	auto t3_sanitized = sanitize_untrusted(t3, false);
	UASSERT(str_starts_with(t3_sanitized, "some ") && str_ends_with(t3_sanitized, " here"));
	UASSERT(t3_sanitized.find('\x1b') == std::string::npos);

	UASSERTEQ(auto, sanitize_untrusted("\x1b[31m"), "[31m");

	// edge cases
	for (bool keep : {true, false}) {
		UASSERTEQ(auto, sanitize_untrusted("\x1b", keep), "");
		UASSERTEQ(auto, sanitize_untrusted("\x1b(", keep), "(");
	}
}

void TestUtilities::testReadSeed()
{
	UASSERTEQ(int, read_seed("123"), 123);
	UASSERTEQ(int, read_seed("0x123"), 0x123);
	// hashing should produce some non-zero number
	UASSERT(read_seed("hello") != 0);
}

void TestUtilities::testMyDoubleStringConversions()
{
	const auto expect_parse_failure = [](const std::string &s) {
		UASSERT(!my_string_to_double(s).has_value());
	};
	expect_parse_failure("");
	expect_parse_failure("helloworld");
	expect_parse_failure("42x");

	const auto expect_double = [](const std::string &s, double expected) {
		auto got = my_string_to_double(s);
		UASSERT(got.has_value());
		UASSERTEQ(double, *got, expected);
	};
	expect_double("1", 1.0);
	expect_double("42", 42.0);
	expect_double("42.25", 42.25);
	expect_double("3e3", 3000.0);
	expect_double("0xff", 255.0);
	expect_double("0x1.0p+1", 2.0);

	UASSERT(std::isnan(my_string_to_double(my_double_to_string(
			std::numeric_limits<double>::quiet_NaN())).value()));
	const auto test_round_trip = [](double number) {
		auto got = my_string_to_double(my_double_to_string(number));
		UASSERT(got.has_value());
		UASSERTEQ(double, *got, number);
	};
	test_round_trip(std::numeric_limits<double>::infinity());
	test_round_trip(-std::numeric_limits<double>::infinity());
	test_round_trip(0.3);
	test_round_trip(0.1 + 0.2);
}
