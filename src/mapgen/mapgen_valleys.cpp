/*
Luanti
SPDX-License-Identifier: LGPL-2.1-or-later
Copyright (C) 2016-2019 Duane Robertson <duane@duanerobertson.com>
Copyright (C) 2016-2019 paramat

Based on Valleys Mapgen by Gael de Sailly
(https://forum.luanti.org/viewtopic.php?f=9&t=11430)
and mapgen_v7, mapgen_flat by kwolekr and paramat.

Licensing changed by permission of Gael de Sailly.
*/


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
#include "mg_biome.h"
#include "mg_ore.h"
#include "mg_decoration.h"
#include "mapgen_valleys.h"
#include "cavegen.h"
#include <cmath>


const FlagDesc flagdesc_mapgen_valleys[] = {
	{"altitude_chill",   MGVALLEYS_ALT_CHILL},
	{"humid_rivers",     MGVALLEYS_HUMID_RIVERS},
	{"vary_river_depth", MGVALLEYS_VARY_RIVER_DEPTH},
	{"altitude_dry",     MGVALLEYS_ALT_DRY},
	{NULL,               0}
};


MapgenValleys::MapgenValleys(MapgenValleysParams *params, EmergeParams *emerge)
	: MapgenBasic(MAPGEN_VALLEYS, params, emerge)
{
	FATAL_ERROR_IF(biomegen->getType() != BIOMEGEN_ORIGINAL,
		"MapgenValleys has a hard dependency on BiomeGenOriginal");
	m_bgen = (BiomeGenOriginal *)biomegen;

	spflags            = params->spflags;
	altitude_chill     = params->altitude_chill;
	river_depth_bed    = params->river_depth + 1.0f;
	river_size_factor  = params->river_size / 100.0f;

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

	//// 2D Terrain noise
	noise_filler_depth       = new Noise(&params->np_filler_depth,       seed, csize.X, csize.Z);
	noise_inter_valley_slope = new Noise(&params->np_inter_valley_slope, seed, csize.X, csize.Z);
	noise_rivers             = new Noise(&params->np_rivers,             seed, csize.X, csize.Z);
	noise_terrain_height     = new Noise(&params->np_terrain_height,     seed, csize.X, csize.Z);
	noise_valley_depth       = new Noise(&params->np_valley_depth,       seed, csize.X, csize.Z);
	noise_valley_profile     = new Noise(&params->np_valley_profile,     seed, csize.X, csize.Z);

	//// 3D Terrain noise
	// 1-up 1-down overgeneration
	noise_inter_valley_fill = new Noise(&params->np_inter_valley_fill,
		seed, csize.X, csize.Y + 2, csize.Z);
	// 1-down overgeneraion
	MapgenBasic::np_cave1    = params->np_cave1;
	MapgenBasic::np_cave2    = params->np_cave2;
	MapgenBasic::np_cavern   = params->np_cavern;
	MapgenBasic::np_dungeons = params->np_dungeons;
}


MapgenValleys::~MapgenValleys()
{
	delete noise_filler_depth;
	delete noise_inter_valley_fill;
	delete noise_inter_valley_slope;
	delete noise_rivers;
	delete noise_terrain_height;
	delete noise_valley_depth;
	delete noise_valley_profile;
}


MapgenValleysParams::MapgenValleysParams():
	np_filler_depth       (0.0,   1.2,  v3f(256,  256,  256),  1605,  3, 0.5,  2.0),
	np_inter_valley_fill  (0.0,   1.0,  v3f(256,  512,  256),  1993,  6, 0.8,  2.0),
	np_inter_valley_slope (0.5,   0.5,  v3f(128,  128,  128),  746,   1, 1.0,  2.0),
	np_rivers             (0.0,   1.0,  v3f(256,  256,  256),  -6050, 5, 0.6,  2.0),
	np_terrain_height     (-10.0, 50.0, v3f(1024, 1024, 1024), 5202,  6, 0.4,  2.0),
	np_valley_depth       (5.0,   4.0,  v3f(512,  512,  512),  -1914, 1, 1.0,  2.0),
	np_valley_profile     (0.6,   0.50, v3f(512,  512,  512),  777,   1, 1.0,  2.0),
	np_cave1              (0.0,   12.0, v3f(61,   61,   61),   52534, 3, 0.5,  2.0),
	np_cave2              (0.0,   12.0, v3f(67,   67,   67),   10325, 3, 0.5,  2.0),
	np_cavern             (0.0,   1.0,  v3f(768,  256,  768),  59033, 6, 0.63, 2.0),
	np_dungeons           (0.9,   0.5,  v3f(500,  500,  500),  0,     2, 0.8,  2.0)
{
}


