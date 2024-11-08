// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "content_mapnode.h"

#include "irrlichttypes_bloated.h"
#include "mapnode.h"
#include "nodedef.h"
#include "nameidmapping.h"
#include "util/string.h"

/*
	Legacy node content type IDs
	Ranges:
	0x000...0x07f (0...127): param2 is fully usable
	126 and 127 are reserved (CONTENT_AIR and CONTENT_IGNORE).
	0x800...0xfff (2048...4095): higher 4 bits of param2 are not usable
*/
#define CONTENT_STONE 0
#define CONTENT_WATER 2
#define CONTENT_TORCH 3
#define CONTENT_WATERSOURCE 9
#define CONTENT_SIGN_WALL 14
#define CONTENT_CHEST 15
#define CONTENT_FURNACE 16
#define CONTENT_LOCKABLE_CHEST 17
#define CONTENT_FENCE 21
#define CONTENT_RAIL 30
#define CONTENT_LADDER 31
#define CONTENT_LAVA 32
#define CONTENT_LAVASOURCE 33
#define CONTENT_GRASS 0x800 //1
#define CONTENT_TREE 0x801 //4
#define CONTENT_LEAVES 0x802 //5
#define CONTENT_GRASS_FOOTSTEPS 0x803 //6
#define CONTENT_MESE 0x804 //7
#define CONTENT_MUD 0x805 //8
#define CONTENT_CLOUD 0x806 //10
#define CONTENT_COALSTONE 0x807 //11
#define CONTENT_WOOD 0x808 //12
#define CONTENT_SAND 0x809 //13
#define CONTENT_COBBLE 0x80a //18
#define CONTENT_STEEL 0x80b //19
#define CONTENT_GLASS 0x80c //20
#define CONTENT_MOSSYCOBBLE 0x80d //22
#define CONTENT_GRAVEL 0x80e //23
#define CONTENT_SANDSTONE 0x80f //24
#define CONTENT_CACTUS 0x810 //25
#define CONTENT_BRICK 0x811 //26
#define CONTENT_CLAY 0x812 //27
#define CONTENT_PAPYRUS 0x813 //28
#define CONTENT_BOOKSHELF 0x814 //29
#define CONTENT_JUNGLETREE 0x815
#define CONTENT_JUNGLEGRASS 0x816
#define CONTENT_NC 0x817
#define CONTENT_NC_RB 0x818
#define CONTENT_APPLE 0x819
#define CONTENT_SAPLING 0x820

/*
	A conversion table for backwards compatibility.
	Maps <=v19 content types to current ones.
	Should never be touched.
*/
content_t trans_table_19[21][2] = {
	{CONTENT_GRASS, 1},
	{CONTENT_TREE, 4},
	{CONTENT_LEAVES, 5},
	{CONTENT_GRASS_FOOTSTEPS, 6},
	{CONTENT_MESE, 7},
	{CONTENT_MUD, 8},
	{CONTENT_CLOUD, 10},
	{CONTENT_COALSTONE, 11},
	{CONTENT_WOOD, 12},
	{CONTENT_SAND, 13},
	{CONTENT_COBBLE, 18},
	{CONTENT_STEEL, 19},
	{CONTENT_GLASS, 20},
	{CONTENT_MOSSYCOBBLE, 22},
	{CONTENT_GRAVEL, 23},
	{CONTENT_SANDSTONE, 24},
	{CONTENT_CACTUS, 25},
	{CONTENT_BRICK, 26},
	{CONTENT_CLAY, 27},
	{CONTENT_PAPYRUS, 28},
	{CONTENT_BOOKSHELF, 29},
};

MapNode mapnode_translate_to_internal(MapNode n_from, u8 version)
{
	MapNode result = n_from;
	if(version <= 19)
	{
		content_t c_from = n_from.getContent();
		for (const auto &tt_i : trans_table_19) {
			if (tt_i[1] == c_from) {
				result.setContent(tt_i[0]);
				break;
			}
		}
	}
	return result;
}

void content_mapnode_get_name_id_mapping(NameIdMapping *nimap)
{
	nimap->set(0, "default:stone");
	nimap->set(2, "default:water_flowing");
	nimap->set(3, "default:torch");
	nimap->set(9, "default:water_source");
	nimap->set(14, "default:sign_wall");
	nimap->set(15, "default:chest");
	nimap->set(16, "default:furnace");
	nimap->set(17, "default:chest_locked");
	nimap->set(21, "default:fence_wood");
	nimap->set(30, "default:rail");
	nimap->set(31, "default:ladder");
	nimap->set(32, "default:lava_flowing");
	nimap->set(33, "default:lava_source");
	nimap->set(0x800, "default:dirt_with_grass");
	nimap->set(0x801, "default:tree");
	nimap->set(0x802, "default:leaves");
	nimap->set(0x803, "default:dirt_with_grass_footsteps");
	nimap->set(0x804, "default:mese");
	nimap->set(0x805, "default:dirt");
	nimap->set(0x806, "default:cloud");
	nimap->set(0x807, "default:coalstone");
	nimap->set(0x808, "default:wood");
	nimap->set(0x809, "default:sand");
	nimap->set(0x80a, "default:cobble");
	nimap->set(0x80b, "default:steelblock");
	nimap->set(0x80c, "default:glass");
	nimap->set(0x80d, "default:mossycobble");
	nimap->set(0x80e, "default:gravel");
	nimap->set(0x80f, "default:sandstone");
	nimap->set(0x810, "default:cactus");
	nimap->set(0x811, "default:brick");
	nimap->set(0x812, "default:clay");
	nimap->set(0x813, "default:papyrus");
	nimap->set(0x814, "default:bookshelf");
	nimap->set(0x815, "default:jungletree");
	nimap->set(0x816, "default:junglegrass");
	nimap->set(0x817, "default:nyancat");
	nimap->set(0x818, "default:nyancat_rainbow");
	nimap->set(0x819, "default:apple");
	nimap->set(0x820, "default:sapling");
	// Static types
	nimap->set(CONTENT_IGNORE, "ignore");
	nimap->set(CONTENT_AIR, "air");
}

