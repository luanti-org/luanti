// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-2019 vlapsley, Vaughan Lapsley <vlapsley@gmail.com>
// Copyright (C) 2017-2019 paramat


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
#include "mg_biome.h"
#include "mg_ore.h"
#include "mg_decoration.h"
#include "mapgen_carpathian.h"


const FlagDesc flagdesc_mapgen_carpathian[] = {
	{"caverns", MGCARPATHIAN_CAVERNS},
	{"rivers",  MGCARPATHIAN_RIVERS},
	{NULL,      0}
};


///////////////////////////////////////////////////////////////////////////////


MapgenCarpathian::MapgenCarpathian(MapgenCarpathianParams *params, EmergeParams *emerge)
	: MapgenBasic(MAPGEN_CARPATHIAN, params, emerge)
{
	base_level       = params->base_level;
	river_width      = params->river_width;
	river_depth      = params->river_depth;
	valley_width     = params->valley_width;

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

	grad_wl = 1 - water_level;

	//// 2D Terrain noise
	noise_filler_depth  = new Noise(&params->np_filler_depth,  seed, csize.X, csize.Z);
	noise_height1       = new Noise(&params->np_height1,       seed, csize.X, csize.Z);
	noise_height2       = new Noise(&params->np_height2,       seed, csize.X, csize.Z);
	noise_height3       = new Noise(&params->np_height3,       seed, csize.X, csize.Z);
	noise_height4       = new Noise(&params->np_height4,       seed, csize.X, csize.Z);
	noise_hills_terrain = new Noise(&params->np_hills_terrain, seed, csize.X, csize.Z);
	noise_ridge_terrain = new Noise(&params->np_ridge_terrain, seed, csize.X, csize.Z);
	noise_step_terrain  = new Noise(&params->np_step_terrain,  seed, csize.X, csize.Z);
	noise_hills         = new Noise(&params->np_hills,         seed, csize.X, csize.Z);
	noise_ridge_mnt     = new Noise(&params->np_ridge_mnt,     seed, csize.X, csize.Z);
	noise_step_mnt      = new Noise(&params->np_step_mnt,      seed, csize.X, csize.Z);
	if (spflags & MGCARPATHIAN_RIVERS)
		noise_rivers    = new Noise(&params->np_rivers,        seed, csize.X, csize.Z);

	//// 3D terrain noise
	// 1 up 1 down overgeneration
	noise_mnt_var = new Noise(&params->np_mnt_var, seed, csize.X, csize.Y + 2, csize.Z);

	//// Cave noise
	MapgenBasic::np_cave1  = params->np_cave1;
	MapgenBasic::np_cave2  = params->np_cave2;
	MapgenBasic::np_cavern = params->np_cavern;
	MapgenBasic::np_dungeons = params->np_dungeons;
}


MapgenCarpathian::~MapgenCarpathian()
{
	delete noise_filler_depth;
	delete noise_height1;
	delete noise_height2;
	delete noise_height3;
	delete noise_height4;
	delete noise_hills_terrain;
	delete noise_ridge_terrain;
	delete noise_step_terrain;
	delete noise_hills;
	delete noise_ridge_mnt;
	delete noise_step_mnt;
	if (spflags & MGCARPATHIAN_RIVERS)
		delete noise_rivers;

	delete noise_mnt_var;
}


