// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2014-2018 paramat

#include "mg_biome.h"
#include "mg_decoration.h"
#include "emerge.h"
#include "server.h"
#include "nodedef.h"
#include "map.h" //for MMVManip
#include "util/numeric.h"
#include "porting.h"
#include "settings.h"

#include <algorithm>

///////////////////////////////////////////////////////////////////////////////


BiomeManager::BiomeManager(Server *server) :
	ObjDefManager(server, OBJDEF_BIOME)
{
	m_server = server;

	// Create default biome to be used in case none exist
	Biome *b = new Biome;
	b->name            = "default";

	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("mapgen_water_source");
	b->m_nodenames.emplace_back("mapgen_water_source");
	b->m_nodenames.emplace_back("mapgen_river_water_source");
	b->m_nodenames.emplace_back("mapgen_stone");
	b->m_nodenames.emplace_back("ignore");
	b->m_nodenames.emplace_back("ignore");
	b->m_nnlistsizes.push_back(1);
	b->m_nodenames.emplace_back("ignore");
	b->m_nodenames.emplace_back("ignore");
	b->m_nodenames.emplace_back("ignore");
	m_ndef->pendNodeResolve(b);

	add(b);
}


void BiomeManager::clear()
{
	EmergeManager *emerge = m_server->getEmergeManager();

	if (emerge) {
		// Remove all dangling references in Decorations
		DecorationManager *decomgr = emerge->getWritableDecorationManager();
		for (size_t i = 0; i != decomgr->getNumObjects(); i++) {
			Decoration *deco = (Decoration *)decomgr->getRaw(i);
			deco->biomes.clear();
		}
	}

	// Don't delete the first biome
	for (size_t i = 1; i < m_objects.size(); i++)
		delete (Biome *)m_objects[i];

	m_objects.resize(1);
}


BiomeManager *BiomeManager::clone() const
{
	auto mgr = new BiomeManager();
	assert(mgr);
	ObjDefManager::cloneTo(mgr);
	mgr->m_server = m_server;
	return mgr;
}

////////////////////////////////////////////////////////////////////////////////

void BiomeParamsOriginal::readParams(const Settings *settings)
{
	settings->getNoiseParams("mg_biome_np_heat",           np_heat);
	settings->getNoiseParams("mg_biome_np_heat_blend",     np_heat_blend);
	settings->getNoiseParams("mg_biome_np_humidity",       np_humidity);
	settings->getNoiseParams("mg_biome_np_humidity_blend", np_humidity_blend);
}


void BiomeParamsOriginal::writeParams(Settings *settings) const
{
	settings->setNoiseParams("mg_biome_np_heat",           np_heat);
	settings->setNoiseParams("mg_biome_np_heat_blend",     np_heat_blend);
	settings->setNoiseParams("mg_biome_np_humidity",       np_humidity);
	settings->setNoiseParams("mg_biome_np_humidity_blend", np_humidity_blend);
}


////////////////////////////////////////////////////////////////////////////////

BiomeGenOriginal::BiomeGenOriginal(BiomeManager *biomemgr,
	const BiomeParamsOriginal *params, v3s16 chunksize)
{
	m_bmgr   = biomemgr;
	m_params = params;
	m_csize  = chunksize;

	noise_heat           = new Noise(&params->np_heat,
									params->seed, m_csize.X, m_csize.Z);
	noise_humidity       = new Noise(&params->np_humidity,
									params->seed, m_csize.X, m_csize.Z);
	noise_heat_blend     = new Noise(&params->np_heat_blend,
									params->seed, m_csize.X, m_csize.Z);
	noise_humidity_blend = new Noise(&params->np_humidity_blend,
									params->seed, m_csize.X, m_csize.Z);

	heatmap  = noise_heat->result;
	humidmap = noise_humidity->result;

	biomemap = new biome_t[m_csize.X * m_csize.Z];
	// Initialise with the ID of 'BIOME_NONE' so that cavegen can get the
	// fallback biome when biome generation (which calculates the biomemap IDs)
	// is disabled.
	memset(biomemap, 0, sizeof(biome_t) * m_csize.X * m_csize.Z);

	// Calculate cache of Y transition points
	std::vector<s16> values;
	values.reserve(m_bmgr->getNumObjects() * 2);
	for (size_t i = 0; i < m_bmgr->getNumObjects(); i++) {
		Biome *b = (Biome *)m_bmgr->getRaw(i);
		values.push_back(b->max_pos.Y);
		// We scan for biomes from high Y to low Y (top to bottom). Hence,
		// biomes effectively transition at (min_pos.Y - 1).
		if (b->min_pos.Y > -MAX_MAP_GENERATION_LIMIT)
			values.push_back(b->min_pos.Y - 1);
	}

	std::sort(values.begin(), values.end(), std::greater<>());
	values.erase(std::unique(values.begin(), values.end()), values.end());

	m_transitions_y = std::move(values);
}

