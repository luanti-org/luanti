#define CATCH_CONFIG_DISABLE_SIGNAL_HANDLING

#include <random>

#include "catch.h"
#include "dummygamedef.h"
#include "dummymap.h"
#include "client/content_mapblock.h"
#include "client/lod_mapblock.h"
#include "client/mapblock_mesh.h"
#include "client/meshgen/collector.h"

enum class MapPattern : u8 {
	All_AIR,
	All_STONE,
	HALF,
	STRIPED,
	RANDOM
};

std::map<MapPattern, std::string> enum_names = {{MapPattern::All_AIR, "ALL_AIR"},
	{MapPattern::All_STONE, "All_STONE"},
	{MapPattern::HALF, "HALF"},
	{MapPattern::STRIPED, "STRIPED"},
	{MapPattern::RANDOM, "RANDOM"}
};

struct BenchmarkCase {
	u8 lod;
	MapPattern pattern;
};

void build_map(VoxelManipulator &manip, MapPattern pattern,
	content_t air, content_t water, content_t stone) {
	switch (pattern) {
		case MapPattern::All_AIR:
			for (s16 x = 0; x < 256; x++)
			for (s16 y = 0; y < 256; y++)
			for (s16 z = 0; z < 256; z++)
				manip.setNodeNoEmerge({x, y, z}, { air });
			break;
		case MapPattern::All_STONE:
			for (s16 x = 0; x < 256; x++)
			for (s16 y = 0; y < 256; y++)
			for (s16 z = 0; z < 256; z++)
				manip.setNodeNoEmerge({x, y, z}, { stone });
			break;
		case MapPattern::HALF:
			for (u8 x = 0; x < 128; x++)
			for (s16 y = 0; y < 256; y++)
			for (u8 z = 0; z < 128; z++)
				manip.setNodeNoEmerge({x, y, z}, { y > 64 ? air : (y > 32 ? water : stone)});
			break;
		case MapPattern::STRIPED:
			for (s16 x = 0; x < 256; x++)
			for (s16 y = 0; y < 256; y++)
			for (s16 z = 0; z < 256; z++)
			if (x % 9 < 3)
				manip.setNodeNoEmerge({x, y, z}, { stone });
			break;
		case MapPattern::RANDOM:
			{
				std::mt19937 rng(0xC0FFEE);
				for (s16 x = 0; x < 256; x++)
				for (s16 y = 0; y < 256; y++)
				for (s16 z = 0; z < 256; z++) {
					u8 n = rng() % 100;
					if (n < 50)
						continue;
					n = rng() % 100;
					manip.setNodeNoEmerge({x, y, z}, { n > 60 ? air : (n > 30 ? water : stone) });
				}
			}
		default:
			;
	}
}

TEST_CASE("benchmark_lod_mesh_gen")
{
	std::vector<BenchmarkCase> cases;
	for (u8 lod = 1; lod <= 8; lod++)
	for (MapPattern pattern : {MapPattern::All_AIR, MapPattern::All_STONE, MapPattern::HALF, MapPattern::STRIPED, MapPattern::RANDOM}) {
		cases.push_back({lod, pattern});
	}

	DummyGameDef gamedef;
	NodeDefManager *ndef = gamedef.getWritableNodeDefManager();

	content_t content_stone;
	{
		ContentFeatures f;
		f.name = "stone";
		f.drawtype = NDT_NORMAL;
		content_stone = ndef->set(f.name, f);
	}

	content_t content_water;
	{
		ContentFeatures f;
		f.name = "water";
		f.drawtype = NDT_LIQUID;
		content_water = ndef->set(f.name, f);
	}

	content_t content_air;
	{
		ContentFeatures f;
		f.name = "air";
		f.drawtype = NDT_AIRLIKE;
		content_air = ndef->set(f.name, f);
	}

	std::map<MapPattern, MeshMakeData> data;
	for (MapPattern pattern : {MapPattern::All_AIR, MapPattern::All_STONE, MapPattern::HALF, MapPattern::STRIPED, MapPattern::RANDOM}) {
		data.emplace(std::piecewise_construct, std::forward_as_tuple(pattern), std::forward_as_tuple(ndef, 256, MeshGrid{16}));
		MeshMakeData &d = data.at(pattern);
		d.fillBlockDataBegin(v3s16(0));
		build_map(d.m_vmanip, MapPattern::All_AIR, content_air, content_water, content_stone);
	}

	for (MapPattern pattern : {MapPattern::All_AIR, MapPattern::All_STONE, MapPattern::HALF, MapPattern::STRIPED, MapPattern::RANDOM}) {
		BENCHMARK_ADVANCED("benchmark_mesh_gen:pattern_" + enum_names[pattern])
		(Catch::Benchmark::Chronometer meter) {
			MeshCollector collector(v3f(0), v3f(0));
			meter.measure([&] {
				MapblockMeshGenerator(&data.at(pattern), &collector).generate();
			});
		};
	}

	for (auto [lod, pattern] : cases) {
		BENCHMARK_ADVANCED("benchmark_lod_mesh_gen:lod_" + std::to_string(lod)
			+ ",pattern_" + enum_names[pattern])
		(Catch::Benchmark::Chronometer meter) {
			MeshCollector collector(v3f(0), v3f(0));
			meter.measure([&] {
				LodMeshGenerator(&data.at(pattern), &collector, true).generate(lod);
				LodMeshGenerator(&data.at(pattern), &collector, false).generate(lod);
			});
		};
	}
}