MapgenCarpathianParams::MapgenCarpathianParams():
	np_filler_depth  (0,   1,   v3f(128,  128,  128),  261,   3, 0.7,  2.0),
	np_height1       (0,   5,   v3f(251,  251,  251),  9613,  5, 0.5,  2.0),
	np_height2       (0,   5,   v3f(383,  383,  383),  1949,  5, 0.5,  2.0),
	np_height3       (0,   5,   v3f(509,  509,  509),  3211,  5, 0.5,  2.0),
	np_height4       (0,   5,   v3f(631,  631,  631),  1583,  5, 0.5,  2.0),
	np_hills_terrain (1,   1,   v3f(1301, 1301, 1301), 1692,  5, 0.5,  2.0),
	np_ridge_terrain (1,   1,   v3f(1889, 1889, 1889), 3568,  5, 0.5,  2.0),
	np_step_terrain  (1,   1,   v3f(1889, 1889, 1889), 4157,  5, 0.5,  2.0),
	np_hills         (0,   3,   v3f(257,  257,  257),  6604,  6, 0.5,  2.0),
	np_ridge_mnt     (0,   12,  v3f(743,  743,  743),  5520,  6, 0.7,  2.0),
	np_step_mnt      (0,   8,   v3f(509,  509,  509),  2590,  6, 0.6,  2.0),
	np_rivers        (0,   1,   v3f(1000, 1000, 1000), 85039, 5, 0.6,  2.0),
	np_mnt_var       (0,   1,   v3f(499,  499,  499),  2490,  5, 0.55, 2.0),
	np_cave1         (0,   12,  v3f(61,   61,   61),   52534, 3, 0.5,  2.0),
	np_cave2         (0,   12,  v3f(67,   67,   67),   10325, 3, 0.5,  2.0),
	np_cavern        (0,   1,   v3f(384,  128,  384),  723,   5, 0.63, 2.0),
	np_dungeons      (0.9, 0.5, v3f(500,  500,  500),  0,     2, 0.8,  2.0)
{
}


void MapgenCarpathianParams::readParams(const Settings *settings)
{
	settings->getFlagStrNoEx("mgcarpathian_spflags", spflags, flagdesc_mapgen_carpathian);

	settings->getFloatNoEx("mgcarpathian_base_level",   base_level);
	settings->getFloatNoEx("mgcarpathian_river_width",  river_width);
	settings->getFloatNoEx("mgcarpathian_river_depth",  river_depth);
	settings->getFloatNoEx("mgcarpathian_valley_width", valley_width);

	settings->getFloatNoEx("mgcarpathian_cave_width",         cave_width);
	settings->getS16NoEx("mgcarpathian_large_cave_depth",     large_cave_depth);
	settings->getU16NoEx("mgcarpathian_small_cave_num_min",   small_cave_num_min);
	settings->getU16NoEx("mgcarpathian_small_cave_num_max",   small_cave_num_max);
	settings->getU16NoEx("mgcarpathian_large_cave_num_min",   large_cave_num_min);
	settings->getU16NoEx("mgcarpathian_large_cave_num_max",   large_cave_num_max);
	settings->getFloatNoEx("mgcarpathian_large_cave_flooded", large_cave_flooded);
	settings->getS16NoEx("mgcarpathian_cavern_limit",         cavern_limit);
	settings->getS16NoEx("mgcarpathian_cavern_taper",         cavern_taper);
	settings->getFloatNoEx("mgcarpathian_cavern_threshold",   cavern_threshold);
	settings->getS16NoEx("mgcarpathian_dungeon_ymin",         dungeon_ymin);
	settings->getS16NoEx("mgcarpathian_dungeon_ymax",         dungeon_ymax);

	settings->getNoiseParams("mgcarpathian_np_filler_depth",  np_filler_depth);
	settings->getNoiseParams("mgcarpathian_np_height1",       np_height1);
	settings->getNoiseParams("mgcarpathian_np_height2",       np_height2);
	settings->getNoiseParams("mgcarpathian_np_height3",       np_height3);
	settings->getNoiseParams("mgcarpathian_np_height4",       np_height4);
	settings->getNoiseParams("mgcarpathian_np_hills_terrain", np_hills_terrain);
	settings->getNoiseParams("mgcarpathian_np_ridge_terrain", np_ridge_terrain);
	settings->getNoiseParams("mgcarpathian_np_step_terrain",  np_step_terrain);
	settings->getNoiseParams("mgcarpathian_np_hills",         np_hills);
	settings->getNoiseParams("mgcarpathian_np_ridge_mnt",     np_ridge_mnt);
	settings->getNoiseParams("mgcarpathian_np_step_mnt",      np_step_mnt);
	settings->getNoiseParams("mgcarpathian_np_rivers",        np_rivers);
	settings->getNoiseParams("mgcarpathian_np_mnt_var",       np_mnt_var);
	settings->getNoiseParams("mgcarpathian_np_cave1",         np_cave1);
	settings->getNoiseParams("mgcarpathian_np_cave2",         np_cave2);
	settings->getNoiseParams("mgcarpathian_np_cavern",        np_cavern);
	settings->getNoiseParams("mgcarpathian_np_dungeons",      np_dungeons);
}


