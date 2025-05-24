// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2015-2018 paramat

#pragma once

#include <unordered_set>
#include "objdef.h"
#include "noise.h"
#include "nodedef.h"

typedef u16 biome_t;  // copy from mg_biome.h to avoid an unnecessary include

class Mapgen;
class MMVManip;
class PcgRandom;
class Schematic;
namespace treegen { struct TreeDef; }

enum DecorationType {
	DECO_SIMPLE,
	DECO_SCHEMATIC,
	DECO_LSYSTEM
};

#define DECO_PLACE_CENTER_X  0x01
#define DECO_PLACE_CENTER_Y  0x02
#define DECO_PLACE_CENTER_Z  0x04
#define DECO_USE_NOISE       0x08
#define DECO_FORCE_PLACEMENT 0x10
#define DECO_LIQUID_SURFACE  0x20
#define DECO_ALL_FLOORS      0x40
#define DECO_ALL_CEILINGS    0x80

extern const FlagDesc flagdesc_deco[];


class Decoration : public ObjDef, public NodeResolver {
public:
	Decoration() = default;
	virtual ~Decoration() = default;

	virtual void resolveNodeNames();

	bool canPlaceDecoration(MMVManip *vm, v3s16 p);
	void placeDeco(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);

	virtual size_t generate(MMVManip *vm, PcgRandom *pr, v3s16 p, bool ceiling) = 0;

	u32 flags = 0;
	int mapseed = 0;
	std::vector<content_t> c_place_on;
	s16 sidelen = 1;
	s16 y_min;
	s16 y_max;
	float fill_ratio = 0.0f;
	NoiseParams np;
	std::vector<content_t> c_spawnby;
	s16 nspawnby;
	s16 place_offset_y = 0;
	s16 check_offset = -1;

	std::unordered_set<biome_t> biomes;

protected:
	void cloneTo(Decoration *def) const;
};


class DecoSimple : public Decoration {
public:
	ObjDef *clone() const;

	virtual void resolveNodeNames();
	virtual size_t generate(MMVManip *vm, PcgRandom *pr, v3s16 p, bool ceiling);

	std::vector<content_t> c_decos;
	s16 deco_height;
	s16 deco_height_max;
	u8 deco_param2;
	u8 deco_param2_max;
};


class DecoSchematic : public Decoration {
public:
	ObjDef *clone() const;

	DecoSchematic() = default;
	virtual ~DecoSchematic();

	virtual size_t generate(MMVManip *vm, PcgRandom *pr, v3s16 p, bool ceiling);

	Rotation rotation;
	Schematic *schematic = nullptr;
	bool was_cloned = false; // see FIXME inside DecoSchemtic::clone()
};


class DecoLSystem : public Decoration {
public:
	ObjDef *clone() const;

	virtual size_t generate(MMVManip *vm, PcgRandom *pr, v3s16 p, bool ceiling);

	// In case it gets cloned it uses the same tree def.
	std::shared_ptr<treegen::TreeDef> tree_def;
};


class DecorationManager : public ObjDefManager {
public:
	DecorationManager(IGameDef *gamedef);
	virtual ~DecorationManager() = default;

	DecorationManager *clone() const;

	const char *getObjectTitle() const
	{
		return "decoration";
	}

	static Decoration *create(DecorationType type)
	{
		switch (type) {
		case DECO_SIMPLE:
			return new DecoSimple;
		case DECO_SCHEMATIC:
			return new DecoSchematic;
		case DECO_LSYSTEM:
			return new DecoLSystem;
		default:
			return NULL;
		}
	}

	void placeAllDecos(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax);

private:
	DecorationManager() {};
};