void MapgenValleysParams::readParams(const Settings *settings)
{
	settings->getFlagStrNoEx("mgvalleys_spflags", spflags, flagdesc_mapgen_valleys);
	settings->getU16NoEx("mgvalleys_altitude_chill",       altitude_chill);
	settings->getS16NoEx("mgvalleys_large_cave_depth",     large_cave_depth);
	settings->getU16NoEx("mgvalleys_small_cave_num_min",   small_cave_num_min);
	settings->getU16NoEx("mgvalleys_small_cave_num_max",   small_cave_num_max);
	settings->getU16NoEx("mgvalleys_large_cave_num_min",   large_cave_num_min);
	settings->getU16NoEx("mgvalleys_large_cave_num_max",   large_cave_num_max);
	settings->getFloatNoEx("mgvalleys_large_cave_flooded", large_cave_flooded);
	settings->getU16NoEx("mgvalleys_river_depth",          river_depth);
	settings->getU16NoEx("mgvalleys_river_size",           river_size);
	settings->getFloatNoEx("mgvalleys_cave_width",         cave_width);
	settings->getS16NoEx("mgvalleys_cavern_limit",         cavern_limit);
	settings->getS16NoEx("mgvalleys_cavern_taper",         cavern_taper);
	settings->getFloatNoEx("mgvalleys_cavern_threshold",   cavern_threshold);
	settings->getS16NoEx("mgvalleys_dungeon_ymin",         dungeon_ymin);
	settings->getS16NoEx("mgvalleys_dungeon_ymax",         dungeon_ymax);

	settings->getNoiseParams("mgvalleys_np_filler_depth",       np_filler_depth);
	settings->getNoiseParams("mgvalleys_np_inter_valley_fill",  np_inter_valley_fill);
	settings->getNoiseParams("mgvalleys_np_inter_valley_slope", np_inter_valley_slope);
	settings->getNoiseParams("mgvalleys_np_rivers",             np_rivers);
	settings->getNoiseParams("mgvalleys_np_terrain_height",     np_terrain_height);
	settings->getNoiseParams("mgvalleys_np_valley_depth",       np_valley_depth);
	settings->getNoiseParams("mgvalleys_np_valley_profile",     np_valley_profile);

	settings->getNoiseParams("mgvalleys_np_cave1",              np_cave1);
	settings->getNoiseParams("mgvalleys_np_cave2",              np_cave2);
	settings->getNoiseParams("mgvalleys_np_cavern",             np_cavern);
	settings->getNoiseParams("mgvalleys_np_dungeons",           np_dungeons);
}