void MapgenCarpathianParams::writeParams(Settings *settings) const
{
	settings->setFlagStr("mgcarpathian_spflags", spflags, flagdesc_mapgen_carpathian);

	settings->setFloat("mgcarpathian_base_level",   base_level);
	settings->setFloat("mgcarpathian_river_width",  river_width);
	settings->setFloat("mgcarpathian_river_depth",  river_depth);
	settings->setFloat("mgcarpathian_valley_width", valley_width);

	settings->setFloat("mgcarpathian_cave_width",         cave_width);
	settings->setS16("mgcarpathian_large_cave_depth",     large_cave_depth);
	settings->setU16("mgcarpathian_small_cave_num_min",   small_cave_num_min);
	settings->setU16("mgcarpathian_small_cave_num_max",   small_cave_num_max);
	settings->setU16("mgcarpathian_large_cave_num_min",   large_cave_num_min);
	settings->setU16("mgcarpathian_large_cave_num_max",   large_cave_num_max);
	settings->setFloat("mgcarpathian_large_cave_flooded", large_cave_flooded);
	settings->setS16("mgcarpathian_cavern_limit",         cavern_limit);
	settings->setS16("mgcarpathian_cavern_taper",         cavern_taper);
	settings->setFloat("mgcarpathian_cavern_threshold",   cavern_threshold);
	settings->setS16("mgcarpathian_dungeon_ymin",         dungeon_ymin);
	settings->setS16("mgcarpathian_dungeon_ymax",         dungeon_ymax);

	settings->setNoiseParams("mgcarpathian_np_filler_depth",  np_filler_depth);
	settings->setNoiseParams("mgcarpathian_np_height1",       np_height1);
	settings->setNoiseParams("mgcarpathian_np_height2",       np_height2);
	settings->setNoiseParams("mgcarpathian_np_height3",       np_height3);
	settings->setNoiseParams("mgcarpathian_np_height4",       np_height4);
	settings->setNoiseParams("mgcarpathian_np_hills_terrain", np_hills_terrain);
	settings->setNoiseParams("mgcarpathian_np_ridge_terrain", np_ridge_terrain);
	settings->setNoiseParams("mgcarpathian_np_step_terrain",  np_step_terrain);
	settings->setNoiseParams("mgcarpathian_np_hills",         np_hills);
	settings->setNoiseParams("mgcarpathian_np_ridge_mnt",     np_ridge_mnt);
	settings->setNoiseParams("mgcarpathian_np_step_mnt",      np_step_mnt);
	settings->setNoiseParams("mgcarpathian_np_rivers",        np_rivers);
	settings->setNoiseParams("mgcarpathian_np_mnt_var",       np_mnt_var);
	settings->setNoiseParams("mgcarpathian_np_cave1",         np_cave1);
	settings->setNoiseParams("mgcarpathian_np_cave2",         np_cave2);
	settings->setNoiseParams("mgcarpathian_np_cavern",        np_cavern);
	settings->setNoiseParams("mgcarpathian_np_dungeons",      np_dungeons);
}


