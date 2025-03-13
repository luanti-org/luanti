// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2015-2018 paramat

#pragma once

#include "mapgen.h"

struct MapgenSinglenodeParams : public MapgenParams
{
	MapgenSinglenodeParams() = default;
	~MapgenSinglenodeParams() = default;

	void readParams(const Settings *settings) {}
	void writeParams(Settings *settings) const {}
};

class MapgenSinglenode : public Mapgen
{
public:
	content_t c_node;
	u8 set_light;

	MapgenSinglenode(MapgenParams *params, EmergeParams *emerge);
	~MapgenSinglenode() = default;

	virtual MapgenType getType() const { return MAPGEN_SINGLENODE; }

	void makeChunk(BlockMakeData *data);
	int getSpawnLevelAtPoint(v2s16 p);
};
