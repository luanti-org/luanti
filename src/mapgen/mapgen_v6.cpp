// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2014-2018 paramat


#include <cmath>
#include "mapgen.h"
#include "voxel.h"
#include "noise.h"
#include "mapblock.h"
#include "mapnode.h"
#include "map.h"
#include "nodedef.h"
#include "voxelalgorithms.h"
//#include "profiler.h" // For TimeTaker
#include "settings.h" // For g_settings
#include "emerge.h"
#include "dungeongen.h"
#include "cavegen.h"
#include "treegen.h"
#include "mg_ore.h"
#include "mg_decoration.h"
#include "mapgen_v6.h"


const FlagDesc flagdesc_mapgen_v6[] = {
	{"jungles",    MGV6_JUNGLES},
	{"biomeblend", MGV6_BIOMEBLEND},
	{"mudflow",    MGV6_MUDFLOW},
	{"snowbiomes", MGV6_SNOWBIOMES},
	{"flat",       MGV6_FLAT},
	{"trees",      MGV6_TREES},
	{"temples",    MGV6_TEMPLES},
	{NULL,         0}
};


/////////////////////////////////////////////////////////////////////////////


MapgenV6::MapgenV6(MapgenV6Params *params, EmergeParams *emerge)
	: Mapgen(MAPGEN_V6, params, emerge)
{
	ystride = csize.X;

	heightmap = new s16[csize.X * csize.Z];

	spflags      = params->spflags;
	freq_desert  = params->freq_desert;
	freq_beach   = params->freq_beach;
	dungeon_ymin = params->dungeon_ymin;
	dungeon_ymax = params->dungeon_ymax;

	np_cave        = &params->np_cave;
	np_humidity    = &params->np_humidity;
	np_trees       = &params->np_trees;
	np_apple_trees = &params->np_apple_trees;

	np_dungeons = NoiseParams(0.9, 0.5, v3f(500.0, 500.0, 500.0), 0, 2, 0.8, 2.0);

	//// Create noise objects
	noise_terrain_base   = new Noise(&params->np_terrain_base,   seed, csize.X, csize.Y);
	noise_terrain_higher = new Noise(&params->np_terrain_higher, seed, csize.X, csize.Y);
	noise_steepness      = new Noise(&params->np_steepness,      seed, csize.X, csize.Y);
	noise_height_select  = new Noise(&params->np_height_select,  seed, csize.X, csize.Y);
	noise_mud            = new Noise(&params->np_mud,            seed, csize.X, csize.Y);
	noise_beach          = new Noise(&params->np_beach,          seed, csize.X, csize.Y);
	noise_biome          = new Noise(&params->np_biome,          seed,
			csize.X + 2 * MAP_BLOCKSIZE, csize.Y + 2 * MAP_BLOCKSIZE);
	noise_humidity       = new Noise(&params->np_humidity,       seed,
			csize.X + 2 * MAP_BLOCKSIZE, csize.Y + 2 * MAP_BLOCKSIZE);

	//// Resolve nodes to be used
	const NodeDefManager *ndef = emerge->ndef;

	c_stone           = ndef->getId("mapgen_stone");
	c_dirt            = ndef->getId("mapgen_dirt");
	c_dirt_with_grass = ndef->getId("mapgen_dirt_with_grass");
	c_sand            = ndef->getId("mapgen_sand");
	c_water_source    = ndef->getId("mapgen_water_source");
	c_lava_source     = ndef->getId("mapgen_lava_source");
	c_gravel          = ndef->getId("mapgen_gravel");
	c_desert_stone    = ndef->getId("mapgen_desert_stone");
	c_desert_sand     = ndef->getId("mapgen_desert_sand");
	c_dirt_with_snow  = ndef->getId("mapgen_dirt_with_snow");
	c_snow            = ndef->getId("mapgen_snow");
	c_snowblock       = ndef->getId("mapgen_snowblock");
	c_ice             = ndef->getId("mapgen_ice");

	if (c_gravel == CONTENT_IGNORE)
		c_gravel = c_stone;
	if (c_desert_stone == CONTENT_IGNORE)
		c_desert_stone = c_stone;
	if (c_desert_sand == CONTENT_IGNORE)
		c_desert_sand = c_sand;
	if (c_dirt_with_snow == CONTENT_IGNORE)
		c_dirt_with_snow = c_dirt_with_grass;
	if (c_snow == CONTENT_IGNORE)
		c_snow = CONTENT_AIR;
	if (c_snowblock == CONTENT_IGNORE)
		c_snowblock = c_dirt_with_grass;
	if (c_ice == CONTENT_IGNORE)
		c_ice = c_water_source;

	c_cobble             = ndef->getId("mapgen_cobble");
	c_mossycobble        = ndef->getId("mapgen_mossycobble");
	c_stair_cobble       = ndef->getId("mapgen_stair_cobble");
	c_stair_desert_stone = ndef->getId("mapgen_stair_desert_stone");

	if (c_mossycobble == CONTENT_IGNORE)
		c_mossycobble = c_cobble;
	if (c_stair_cobble == CONTENT_IGNORE)
		c_stair_cobble = c_cobble;
	if (c_stair_desert_stone == CONTENT_IGNORE)
		c_stair_desert_stone = c_desert_stone;

	if (c_stone == CONTENT_IGNORE)
		errorstream << "Mapgen v6: Mapgen alias 'mapgen_stone' is invalid!" << std::endl;
	if (c_dirt == CONTENT_IGNORE)
		errorstream << "Mapgen v6: Mapgen alias 'mapgen_dirt' is invalid!" << std::endl;
	if (c_dirt_with_grass == CONTENT_IGNORE)
		errorstream << "Mapgen v6: Mapgen alias 'mapgen_dirt_with_grass' is invalid!" << std::endl;
	if (c_sand == CONTENT_IGNORE)
		errorstream << "Mapgen v6: Mapgen alias 'mapgen_sand' is invalid!" << std::endl;
	if (c_water_source == CONTENT_IGNORE)
		errorstream << "Mapgen v6: Mapgen alias 'mapgen_water_source' is invalid!" << std::endl;
	if (c_lava_source == CONTENT_IGNORE)
		errorstream << "Mapgen v6: Mapgen alias 'mapgen_lava_source' is invalid!" << std::endl;
	if (c_cobble == CONTENT_IGNORE)
		errorstream << "Mapgen v6: Mapgen alias 'mapgen_cobble' is invalid!" << std::endl;
}


