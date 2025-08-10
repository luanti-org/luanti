// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015-2020 paramat
// Copyright (C) 2015-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>


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
#include "mapgen_flat.h"


const FlagDesc flagdesc_mapgen_flat[] = {
	{"lakes",   MGFLAT_LAKES},
	{"hills",   MGFLAT_HILLS},
	{"caverns", MGFLAT_CAVERNS},
	{NULL,    0}
};

///////////////////////////////////////////////////////////////////////////////////////


MapgenFlat::MapgenFlat(MapgenFlatParams *params, EmergeParams *emerge)
	: MapgenBasic(MAPGEN_FLAT, params, emerge)
{
	spflags            = params->spflags;
	ground_level       = params->ground_level;
	lake_threshold     = params->lake_threshold;
	lake_steepness     = params->lake_steepness;
	hill_threshold     = params->hill_threshold;
	hill_steepness     = params->hill_steepness;

	cave_width         = params->cave_width;
	small_cave_num_min = params->small_cave_num_min;
	small_cave_num_max = params->small_cave_num_max;
	large_cave_num_min = params->large_cave_num_min;
	large_cave_num_max = params->large_cave_num_max;
	large_cave_depth   = params->large_cave_depth;
	large_cave_flooded = params->large_cave_flooded;
	cavern_limit       = params->cavern_limit;
	cavern_taper       = params->cavern_taper;
	cavern_threshold   = params->cavern_threshold;
	dungeon_ymin       = params->dungeon_ymin;
	dungeon_ymax       = params->dungeon_ymax;

	// 2D noise
	noise_filler_depth = new Noise(&params->np_filler_depth, seed, csize.X, csize.Z);

	if ((spflags & MGFLAT_LAKES) || (spflags & MGFLAT_HILLS))
		noise_terrain = new Noise(&params->np_terrain, seed, csize.X, csize.Z);

	// 3D noise
	MapgenBasic::np_cave1    = params->np_cave1;
	MapgenBasic::np_cave2    = params->np_cave2;
	MapgenBasic::np_cavern   = params->np_cavern;
	MapgenBasic::np_dungeons = params->np_dungeons;
}


MapgenFlat::~MapgenFlat()
{
	delete noise_filler_depth;

	if ((spflags & MGFLAT_LAKES) || (spflags & MGFLAT_HILLS))
		delete noise_terrain;
}


MapgenFlatParams::MapgenFlatParams():
	np_terrain      (0,   1,   v3f(600, 600, 600), 7244,  5, 0.6,  2.0),
	np_filler_depth (0,   1.2, v3f(150, 150, 150), 261,   3, 0.7,  2.0),
	np_cavern       (0.0, 1.0, v3f(384, 128, 384), 723,   5, 0.63, 2.0),
	np_cave1        (0,   12,  v3f(61,  61,  61),  52534, 3, 0.5,  2.0),
	np_cave2        (0,   12,  v3f(67,  67,  67),  10325, 3, 0.5,  2.0),
	np_dungeons     (0.9, 0.5, v3f(500, 500, 500), 0,     2, 0.8,  2.0)
{
}


void MapgenFlatParams::readParams(const Settings *settings)
{
	settings->getFlagStrNoEx("mgflat_spflags", spflags, flagdesc_mapgen_flat);
	settings->getS16NoEx("mgflat_ground_level",         ground_level);
	settings->getS16NoEx("mgflat_large_cave_depth",     large_cave_depth);
	settings->getU16NoEx("mgflat_small_cave_num_min",   small_cave_num_min);
	settings->getU16NoEx("mgflat_small_cave_num_max",   small_cave_num_max);
	settings->getU16NoEx("mgflat_large_cave_num_min",   large_cave_num_min);
	settings->getU16NoEx("mgflat_large_cave_num_max",   large_cave_num_max);
	settings->getFloatNoEx("mgflat_large_cave_flooded", large_cave_flooded);
	settings->getFloatNoEx("mgflat_cave_width",         cave_width);
	settings->getFloatNoEx("mgflat_lake_threshold",     lake_threshold);
	settings->getFloatNoEx("mgflat_lake_steepness",     lake_steepness);
	settings->getFloatNoEx("mgflat_hill_threshold",     hill_threshold);
	settings->getFloatNoEx("mgflat_hill_steepness",     hill_steepness);
	settings->getS16NoEx("mgflat_cavern_limit",         cavern_limit);
	settings->getS16NoEx("mgflat_cavern_taper",         cavern_taper);
	settings->getFloatNoEx("mgflat_cavern_threshold",   cavern_threshold);
	settings->getS16NoEx("mgflat_dungeon_ymin",         dungeon_ymin);
	settings->getS16NoEx("mgflat_dungeon_ymax",         dungeon_ymax);

	settings->getNoiseParams("mgflat_np_terrain",      np_terrain);
	settings->getNoiseParams("mgflat_np_filler_depth", np_filler_depth);
	settings->getNoiseParams("mgflat_np_cavern",       np_cavern);
	settings->getNoiseParams("mgflat_np_cave1",        np_cave1);
	settings->getNoiseParams("mgflat_np_cave2",        np_cave2);
	settings->getNoiseParams("mgflat_np_dungeons",     np_dungeons);
}