BiomeGenOriginal::~BiomeGenOriginal()
{
	delete []biomemap;

	delete noise_heat;
	delete noise_humidity;
	delete noise_heat_blend;
	delete noise_humidity_blend;
}

s16 BiomeGenOriginal::getNextTransitionY(s16 y) const
{
	// Find first value that is less than y using binary search
	auto it = std::lower_bound(m_transitions_y.begin(), m_transitions_y.end(), y, std::greater_equal<>());
	return (it == m_transitions_y.end()) ? S16_MIN : *it;
}

BiomeGen *BiomeGenOriginal::clone(BiomeManager *biomemgr) const
{
	return new BiomeGenOriginal(biomemgr, m_params, m_csize);
}

float BiomeGenOriginal::calcHeatAtPoint(v3s16 pos) const
{
	return NoiseFractal2D(&m_params->np_heat, pos.X, pos.Z, m_params->seed) +
		NoiseFractal2D(&m_params->np_heat_blend, pos.X, pos.Z, m_params->seed);
}

float BiomeGenOriginal::calcHumidityAtPoint(v3s16 pos) const
{
	return NoiseFractal2D(&m_params->np_humidity, pos.X, pos.Z, m_params->seed) +
		NoiseFractal2D(&m_params->np_humidity_blend, pos.X, pos.Z, m_params->seed);
}

Biome *BiomeGenOriginal::calcBiomeAtPoint(v3s16 pos) const
{
	return calcBiomeFromNoise(calcHeatAtPoint(pos), calcHumidityAtPoint(pos), pos);
}


void BiomeGenOriginal::calcBiomeNoise(v3s16 pmin)
{
	m_pmin = pmin;

	noise_heat->noiseMap2D(pmin.X, pmin.Z);
	noise_humidity->noiseMap2D(pmin.X, pmin.Z);
	noise_heat_blend->noiseMap2D(pmin.X, pmin.Z);
	noise_humidity_blend->noiseMap2D(pmin.X, pmin.Z);

	for (s32 i = 0; i < m_csize.X * m_csize.Z; i++) {
		noise_heat->result[i]     += noise_heat_blend->result[i];
		noise_humidity->result[i] += noise_humidity_blend->result[i];
	}
}


biome_t *BiomeGenOriginal::getBiomes(s16 *heightmap, v3s16 pmin)
{
	for (s16 zr = 0; zr < m_csize.Z; zr++)
	for (s16 xr = 0; xr < m_csize.X; xr++) {
		s32 i = zr * m_csize.X + xr;
		Biome *biome = calcBiomeFromNoise(
			noise_heat->result[i],
			noise_humidity->result[i],
			v3s16(pmin.X + xr, heightmap[i], pmin.Z + zr));

		biomemap[i] = biome->index;
	}

	return biomemap;
}


Biome *BiomeGenOriginal::getBiomeAtPoint(v3s16 pos) const
{
	return getBiomeAtIndex(
		(pos.Z - m_pmin.Z) * m_csize.X + (pos.X - m_pmin.X),
		pos);
}


Biome *BiomeGenOriginal::getBiomeAtIndex(size_t index, v3s16 pos) const
{
	return calcBiomeFromNoise(
		noise_heat->result[index],
		noise_humidity->result[index],
		pos);
}