MapgenV6::~MapgenV6()
{
	delete noise_terrain_base;
	delete noise_terrain_higher;
	delete noise_steepness;
	delete noise_height_select;
	delete noise_mud;
	delete noise_beach;
	delete noise_biome;
	delete noise_humidity;

	delete[] heightmap;
}


MapgenV6Params::MapgenV6Params():
	np_terrain_base   (-4,   20.0, v3f(250.0, 250.0, 250.0), 82341,  5, 0.6,  2.0),
	np_terrain_higher (20,   16.0, v3f(500.0, 500.0, 500.0), 85039,  5, 0.6,  2.0),
	np_steepness      (0.85, 0.5,  v3f(125.0, 125.0, 125.0), -932,   5, 0.7,  2.0),
	np_height_select  (0,    1.0,  v3f(250.0, 250.0, 250.0), 4213,   5, 0.69, 2.0),
	np_mud            (4,    2.0,  v3f(200.0, 200.0, 200.0), 91013,  3, 0.55, 2.0),
	np_beach          (0,    1.0,  v3f(250.0, 250.0, 250.0), 59420,  3, 0.50, 2.0),
	np_biome          (0,    1.0,  v3f(500.0, 500.0, 500.0), 9130,   3, 0.50, 2.0),
	np_cave           (6,    6.0,  v3f(250.0, 250.0, 250.0), 34329,  3, 0.50, 2.0),
	np_humidity       (0.5,  0.5,  v3f(500.0, 500.0, 500.0), 72384,  3, 0.50, 2.0),
	np_trees          (0,    1.0,  v3f(125.0, 125.0, 125.0), 2,      4, 0.66, 2.0),
	np_apple_trees    (0,    1.0,  v3f(100.0, 100.0, 100.0), 342902, 3, 0.45, 2.0)
{
}


void MapgenV6Params::readParams(const Settings *settings)
{
	settings->getFlagStrNoEx("mgv6_spflags", spflags, flagdesc_mapgen_v6);
	settings->getFloatNoEx("mgv6_freq_desert", freq_desert);
	settings->getFloatNoEx("mgv6_freq_beach",  freq_beach);
	settings->getS16NoEx("mgv6_dungeon_ymin",  dungeon_ymin);
	settings->getS16NoEx("mgv6_dungeon_ymax",  dungeon_ymax);

	settings->getNoiseParams("mgv6_np_terrain_base",   np_terrain_base);
	settings->getNoiseParams("mgv6_np_terrain_higher", np_terrain_higher);
	settings->getNoiseParams("mgv6_np_steepness",      np_steepness);
	settings->getNoiseParams("mgv6_np_height_select",  np_height_select);
	settings->getNoiseParams("mgv6_np_mud",            np_mud);
	settings->getNoiseParams("mgv6_np_beach",          np_beach);
	settings->getNoiseParams("mgv6_np_biome",          np_biome);
	settings->getNoiseParams("mgv6_np_cave",           np_cave);
	settings->getNoiseParams("mgv6_np_humidity",       np_humidity);
	settings->getNoiseParams("mgv6_np_trees",          np_trees);
	settings->getNoiseParams("mgv6_np_apple_trees",    np_apple_trees);
}


void MapgenV6Params::writeParams(Settings *settings) const
{
	settings->setFlagStr("mgv6_spflags", spflags, flagdesc_mapgen_v6);
	settings->setFloat("mgv6_freq_desert", freq_desert);
	settings->setFloat("mgv6_freq_beach",  freq_beach);
	settings->setS16("mgv6_dungeon_ymin",  dungeon_ymin);
	settings->setS16("mgv6_dungeon_ymax",  dungeon_ymax);

	settings->setNoiseParams("mgv6_np_terrain_base",   np_terrain_base);
	settings->setNoiseParams("mgv6_np_terrain_higher", np_terrain_higher);
	settings->setNoiseParams("mgv6_np_steepness",      np_steepness);
	settings->setNoiseParams("mgv6_np_height_select",  np_height_select);
	settings->setNoiseParams("mgv6_np_mud",            np_mud);
	settings->setNoiseParams("mgv6_np_beach",          np_beach);
	settings->setNoiseParams("mgv6_np_biome",          np_biome);
	settings->setNoiseParams("mgv6_np_cave",           np_cave);
	settings->setNoiseParams("mgv6_np_humidity",       np_humidity);
	settings->setNoiseParams("mgv6_np_trees",          np_trees);
	settings->setNoiseParams("mgv6_np_apple_trees",    np_apple_trees);
}