void MapgenFlatParams::writeParams(Settings *settings) const
{
	settings->setFlagStr("mgflat_spflags", spflags, flagdesc_mapgen_flat);
	settings->setS16("mgflat_ground_level",         ground_level);
	settings->setS16("mgflat_large_cave_depth",     large_cave_depth);
	settings->setU16("mgflat_small_cave_num_min",   small_cave_num_min);
	settings->setU16("mgflat_small_cave_num_max",   small_cave_num_max);
	settings->setU16("mgflat_large_cave_num_min",   large_cave_num_min);
	settings->setU16("mgflat_large_cave_num_max",   large_cave_num_max);
	settings->setFloat("mgflat_large_cave_flooded", large_cave_flooded);
	settings->setFloat("mgflat_cave_width",         cave_width);
	settings->setFloat("mgflat_lake_threshold",     lake_threshold);
	settings->setFloat("mgflat_lake_steepness",     lake_steepness);
	settings->setFloat("mgflat_hill_threshold",     hill_threshold);
	settings->setFloat("mgflat_hill_steepness",     hill_steepness);
	settings->setS16("mgflat_cavern_limit",         cavern_limit);
	settings->setS16("mgflat_cavern_taper",         cavern_taper);
	settings->setFloat("mgflat_cavern_threshold",   cavern_threshold);
	settings->setS16("mgflat_dungeon_ymin",         dungeon_ymin);
	settings->setS16("mgflat_dungeon_ymax",         dungeon_ymax);

	settings->setNoiseParams("mgflat_np_terrain",      np_terrain);
	settings->setNoiseParams("mgflat_np_filler_depth", np_filler_depth);
	settings->setNoiseParams("mgflat_np_cavern",       np_cavern);
	settings->setNoiseParams("mgflat_np_cave1",        np_cave1);
	settings->setNoiseParams("mgflat_np_cave2",        np_cave2);
	settings->setNoiseParams("mgflat_np_dungeons",     np_dungeons);
}


void MapgenFlatParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgflat_spflags", flagdesc_mapgen_flat, 0);
}


/////////////////////////////////////////////////////////////////


int MapgenFlat::getSpawnLevelAtPoint(v2s16 p)
{
	s16 stone_level = ground_level;
	float n_terrain =
		((spflags & MGFLAT_LAKES) || (spflags & MGFLAT_HILLS)) ?
		NoiseFractal2D(&noise_terrain->np, p.X, p.Y, seed) :
		0.0f;

	if ((spflags & MGFLAT_LAKES) && n_terrain < lake_threshold) {
		s16 depress = (lake_threshold - n_terrain) * lake_steepness;
		stone_level = ground_level - depress;
	} else if ((spflags & MGFLAT_HILLS) && n_terrain > hill_threshold) {
		s16 rise = (n_terrain - hill_threshold) * hill_steepness;
		stone_level = ground_level + rise;
	}

	if (ground_level < water_level)
		// Ocean world, may not have islands so allow spawn in water
		return MYMAX(stone_level + 2, water_level);

	if (stone_level >= water_level)
		// Spawn on land
		// + 2 not + 1, to spawn above biome 'dust' nodes
		return stone_level + 2;

	// Unsuitable spawn point
	return MAX_MAP_GENERATION_LIMIT;
}


void MapgenFlat::makeChunk(BlockMakeData *data)
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

	blockseed = getBlockSeed2(full_node_min, seed);

	// Generate base terrain, mountains, and ridges with initial heightmaps
	s16 stone_surface_max_y = generateTerrain();

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
		if (spflags & MGFLAT_CAVERNS)
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

	if (flags & MG_DUNGEONS)
		generateDungeons(stone_surface_max_y);

	// Generate the registered decorations
	if (flags & MG_DECORATIONS)
		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

	// Sprinkle some dust on top after everything else was generated
	if (flags & MG_BIOMES)
		dustTopNodes();

	//printf("makeChunk: %dms\n", t.stop());

	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	if (flags & MG_LIGHT)
		calcLighting(node_min - v3s16(0, 1, 0), node_max + v3s16(0, 1, 0),
			full_node_min, full_node_max);

	//setLighting(node_min - v3s16(1, 0, 1) * MAP_BLOCKSIZE,
	//			node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE, 0xFF);

	this->generating = false;
}


s16 MapgenFlat::generateTerrain()
{
	MapNode n_air(CONTENT_AIR);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);

	const v3s32 &em = vm->m_area.getExtent();
	s16 stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	u32 ni2d = 0;

	bool use_noise = (spflags & MGFLAT_LAKES) || (spflags & MGFLAT_HILLS);
	if (use_noise)
		noise_terrain->noiseMap2D(node_min.X, node_min.Z);

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, ni2d++) {
		s16 stone_level = ground_level;
		float n_terrain = use_noise ? noise_terrain->result[ni2d] : 0.0f;

		if ((spflags & MGFLAT_LAKES) && n_terrain < lake_threshold) {
			s16 depress = (lake_threshold - n_terrain) * lake_steepness;
			stone_level = ground_level - depress;
		} else if ((spflags & MGFLAT_HILLS) && n_terrain > hill_threshold) {
			s16 rise = (n_terrain - hill_threshold) * hill_steepness;
			stone_level = ground_level + rise;
		}

		u32 vi = vm->m_area.index(x, node_min.Y - 1, z);
		for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
			if (vm->m_data[vi].getContent() == CONTENT_IGNORE) {
				if (y <= stone_level) {
					vm->m_data[vi] = n_stone;
					if (y > stone_surface_max_y)
						stone_surface_max_y = y;
				} else if (y <= water_level) {
					vm->m_data[vi] = n_water;
				} else {
					vm->m_data[vi] = n_air;
				}
			}
			VoxelArea::add_y(em, vi, 1);
		}
	}

	return stone_surface_max_y;
}
