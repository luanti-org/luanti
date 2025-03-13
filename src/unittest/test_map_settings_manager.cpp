// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#include "test.h"

#include "noise.h"
#include "settings.h"
#include "mapgen/mapgen_v5.h"
#include "util/hashing.h"
#include "map_settings_manager.h"

class TestMapSettingsManager : public TestBase {
public:
	TestMapSettingsManager() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMapSettingsManager"; }

	void makeUserConfig();
	std::string makeMetaFile(bool make_corrupt);

	void runTests(IGameDef *gamedef);

	void testMapSettingsManager();
	void testMapMetaSaveLoad();
	void testMapMetaFailures();
};

static TestMapSettingsManager g_test_instance;

void TestMapSettingsManager::runTests(IGameDef *gamedef)
{
	TEST(testMapSettingsManager);
	TEST(testMapMetaSaveLoad);
	TEST(testMapMetaFailures);
}

////////////////////////////////////////////////////////////////////////////////


void check_noise_params(const NoiseParams *np1, const NoiseParams *np2)
{
	UASSERTEQ(float, np1->offset, np2->offset);
	UASSERTEQ(float, np1->scale, np2->scale);
	UASSERT(np1->spread == np2->spread);
	UASSERTEQ(s32, np1->seed, np2->seed);
	UASSERTEQ(u16, np1->octaves, np2->octaves);
	UASSERTEQ(float, np1->persist, np2->persist);
	UASSERTEQ(float, np1->lacunarity, np2->lacunarity);
	UASSERTEQ(u32, np1->flags, np2->flags);
}


void TestMapSettingsManager::makeUserConfig()
{
	delete Settings::getLayer(SL_GLOBAL);
	Settings *conf = Settings::createLayer(SL_GLOBAL);

	conf->set("mg_name", "v7");
	conf->set("seed", "5678");
	conf->set("water_level", "20");
	conf->set("mgv5_np_factor", "0, 12,  (500, 250, 500), 920382, 5, 0.45, 3.0");
	conf->set("mgv5_np_height", "0, 15, (500, 250, 500), 841746,  5, 0.5,  3.0");
	conf->set("mgv5_np_filler_depth", "20, 1, (150, 150, 150), 261, 4, 0.7,  1.0");
	conf->set("mgv5_np_ground", "-43, 40, (80,  80,  80),  983240, 4, 0.55, 2.0");
}


std::string TestMapSettingsManager::makeMetaFile(bool make_corrupt)
{
	std::string metafile = getTestTempFile();

	const char *metafile_contents =
		"mg_name = v5\n"
		"seed = 1234\n"
		"mg_flags = light\n"
		"mgv5_np_filler_depth = 20, 1, (150, 150, 150), 261, 4, 0.7,  1.0\n"
		"mgv5_np_height = 20, 10, (250, 250, 250), 84174,  4, 0.5,  1.0\n";

	FILE *f = fopen(metafile.c_str(), "wb");
	UASSERT(f != NULL);

	fputs(metafile_contents, f);
	if (!make_corrupt)
		fputs("[end_of_params]\n", f);

	fclose(f);

	return metafile;
}