void MapgenV6Params::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgv6_spflags", flagdesc_mapgen_v6, MGV6_JUNGLES |
		MGV6_SNOWBIOMES | MGV6_TREES | MGV6_BIOMEBLEND | MGV6_MUDFLOW |
		MGV6_TEMPLES);
}


//////////////////////// Some helper functions for the map generator


// Returns Y one under area minimum if not found
s16 MapgenV6::find_stone_level(v2s16 p2d)
{
	const v3s32 &em = vm->m_area.getExtent();
	s16 y_nodes_max = vm->m_area.MaxEdge.Y;
	s16 y_nodes_min = vm->m_area.MinEdge.Y;
	u32 i = vm->m_area.index(p2d.X, y_nodes_max, p2d.Y);
	s16 y;

	for (y = y_nodes_max; y >= y_nodes_min; y--) {
		content_t c = vm->m_data[i].getContent();
		if (c != CONTENT_IGNORE && (c == c_stone || c == c_desert_stone))
			break;

		VoxelArea::add_y(em, i, -1);
	}
	return (y >= y_nodes_min) ? y : y_nodes_min - 1;
}


// Required by mapgen.h
bool MapgenV6::block_is_underground(u64 seed, v3s16 blockpos)
{
	/*s16 minimum_groundlevel = (s16)get_sector_minimum_ground_level(
			seed, v2s16(blockpos.X, blockpos.Z));*/
	// Nah, this is just a heuristic, just return something
	s16 minimum_groundlevel = water_level;

	if(blockpos.Y * MAP_BLOCKSIZE + MAP_BLOCKSIZE <= minimum_groundlevel)
		return true;

	return false;
}


//////////////////////// Base terrain height functions

float MapgenV6::baseTerrainLevel(float terrain_base, float terrain_higher,
	float steepness, float height_select)
{
	float base   = 1 + terrain_base;
	float higher = 1 + terrain_higher;

	// Limit higher ground level to at least base
	if(higher < base)
		higher = base;

	// Steepness factor of cliffs
	float b = steepness;
	b = rangelim(b, 0.0, 1000.0);
	b = 5 * b * b * b * b * b * b * b;
	b = rangelim(b, 0.5, 1000.0);

	// Values 1.5...100 give quite horrible looking slopes
	if (b > 1.5 && b < 100.0)
		b = (b < 10.0) ? 1.5 : 100.0;

	float a_off = -0.20; // Offset to more low
	float a = 0.5 + b * (a_off + height_select);
	a = rangelim(a, 0.0, 1.0); // Limit

	return base * (1.0 - a) + higher * a;
}


float MapgenV6::baseTerrainLevelFromNoise(v2s16 p)
{
	if (spflags & MGV6_FLAT)
		return water_level;

	float terrain_base   = NoiseFractal2D_PO(&noise_terrain_base->np,
							p.X, 0.5, p.Y, 0.5, seed);
	float terrain_higher = NoiseFractal2D_PO(&noise_terrain_higher->np,
							p.X, 0.5, p.Y, 0.5, seed);
	float steepness      = NoiseFractal2D_PO(&noise_steepness->np,
							p.X, 0.5, p.Y, 0.5, seed);
	float height_select  = NoiseFractal2D_PO(&noise_height_select->np,
							p.X, 0.5, p.Y, 0.5, seed);

	return baseTerrainLevel(terrain_base, terrain_higher,
							steepness, height_select);
}


float MapgenV6::baseTerrainLevelFromMap(v2s16 p)
{
	int index = (p.Y - node_min.Z) * ystride + (p.X - node_min.X);
	return baseTerrainLevelFromMap(index);
}


float MapgenV6::baseTerrainLevelFromMap(int index)
{
	if (spflags & MGV6_FLAT)
		return water_level;

	float terrain_base   = noise_terrain_base->result[index];
	float terrain_higher = noise_terrain_higher->result[index];
	float steepness      = noise_steepness->result[index];
	float height_select  = noise_height_select->result[index];

	return baseTerrainLevel(terrain_base, terrain_higher,
							steepness, height_select);
}


int MapgenV6::getGroundLevelAtPoint(v2s16 p)
{
	return baseTerrainLevelFromNoise(p) + MGV6_AVERAGE_MUD_AMOUNT;
}


int MapgenV6::getSpawnLevelAtPoint(v2s16 p)
{
	s16 level_at_point = baseTerrainLevelFromNoise(p) + MGV6_AVERAGE_MUD_AMOUNT;
	if (level_at_point <= water_level ||
			level_at_point > water_level + 16)
		return MAX_MAP_GENERATION_LIMIT;  // Unsuitable spawn point

	return level_at_point;
}


//////////////////////// Noise functions


BiomeV6Type MapgenV6::getBiome(v2s16 p)
{
	int index = (p.Y - full_node_min.Z) * (ystride + 2 * MAP_BLOCKSIZE)
			+ (p.X - full_node_min.X);
	return getBiome(index, p);
}