void MapgenValleysParams::writeParams(Settings *settings) const
{
	settings->setFlagStr("mgvalleys_spflags", spflags, flagdesc_mapgen_valleys);
	settings->setU16("mgvalleys_altitude_chill",       altitude_chill);
	settings->setS16("mgvalleys_large_cave_depth",     large_cave_depth);
	settings->setU16("mgvalleys_small_cave_num_min",   small_cave_num_min);
	settings->setU16("mgvalleys_small_cave_num_max",   small_cave_num_max);
	settings->setU16("mgvalleys_large_cave_num_min",   large_cave_num_min);
	settings->setU16("mgvalleys_large_cave_num_max",   large_cave_num_max);
	settings->setFloat("mgvalleys_large_cave_flooded", large_cave_flooded);
	settings->setU16("mgvalleys_river_depth",          river_depth);
	settings->setU16("mgvalleys_river_size",           river_size);
	settings->setFloat("mgvalleys_cave_width",         cave_width);
	settings->setS16("mgvalleys_cavern_limit",         cavern_limit);
	settings->setS16("mgvalleys_cavern_taper",         cavern_taper);
	settings->setFloat("mgvalleys_cavern_threshold",   cavern_threshold);
	settings->setS16("mgvalleys_dungeon_ymin",         dungeon_ymin);
	settings->setS16("mgvalleys_dungeon_ymax",         dungeon_ymax);

	settings->setNoiseParams("mgvalleys_np_filler_depth",       np_filler_depth);
	settings->setNoiseParams("mgvalleys_np_inter_valley_fill",  np_inter_valley_fill);
	settings->setNoiseParams("mgvalleys_np_inter_valley_slope", np_inter_valley_slope);
	settings->setNoiseParams("mgvalleys_np_rivers",             np_rivers);
	settings->setNoiseParams("mgvalleys_np_terrain_height",     np_terrain_height);
	settings->setNoiseParams("mgvalleys_np_valley_depth",       np_valley_depth);
	settings->setNoiseParams("mgvalleys_np_valley_profile",     np_valley_profile);

	settings->setNoiseParams("mgvalleys_np_cave1",              np_cave1);
	settings->setNoiseParams("mgvalleys_np_cave2",              np_cave2);
	settings->setNoiseParams("mgvalleys_np_cavern",             np_cavern);
	settings->setNoiseParams("mgvalleys_np_dungeons",           np_dungeons);
}


void MapgenValleysParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgvalleys_spflags", flagdesc_mapgen_valleys,
		MGVALLEYS_ALT_CHILL | MGVALLEYS_HUMID_RIVERS |
		MGVALLEYS_VARY_RIVER_DEPTH | MGVALLEYS_ALT_DRY);
}


/////////////////////////////////////////////////////////////////


