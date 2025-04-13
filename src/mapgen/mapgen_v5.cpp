// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2018 paramat
// Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>


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
#include "mg_biome.h"
#include "mg_ore.h"
#include "mg_decoration.h"
#include "mapgen_v5.h"


const FlagDesc flagdesc_mapgen_v5[] = {
	{"caverns", MGV5_CAVERNS},
	{NULL,      0}
};


MapgenV5::MapgenV5(MapgenV5Params *params, EmergeParams *emerge)
	: MapgenBasic(MAPGEN_V5, params, emerge)
{
	spflags            = params->spflags;
	cave_width         = params->cave_width;
	large_cave_depth   = params->large_cave_depth;
	small_cave_num_min = params->small_cave_num_min;
	small_cave_num_max = params->small_cave_num_max;
	large_cave_num_min = params->large_cave_num_min;
	large_cave_num_max = params->large_cave_num_max;
	large_cave_flooded = params->large_cave_flooded;
	cavern_limit       = params->cavern_limit;
	cavern_taper       = params->cavern_taper;
	cavern_threshold   = params->cavern_threshold;
	dungeon_ymin       = params->dungeon_ymin;
	dungeon_ymax       = params->dungeon_ymax;

	// Terrain noise
	noise_filler_depth = new Noise(&params->np_filler_depth, seed, csize.X, csize.Z);
	noise_factor       = new Noise(&params->np_factor,       seed, csize.X, csize.Z);
	noise_height       = new Noise(&params->np_height,       seed, csize.X, csize.Z);

	// 3D terrain noise
	// 1-up 1-down overgeneration
	noise_ground = new Noise(&params->np_ground, seed, csize.X, csize.Y + 2, csize.Z);
	// 1 down overgeneration
	MapgenBasic::np_cave1    = params->np_cave1;
	MapgenBasic::np_cave2    = params->np_cave2;
	MapgenBasic::np_cavern   = params->np_cavern;
	MapgenBasic::np_dungeons = params->np_dungeons;
}


MapgenV5::~MapgenV5()
{
	delete noise_filler_depth;
	delete noise_factor;
	delete noise_height;
	delete noise_ground;
}


MapgenV5Params::MapgenV5Params():
	np_filler_depth (0,   1,   v3f(150, 150, 150), 261,    4, 0.7,  2.0),
	np_factor       (0,   1,   v3f(250, 250, 250), 920381, 3, 0.45, 2.0),
	np_height       (0,   10,  v3f(250, 250, 250), 84174,  4, 0.5,  2.0),
	np_ground       (0,   40,  v3f(80,  80,  80),  983240, 4, 0.55, 2.0, NOISE_FLAG_EASED),
	np_cave1        (0,   12,  v3f(61,  61,  61),  52534,  3, 0.5,  2.0),
	np_cave2        (0,   12,  v3f(67,  67,  67),  10325,  3, 0.5,  2.0),
	np_cavern       (0,   1,   v3f(384, 128, 384), 723,    5, 0.63, 2.0),
	np_dungeons     (0.9, 0.5, v3f(500, 500, 500), 0,      2, 0.8,  2.0)
{
}


void MapgenV5Params::readParams(const Settings *settings)
{
	settings->getFlagStrNoEx("mgv5_spflags", spflags, flagdesc_mapgen_v5);
	settings->getFloatNoEx("mgv5_cave_width",         cave_width);
	settings->getS16NoEx("mgv5_large_cave_depth",     large_cave_depth);
	settings->getU16NoEx("mgv5_small_cave_num_min",   small_cave_num_min);
	settings->getU16NoEx("mgv5_small_cave_num_max",   small_cave_num_max);
	settings->getU16NoEx("mgv5_large_cave_num_min",   large_cave_num_min);
	settings->getU16NoEx("mgv5_large_cave_num_max",   large_cave_num_max);
	settings->getFloatNoEx("mgv5_large_cave_flooded", large_cave_flooded);
	settings->getS16NoEx("mgv5_cavern_limit",         cavern_limit);
	settings->getS16NoEx("mgv5_cavern_taper",         cavern_taper);
	settings->getFloatNoEx("mgv5_cavern_threshold",   cavern_threshold);
	settings->getS16NoEx("mgv5_dungeon_ymin",         dungeon_ymin);
	settings->getS16NoEx("mgv5_dungeon_ymax",         dungeon_ymax);

	settings->getNoiseParams("mgv5_np_filler_depth", np_filler_depth);
	settings->getNoiseParams("mgv5_np_factor",       np_factor);
	settings->getNoiseParams("mgv5_np_height",       np_height);
	settings->getNoiseParams("mgv5_np_ground",       np_ground);
	settings->getNoiseParams("mgv5_np_cave1",        np_cave1);
	settings->getNoiseParams("mgv5_np_cave2",        np_cave2);
	settings->getNoiseParams("mgv5_np_cavern",       np_cavern);
	settings->getNoiseParams("mgv5_np_dungeons",     np_dungeons);
}