void MapgenCarpathianParams::setDefaultSettings(Settings *settings)
{
	settings->setDefault("mgcarpathian_spflags", flagdesc_mapgen_carpathian,
		MGCARPATHIAN_CAVERNS);
}

////////////////////////////////////////////////////////////////////////////////


// Lerp function
inline float MapgenCarpathian::getLerp(float noise1, float noise2, float mod)
{
	return noise1 + mod * (noise2 - noise1);
}

// Steps function
float MapgenCarpathian::getSteps(float noise)
{
	float w = 0.5f;
	float k = std::floor(noise / w);
	float f = (noise - k * w) / w;
	float s = std::fmin(2.f * f, 1.f);
	return (k + s) * w;
}


////////////////////////////////////////////////////////////////////////////////


void MapgenCarpathian::makeChunk(BlockMakeData *data)
{
	// Pre-conditions
	assert(data->vmanip);
	assert(data->nodedef);

	this->generating = true;
	this->vm = data->vmanip;
	this->ndef = data->nodedef;

	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);
	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	// Create a block-specific seed
	blockseed = getBlockSeed2(full_node_min, seed);

	// Generate terrain
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
		if (spflags & MGCARPATHIAN_CAVERNS)
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

	// Generate dungeons
	if (flags & MG_DUNGEONS)
		generateDungeons(stone_surface_max_y);

	// Generate the registered decorations
	if (flags & MG_DECORATIONS)
		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

	// Sprinkle some dust on top after everything else was generated
	if (flags & MG_BIOMES)
		dustTopNodes();

	// Update liquids
	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	// Calculate lighting
	if (flags & MG_LIGHT) {
		calcLighting(node_min - v3s16(0, 1, 0), node_max + v3s16(0, 1, 0),
				full_node_min, full_node_max);
	}

	this->generating = false;
}


////////////////////////////////////////////////////////////////////////////////


