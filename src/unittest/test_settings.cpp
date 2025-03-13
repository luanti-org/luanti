// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <cmath>
#include "settings.h"
#include "defaultsettings.h"
#include "noise.h"

class TestSettings : public TestBase {
public:
	TestSettings() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSettings"; }

	void runTests(IGameDef *gamedef);

	void testAllSettings();
	void testDefaults();
	void testFlagDesc();

	static const char *config_text_before;
	static const char *config_text_after;
};

static TestSettings g_test_instance;

void TestSettings::runTests(IGameDef *gamedef)
{
	TEST(testAllSettings);
	TEST(testDefaults);
	TEST(testFlagDesc);
}

////////////////////////////////////////////////////////////////////////////////

const char *TestSettings::config_text_before =
	u8"leet = 1337\n"
	"leetleet = 13371337\n"
	"leetleet_neg = -13371337\n"
	"floaty_thing = 1.1\n"
	"stringy_thing = asd /( ¤%&(/\" BLÖÄRP\n"
	"coord = (1, 2, 4.5)\n"
	"coord_invalid = (1,2,3\n"
	"coord_invalid_2 = 1, 2, 3 test\n"
	"coord_invalid_3 = (test, something, stupid)\n"
	"coord_invalid_4 = (1, test, 3)\n"
	"coord_invalid_5 = ()\n"
	"coord_invalid_6 = (1, 2)\n"
	"coord_invalid_7 = (1)\n"
	"coord_no_parenthesis = 1,2,3\n"
	"      # this is just a comment\n"
	"this is an invalid line\n"
	"asdf = {\n"
	"	a   = 5\n"
	"	bb  = 2.5\n"
	"	ccc = \"\"\"\n"
	"testy\n"
	"   testa   \n"
	"\"\"\"\n"
	"\n"
	"}\n"
	"blarg = \"\"\" \n"
	"some multiline text\n"
	"     with leading whitespace!\n"
	"\"\"\"\n"
	"np_terrain = 5, 40, (250, 250, 250), 12341, 5, 0.700012505, 2.40012503\n"
	"zoop = true\n"
	"[dummy_eof_end_tag]\n";

const char *TestSettings::config_text_after =
	u8"leet = 1337\n"
	"leetleet = 13371337\n"
	"leetleet_neg = -13371337\n"
	"floaty_thing = 1.1\n"
	"stringy_thing = asd /( ¤%&(/\" BLÖÄRP\n"
	"coord = (1, 2, 4.5)\n"
	"      # this is just a comment\n"
	"this is an invalid line\n"
	"asdf = {\n"
	"	a   = 5\n"
	"	bb  = 2.5\n"
	"	ccc = \"\"\"\n"
	"testy\n"
	"   testa   \n"
	"\"\"\"\n"
	"\n"
	"}\n"
	"blarg = \"\"\" \n"
	"some multiline text\n"
	"     with leading whitespace!\n"
	"\"\"\"\n"
	"np_terrain = {\n"
	"	flags = defaults\n"
	"	lacunarity = 2.40012503\n"
	"	octaves = 6\n"
	"	offset = 3.5\n"
	"	persistence = 0.700012505\n"
	"	scale = 40\n"
	"	seed = 12341\n"
	"	spread = (250,250,250)\n"
	"}\n"
	"zoop = true\n"
	"coord2 = (1,2,3.25)\n"
	"coord_invalid = (1,2,3\n"
	"coord_invalid_2 = 1, 2, 3 test\n"
	"coord_invalid_3 = (test, something, stupid)\n"
	"coord_invalid_4 = (1, test, 3)\n"
	"coord_invalid_5 = ()\n"
	"coord_invalid_6 = (1, 2)\n"
	"coord_invalid_7 = (1)\n"
	"coord_no_parenthesis = 1,2,3\n"
	"floaty_thing_2 = 1.25\n"
	"groupy_thing = {\n"
	"	animals = cute\n"
	"	num_apples = 4\n"
	"	num_oranges = 53\n"
	"}\n"
	"[dummy_eof_end_tag]";