float MapgenV6::getHumidity(v2s16 p)
{
	/*double noise = noise2d_fractal(
		0.5+(float)p.X/500, 0.5+(float)p.Y/500,
		seed+72384, 4, 0.66);
	noise = (noise + 1.0)/2.0;*/

	int index = (p.Y - full_node_min.Z) * (ystride + 2 * MAP_BLOCKSIZE)
			+ (p.X - full_node_min.X);
	float noise = noise_humidity->result[index];

	if (noise < 0.0)
		noise = 0.0;
	if (noise > 1.0)
		noise = 1.0;
	return noise;
}


float MapgenV6::getTreeAmount(v2s16 p)
{
	/*double noise = noise2d_fractal(
			0.5+(float)p.X/125, 0.5+(float)p.Y/125,
			seed+2, 4, 0.66);*/

	float noise = NoiseFractal2D(np_trees, p.X, p.Y, seed);
	float zeroval = -0.39;
	if (noise < zeroval)
		return 0;

	return 0.04 * (noise - zeroval) / (1.0 - zeroval);
}


bool MapgenV6::getHaveAppleTree(v2s16 p)
{
	/*is_apple_tree = noise2d_fractal(
		0.5+(float)p.X/100, 0.5+(float)p.Z/100,
		data->seed+342902, 3, 0.45) > 0.2;*/

	float noise = NoiseFractal2D(np_apple_trees, p.X, p.Y, seed);

	return noise > 0.2;
}


float MapgenV6::getMudAmount(int index)
{
	if (spflags & MGV6_FLAT)
		return MGV6_AVERAGE_MUD_AMOUNT;

	/*return ((float)AVERAGE_MUD_AMOUNT + 2.0 * noise2d_fractal(
			0.5+(float)p.X/200, 0.5+(float)p.Y/200,
			seed+91013, 3, 0.55));*/

	return noise_mud->result[index];
}


bool MapgenV6::getHaveBeach(int index)
{
	// Determine whether to have sand here
	/*double sandnoise = noise2d_fractal(
			0.2+(float)p2d.X/250, 0.7+(float)p2d.Y/250,
			seed+59420, 3, 0.50);*/

	float sandnoise = noise_beach->result[index];
	return (sandnoise > freq_beach);
}


BiomeV6Type MapgenV6::getBiome(int index, v2s16 p)
{
	// Just do something very simple as for now
	/*double d = noise2d_fractal(
			0.6+(float)p2d.X/250, 0.2+(float)p2d.Y/250,
			seed+9130, 3, 0.50);*/

	float d = noise_biome->result[index];
	float h = noise_humidity->result[index];

	if (spflags & MGV6_SNOWBIOMES) {
		float blend = (spflags & MGV6_BIOMEBLEND) ? noise2d(p.X, p.Y, seed) / 40 : 0;

		if (d > MGV6_FREQ_HOT + blend) {
			if (h > MGV6_FREQ_JUNGLE + blend)
				return BT_JUNGLE;

			return BT_DESERT;
		}

		if (d < MGV6_FREQ_SNOW + blend) {
			if (h > MGV6_FREQ_TAIGA + blend)
				return BT_TAIGA;

			return BT_TUNDRA;
		}

		return BT_NORMAL;
	}

	if (d > freq_desert)
		return BT_DESERT;

	if ((spflags & MGV6_BIOMEBLEND) && (d > freq_desert - 0.10) &&
			((noise2d(p.X, p.Y, seed) + 1.0) > (freq_desert - d) * 20.0))
		return BT_DESERT;

	if ((spflags & MGV6_JUNGLES) && h > 0.75)
		return BT_JUNGLE;

	return BT_NORMAL;

}


u32 MapgenV6::get_blockseed(u64 seed, v3s16 p)
{
	s32 x = p.X, y = p.Y, z = p.Z;
	return (u32)(seed % 0x100000000ULL) + z * 38134234 + y * 42123 + x * 23;
}


//////////////////////// Map generator