int MapgenCarpathian::getSpawnLevelAtPoint(v2s16 p)
{
	// If rivers are enabled, first check if in a river channel
	if (spflags & MGCARPATHIAN_RIVERS) {
		float river = std::fabs(NoiseFractal2D(&noise_rivers->np, p.X, p.Y, seed)) -
			river_width;
		if (river < 0.0f)
			return MAX_MAP_GENERATION_LIMIT; // Unsuitable spawn point
	}

	float height1 = NoiseFractal2D(&noise_height1->np, p.X, p.Y, seed);
	float height2 = NoiseFractal2D(&noise_height2->np, p.X, p.Y, seed);
	float height3 = NoiseFractal2D(&noise_height3->np, p.X, p.Y, seed);
	float height4 = NoiseFractal2D(&noise_height4->np, p.X, p.Y, seed);

	float hterabs = std::fabs(NoiseFractal2D(&noise_hills_terrain->np, p.X, p.Y, seed));
	float n_hills = NoiseFractal2D(&noise_hills->np, p.X, p.Y, seed);
	float hill_mnt = hterabs * hterabs * hterabs * n_hills * n_hills;

	float rterabs = std::fabs(NoiseFractal2D(&noise_ridge_terrain->np, p.X, p.Y, seed));
	float n_ridge_mnt = NoiseFractal2D(&noise_ridge_mnt->np, p.X, p.Y, seed);
	float ridge_mnt = rterabs * rterabs * rterabs * (1.0f - std::fabs(n_ridge_mnt));

	float sterabs = std::fabs(NoiseFractal2D(&noise_step_terrain->np, p.X, p.Y, seed));
	float n_step_mnt = NoiseFractal2D(&noise_step_mnt->np, p.X, p.Y, seed);
	float step_mnt = sterabs * sterabs * sterabs * getSteps(n_step_mnt);

	float valley = 1.0f;
	float river = 0.0f;

	if ((spflags & MGCARPATHIAN_RIVERS) && node_max.Y >= water_level - 16) {
		river = std::fabs(NoiseFractal2D(&noise_rivers->np, p.X, p.Y, seed)) - river_width;
		if (river <= valley_width) {
			// Within river valley
			if (river < 0.0f) {
				// River channel
				valley = river;
			} else {
				// Valley slopes.
				// 0 at river edge, 1 at valley edge.
				float riversc = river / valley_width;
				// Smoothstep
				valley = riversc * riversc * (3.0f - 2.0f * riversc);
			}
		}
	}

	bool solid_below = false;
	u8 cons_non_solid = 0; // consecutive non-solid nodes

	for (s16 y = water_level; y <= water_level + 32; y++) {
		float mnt_var = NoiseFractal3D(&noise_mnt_var->np, p.X, y, p.Y, seed);
		float hill1 = getLerp(height1, height2, mnt_var);
		float hill2 = getLerp(height3, height4, mnt_var);
		float hill3 = getLerp(height3, height2, mnt_var);
		float hill4 = getLerp(height1, height4, mnt_var);

		float hilliness = std::fmax(std::fmin(hill1, hill2), std::fmin(hill3, hill4));
		float hills = hill_mnt * hilliness;
		float ridged_mountains = ridge_mnt * hilliness;
		float step_mountains = step_mnt * hilliness;

		s32 grad = 1 - y;

		float mountains = hills + ridged_mountains + step_mountains;
		float surface_level = base_level + mountains + grad;

		if ((spflags & MGCARPATHIAN_RIVERS) && river <= valley_width) {
			if (valley < 0.0f) {
				// River channel
				surface_level = std::fmin(surface_level,
					water_level - std::sqrt(-valley) * river_depth);
			} else if (surface_level > water_level) {
				// Valley slopes
				surface_level = water_level + (surface_level - water_level) * valley;
			}
		}

		if (y < surface_level) { //TODO '<=' fix from generateTerrain()
			// solid node
			solid_below = true;
			cons_non_solid = 0;
		} else {
			// non-solid node
			cons_non_solid++;
			if (cons_non_solid == 3 && solid_below)
				return y - 1;
		}
	}

	return MAX_MAP_GENERATION_LIMIT; // No suitable spawn point found
}


////////////////////////////////////////////////////////////////////////////////