void TestMapSettingsManager::testMapSettingsManager()
{
	makeUserConfig();

	std::string test_mapmeta_path = makeMetaFile(false);

	MapSettingsManager mgr(test_mapmeta_path);
	std::string value;

	UASSERT(mgr.getMapSetting("mg_name", &value));
	UASSERT(value == "v7");

	// Pretend we're initializing the ServerMap
	UASSERT(mgr.loadMapMeta());

	// Pretend some scripts are requesting mapgen params
	UASSERT(mgr.getMapSetting("mg_name", &value));
	UASSERT(value == "v5");
	UASSERT(mgr.getMapSetting("seed", &value));
	UASSERT(value == "1234");
	UASSERT(mgr.getMapSetting("water_level", &value));
	UASSERT(value == "20");

	// Pretend we have some mapgen settings configured from the scripting
	UASSERT(mgr.setMapSetting("water_level", "15"));
	UASSERT(mgr.setMapSetting("seed", "02468"));
	UASSERT(mgr.setMapSetting("mg_flags", "nolight", true));

	NoiseParams script_np_filler_depth(0, 100, v3f(200, 100, 200), 261, 4, 0.7, 2.0);
	NoiseParams script_np_factor(0, 100, v3f(50, 50, 50), 920381, 3, 0.45, 2.0);
	NoiseParams script_np_height(0, 100, v3f(450, 450, 450), 84174, 4, 0.5, 2.0);
	NoiseParams meta_np_height(20, 10, v3f(250, 250, 250), 84174,  4, 0.5,  1.0);
	NoiseParams user_np_ground(-43, 40, v3f(80,  80,  80),  983240, 4, 0.55, 2.0, NOISE_FLAG_EASED);

	mgr.setMapSettingNoiseParams("mgv5_np_filler_depth", &script_np_filler_depth, true);
	mgr.setMapSettingNoiseParams("mgv5_np_height", &script_np_height);
	mgr.setMapSettingNoiseParams("mgv5_np_factor", &script_np_factor);

	{
		NoiseParams dummy;
		mgr.getNoiseParams("mgv5_np_factor", &dummy);
		check_noise_params(&dummy, &script_np_factor);
	}

	// The settings manager MUST leave user settings alone
	mgr.setMapSetting("testname", "1");
	mgr.setMapSetting("testname", "1", true);
	UASSERT(!Settings::getLayer(SL_GLOBAL)->exists("testname"));

	// Now make our Params and see if the values are correctly sourced
	MapgenParams *params = mgr.makeMapgenParams();
	UASSERT(params->mgtype == MAPGEN_V5);
	UASSERT(params->chunksize == 5);
	UASSERT(params->water_level == 15);
	UASSERT(params->seed == 1234);
	UASSERT((params->flags & MG_LIGHT) == 0);

	MapgenV5Params *v5params = (MapgenV5Params *)params;

	check_noise_params(&v5params->np_filler_depth, &script_np_filler_depth);
	check_noise_params(&v5params->np_factor, &script_np_factor);
	check_noise_params(&v5params->np_height, &meta_np_height);
	check_noise_params(&v5params->np_ground, &user_np_ground);

	UASSERT(mgr.setMapSetting("foobar", "25") == false);

	// Pretend the ServerMap is shutting down
	UASSERT(mgr.saveMapMeta());

	// Make sure our interface expectations are met
	UASSERT(mgr.mapgen_params == params);
	UASSERT(mgr.makeMapgenParams() == params);

#if 0
	// TODO(paramat or hmmmm): change this to compare the result against a static file

	// Load the resulting map_meta.txt and make sure it contains what we expect
	unsigned char expected_contents_hash[20] = {
		0x48, 0x3f, 0x88, 0x5a, 0xc0, 0x7a, 0x14, 0x48, 0xa4, 0x71,
		0x78, 0x56, 0x95, 0x2d, 0xdc, 0x6a, 0xf7, 0x61, 0x36, 0x5f
	};

	std::string metafile_contents;
	UASSERT(fs::ReadFile(test_mapmeta_path, metafile_contents));
	std::string sha1_result = hashing::sha1(metafile_contents);
	int resultdiff = memcmp(sha1_result.data(), expected_contents_hash, 20);

	UASSERT(!resultdiff);
#endif
}


void TestMapSettingsManager::testMapMetaSaveLoad()
{
	std::string path = getTestTempDirectory()
		+ DIR_DELIM + "foobar" + DIR_DELIM + "map_meta.txt";

	makeUserConfig();
	Settings &conf = *Settings::getLayer(SL_GLOBAL);

	// There cannot be two MapSettingsManager
	// copy the mapgen params to compare them
	MapgenParams params1, params2;
	// Create a set of mapgen params and save them to map meta
	{
		conf.set("seed", "12345");
		conf.set("water_level", "5");
		MapSettingsManager mgr(path);
		MapgenParams *params = mgr.makeMapgenParams();
		UASSERT(params);
		params1 = *params;
		params1.bparams = nullptr; // No double-free
		UASSERT(mgr.saveMapMeta());
	}

	// Now try loading the map meta to mapgen params
	{
		conf.set("seed", "67890");
		conf.set("water_level", "32");
		MapSettingsManager mgr(path);
		UASSERT(mgr.loadMapMeta());
		MapgenParams *params = mgr.makeMapgenParams();
		UASSERT(params);
		params2 = *params;
		params2.bparams = nullptr; // No double-free
	}

	// Check that both results are correct
	UASSERTEQ(u64, params1.seed, 12345);
	UASSERTEQ(s16, params1.water_level, 5);
	UASSERTEQ(u64, params2.seed, 12345);
	UASSERTEQ(s16, params2.water_level, 5);
}


void TestMapSettingsManager::testMapMetaFailures()
{
	std::string test_mapmeta_path;

	// Check to see if it'll fail on a non-existent map meta file
	{
		test_mapmeta_path = "woobawooba/fgdfg/map_meta.txt";
		UASSERT(!fs::PathExists(test_mapmeta_path));

		MapSettingsManager mgr1(test_mapmeta_path);
		UASSERT(!mgr1.loadMapMeta());
	}

	// Check to see if it'll fail on a corrupt map meta file
	{
		test_mapmeta_path = makeMetaFile(true);
		UASSERT(fs::PathExists(test_mapmeta_path));

		MapSettingsManager mgr2(test_mapmeta_path);
		UASSERT(!mgr2.loadMapMeta());
	}
}