void MapgenValleys::makeChunk(BlockMakeData *data)
{
	// Pre-conditions
	assert(data->vmanip);
	assert(data->nodedef);

	//TimeTaker t("makeChunk");

	this->generating = true;
	this->vm = data->vmanip;
	this->ndef = data->nodedef;

	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	blockseed = getBlockSeed2(full_node_min, seed);

	// Generate biome noises. Note this must be executed strictly before
	// generateTerrain, because generateTerrain depends on intermediate
	// biome-related noises.
	m_bgen->calcBiomeNoise(node_min);

	// Generate terrain
	s16 stone_surface_max_y = generateTerrain();

	// Create heightmap
	updateHeightmap(node_min, node_max);

	// Place biome-specific nodes and build biomemap
	if (flags & MG_BIOMES) {
		generateBiomes();
	}

	// Generate tunnels, caverns and large randomwalk caves
	if (flags & MG_CAVES) {
		// Generate tunnels first as caverns confuse them
		generateCavesNoiseIntersection(stone_surface_max_y);

		// Generate caverns
		bool near_cavern = generateCavernsNoise(stone_surface_max_y);

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

	// Dungeon creation
	if (flags & MG_DUNGEONS)
		generateDungeons(stone_surface_max_y);

	// Generate the registered decorations
	if (flags & MG_DECORATIONS)
		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

	// Sprinkle some dust on top after everything else was generated
	if (flags & MG_BIOMES)
		dustTopNodes();

	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	if (flags & MG_LIGHT)
		calcLighting(node_min - v3s16(0, 1, 0), node_max + v3s16(0, 1, 0),
			full_node_min, full_node_max);

	this->generating = false;

	//printf("makeChunk: %lums\n", t.stop());
}


int MapgenValleys::getSpawnLevelAtPoint(v2s16 p)
{
	// Check if in a river channel
	float n_rivers = NoiseFractal2D(&noise_rivers->np, p.X, p.Y, seed);
	if (std::fabs(n_rivers) <= river_size_factor)
		// Unsuitable spawn point
		return MAX_MAP_GENERATION_LIMIT;

	float n_slope          = NoiseFractal2D(&noise_inter_valley_slope->np, p.X, p.Y, seed);
	float n_terrain_height = NoiseFractal2D(&noise_terrain_height->np, p.X, p.Y, seed);
	float n_valley         = NoiseFractal2D(&noise_valley_depth->np, p.X, p.Y, seed);
	float n_valley_profile = NoiseFractal2D(&noise_valley_profile->np, p.X, p.Y, seed);

	float valley_d = n_valley * n_valley;
	float base = n_terrain_height + valley_d;
	float river = std::fabs(n_rivers) - river_size_factor;
	float tv = std::fmax(river / n_valley_profile, 0.0f);
	float valley_h = valley_d * (1.0f - std::exp(-tv * tv));
	float surface_y = base + valley_h;
	float slope = n_slope * valley_h;
	float river_y = base - 1.0f;

	// Raising the maximum spawn level above 'water_level + 16' is necessary for custom
	// parameters that set average terrain level much higher than water_level.
	s16 max_spawn_y = std::fmax(
		noise_terrain_height->np.offset +
		noise_valley_depth->np.offset * noise_valley_depth->np.offset,
		water_level + 16);

	// Starting spawn search at max_spawn_y + 128 ensures 128 nodes of open
	// space above spawn position. Avoids spawning in possibly sealed voids.
	for (s16 y = max_spawn_y + 128; y >= water_level; y--) {
		float n_fill = NoiseFractal3D(&noise_inter_valley_fill->np, p.X, y, p.Y, seed);
		float surface_delta = (float)y - surface_y;
		float density = slope * n_fill - surface_delta;

		if (density > 0.0f) {  // If solid
			// Sometimes surface level is below river water level in places that are not
			// river channels.
			if (y < water_level || y > max_spawn_y || y < (s16)river_y)
				// Unsuitable spawn point
				return MAX_MAP_GENERATION_LIMIT;

			// y + 2 because y is surface and due to biome 'dust' nodes.
			return y + 2;
		}
	}
	// Unsuitable spawn position, no ground found
	return MAX_MAP_GENERATION_LIMIT;
}


int MapgenValleys::generateTerrain()
{
	MapNode n_air(CONTENT_AIR);
	MapNode n_river_water(c_river_water_source);
	MapNode n_stone(c_stone);
	MapNode n_water(c_water_source);

	noise_inter_valley_slope->noiseMap2D(node_min.X, node_min.Z);
	noise_rivers->noiseMap2D(node_min.X, node_min.Z);
	noise_terrain_height->noiseMap2D(node_min.X, node_min.Z);
	noise_valley_depth->noiseMap2D(node_min.X, node_min.Z);
	noise_valley_profile->noiseMap2D(node_min.X, node_min.Z);

	noise_inter_valley_fill->noiseMap3D(node_min.X, node_min.Y - 1, node_min.Z);

	const v3s32 &em = vm->m_area.getExtent();
	s16 surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	u32 index_2d = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index_2d++) {
		float n_slope          = noise_inter_valley_slope->result[index_2d];
		float n_rivers         = noise_rivers->result[index_2d];
		float n_terrain_height = noise_terrain_height->result[index_2d];
		float n_valley         = noise_valley_depth->result[index_2d];
		float n_valley_profile = noise_valley_profile->result[index_2d];

		float valley_d = n_valley * n_valley;
		// 'base' represents the level of the river banks
		float base = n_terrain_height + valley_d;
		// 'river' represents the distance from the river edge
		float river = std::fabs(n_rivers) - river_size_factor;
		// Use the curve of the function 1-exp(-(x/a)^2) to model valleys.
		// 'valley_h' represents the height of the terrain, from the rivers.
		float tv = std::fmax(river / n_valley_profile, 0.0f);
		float valley_h = valley_d * (1.0f - std::exp(-tv * tv));
		// Approximate height of the terrain
		float surface_y = base + valley_h;
		float slope = n_slope * valley_h;
		// River water surface is 1 node below river banks
		float river_y = base - 1.0f;

		// Rivers are placed where 'river' is negative
		if (river < 0.0f) {
			// Use the function -sqrt(1-x^2) which models a circle
			float tr = river / river_size_factor + 1.0f;
			float depth = (river_depth_bed *
				std::sqrt(std::fmax(0.0f, 1.0f - tr * tr)));
			// There is no logical equivalent to this using rangelim
			surface_y = std::fmin(
				std::fmax(base - depth, (float)(water_level - 3)),
				surface_y);
			slope = 0.0f;
		}

		// Optionally vary river depth according to heat and humidity
		if (spflags & MGVALLEYS_VARY_RIVER_DEPTH) {
			float t_heat = m_bgen->heatmap[index_2d];
			float heat = (spflags & MGVALLEYS_ALT_CHILL) ?
				// Match heat value calculated below in
				// 'Optionally decrease heat with altitude'.
				// In rivers, 'ground height ignoring riverbeds' is 'base'.
				// As this only affects river water we can assume y > water_level.
				t_heat + 5.0f - (base - water_level) * 20.0f / altitude_chill :
				t_heat;
			float delta = m_bgen->humidmap[index_2d] - 50.0f;
			if (delta < 0.0f) {
				float t_evap = (heat - 32.0f) / 300.0f;
				river_y += delta * std::fmax(t_evap, 0.08f);
			}
		}

		// Highest solid node in column
		s16 column_max_y = surface_y;
		u32 index_3d = (z - node_min.Z) * zstride_1u1d + (x - node_min.X);
		u32 index_data = vm->m_area.index(x, node_min.Y - 1, z);

		for (s16 y = node_min.Y - 1; y <= node_max.Y + 1; y++) {
			if (vm->m_data[index_data].getContent() == CONTENT_IGNORE) {
				float n_fill = noise_inter_valley_fill->result[index_3d];
				float surface_delta = (float)y - surface_y;
				// Density = density noise + density gradient
				float density = slope * n_fill - surface_delta;

				if (density > 0.0f) {
					vm->m_data[index_data] = n_stone; // Stone
					if (y > surface_max_y)
						surface_max_y = y;
					if (y > column_max_y)
						column_max_y = y;
				} else if (y <= water_level) {
					vm->m_data[index_data] = n_water; // Water
				} else if (y <= (s16)river_y) {
					vm->m_data[index_data] = n_river_water; // River water
				} else {
					vm->m_data[index_data] = n_air; // Air
				}
			}

			VoxelArea::add_y(em, index_data, 1);
			index_3d += ystride;
		}

		// Optionally increase humidity around rivers
		if (spflags & MGVALLEYS_HUMID_RIVERS) {
			// Compensate to avoid increasing average humidity
			m_bgen->humidmap[index_2d] *= 0.8f;
			// Ground height ignoring riverbeds
			float t_alt = std::fmax(base, (float)column_max_y);
			float water_depth = (t_alt - base) / 4.0f;
			m_bgen->humidmap[index_2d] *=
				1.0f + std::pow(0.5f, std::fmax(water_depth, 1.0f));
		}

		// Optionally decrease humidity with altitude
		if (spflags & MGVALLEYS_ALT_DRY) {
			// Ground height ignoring riverbeds
			float t_alt = std::fmax(base, (float)column_max_y);
			// Only decrease above water_level
			if (t_alt > water_level)
				m_bgen->humidmap[index_2d] -=
					(t_alt - water_level) * 10.0f / altitude_chill;
		}

		// Optionally decrease heat with altitude
		if (spflags & MGVALLEYS_ALT_CHILL) {
			// Compensate to avoid reducing the average heat
			m_bgen->heatmap[index_2d] += 5.0f;
			// Ground height ignoring riverbeds
			float t_alt = std::fmax(base, (float)column_max_y);
			// Only decrease above water_level
			if (t_alt > water_level)
				m_bgen->heatmap[index_2d] -=
					(t_alt - water_level) * 20.0f / altitude_chill;
		}
	}

	return surface_max_y;
}