void compare_settings(const std::string &name, Settings *a, Settings *b)
{
	auto keys = a->getNames();
	Settings *group1, *group2;
	std::string value1, value2;
	for (auto &key : keys) {
		if (a->getGroupNoEx(key, group1)) {
			UASSERT(b->getGroupNoEx(key, group2));

			compare_settings(name + "->" + key, group1, group2);
			continue;
		}

		UASSERT(b->getNoEx(key, value1));
		// For identification
		value1 = name + "->" + key + "=" + value1;
		value2 = name + "->" + key + "=" + a->get(key);
		UASSERTCMP(std::string, ==, value2, value1);
	}
}

void TestSettings::testAllSettings()
{
	try {
	Settings s("[dummy_eof_end_tag]");

	// Test reading of settings
	std::istringstream is(config_text_before);
	s.parseConfigLines(is);

	UASSERT(s.getS32("leet") == 1337);
	UASSERT(s.getS16("leetleet") == 32767);
	UASSERT(s.getS16("leetleet_neg") == -32768);

	// Not sure if 1.1 is an exact value as a float, but doesn't matter
	UASSERT(fabs(s.getFloat("floaty_thing") - 1.1) < 0.001);
	UASSERT(s.get("stringy_thing") == u8"asd /( ¤%&(/\" BLÖÄRP");
	UASSERT(s.getV3F("coord").value().X == 1.0);
	UASSERT(s.getV3F("coord").value().Y == 2.0);
	UASSERT(s.getV3F("coord").value().Z == 4.5);

	// Test the setting of settings too
	s.setFloat("floaty_thing_2", 1.25);
	s.setV3F("coord2", v3f(1, 2, 3.25));
	UASSERT(s.get("floaty_thing_2").substr(0,4) == "1.25");
	UASSERT(s.getFloat("floaty_thing_2") == 1.25);
	UASSERT(s.getV3F("coord2").value().X == 1.0);
	UASSERT(s.getV3F("coord2").value().Y == 2.0);
	UASSERT(s.getV3F("coord2").value().Z == 3.25);

	std::optional<v3f> testNotExist;
	UASSERT(!s.getV3FNoEx("coord_not_exist", testNotExist));
	EXCEPTION_CHECK(SettingNotFoundException, s.getV3F("coord_not_exist"));

	UASSERT(!s.getV3F("coord_invalid").has_value());
	UASSERT(!s.getV3F("coord_invalid_2").has_value());
	UASSERT(!s.getV3F("coord_invalid_3").has_value());
	UASSERT(!s.getV3F("coord_invalid_4").has_value());
	UASSERT(!s.getV3F("coord_invalid_5").has_value());
	UASSERT(!s.getV3F("coord_invalid_6").has_value());
	UASSERT(!s.getV3F("coord_invalid_7").has_value());

	std::optional<v3f> testNoParenthesis = s.getV3F("coord_no_parenthesis");
	UASSERT(testNoParenthesis.value() == v3f(1, 2, 3));

	// Test settings groups
	Settings *group = s.getGroup("asdf");
	UASSERT(group != NULL);
	UASSERT(s.getGroupNoEx("zoop", group) == false);
	UASSERT(group->getS16("a") == 5);
	UASSERT(fabs(group->getFloat("bb") - 2.5) < 0.001);

	Settings group3;
	group3.set("cat", "meow");
	group3.set("dog", "woof");

	Settings group2;
	group2.setS16("num_apples", 4);
	group2.setS16("num_oranges", 53);
	group2.setGroup("animals", group3);
	group2.set("animals", "cute"); //destroys group 3
	s.setGroup("groupy_thing", group2);

	// Test set failure conditions
	UASSERT(s.set("Zoop = Poop\nsome_other_setting", "false") == false);
	UASSERT(s.set("sneaky", "\"\"\"\njabberwocky = false") == false);
	UASSERT(s.set("hehe", "asdfasdf\n\"\"\"\nsomething = false") == false);

	// Test multiline settings
	UASSERT(group->get("ccc") == "testy\n   testa   ");

	UASSERT(s.get("blarg") ==
		"some multiline text\n"
		"     with leading whitespace!");

	// Test NoiseParams
	UASSERT(s.getEntry("np_terrain").is_group == false);

	NoiseParams np;
	UASSERT(s.getNoiseParams("np_terrain", np) == true);
	UASSERT(std::fabs(np.offset - 5) < 0.001f);
	UASSERT(std::fabs(np.scale - 40) < 0.001f);
	UASSERT(std::fabs(np.spread.X - 250) < 0.001f);
	UASSERT(std::fabs(np.spread.Y - 250) < 0.001f);
	UASSERT(std::fabs(np.spread.Z - 250) < 0.001f);
	UASSERT(np.seed == 12341);
	UASSERT(np.octaves == 5);
	UASSERT(std::fabs(np.persist - 0.7) < 0.001f);

	np.offset  = 3.5;
	np.octaves = 6;
	s.setNoiseParams("np_terrain", np);

	UASSERT(s.getEntry("np_terrain").is_group == true);

	// Test writing
	std::ostringstream os(std::ios_base::binary);
	is.clear();
	is.seekg(0);

	UASSERT(s.updateConfigObject(is, os, 0) == true);

	{
		// Confirm settings
		Settings s2("[dummy_eof_end_tag]");
		std::istringstream is(config_text_after, std::ios_base::binary);
		UASSERT(s2.parseConfigLines(is) == true);

		compare_settings("(main)", &s, &s2);
	}

	} catch (SettingNotFoundException &e) {
		UASSERT(!"Setting not found!");
	}
}

