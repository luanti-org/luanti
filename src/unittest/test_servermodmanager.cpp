// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "test.h"
#include <algorithm>
#include "server/mods.h"
#include "settings.h"
#include "util/string.h"

#define SUBGAME_ID "devtest"

class TestServerModManager : public TestBase
{
public:
	TestServerModManager() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestServerModManager"; }

	void runTests(IGameDef *gamedef);

	std::string m_worlddir;

	static ServerModManager makeManager(const std::string &worldpath) {
		return ServerModManager(worldpath, findWorldSubgame(worldpath));
	}

	void testCreation();
	void testIsConsistent();
	void testUnsatisfiedMods();
	void testGetMods();
	void testGetModsWrongDir();
	void testGetModspec();
	void testGetModNamesWrongDir();
	void testGetModNames();
	void testGetModMediaPathsWrongDir();
	void testGetModMediaPaths();
};

static TestServerModManager g_test_instance;

void TestServerModManager::runTests(IGameDef *gamedef)
{
	if (!findSubgame(SUBGAME_ID).isValid()) {
		warningstream << "Can't find game " SUBGAME_ID ", skipping this module." << std::endl;
		return;
	}

	auto test_mods = getTestTempDirectory().append(DIR_DELIM "test_mods");
	{
		auto p = test_mods + (DIR_DELIM "test_mod" DIR_DELIM);
		fs::CreateAllDirs(p);
		std::ofstream ofs1(p + "mod.conf", std::ios::out | std::ios::binary);
		ofs1 << "name = test_mod\n"
			<< "description = Does nothing\n";
		std::ofstream ofs2(p + "init.lua", std::ios::out | std::ios::binary);
		ofs2 << "-- intentionally empty\n";
	}

	setenv("MINETEST_MOD_PATH", test_mods.c_str(), 1);

	m_worlddir = getTestTempDirectory().append(DIR_DELIM "world");
	fs::CreateDir(m_worlddir);

	TEST(testCreation);
	TEST(testIsConsistent);
	TEST(testGetModsWrongDir);
	TEST(testUnsatisfiedMods);
	TEST(testGetMods);
	TEST(testGetModspec);
	TEST(testGetModNamesWrongDir);
	TEST(testGetModNames);
	TEST(testGetModMediaPathsWrongDir);
	TEST(testGetModMediaPaths);
	// TODO: test MINETEST_GAME_PATH

	unsetenv("MINETEST_MOD_PATH");
}

void TestServerModManager::testCreation()
{
	std::string path = m_worlddir + DIR_DELIM + "world.mt";
	Settings world_config;
	world_config.set("gameid", SUBGAME_ID);
	world_config.set("load_mod_test_mod", "true");
	UASSERTEQ(bool, world_config.updateConfigFile(path.c_str()), true);

	auto sm = makeManager(m_worlddir);
}

void TestServerModManager::testGetModsWrongDir()
{
	// Test in non worlddir to ensure no mods are found
	auto sm = makeManager(m_worlddir + DIR_DELIM "..");
	UASSERTEQ(bool, sm.getMods().empty(), true);
}

void TestServerModManager::testUnsatisfiedMods()
{
	auto sm = makeManager(m_worlddir);
	UASSERTEQ(bool, sm.getUnsatisfiedMods().empty(), true);
}

void TestServerModManager::testIsConsistent()
{
	auto sm = makeManager(m_worlddir);
	UASSERTEQ(bool, sm.isConsistent(), true);
}

void TestServerModManager::testGetMods()
{
	auto sm = makeManager(m_worlddir);
	const auto &mods = sm.getMods();
	// `ls ./games/devtest/mods | wc -l` + 1 (test mod)
	UASSERTEQ(std::size_t, mods.size(), 35 + 1);

	// Ensure we found basenodes mod (part of devtest)
	// and test_mod (for testing MINETEST_MOD_PATH).
	bool default_found = false;
	bool test_mod_found = false;
	for (const auto &m : mods) {
		if (m.name == "basenodes")
			default_found = true;
		if (m.name == "test_mod")
			test_mod_found = true;

		// Verify if paths are not empty
		UASSERTEQ(bool, m.path.empty(), false);
	}

	UASSERTEQ(bool, default_found, true);
	UASSERTEQ(bool, test_mod_found, true);

	UASSERT(mods.front().name == "first_mod");
	UASSERT(mods.back().name == "last_mod");
}

void TestServerModManager::testGetModspec()
{
	auto sm = makeManager(m_worlddir);
	UASSERTEQ(const ModSpec *, sm.getModSpec("wrongmod"), NULL);
	UASSERT(sm.getModSpec("basenodes") != NULL);
}

void TestServerModManager::testGetModNamesWrongDir()
{
	auto sm = makeManager(m_worlddir + DIR_DELIM "..");
	std::vector<std::string> result;
	sm.getModNames(result);
	UASSERTEQ(bool, result.empty(), true);
}

void TestServerModManager::testGetModNames()
{
	auto sm = makeManager(m_worlddir);
	std::vector<std::string> result;
	sm.getModNames(result);
	UASSERTEQ(bool, result.empty(), false);
	UASSERT(std::find(result.begin(), result.end(), "basenodes") != result.end());
}

void TestServerModManager::testGetModMediaPathsWrongDir()
{
	auto sm = makeManager(m_worlddir + DIR_DELIM "..");
	std::vector<std::string> result;
	sm.getModsMediaPaths(result);
	UASSERTEQ(bool, result.empty(), true);
}

void TestServerModManager::testGetModMediaPaths()
{
	auto sm = makeManager(m_worlddir);
	std::vector<std::string> result;
	sm.getModsMediaPaths(result);
	UASSERTEQ(bool, result.empty(), false);

	// Test media overriding:
	// unittests depends on basenodes to override default_dirt.png,
	// thus the unittests texture path must come first in the returned media paths to take priority
	auto it = std::find(result.begin(), result.end(), sm.getModSpec("unittests")->path + DIR_DELIM + "textures");
	UASSERT(it != result.end());
	UASSERT(std::find(++it, result.end(), sm.getModSpec("basenodes")->path + DIR_DELIM + "textures") != result.end());
}
