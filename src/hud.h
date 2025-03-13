// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2017 red-001 <red-001@outlook.ie>

#pragma once

#include "irrlichttypes_bloated.h"
#include <string>
#include "util/enum_string.h"

#define HUD_DIR_LEFT_RIGHT 0
#define HUD_DIR_RIGHT_LEFT 1
#define HUD_DIR_TOP_BOTTOM 2
#define HUD_DIR_BOTTOM_TOP 3

#define HUD_CORNER_UPPER  0
#define HUD_CORNER_LOWER  1
#define HUD_CORNER_CENTER 2

#define HUD_STYLE_BOLD   1
#define HUD_STYLE_ITALIC 2
#define HUD_STYLE_MONO   4

// Note that these visibility flags do not determine if the hud items are
// actually drawn, but rather, whether to draw the item should the rest
// of the game state permit it.
#define HUD_FLAG_HOTBAR_VISIBLE        (1 << 0)
#define HUD_FLAG_HEALTHBAR_VISIBLE     (1 << 1)
#define HUD_FLAG_CROSSHAIR_VISIBLE     (1 << 2)
#define HUD_FLAG_WIELDITEM_VISIBLE     (1 << 3)
#define HUD_FLAG_BREATHBAR_VISIBLE     (1 << 4)
#define HUD_FLAG_MINIMAP_VISIBLE       (1 << 5)
#define HUD_FLAG_MINIMAP_RADAR_VISIBLE (1 << 6)
#define HUD_FLAG_BASIC_DEBUG           (1 << 7)
#define HUD_FLAG_CHAT_VISIBLE          (1 << 8)

#define HUD_PARAM_HOTBAR_ITEMCOUNT 1
#define HUD_PARAM_HOTBAR_IMAGE 2
#define HUD_PARAM_HOTBAR_SELECTED_IMAGE 3

#define HUD_HOTBAR_ITEMCOUNT_DEFAULT 8
#define HUD_HOTBAR_ITEMCOUNT_MAX     32

#define HOTBAR_IMAGE_SIZE 48

enum HudElementType {
	HUD_ELEM_IMAGE     = 0,
	HUD_ELEM_TEXT      = 1,
	HUD_ELEM_STATBAR   = 2,
	HUD_ELEM_INVENTORY = 3,
	HUD_ELEM_WAYPOINT  = 4,
	HUD_ELEM_IMAGE_WAYPOINT = 5,
	HUD_ELEM_COMPASS   = 6,
	HUD_ELEM_MINIMAP   = 7,
	HUD_ELEM_HOTBAR    = 8,
};

enum HudElementStat : u8 {
	HUD_STAT_POS = 0,
	HUD_STAT_NAME,
	HUD_STAT_SCALE,
	HUD_STAT_TEXT,
	HUD_STAT_NUMBER,
	HUD_STAT_ITEM,
	HUD_STAT_DIR,
	HUD_STAT_ALIGN,
	HUD_STAT_OFFSET,
	HUD_STAT_WORLD_POS,
	HUD_STAT_SIZE,
	HUD_STAT_Z_INDEX,
	HUD_STAT_TEXT2,
	HUD_STAT_STYLE,
	HudElementStat_END // Dummy for validity check
};

enum HudCompassDir {
	HUD_COMPASS_ROTATE = 0,
	HUD_COMPASS_ROTATE_REVERSE,
	HUD_COMPASS_TRANSLATE,
	HUD_COMPASS_TRANSLATE_REVERSE,
};

struct HudElement {
	HudElementType type;
	v2f pos;
	std::string name;
	v2f scale;
	std::string text;
	u32 number;
	u32 item;
	u32 dir;
	v2f align;
	v2f offset;
	v3f world_pos;
	v2s32 size;
	s16 z_index = 0;
	std::string text2;
	u32 style;
};

extern const EnumString es_HudElementType[];
extern const EnumString es_HudElementStat[];
extern const EnumString es_HudBuiltinElement[];

// Minimap stuff

enum MinimapType {
	MINIMAP_TYPE_OFF,
	MINIMAP_TYPE_SURFACE,
	MINIMAP_TYPE_RADAR,
	MINIMAP_TYPE_TEXTURE,
};