void MapgenV6::makeChunk(BlockMakeData *data)
{
	// Pre-conditions
	assert(data->vmanip);
	assert(data->nodedef);

	this->generating = true;
	this->vm   = data->vmanip;
	this->ndef = data->nodedef;

	// Hack: use minimum block coords for old code that assumes a single block
	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;

	// Area of central chunk
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	// Full allocated area
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	central_area_size = node_max - node_min + v3s16(1, 1, 1);
	assert(central_area_size.X == central_area_size.Z);

	// Create a block-specific seed
	blockseed = get_blockseed(data->seed, full_node_min);

	// Make some noise
	calculateNoise();

	// Maximum height of the stone surface and obstacles.
	// This is used to guide the cave generation
	s16 stone_surface_max_y;

	// Generate general ground level to full area
	stone_surface_max_y = generateGround();

	// Create initial heightmap to limit caves
	updateHeightmap(node_min, node_max);

	const s16 max_spread_amount = MAP_BLOCKSIZE;
	// Limit dirt flow area by 1 because mud is flowed into neighbors
	s16 mudflow_minpos = -max_spread_amount + 1;
	s16 mudflow_maxpos = central_area_size.X + max_spread_amount - 2;

	// Loop this part, it will make stuff look older and newer nicely
	const u32 age_loops = 2;
	for (u32 i_age = 0; i_age < age_loops; i_age++) { // Aging loop
		// Make caves (this code is relatively horrible)
		if (flags & MG_CAVES)
			generateCaves(stone_surface_max_y);

		// Add mud to the central chunk
		addMud();

		// Flow mud away from steep edges
		if (spflags & MGV6_MUDFLOW)
			flowMud(mudflow_minpos, mudflow_maxpos);

	}

	// Update heightmap after mudflow
	updateHeightmap(node_min, node_max);

	// Add dungeons
	if ((flags & MG_DUNGEONS) && stone_surface_max_y >= node_min.Y &&
			full_node_min.Y >= dungeon_ymin && full_node_max.Y <= dungeon_ymax) {
		u16 num_dungeons = std::fmax(std::floor(
			NoiseFractal3D(&np_dungeons, node_min.X, node_min.Y, node_min.Z, seed)), 0.0f);

		if (num_dungeons >= 1) {
			PseudoRandom ps(blockseed + 4713);

			DungeonParams dp;

			dp.seed              = seed;
			dp.num_dungeons      = num_dungeons;
			dp.only_in_ground    = true;
			dp.corridor_len_min  = 1;
			dp.corridor_len_max  = 13;
			dp.num_rooms         = ps.range(2, 16);
			dp.large_room_chance = (ps.range(1, 4) == 1) ? 1 : 0;

			dp.np_alt_wall
				= NoiseParams(-0.4, 1.0, v3f(40.0, 40.0, 40.0), 32474, 6, 1.1, 2.0);

			if ((spflags & MGV6_TEMPLES) &&
					getBiome(0, v2s16(node_min.X, node_min.Z)) == BT_DESERT) {
				dp.c_wall              = c_desert_stone;
				dp.c_alt_wall          = CONTENT_IGNORE;
				dp.c_stair             = c_stair_desert_stone;

				dp.diagonal_dirs       = true;
				dp.holesize            = v3s16(2, 3, 2);
				dp.room_size_min       = v3s16(6, 9, 6);
				dp.room_size_max       = v3s16(10, 11, 10);
				dp.room_size_large_min = v3s16(10, 13, 10);
				dp.room_size_large_max = v3s16(18, 21, 18);
				dp.notifytype          = GENNOTIFY_TEMPLE;
			} else {
				dp.c_wall              = c_cobble;
				dp.c_alt_wall          = c_mossycobble;
				dp.c_stair             = c_stair_cobble;

				dp.diagonal_dirs       = false;
				dp.holesize            = v3s16(1, 2, 1);
				dp.room_size_min       = v3s16(4, 4, 4);
				dp.room_size_max       = v3s16(8, 6, 8);
				dp.room_size_large_min = v3s16(8, 8, 8);
				dp.room_size_large_max = v3s16(16, 16, 16);
				dp.notifytype          = GENNOTIFY_DUNGEON;
			}

			DungeonGen dgen(ndef, &gennotify, &dp);
			dgen.generate(vm, blockseed, full_node_min, full_node_max);
		}
	}

	// Add top and bottom side of water to transforming_liquid queue
	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	// Add surface nodes
	growGrass();

	// Generate some trees, and add grass, if a jungle
	if (spflags & MGV6_TREES)
		placeTreesAndJungleGrass();

	// Generate the registered decorations
	if (flags & MG_DECORATIONS)
		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

	// Generate the registered ores
	if (flags & MG_ORES)
		m_emerge->oremgr->placeAllOres(this, blockseed, node_min, node_max);

	// Calculate lighting
	if (flags & MG_LIGHT)
		calcLighting(node_min - v3s16(1, 1, 1) * MAP_BLOCKSIZE,
			node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE,
			full_node_min, full_node_max);

	this->generating = false;
}


void MapgenV6::calculateNoise()
{
	int x = node_min.X;
	int z = node_min.Z;
	int fx = full_node_min.X;
	int fz = full_node_min.Z;

	if (!(spflags & MGV6_FLAT)) {
		noise_terrain_base->noiseMap2D_PO(x, 0.5, z, 0.5);
		noise_terrain_higher->noiseMap2D_PO(x, 0.5, z, 0.5);
		noise_steepness->noiseMap2D_PO(x, 0.5, z, 0.5);
		noise_height_select->noiseMap2D_PO(x, 0.5, z, 0.5);
		noise_mud->noiseMap2D_PO(x, 0.5, z, 0.5);
	}

	noise_beach->noiseMap2D_PO(x, 0.2, z, 0.7);

	noise_biome->noiseMap2D_PO(fx, 0.6, fz, 0.2);
	noise_humidity->noiseMap2D_PO(fx, 0.0, fz, 0.0);
	// Humidity map does not need range limiting 0 to 1,
	// only humidity at point does
}


int MapgenV6::generateGround()
{
	//TimeTaker timer1("Generating ground level");
	MapNode n_air(CONTENT_AIR), n_water_source(c_water_source);
	MapNode n_stone(c_stone), n_desert_stone(c_desert_stone);
	MapNode n_ice(c_ice);
	int stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;

	u32 index = 0;
	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		// Surface height
		s16 surface_y = (s16)baseTerrainLevelFromMap(index);

		// Log it
		if (surface_y > stone_surface_max_y)
			stone_surface_max_y = surface_y;

		BiomeV6Type bt = getBiome(v2s16(x, z));

		// Fill ground with stone
		const v3s32 &em = vm->m_area.getExtent();
		u32 i = vm->m_area.index(x, node_min.Y, z);
		for (s16 y = node_min.Y; y <= node_max.Y; y++) {
			if (vm->m_data[i].getContent() == CONTENT_IGNORE) {
				if (y <= surface_y) {
					vm->m_data[i] = (y >= MGV6_DESERT_STONE_BASE
							&& bt == BT_DESERT) ?
						n_desert_stone : n_stone;
				} else if (y <= water_level) {
					vm->m_data[i] = (y >= MGV6_ICE_BASE
							&& bt == BT_TUNDRA) ?
						n_ice : n_water_source;
				} else {
					vm->m_data[i] = n_air;
				}
			}
			VoxelArea::add_y(em, i, 1);
		}
	}

	return stone_surface_max_y;
}