int MapgenCarpathian::generateTerrain()
{
	MapNode mn_air(CONTENT_AIR);
	MapNode mn_stone(c_stone);
	MapNode mn_water(c_water_source);

	// Calculate noise for terrain generation
	noise_height1->noiseMap2D(node_min.X, node_min.Z);
	noise_height2->noiseMap2D(node_min.X, node_min.Z);
	noise_height3->noiseMap2D(node_min.X, node_min.Z);
	noise_height4->noiseMap2D(node_min.X, node_min.Z);
	noise_hills_terrain->noiseMap2D(node_min.X, node_min.Z);
	noise_ridge_terrain->noiseMap2D(node_min.X, node_min.Z);
	noise_step_terrain->noiseMap2D(node_min.X, node_min.Z);
	noise_hills->noiseMap2D(node_min.X, node_min.Z);
	noise_ridge_mnt->noiseMap2D(node_min.X, node_min.Z);
	noise_step_mnt->noiseMap2D(node_min.X, node_min.Z);
	noise_mnt_var->noiseMap3D(node_min.X, node_min.Y - 1, node_min.Z);

	if (spflags & MGCARPATHIAN_RIVERS)
		noise_rivers->noiseMap2D(node_min.X, node_min.Z);

	//// Place nodes
	const v3s32 &em = vm->m_area.getExtent();
	s16 stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;
	u32 index2d = 0;

	for (s16 z = node_min.Z; z <= node_max.Z; z++)
	for (s16 x = node_min.X; x <= node_max.X; x++, index2d++) {
		// Hill/Mountain height (hilliness)
		float height1 = noise_height1->result[index2d];
		float height2 = noise_height2->result[index2d];
		float height3 = noise_height3->result[index2d];
		float height4 = noise_height4->result[index2d];

		// Rolling hills
		float hterabs = std::fabs(noise_hills_terrain->result[index2d]);
		float n_hills = noise_hills->result[index2d];
		float hill_mnt = hterabs * hterabs * hterabs * n_hills * n_hills;

		// Ridged mountains
		float rterabs = std::fabs(noise_ridge_terrain->result[index2d]);
		float n_ridge_mnt = noise_ridge_mnt->result[index2d];
		float ridge_mnt = rterabs * rterabs * rterabs *
			(1.0f - std::fabs(n_ridge_mnt));

		// Step (terraced) mountains
		float sterabs = std::fabs(noise_step_terrain->result[index2d]);
		float n_step_mnt = noise_step_mnt->result[index2d];
		float step_mnt = sterabs * sterabs * sterabs * getSteps(n_step_mnt);

		// Rivers
		float valley = 1.0f;
		float river = 0.0f;

		if ((spflags & MGCARPATHIAN_RIVERS) && node_max.Y >= water_level - 16) {
			river = std::fabs(noise_rivers->result[index2d]) - river_width;
			if (river <= valley_width) {
				// Within river valley
				if (river < 0.0f) {
					// River channel
					valley = river;
				} else {
					// Valley slopes.
					// 0 at river edge, 1 at valley edge.
					float riversc = river / valley_width;
					// Smoothstep
					valley = riversc * riversc * (3.0f - 2.0f * riversc);
				}
			}
		}

		// Initialise 3D noise index and voxelmanip index to column base
		u32 index3d = (z - node_min.Z) * zstride_1u1d + (x - node_min.X);
		u32 vi = vm->m_area.index(x, node_min.Y - 1, z);

		for (s16 y = node_min.Y - 1; y <= node_max.Y + 1;
				y++,
				index3d += ystride,
				VoxelArea::add_y(em, vi, 1)) {
			if (vm->m_data[vi].getContent() != CONTENT_IGNORE)
				continue;

			// Combine height noises and apply 3D variation
			float mnt_var = noise_mnt_var->result[index3d];
			float hill1 = getLerp(height1, height2, mnt_var);
			float hill2 = getLerp(height3, height4, mnt_var);
			float hill3 = getLerp(height3, height2, mnt_var);
			float hill4 = getLerp(height1, height4, mnt_var);

			// 'hilliness' determines whether hills/mountains are
			// small or large
			float hilliness =
				std::fmax(std::fmin(hill1, hill2), std::fmin(hill3, hill4));
			float hills = hill_mnt * hilliness;
			float ridged_mountains = ridge_mnt * hilliness;
			float step_mountains = step_mnt * hilliness;

			// Gradient & shallow seabed
			s32 grad = (y < water_level) ? grad_wl + (water_level - y) * 3 :
				1 - y;

			// Final terrain level
			float mountains = hills + ridged_mountains + step_mountains;
			float surface_level = base_level + mountains + grad;

			// Rivers
			if ((spflags & MGCARPATHIAN_RIVERS) && node_max.Y >= water_level - 16 &&
					river <= valley_width) {
				if (valley < 0.0f) {
					// River channel
					surface_level = std::fmin(surface_level,
						water_level - std::sqrt(-valley) * river_depth);
				} else if (surface_level > water_level) {
					// Valley slopes
					surface_level = water_level + (surface_level - water_level) * valley;
				}
			}

			if (y < surface_level) { //TODO '<='
				vm->m_data[vi] = mn_stone; // Stone
				if (y > stone_surface_max_y)
					stone_surface_max_y = y;
			} else if (y <= water_level) {
				vm->m_data[vi] = mn_water; // Sea water
			} else {
				vm->m_data[vi] = mn_air; // Air
			}
		}
	}

	return stone_surface_max_y;
}
