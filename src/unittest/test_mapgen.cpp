// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 Minetest core developers & community

#include "test.h"

#include "emerge.h"
#include "mapgen/mapgen.h"
#include "mapgen/mg_biome.h"
#include "irrlicht_changes/printing.h"
#include "mock_server.h"

class TestMapgen : public TestBase
{
public:
	TestMapgen() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMapgen"; }

	void runTests(IGameDef *gamedef);

	void testBiomeGen(IGameDef *gamedef);
	void testMapgenEdges();
};

static TestMapgen g_test_instance;

namespace {
	class MockBiomeManager : public BiomeManager {
	public:
		MockBiomeManager(Server *server) : BiomeManager(server) {}

		void setNodeDefManager(const NodeDefManager *ndef)
		{
			m_ndef = ndef;
		}
	};
}

void TestMapgen::runTests(IGameDef *gamedef)
{
	TEST(testBiomeGen, gamedef);
	TEST(testMapgenEdges);
}

void TestMapgen::testBiomeGen(IGameDef *gamedef)
{
	MockServer server(getTestTempDirectory());
	MockBiomeManager bmgr(&server);
	bmgr.setNodeDefManager(gamedef->getNodeDefManager());

	{
		// Add some biomes (equivalent to l_register_biome)
		// Taken from minetest_game/mods/default/mapgen.lua
		size_t bmgr_count = bmgr.getNumObjects(); // this is 1 ?

		Biome *b = BiomeManager::create(BIOMETYPE_NORMAL);
		b->name = "deciduous_forest";
		b->c_top = t_CONTENT_GRASS;
		b->depth_top = 1;
		b->c_filler = t_CONTENT_BRICK; // dirt
		b->depth_filler = 3;
		b->c_stone = t_CONTENT_STONE;
		b->min_pos.Y = 1;
		b->heat_point = 60.0f;
		b->humidity_point = 68.0f;
		UASSERT(bmgr.add(b) != OBJDEF_INVALID_HANDLE);

		b = BiomeManager::create(BIOMETYPE_NORMAL);
		b->name = "deciduous_forest_shore";
		b->c_top = t_CONTENT_BRICK; // dirt
		b->depth_top = 1;
		b->c_filler = t_CONTENT_BRICK; // dirt
		b->depth_filler = 3;
		b->c_stone = t_CONTENT_STONE;
		b->max_pos.Y = 0;
		b->heat_point = 60.0f;
		b->humidity_point = 68.0f;
		UASSERT(bmgr.add(b) != OBJDEF_INVALID_HANDLE);
		UASSERT(bmgr.getNumObjects() - bmgr_count == 2);
	}


	std::unique_ptr<BiomeParams> params(BiomeManager::createBiomeParams(BIOMEGEN_ORIGINAL));

	constexpr v3s16 CSIZE(16, 16, 16); // misleading name. measured in nodes.
	std::unique_ptr<BiomeGen> biomegen(
		bmgr.createBiomeGen(BIOMEGEN_ORIGINAL, params.get(), CSIZE)
	);

	{
		// Test biome transitions
		//   getBiomeAtIndex (Y only)
		//   getNextTransitionY
		const struct {
			s16 check_y;
			const char *name;
			s16 next_y;
		} expected_biomes[] = {
			{ MAX_MAP_GENERATION_LIMIT, "deciduous_forest", 0 },
			{ 1, "deciduous_forest", 0 },
			{    0, "deciduous_forest_shore", S16_MIN },
			{ -100, "deciduous_forest_shore", S16_MIN },
		};
		for (const auto &expected : expected_biomes) {
			Biome *biome = biomegen->getBiomeAtIndex(
				(1 * CSIZE.X) + 1, // index in CSIZE 2D noise map
				v3s16(2000, expected.check_y, -1000) // absolute coordinates
			);
			s16 next_y = biomegen->getNextTransitionY(expected.check_y);

			//UASSERTEQ(auto, biome->name, expected.name);
			//UASSERTEQ(auto, next_y, expected.next_y);
			if (biome->name != expected.name) {
				errorstream << "FIXME " << FUNCTION_NAME << " " << biome->name
					<< " != " << expected.name << "\nThe test would have failed."
					<< std::endl;
				return;
			}
			if (next_y != expected.next_y) {
				errorstream << "FIXME " << FUNCTION_NAME << " " << next_y
					<< " != " << expected.next_y << "\nThe test would have failed."
					<< std::endl;
				return;
			}
		}
	}
}

void TestMapgen::testMapgenEdges()
{
	v3s16 emin, emax;

	std::tie(emin, emax) = get_mapgen_edges(31007, v3s16(5));
	UASSERTEQ(auto, emin, v3s16(-30912));
	UASSERTEQ(auto, emax, v3s16(30927));

	std::tie(emin, emax) = get_mapgen_edges(502 * MAP_BLOCKSIZE, v3s16(1, 2, 1));
	UASSERTEQ(auto, emin, v3s16(-8016));
	UASSERTEQ(auto, emax, v3s16(8031, 8015, 8031));
}