void MapgenV6::addMud()
{
	// 15ms @cs=8
	//TimeTaker timer1("add mud");
	MapNode n_dirt(c_dirt), n_gravel(c_gravel);
	MapNode n_sand(c_sand), n_desert_sand(c_desert_sand);
	MapNode addnode;

	u32 index = 0;
	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index++) {
		// Randomize mud amount
		s16 mud_add_amount = getMudAmount(index) / 2.0 + 0.5;

		// Find ground level
		s16 surface_y = find_stone_level(v2s16(x, z)); /////////////////optimize this!

		// Handle area not found
		if (surface_y == vm->m_area.MinEdge.Y - 1)
			continue;

		BiomeV6Type bt = getBiome(v2s16(x, z));
		addnode = (bt == BT_DESERT) ? n_desert_sand : n_dirt;

		if (bt == BT_DESERT && surface_y + mud_add_amount <= water_level + 1) {
			addnode = n_sand;
		} else if (mud_add_amount <= 0) {
			mud_add_amount = 1 - mud_add_amount;
			addnode = n_gravel;
		} else if (bt != BT_DESERT && getHaveBeach(index) &&
				surface_y + mud_add_amount <= water_level + 2) {
			addnode = n_sand;
		}

		if ((bt == BT_DESERT || bt == BT_TUNDRA) && surface_y > 20)
			mud_add_amount = MYMAX(0, mud_add_amount - (surface_y - 20) / 5);

		/* If topmost node is grass, change it to mud.  It might be if it was
		// flown to there from a neighboring chunk and then converted.
		u32 i = vm->m_area.index(x, surface_y, z);
		if (vm->m_data[i].getContent() == c_dirt_with_grass)
			vm->m_data[i] = n_dirt;*/

		// Add mud on ground
		s16 mudcount = 0;
		const v3s32 &em = vm->m_area.getExtent();
		s16 y_start = surface_y + 1;
		u32 i = vm->m_area.index(x, y_start, z);
		for (s16 y = y_start; y <= node_max.Y; y++) {
			if (mudcount >= mud_add_amount)
				break;

			vm->m_data[i] = addnode;
			mudcount++;

			VoxelArea::add_y(em, i, 1);
		}
	}
}


void MapgenV6::flowMud(s16 &mudflow_minpos, s16 &mudflow_maxpos)
{
	const v3s32 &em = vm->m_area.getExtent();
	static const v3s16 dirs4[4] = {
		v3s16(0, 0, 1), // Back
		v3s16(1, 0, 0), // Right
		v3s16(0, 0, -1), // Front
		v3s16(-1, 0, 0), // Left
	};

	// Iterate twice
	for (s16 k = 0; k < 2; k++) {
		for (s16 z = mudflow_minpos; z <= mudflow_maxpos; z++)
		for (s16 x = mudflow_minpos; x <= mudflow_maxpos; x++) {
			// Node column position
			v2s16 p2d;
			// Invert coordinates on second iteration to process columns in
			// opposite order, to avoid a directional bias.
			if (k == 1)
				p2d = v2s16(node_max.X, node_max.Z) - v2s16(x, z);
			else
				p2d = v2s16(node_min.X, node_min.Z) + v2s16(x, z);

			s16 y = node_max.Y;

			while (y >= node_min.Y) {
				for (;; y--) {
					u32 i = vm->m_area.index(p2d.X, y, p2d.Y);
					MapNode *n = nullptr;

					// Find next mud node in mapchunk column
					for (; y >= node_min.Y; y--) {
						n = &vm->m_data[i];
						if (n->getContent() == c_dirt ||
								n->getContent() == c_dirt_with_grass ||
								n->getContent() == c_gravel)
							break;

						VoxelArea::add_y(em, i, -1);
					}
					if (y < node_min.Y)
						// No mud found in mapchunk column, process the next column
						break;

					if (n->getContent() == c_dirt || n->getContent() == c_dirt_with_grass) {
						// Convert dirt_with_grass to dirt
						n->setContent(c_dirt);
						// Don't flow mud if the stuff under it is not mud,
						// to leave at least 1 node of mud.
						u32 i2 = i;
						VoxelArea::add_y(em, i2, -1);
						MapNode *n2 = &vm->m_data[i2];
						if (n2->getContent() != c_dirt &&
								n2->getContent() != c_dirt_with_grass)
							// Find next mud node in column
							continue;
					}

					// Check if node above is walkable. If so, cancel
					// flowing as if node above keeps it in place.
					u32 i3 = i;
					VoxelArea::add_y(em, i3, 1);
					MapNode *n3 = &vm->m_data[i3];
					if (ndef->get(*n3).walkable)
						// Find next mud node in column
						continue;

					// Drop mud on one side
					for (const v3s16 &dirp : dirs4) {
						u32 i2 = i;
						// Move to side
						VoxelArea::add_p(em, i2, dirp);
						// Check that side is air
						MapNode *n2 = &vm->m_data[i2];
						if (ndef->get(*n2).walkable)
							continue;

						// Check that under side is air
						VoxelArea::add_y(em, i2, -1);
						n2 = &vm->m_data[i2];
						if (ndef->get(*n2).walkable)
							continue;

						// Loop further down until not air
						s16 y2 = y - 1; // y of i2
						bool dropped_to_unknown = false;
						do {
							y2--;
							VoxelArea::add_y(em, i2, -1);
							n2 = &vm->m_data[i2];
							// If out of area or in ungenerated world
							if (y2 < full_node_min.Y || n2->getContent() == CONTENT_IGNORE) {
								dropped_to_unknown = true;
								break;
							}
						} while (!ndef->get(*n2).walkable);

						if (!dropped_to_unknown) {
							// Move up one so that we're in air
							VoxelArea::add_y(em, i2, 1);
							// Move mud to new place, and if outside mapchunk remove
							// any decorations above removed or placed mud.
							moveMud(i, i2, i3, p2d, em);
						}
						// Done, find next mud node in column
						break;
					}
				}
			}
		}
	}
}


