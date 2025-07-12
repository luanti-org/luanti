// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2018 paramat
// Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#include "mapgen.h"

///////// Mapgen V5 flags
#define MGV5_CAVERNS 0x01


extern const FlagDesc flagdesc_mapgen_v5[];

struct MapgenV5Params : public MapgenParams
{
	float cave_width = 0.09f;
	s16 large_cave_depth = -256;
	u16 small_cave_num_min = 0;
	u16 small_cave_num_max = 0;
	u16 large_cave_num_min = 0;
	u16 large_cave_num_max = 2;
	float large_cave_flooded = 0.5f;
	s16 cavern_limit = -256;
	s16 cavern_taper = 256;
	float cavern_threshold = 0.7f;
	s16 dungeon_ymin = -31000;
	s16 dungeon_ymax = 31000;

	NoiseParams np_filler_depth;
	NoiseParams np_factor;
	NoiseParams np_height;
	NoiseParams np_ground;
	NoiseParams np_cave1;
	NoiseParams np_cave2;
	NoiseParams np_cavern;
	NoiseParams np_dungeons;

	MapgenV5Params();
	~MapgenV5Params() = default;

	void readParams(const Settings *settings);
	void writeParams(Settings *settings) const;
	void setDefaultSettings(Settings *settings);
};

class MapgenV5 : public MapgenBasic
{
public:
	MapgenV5(MapgenV5Params *params, EmergeParams *emerge);
	~MapgenV5();

	virtual MapgenType getType() const { return MAPGEN_V5; }

	virtual void makeChunk(BlockMakeData *data);
	int getSpawnLevelAtPoint(v2s16 p);
	int generateBaseTerrain();

private:
	Noise *noise_factor;
	Noise *noise_height;
	Noise *noise_ground;
};