void MapgenV5Params::writeParams(Settings *settings) const
{
	settings->setFlagStr("mgv5_spflags", spflags, flagdesc_mapgen_v5);
	settings->setFloat("mgv5_cave_width",         cave_width);
	settings->setS16("mgv5_large_cave_depth",     large_cave_depth);
	settings->setU16("mgv5_small_cave_num_min",   small_cave_num_min);
	settings->setU16("mgv5_small_cave_num_max",   small_cave_num_max);
	settings->setU16("mgv5_large_cave_num_min",   large_cave_num_min);
	settings->setU16("mgv5_large_cave_num_max",   large_cave_num_max);
	settings->setFloat("mgv5_large_cave_flooded", large_cave_flooded);
	settings->setS16("mgv5_cavern_limit",         cavern_limit);
	settings->setS16("mgv5_cavern_taper",         cavern_taper);
	settings->setFloat("mgv5_cavern_threshold",   cavern_threshold);
	settings->setS16("mgv5_dungeon_ymin",         dungeon_ymin);
	settings->setS16("mgv5_dungeon_ymax",         dungeon_ymax);

	settings->setNoiseParams("mgv5_np_filler_depth", np_filler_depth);
	settings->setNoiseParams("mgv5_np_factor",       np_factor);
	settings->setNoiseParams("mgv5_np_height",       np_height);
	settings->setNoiseParams("mgv5_np_ground",       np_ground);
	settings->setNoiseParams("mgv5_np_cave1",        np_cave1);
	settings->setNoiseParams("mgv5_np_cave2",        np_cave2);
	settings->setNoiseParams("mgv5_np_cavern",       np_cavern);
	settings->setNoiseParams("mgv5_np_dungeons",     np_dungeons);
}


void MapgenV5Params::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgv5_spflags", flagdesc_mapgen_v5, MGV5_CAVERNS);
}


/////////////////////////////////////////////////////////////////


int MapgenV5::getSpawnLevelAtPoint(v2s16 p)
{

	float f = 0.55 + NoiseFractal2D(&noise_factor->np, p.X, p.Y, seed);
	if (f < 0.01)
		f = 0.01;
	else if (f >= 1.0)
		f *= 1.6;
	float h = NoiseFractal2D(&noise_height->np, p.X, p.Y, seed);

	// noise_height 'offset' is the average level of terrain. At least 50% of
	// terrain will be below this.
	// Raising the maximum spawn level above 'water_level + 16' is necessary
	// for when noise_height 'offset' is set much higher than water_level.
	s16 max_spawn_y = MYMAX(noise_height->np.offset, water_level + 16);

	// Starting spawn search at max_spawn_y + 128 ensures 128 nodes of open
	// space above spawn position. Avoids spawning in possibly sealed voids.
	for (s16 y = max_spawn_y + 128; y >= water_level; y--) {
		float n_ground = NoiseFractal3D(&noise_ground->np, p.X, y, p.Y, seed);

		if (n_ground * f > y - h) {  // If solid
			if (y < water_level || y > max_spawn_y)
				return MAX_MAP_GENERATION_LIMIT;  // Unsuitable spawn point

			// y + 2 because y is surface and due to biome 'dust' nodes.
			return y + 2;
		}
	}
	// Unsuitable spawn position, no ground found
	return MAX_MAP_GENERATION_LIMIT;
}