void MapgenV6::moveMud(u32 remove_index, u32 place_index,
	u32 above_remove_index, v2s16 pos, v3s32 em)
{
	MapNode n_air(CONTENT_AIR);
	// Copy mud from old place to new place
	vm->m_data[place_index] = vm->m_data[remove_index];
	// Set old place to be air
	vm->m_data[remove_index] = n_air;
	// Outside the mapchunk decorations may need to be removed if above removed
	// mud or if half-buried in placed mud. Placed mud is to the side of pos so
	// use 'pos.X >= node_max.X' etc.
	if (pos.X >= node_max.X || pos.X <= node_min.X ||
			pos.Y >= node_max.Z || pos.Y <= node_min.Z) {
		// 'above remove' node is above removed mud. If it is not air, water or
		// 'ignore' it is a decoration that needs removing. Also search upwards
		// to remove a possible stacked decoration.
		// Check for 'ignore' because stacked decorations can penetrate into
		// 'ignore' nodes above the mapchunk.
		while (vm->m_area.contains(above_remove_index) &&
				vm->m_data[above_remove_index].getContent() != CONTENT_AIR &&
				vm->m_data[above_remove_index].getContent() != c_water_source &&
				vm->m_data[above_remove_index].getContent() != CONTENT_IGNORE) {
			vm->m_data[above_remove_index] = n_air;
			VoxelArea::add_y(em, above_remove_index, 1);
		}
		// Mud placed may have partially-buried a stacked decoration, search
		// above and remove.
		VoxelArea::add_y(em, place_index, 1);
		while (vm->m_area.contains(place_index) &&
				vm->m_data[place_index].getContent() != CONTENT_AIR &&
				vm->m_data[place_index].getContent() != c_water_source &&
				vm->m_data[place_index].getContent() != CONTENT_IGNORE) {
			vm->m_data[place_index] = n_air;
			VoxelArea::add_y(em, place_index, 1);
		}
	}
}


void MapgenV6::placeTreesAndJungleGrass()
{
	//TimeTaker t("placeTrees");
	if (node_max.Y < water_level)
		return;

	PseudoRandom grassrandom(blockseed + 53);
	content_t c_junglegrass = ndef->getId("mapgen_junglegrass");
	// if we don't have junglegrass, don't place cignore... that's bad
	if (c_junglegrass == CONTENT_IGNORE)
		c_junglegrass = CONTENT_AIR;
	MapNode n_junglegrass(c_junglegrass);
	const v3s32 &em = vm->m_area.getExtent();

	// Divide area into parts
	s16 div = 8;
	s16 sidelen = central_area_size.X / div;
	double area = sidelen * sidelen;

	// N.B.  We must add jungle grass first, since tree leaves will
	// obstruct the ground, giving us a false ground level
	for (s16 z0 = 0; z0 < div; z0++)
	for (s16 x0 = 0; x0 < div; x0++) {
		// Center position of part of division
		v2s16 p2d_center(
			node_min.X + sidelen / 2 + sidelen * x0,
			node_min.Z + sidelen / 2 + sidelen * z0
		);
		// Minimum edge of part of division
		v2s16 p2d_min(
			node_min.X + sidelen * x0,
			node_min.Z + sidelen * z0
		);
		// Maximum edge of part of division
		v2s16 p2d_max(
			node_min.X + sidelen + sidelen * x0 - 1,
			node_min.Z + sidelen + sidelen * z0 - 1
		);

		// Get biome at center position of part of division
		BiomeV6Type bt = getBiome(p2d_center);

		// Amount of trees
		u32 tree_count;
		if (bt == BT_JUNGLE || bt == BT_TAIGA || bt == BT_NORMAL) {
			tree_count = area * getTreeAmount(p2d_center);
			if (bt == BT_JUNGLE)
				tree_count *= 4;
		} else {
			tree_count = 0;
		}

		// Add jungle grass
		if (bt == BT_JUNGLE) {
			float humidity = getHumidity(p2d_center);
			u32 grass_count = 5 * humidity * tree_count;
			for (u32 i = 0; i < grass_count; i++) {
				s16 x = grassrandom.range(p2d_min.X, p2d_max.X);
				s16 z = grassrandom.range(p2d_min.Y, p2d_max.Y);
				int mapindex = central_area_size.X * (z - node_min.Z)
								+ (x - node_min.X);
				s16 y = heightmap[mapindex];
				if (y < water_level)
					continue;

				u32 vi = vm->m_area.index(x, y, z);
				// place on dirt_with_grass, since we know it is exposed to sunlight
				if (vm->m_data[vi].getContent() == c_dirt_with_grass) {
					VoxelArea::add_y(em, vi, 1);
					vm->m_data[vi] = n_junglegrass;
				}
			}
		}

		// Put trees in random places on part of division
		for (u32 i = 0; i < tree_count; i++) {
			s16 x = myrand_range(p2d_min.X, p2d_max.X);
			s16 z = myrand_range(p2d_min.Y, p2d_max.Y);
			int mapindex = central_area_size.X * (z - node_min.Z)
							+ (x - node_min.X);
			s16 y = heightmap[mapindex];
			// Don't make a tree under water level
			// Don't make a tree so high that it doesn't fit
			if (y < water_level || y > node_max.Y - 6)
				continue;

			v3s16 p(x, y, z);
			// Trees grow only on mud and grass
			{
				u32 i = vm->m_area.index(p);
				content_t c = vm->m_data[i].getContent();
				if (c != c_dirt &&
						c != c_dirt_with_grass &&
						c != c_dirt_with_snow)
					continue;
			}
			p.Y++;

			// Make a tree
			if (bt == BT_JUNGLE) {
				treegen::make_jungletree(*vm, p, ndef, myrand());
			} else if (bt == BT_TAIGA) {
				treegen::make_pine_tree(*vm, p - v3s16(0, 1, 0), ndef, myrand());
			} else if (bt == BT_NORMAL) {
				bool is_apple_tree = (myrand_range(0, 3) == 0) &&
							getHaveAppleTree(v2s16(x, z));
				treegen::make_tree(*vm, p, is_apple_tree, ndef, myrand());
			}
		}
	}
	//printf("placeTreesAndJungleGrass: %dms\n", t.stop());
}


