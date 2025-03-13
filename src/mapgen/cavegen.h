// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015-2020 paramat
// Copyright (C) 2010-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#define VMANIP_FLAG_CAVE VOXELFLAG_CHECKED1

typedef u16 biome_t;  // copy from mg_biome.h to avoid an unnecessary include

class GenerateNotifier;

class BiomeGen;

/*
	CavesNoiseIntersection is a cave digging algorithm that carves smooth,
	web-like, continuous tunnels at points where the density of the intersection
	between two separate 3d noises is above a certain value.  This value,
	cave_width, can be modified to set the effective width of these tunnels.

	This algorithm is relatively heavyweight, taking ~80ms to generate an
	80x80x80 chunk of map on a modern processor.  Use sparingly!

	TODO(hmmmm): Remove dependency on biomes
	TODO(hmmmm): Find alternative to overgeneration as solution for sunlight issue
*/
class CavesNoiseIntersection
{
public:
	CavesNoiseIntersection(const NodeDefManager *nodedef,
		BiomeManager *biomemgr, BiomeGen *biomegen, v3s16 chunksize, NoiseParams *np_cave1,
		NoiseParams *np_cave2, s32 seed, float cave_width);
	~CavesNoiseIntersection();

	void generateCaves(MMVManip *vm, v3s16 nmin, v3s16 nmax, biome_t *biomemap);

private:
	const NodeDefManager *m_ndef;
	BiomeManager *m_bmgr;

	BiomeGen *m_bmgn;

	// configurable parameters
	v3s16 m_csize;
	float m_cave_width;

	// intermediate state variables
	u16 m_ystride;
	u16 m_zstride_1d;

	Noise *noise_cave1;
	Noise *noise_cave2;
};

/*
	CavernsNoise is a cave digging algorithm
*/
class CavernsNoise
{
public:
	CavernsNoise(const NodeDefManager *nodedef, v3s16 chunksize,
		NoiseParams *np_cavern, s32 seed, float cavern_limit,
		float cavern_taper, float cavern_threshold);
	~CavernsNoise();

	bool generateCaverns(MMVManip *vm, v3s16 nmin, v3s16 nmax);

private:
	const NodeDefManager *m_ndef;

	// configurable parameters
	v3s16 m_csize;
	float m_cavern_limit;
	float m_cavern_taper;
	float m_cavern_threshold;

	// intermediate state variables
	u16 m_ystride;
	u16 m_zstride_1d;

	Noise *noise_cavern;

	content_t c_water_source;
	content_t c_lava_source;
};

/*
	CavesRandomWalk is an implementation of a cave-digging algorithm that
	operates on the principle of a "random walk" to approximate the stochiastic
	activity of cavern development.

	In summary, this algorithm works by carving a randomly sized tunnel in a
	random direction a random amount of times, randomly varying in width.
	All randomness here is uniformly distributed; alternative distributions have
	not yet been implemented.

	This algorithm is very fast, executing in less than 1ms on average for an
	80x80x80 chunk of map on a modern processor.
*/
class CavesRandomWalk
{
public:
	MMVManip *vm;
	const NodeDefManager *ndef;
	GenerateNotifier *gennotify;
	s16 *heightmap;
	BiomeGen *bmgn;

	s32 seed;
	int water_level;
	float large_cave_flooded;
	// TODO 'np_caveliquids' is deprecated and should eventually be removed.
	// Cave liquids are now defined and located using biome definitions.
	NoiseParams *np_caveliquids;

	u16 ystride;

	s16 min_tunnel_diameter;
	s16 max_tunnel_diameter;
	u16 tunnel_routepoints;
	int part_max_length_rs;

	bool large_cave;
	bool large_cave_is_flat;
	bool flooded;
	bool use_biome_liquid;

	v3s16 node_min;
	v3s16 node_max;

	v3f orp;  // starting point, relative to caved space
	v3s16 of; // absolute coordinates of caved space
	v3s16 ar; // allowed route area
	s16 rs;   // tunnel radius size
	v3f main_direction;

	s16 route_y_min;
	s16 route_y_max;

	PseudoRandom *ps;

	content_t c_water_source;
	content_t c_lava_source;
	content_t c_biome_liquid;

	// ndef is a mandatory parameter.
	// If gennotify is NULL, generation events are not logged.
	// If biomegen is NULL, cave liquids have classic behavior.
	CavesRandomWalk(const NodeDefManager *ndef, GenerateNotifier *gennotify =
		NULL, s32 seed = 0, int water_level = 1, content_t water_source =
		CONTENT_IGNORE, content_t lava_source = CONTENT_IGNORE,
		float large_cave_flooded = 0.5f, BiomeGen *biomegen = NULL);

	// vm and ps are mandatory parameters.
	// If heightmap is NULL, the surface level at all points is assumed to
	// be water_level.
	void makeCave(MMVManip *vm, v3s16 nmin, v3s16 nmax, PseudoRandom *ps,
			bool is_large_cave, int max_stone_height, s16 *heightmap);

private:
	void makeTunnel(bool dirswitch);
	void carveRoute(v3f vec, float f, bool randomize_xz);

	inline bool isPosAboveSurface(v3s16 p);
};

/*
	CavesV6 is the original version of caves used with Mapgen V6.

	Though it uses the same fundamental algorithm as CavesRandomWalk, it is made
	separate to preserve the exact sequence of PseudoRandom calls - any change
	to this ordering results in the output being radically different.
	Because caves in Mapgen V6 are responsible for a large portion of the basic
	terrain shape, modifying this will break our contract of reverse
	compatibility for a 'stable' mapgen such as V6.

	tl;dr,
	*** DO NOT TOUCH THIS CLASS UNLESS YOU KNOW WHAT YOU ARE DOING ***
*/
class CavesV6
{
public:
	MMVManip *vm;
	const NodeDefManager *ndef;
	GenerateNotifier *gennotify;
	PseudoRandom *ps;
	PseudoRandom *ps2;

	// configurable parameters
	s16 *heightmap;
	content_t c_water_source;
	content_t c_lava_source;
	int water_level;

	// intermediate state variables
	u16 ystride;

	s16 min_tunnel_diameter;
	s16 max_tunnel_diameter;
	u16 tunnel_routepoints;
	int part_max_length_rs;

	bool large_cave;
	bool large_cave_is_flat;

	v3s16 node_min;
	v3s16 node_max;

	v3f orp;  // starting point, relative to caved space
	v3s16 of; // absolute coordinates of caved space
	v3s16 ar; // allowed route area
	s16 rs;   // tunnel radius size
	v3f main_direction;

	s16 route_y_min;
	s16 route_y_max;

	// ndef is a mandatory parameter.
	// If gennotify is NULL, generation events are not logged.
	CavesV6(const NodeDefManager *ndef, GenerateNotifier *gennotify = NULL,
			int water_level = 1, content_t water_source = CONTENT_IGNORE,
			content_t lava_source = CONTENT_IGNORE);

	// vm, ps, and ps2 are mandatory parameters.
	// If heightmap is NULL, the surface level at all points is assumed to
	// be water_level.
	void makeCave(MMVManip *vm, v3s16 nmin, v3s16 nmax, PseudoRandom *ps,
			PseudoRandom *ps2, bool is_large_cave, int max_stone_height,
			s16 *heightmap = NULL);

private:
	void makeTunnel(bool dirswitch);
	void carveRoute(v3f vec, float f, bool randomize_xz, bool tunnel_above_ground);

	inline s16 getSurfaceFromHeightmap(v3s16 p);
};
