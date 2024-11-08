// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2015-2018 paramat

#pragma once

#include "voxel.h"
#include "noise.h"
#include "mapgen.h"

#define VMANIP_FLAG_DUNGEON_INSIDE VOXELFLAG_CHECKED1
#define VMANIP_FLAG_DUNGEON_PRESERVE VOXELFLAG_CHECKED2
#define VMANIP_FLAG_DUNGEON_UNTOUCHABLE (\
		VMANIP_FLAG_DUNGEON_INSIDE|VMANIP_FLAG_DUNGEON_PRESERVE)

class MMVManip;
class NodeDefManager;

v3s16 rand_ortho_dir(PseudoRandom &random, bool diagonal_dirs);
v3s16 turn_xz(v3s16 olddir, int t);
void random_turn(PseudoRandom &random, v3s16 &dir);
int dir_to_facedir(v3s16 d);


struct DungeonParams {
	s32 seed;

	content_t c_wall;
	// Randomly scattered alternative wall nodes
	content_t c_alt_wall;
	content_t c_stair;

	// 3D noise that determines which c_wall nodes are converted to c_alt_wall
	NoiseParams np_alt_wall;

	// Number of dungeons generated in mapchunk. All will use the same set of
	// dungeonparams.
	u16 num_dungeons;
	// Dungeons only generate in ground
	bool only_in_ground;
	// Number of rooms
	u16 num_rooms;
	// Room size random range. Includes walls / floor / ceilng
	v3s16 room_size_min;
	v3s16 room_size_max;
	// Large room size random range. Includes walls / floor / ceilng
	v3s16 room_size_large_min;
	v3s16 room_size_large_max;
	// Value 0 disables large rooms.
	// Value 1 results in 1 large room, the first generated room.
	// Value > 1 makes the first generated room large, all other rooms have a
	// '1 in value' chance of being large.
	u16 large_room_chance;
	// Dimensions of 3D 'brush' that creates corridors.
	// Dimensions are of the empty space, not including walls / floor / ceilng.
	// Diagonal corridors must have hole width >=2 to be passable.
	// Currently, hole width >= 3 causes stair corridor bugs.
	v3s16 holesize;
	// Corridor length random range
	u16 corridor_len_min;
	u16 corridor_len_max;
	// Diagonal corridors are possible, 1 in 4 corridors will be diagonal
	bool diagonal_dirs;
	// Usually 'GENNOTIFY_DUNGEON', but mapgen v6 uses 'GENNOTIFY_TEMPLE' for
	// desert dungeons.
	GenNotifyType notifytype;
};

class DungeonGen {
public:
	MMVManip *vm = nullptr;
	const NodeDefManager *ndef;
	GenerateNotifier *gennotify;

	u32 blockseed;
	PseudoRandom random;
	v3s16 csize;

	content_t c_torch;
	DungeonParams dp;

	// RoomWalker
	v3s16 m_pos;
	v3s16 m_dir;

	DungeonGen(const NodeDefManager *ndef,
		GenerateNotifier *gennotify, DungeonParams *dparams);

	void generate(MMVManip *vm, u32 bseed, v3s16 full_node_min, v3s16 full_node_max);

	void makeDungeon(v3s16 start_padding);
	void makeRoom(v3s16 roomsize, v3s16 roomplace);
	void makeCorridor(v3s16 doorplace, v3s16 doordir,
		v3s16 &result_place, v3s16 &result_dir);
	void makeDoor(v3s16 doorplace, v3s16 doordir);
	void makeFill(v3s16 place, v3s16 size, u8 avoid_flags, MapNode n, u8 or_flags);
	void makeHole(v3s16 place);

	bool findPlaceForDoor(v3s16 &result_place, v3s16 &result_dir);
	bool findPlaceForRoomDoor(v3s16 roomsize, v3s16 &result_doorplace,
			v3s16 &result_doordir, v3s16 &result_roomplace);

	inline void randomizeDir()
	{
		m_dir = rand_ortho_dir(random, dp.diagonal_dirs);
	}
};

extern NoiseParams nparams_dungeon_density;
extern NoiseParams nparams_dungeon_alt_wall;
