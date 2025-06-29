// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Minetest Authors

#include "catch.h"
#include "dummygamedef.h"
#include "map.h"
#include "mapsector.h"

namespace {
class TestMap : public Map {
public:
	TestMap(IGameDef *gamedef) : Map(gamedef) {}

	MapBlock * createBlockTest(v3s16 p)
	{
		v2s16 p2d(p.X, p.Z);
		s16 block_y = p.Y;

		MapSector *sector = getSectorNoGenerate(p2d);
		if (!sector) {
			sector = new MapSector(this, p2d, m_gamedef);
			m_sectors[p2d] = sector;
		}

		MapBlock *block = sector->getBlockNoCreateNoEx(block_y);
		if (block)
			return block;

		return sector->createBlankBlock(block_y);
	}

};
}

static void fillMap(TestMap &map, s16 n)
{
	for(s16 z=0; z<n; z++)
	for(s16 y=0; y<n; y++)
	for(s16 x=0; x<n; x++) {
		v3s16 p(x,y,z);
		// create an empty block
		map.createBlockTest(p);
	}
}

static int readBlocks(Map &map, s16 n)
{
	int result = 0;
	for(s16 z=0; z<n; z++)
	for(s16 y=0; y<n; y++)
	for(s16 x=0; x<n; x++) {
		v3s16 p(x,y,z);
		MapBlock *block = map.getBlockNoCreateNoEx(p);
		if (block) {
			result++;
		}
	}
	return result;
}

static int readRandomBlocks(Map &map, s16 n)
{
	int result = 0;
	for(int i=0; i < n * n * n; i++) {
		v3s16 p(myrand_range(0, n), myrand_range(0, n), myrand_range(0, n));
		MapBlock *block = map.getBlockNoCreateNoEx(p);
		if (block) {
			result++;
		}
	}
	return result;
}


static int readYColumn(Map &map, s16 n)
{
	int result = 0;
	for(s16 z=0; z<n; z++)
	for(s16 x=0; x<n; x++)
	for(s16 y=n-1; y>0; y--) {
		v3s16 p(x,y,z);
		MapBlock *block = map.getBlockNoCreateNoEx(p);
		if (block) {
			result++;
		}
	}
	return result;
}

static int readNodes(Map &map, s16 n)
{
	int result = 0;
	for(s16 z=0; z<n * MAP_BLOCKSIZE; z+=8)
	for(s16 y=0; y<n * MAP_BLOCKSIZE; y+=4)
	for(s16 x=0; x<n * MAP_BLOCKSIZE; x++) {
		v3s16 p(x,y,z);
		MapNode n = map.getNode(p);
		if (n.getContent() != CONTENT_IGNORE)
			result++;
	}
	return result;
}


#define BENCH1(_count) \
	BENCHMARK_ADVANCED("create_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		meter.measure([&] { \
			fillMap(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readEmpty_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		meter.measure([&] { \
			return readBlocks(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readFilled_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		fillMap(map, _count); \
		meter.measure([&] { \
			return readBlocks(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readEmptyYCol_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		meter.measure([&] { \
			return readYColumn(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readFilledYCol_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		fillMap(map, _count); \
		meter.measure([&] { \
			return readYColumn(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readEmptyRandom_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		meter.measure([&] { \
			return readRandomBlocks(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readFilledRandom_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		fillMap(map, _count); \
		meter.measure([&] { \
			return readRandomBlocks(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readEmptyNodes_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		meter.measure([&] { \
			return readNodes(map, _count); \
		}); \
	}; \
	BENCHMARK_ADVANCED("readFilledNodes_" #_count)(Catch::Benchmark::Chronometer meter) { \
		DummyGameDef gamedef; \
		TestMap map(&gamedef); \
		fillMap(map, _count); \
		meter.measure([&] { \
			return readNodes(map, _count); \
		}); \
	}; \


TEST_CASE("benchmark_map") {
	BENCH1(10)
	BENCH1(40) // 64.000 blocks
}
