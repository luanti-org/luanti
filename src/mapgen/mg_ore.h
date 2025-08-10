// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015-2020 paramat
// Copyright (C) 2014-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#include <unordered_set>
#include "objdef.h"
#include "noise.h"
#include "nodedef.h"

typedef u16 biome_t;  // copy from mg_biome.h to avoid an unnecessary include

class Noise;
class Mapgen;
class MMVManip;

/////////////////// Ore generation flags

#define OREFLAG_ABSHEIGHT     0x01 // Non-functional but kept to not break flags
#define OREFLAG_PUFF_CLIFFS   0x02
#define OREFLAG_PUFF_ADDITIVE 0x04
#define OREFLAG_USE_NOISE     0x08
#define OREFLAG_USE_NOISE2    0x10

enum OreType {
	ORE_SCATTER,
	ORE_SHEET,
	ORE_PUFF,
	ORE_BLOB,
	ORE_VEIN,
	ORE_STRATUM,
};

extern const FlagDesc flagdesc_ore[];

class Ore : public ObjDef, public NodeResolver {
public:
	const bool needs_noise;

	content_t c_ore;                  // the node to place
	std::vector<content_t> c_wherein; // the nodes to be placed in
	u32 clust_scarcity; // ore cluster has a 1-in-clust_scarcity chance of appearing at a node
	s16 clust_num_ores; // how many ore nodes are in a chunk
	s16 clust_size;     // how large (in nodes) a chunk of ore is
	s16 y_min;
	s16 y_max;
	u8 ore_param2;		// to set node-specific attributes
	u32 flags = 0;          // attributes for this ore
	float nthresh;      // threshold for noise at which an ore is placed
	NoiseParams np;     // noise for distribution of clusters (NULL for uniform scattering)
	Noise *noise = nullptr;
	std::unordered_set<biome_t> biomes;

	explicit Ore(bool needs_noise): needs_noise(needs_noise) {}
	virtual ~Ore();

	virtual void resolveNodeNames();

	size_t placeOre(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);
	virtual void generate(MMVManip *vm, int mapseed, u32 blockseed,
		v3s16 nmin, v3s16 nmax, biome_t *biomemap) = 0;

protected:
	void cloneTo(Ore *def) const;
};

class OreScatter : public Ore {
public:
	OreScatter() : Ore(false) {}

	ObjDef *clone() const override;

	void generate(MMVManip *vm, int mapseed, u32 blockseed,
			v3s16 nmin, v3s16 nmax, biome_t *biomemap) override;
};

class OreSheet : public Ore {
public:
	OreSheet() : Ore(true) {}

	ObjDef *clone() const override;

	u16 column_height_min;
	u16 column_height_max;
	float column_midpoint_factor;

	void generate(MMVManip *vm, int mapseed, u32 blockseed,
			v3s16 nmin, v3s16 nmax, biome_t *biomemap) override;
};

class OrePuff : public Ore {
public:
	ObjDef *clone() const override;

	NoiseParams np_puff_top;
	NoiseParams np_puff_bottom;
	Noise *noise_puff_top = nullptr;
	Noise *noise_puff_bottom = nullptr;

	OrePuff() : Ore(true) {}
	virtual ~OrePuff();

	void generate(MMVManip *vm, int mapseed, u32 blockseed,
			v3s16 nmin, v3s16 nmax, biome_t *biomemap) override;
};

class OreBlob : public Ore {
public:
	ObjDef *clone() const override;

	OreBlob() : Ore(true) {}
	void generate(MMVManip *vm, int mapseed, u32 blockseed,
			v3s16 nmin, v3s16 nmax, biome_t *biomemap) override;
};

class OreVein : public Ore {
public:
	ObjDef *clone() const override;

	float random_factor;
	Noise *noise2 = nullptr;
	int sizey_prev = 0;

	OreVein() : Ore(true) {}
	virtual ~OreVein();

	void generate(MMVManip *vm, int mapseed, u32 blockseed,
			v3s16 nmin, v3s16 nmax, biome_t *biomemap) override;
};

class OreStratum : public Ore {
public:
	ObjDef *clone() const override;

	NoiseParams np_stratum_thickness;
	Noise *noise_stratum_thickness = nullptr;
	u16 stratum_thickness;

	OreStratum() : Ore(false) {}
	virtual ~OreStratum();

	void generate(MMVManip *vm, int mapseed, u32 blockseed,
			v3s16 nmin, v3s16 nmax, biome_t *biomemap) override;
};

class OreManager : public ObjDefManager {
public:
	OreManager(IGameDef *gamedef);
	virtual ~OreManager() = default;

	OreManager *clone() const;

	const char *getObjectTitle() const
	{
		return "ore";
	}

	static Ore *create(OreType type)
	{
		switch (type) {
		case ORE_SCATTER:
			return new OreScatter;
		case ORE_SHEET:
			return new OreSheet;
		case ORE_PUFF:
			return new OrePuff;
		case ORE_BLOB:
			return new OreBlob;
		case ORE_VEIN:
			return new OreVein;
		case ORE_STRATUM:
			return new OreStratum;
		}
		return nullptr;
	}

	void clear();

	size_t placeAllOres(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);

private:
	OreManager() {};
};