void MapgenV6::growGrass() // Add surface nodes
{
	MapNode n_dirt_with_grass(c_dirt_with_grass);
	MapNode n_dirt_with_snow(c_dirt_with_snow);
	MapNode n_snowblock(c_snowblock);
	const v3s32 &em = vm->m_area.getExtent();

	u32 index = 0;
	for (s16 z = full_node_min.Z; z <= full_node_max.Z; z++)
	for (s16 x = full_node_min.X; x <= full_node_max.X; x++, index++) {
		// Find the lowest surface to which enough light ends up to make
		// grass grow.  Basically just wait until not air and not leaves.
		s16 surface_y = 0;
		{
			u32 i = vm->m_area.index(x, node_max.Y, z);
			s16 y;
			// Go to ground level
			for (y = node_max.Y; y >= full_node_min.Y; y--) {
				MapNode &n = vm->m_data[i];
				if (ndef->get(n).param_type != CPT_LIGHT ||
						ndef->get(n).liquid_type != LIQUID_NONE ||
						n.getContent() == c_ice)
					break;
				VoxelArea::add_y(em, i, -1);
			}
			surface_y = (y >= full_node_min.Y) ? y : full_node_min.Y;
		}

		BiomeV6Type bt = getBiome(index, v2s16(x, z));
		u32 i = vm->m_area.index(x, surface_y, z);
		content_t c = vm->m_data[i].getContent();
		if (surface_y >= water_level - 20) {
			if (bt == BT_TAIGA && c == c_dirt) {
				vm->m_data[i] = n_dirt_with_snow;
			} else if (bt == BT_TUNDRA) {
				if (c == c_dirt) {
					vm->m_data[i] = n_snowblock;
					VoxelArea::add_y(em, i, -1);
					vm->m_data[i] = n_dirt_with_snow;
				} else if (c == c_stone && surface_y < node_max.Y) {
					VoxelArea::add_y(em, i, 1);
					vm->m_data[i] = n_snowblock;
				}
			} else if (c == c_dirt) {
				vm->m_data[i] = n_dirt_with_grass;
			}
		}
	}
}


void MapgenV6::generateCaves(int max_stone_y)
{
	float cave_amount = NoiseFractal2D(np_cave, node_min.X, node_min.Y, seed);
	int volume_nodes = (node_max.X - node_min.X + 1) *
					   (node_max.Y - node_min.Y + 1) * MAP_BLOCKSIZE;
	cave_amount = MYMAX(0.0, cave_amount);
	u32 caves_count = cave_amount * volume_nodes / 50000;
	u32 bruises_count = 1;
	PseudoRandom ps(blockseed + 21343);
	PseudoRandom ps2(blockseed + 1032);

	if (ps.range(1, 6) == 1)
		bruises_count = ps.range(0, ps.range(0, 2));

	if (getBiome(v2s16(node_min.X, node_min.Z)) == BT_DESERT) {
		caves_count   /= 3;
		bruises_count /= 3;
	}

	for (u32 i = 0; i < caves_count + bruises_count; i++) {
		CavesV6 cave(ndef, &gennotify, water_level, c_water_source, c_lava_source);

		bool large_cave = (i >= caves_count);
		cave.makeCave(vm, node_min, node_max, &ps, &ps2,
			large_cave, max_stone_y, heightmap);
	}
}