void TestSettings::testDefaults()
{
	Settings *game = Settings::createLayer(SL_GAME);
	Settings *def = Settings::getLayer(SL_DEFAULTS);

	def->set("name", "FooBar");
	UASSERT(def->get("name") == "FooBar");
	UASSERT(game->get("name") == "FooBar");

	game->set("name", "Baz");
	UASSERT(game->get("name") == "Baz");

	delete game;

	// Restore default settings
	delete Settings::getLayer(SL_DEFAULTS);
	set_default_settings();
}

void TestSettings::testFlagDesc()
{
	Settings &s = *Settings::createLayer(SL_GAME);
	FlagDesc flagdesc[] = {
		{ "biomes",  0x01 },
		{ "trees",   0x02 },
		{ "jungles", 0x04 },
		{ "oranges", 0x08 },
		{ "tables",  0x10 },
		{ nullptr,      0 }
	};

	// Enabled: biomes, jungles, oranges (default)
	s.setDefault("test_desc", flagdesc, readFlagString(
		"biomes,notrees,jungles,oranges", flagdesc, nullptr));
	UASSERT(s.getFlagStr("test_desc", flagdesc, nullptr) == (0x01 | 0x04 | 0x08));

	// Enabled: jungles, oranges, tables
	s.set("test_desc", "nobiomes,tables");
	UASSERT(s.getFlagStr("test_desc", flagdesc, nullptr) == (0x04 | 0x08 | 0x10));

	// Enabled: (nothing)
	s.set("test_desc", "nobiomes,nojungles,nooranges,notables");
	UASSERT(s.getFlagStr("test_desc", flagdesc, nullptr) == 0x00);

	// Numeric flag tests (override)
	// Enabled: trees, tables
	s.setDefault("test_flags", flagdesc, 0x02 | 0x10);
	UASSERT(s.getFlagStr("test_flags", flagdesc, nullptr) == (0x02 | 0x10));

	// Enabled: tables
	s.set("test_flags", "16");
	UASSERT(s.getFlagStr("test_flags", flagdesc, nullptr) == 0x10);

	delete &s;
}