Biome *BiomeGenOriginal::calcBiomeFromNoise(float heat, float humidity, v3s16 pos) const
{
	Biome *biome_closest = nullptr;
	Biome *biome_closest_blend = nullptr;
	float dist_min = FLT_MAX;
	float dist_min_blend = FLT_MAX;

	for (size_t i = 1; i < m_bmgr->getNumObjects(); i++) {
		Biome *b = (Biome *)m_bmgr->getRaw(i);
		if (!b ||
				pos.Y < b->min_pos.Y || pos.Y > b->max_pos.Y + b->vertical_blend ||
				pos.X < b->min_pos.X || pos.X > b->max_pos.X ||
				pos.Z < b->min_pos.Z || pos.Z > b->max_pos.Z)
			continue;

		float d_heat = heat - b->heat_point;
		float d_humidity = humidity - b->humidity_point;
		float dist = ((d_heat * d_heat) + (d_humidity * d_humidity));
		if (b->weight > 0.f)
		       dist /= b->weight;

		if (pos.Y <= b->max_pos.Y) { // Within y limits of biome b
			if (dist < dist_min) {
				dist_min = dist;
				biome_closest = b;
			}
		} else if (dist < dist_min_blend) { // Blend area above biome b
			dist_min_blend = dist;
			biome_closest_blend = b;
		}
	}

	// Carefully tune pseudorandom seed variation to avoid single node dither
	// and create larger scale blending patterns similar to horizontal biome
	// blend.
	// The calculation can be a negative floating point number, which is an
	// undefined behavior if assigned to unsigned integer. Cast the result
	// into signed integer before it is casted into unsigned integer to
	// eliminate the undefined behavior.
	const u64 seed = static_cast<s64>(pos.Y + (heat + humidity) * 0.9f);
	PcgRandom rng(seed);

	if (biome_closest_blend && dist_min_blend <= dist_min &&
			rng.range(0, biome_closest_blend->vertical_blend) >=
			pos.Y - biome_closest_blend->max_pos.Y)
		return biome_closest_blend;

	return (biome_closest) ? biome_closest : (Biome *)m_bmgr->getRaw(BIOME_NONE);
}


////////////////////////////////////////////////////////////////////////////////

ObjDef *Biome::clone() const
{
	auto obj = new Biome();
	ObjDef::cloneTo(obj);
	NodeResolver::cloneTo(obj);

	obj->c_top = c_top;
	obj->c_filler = c_filler;
	obj->c_stone = c_stone;
	obj->c_water_top = c_water_top;
	obj->c_water = c_water;
	obj->c_river_water = c_river_water;
	obj->c_riverbed = c_riverbed;
	obj->c_dust = c_dust;
	obj->c_cave_liquid = c_cave_liquid;
	obj->c_dungeon = c_dungeon;
	obj->c_dungeon_alt = c_dungeon_alt;
	obj->c_dungeon_stair = c_dungeon_stair;

	obj->depth_top = depth_top;
	obj->depth_filler = depth_filler;
	obj->depth_water_top = depth_water_top;
	obj->depth_riverbed = depth_riverbed;

	obj->min_pos = min_pos;
	obj->max_pos = max_pos;
	obj->heat_point = heat_point;
	obj->humidity_point = humidity_point;
	obj->vertical_blend = vertical_blend;
	obj->weight = weight;

	return obj;
}

void Biome::resolveNodeNames()
{
	getIdFromNrBacklog(&c_top,           "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_filler,        "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_stone,         "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_water_top,     "mapgen_water_source",       CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_water,         "mapgen_water_source",       CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_river_water,   "mapgen_river_water_source", CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_riverbed,      "mapgen_stone",              CONTENT_AIR,    false);
	getIdFromNrBacklog(&c_dust,          "ignore",                    CONTENT_IGNORE, false);
	getIdsFromNrBacklog(&c_cave_liquid);
	getIdFromNrBacklog(&c_dungeon,       "ignore",                    CONTENT_IGNORE, false);
	getIdFromNrBacklog(&c_dungeon_alt,   "ignore",                    CONTENT_IGNORE, false);
	getIdFromNrBacklog(&c_dungeon_stair, "ignore",                    CONTENT_IGNORE, false);
}