void MapgenV5::makeChunk(BlockMakeData *data)
{
	// Pre-conditions
	assert(data->vmanip);
	assert(data->nodedef);

	this->generating = true;
	this->vm   = data->vmanip;
	this->ndef = data->nodedef;
	//TimeTaker t("makeChunk");

	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	// Create a block-specific seed
	blockseed = getBlockSeed2(full_node_min, seed);

	// Generate base terrain
	s16 stone_surface_max_y = generateBaseTerrain();

	// Create heightmap
	updateHeightmap(node_min, node_max);

	// Init biome generator, place biome-specific nodes, and build biomemap
	if (flags & MG_BIOMES) {
		biomegen->calcBiomeNoise(node_min);
		generateBiomes();
	}

	// Generate tunnels, caverns and large randomwalk caves
	if (flags & MG_CAVES) {
		// Generate tunnels first as caverns confuse them
		generateCavesNoiseIntersection(stone_surface_max_y);

		// Generate caverns
		bool near_cavern = false;
		if (spflags & MGV5_CAVERNS)
			near_cavern = generateCavernsNoise(stone_surface_max_y);

		// Generate large randomwalk caves
		if (near_cavern)
			// Disable large randomwalk caves in this mapchunk by setting
			// 'large cave depth' to world base. Avoids excessive liquid in
			// large caverns and floating blobs of overgenerated liquid.
			generateCavesRandomWalk(stone_surface_max_y,
				-MAX_MAP_GENERATION_LIMIT);
		else
			generateCavesRandomWalk(stone_surface_max_y, large_cave_depth);
	}

	// Generate the registered ores
	if (flags & MG_ORES)
		m_emerge->oremgr->placeAllOres(this, blockseed, node_min, node_max);

	// Generate dungeons and desert temples
	if (flags & MG_DUNGEONS)
		generateDungeons(stone_surface_max_y);

	// Generate the registered decorations
	if (flags & MG_DECORATIONS)
		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

	// Sprinkle some dust on top after everything else was generated
	if (flags & MG_BIOMES)
		dustTopNodes();

	//printf("makeChunk: %dms\n", t.stop());

	// Add top and bottom side of water to transforming_liquid queue
	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	// Calculate lighting
	if (flags & MG_LIGHT) {
		calcLighting(node_min - v3s16(0, 1, 0), node_max + v3s16(0, 1, 0),
			full_node_min, full_node_max);
	}

	this->generating = false;
}


int MapgenV5::generateBaseTerrain()
{
	u32 index = 0;
	u32 index2d = 0;
	int stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;

	noise_factor->noiseMap2D(node_min.X, node_min.Z);
	noise_height->noiseMap2D(node_min.X, node_min.Z);
	noise_ground->noiseMap3D(node_min.X, node_min.Y - 1, node_min.Z);

	for (s16 z=node_min.Z; z<=node_max.Z; z++) {
		for (s16 y=node_min.Y - 1; y<=node_max.Y + 1; y++) {
			u32 vi = vm->m_area.index(node_min.X, y, z);
			for (s16 x=node_min.X; x<=node_max.X; x++, vi++, index++, index2d++) {
				if (vm->m_data[vi].getContent() != CONTENT_IGNORE)
					continue;

				float f = 0.55 + noise_factor->result[index2d];
				if (f < 0.01)
					f = 0.01;
				else if (f >= 1.0)
					f *= 1.6;
				float h = noise_height->result[index2d];

				if (noise_ground->result[index] * f < y - h) {
					if (y <= water_level)
						vm->m_data[vi] = MapNode(c_water_source);
					else
						vm->m_data[vi] = MapNode(CONTENT_AIR);
				} else {
					vm->m_data[vi] = MapNode(c_stone);
					if (y > stone_surface_max_y)
						stone_surface_max_y = y;
				}
			}
			index2d -= ystride;
		}
		index2d += ystride;
	}

	return stone_surface_max_y;
}
