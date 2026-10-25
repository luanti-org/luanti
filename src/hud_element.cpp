// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "hud_element.h"

const struct EnumString es_HudElementType[] =
{
	{HUD_ELEM_IMAGE,     "image"},
	{HUD_ELEM_TEXT,      "text"},
	{HUD_ELEM_STATBAR,   "statbar"},
	{HUD_ELEM_INVENTORY, "inventory"},
	{HUD_ELEM_WAYPOINT,  "waypoint"},
	{HUD_ELEM_IMAGE_WAYPOINT, "image_waypoint"},
	{HUD_ELEM_COMPASS,   "compass"},
	{HUD_ELEM_MINIMAP,   "minimap"},
	{HUD_ELEM_HOTBAR,    "hotbar"},
	{0, NULL},
};

const struct EnumString es_HudElementStat[] =
{
	{HUD_STAT_POS,    "position"},
	{HUD_STAT_POS,    "pos"}, /* Deprecated, only for compatibility's sake */
	{HUD_STAT_NAME,   "name"},
	{HUD_STAT_SCALE,  "scale"},
	{HUD_STAT_TEXT,   "text"},
	{HUD_STAT_NUMBER, "number"},
	{HUD_STAT_ITEM,   "item"},
	{HUD_STAT_ITEM,   "precision"},
	{HUD_STAT_DIR,    "direction"},
	{HUD_STAT_ALIGN,  "alignment"},
	{HUD_STAT_OFFSET, "offset"},
	{HUD_STAT_WORLD_POS, "world_pos"},
	{HUD_STAT_SIZE,    "size"},
	{HUD_STAT_Z_INDEX, "z_index"},
	{HUD_STAT_TEXT2,   "text2"},
	{HUD_STAT_STYLE,   "style"},
	{HUD_STAT_HIDEABLE, "hideable"},
	{0, NULL},
};

void apply_hud_stat(HudElementStat stat, const HudElementStatValue &value, HudElement *elem)
{
	switch (stat) {
		case HUD_STAT_POS:      elem->pos      = std::get<v2f>(value);         break;
		case HUD_STAT_NAME:     elem->name     = std::get<std::string>(value); break;
		case HUD_STAT_SCALE:    elem->scale    = std::get<v2f>(value);         break;
		case HUD_STAT_TEXT:     elem->text     = std::get<std::string>(value); break;
		case HUD_STAT_NUMBER:   elem->number   = std::get<u32>(value);         break;
		case HUD_STAT_ITEM:     elem->item     = std::get<u32>(value);         break;
		case HUD_STAT_DIR:      elem->dir      = std::get<u32>(value);         break;
		case HUD_STAT_ALIGN:    elem->align    = std::get<v2f>(value);         break;
		case HUD_STAT_OFFSET:   elem->offset   = std::get<v2f>(value);         break;
		case HUD_STAT_WORLD_POS: elem->world_pos = std::get<v3f>(value);       break;
		case HUD_STAT_SIZE:     elem->size     = std::get<v2f>(value);         break;
		case HUD_STAT_Z_INDEX:  elem->z_index  = std::get<s16>(value);         break;
		case HUD_STAT_TEXT2:    elem->text2    = std::get<std::string>(value); break;
		case HUD_STAT_STYLE:    elem->style    = std::get<u32>(value);         break;
		case HUD_STAT_HIDEABLE: elem->hideable = std::get<bool>(value);        break;
		case HudElementStat_END:
			break;
	}
}

const struct EnumString es_HudBuiltinElement[] =
{
	{HUD_FLAG_HOTBAR_VISIBLE,        "hotbar"},
	{HUD_FLAG_HEALTHBAR_VISIBLE,     "healthbar"},
	{HUD_FLAG_CROSSHAIR_VISIBLE,     "crosshair"},
	{HUD_FLAG_WIELDITEM_VISIBLE,     "wielditem"},
	{HUD_FLAG_BREATHBAR_VISIBLE,     "breathbar"},
	{HUD_FLAG_MINIMAP_VISIBLE,       "minimap"},
	{HUD_FLAG_MINIMAP_RADAR_VISIBLE, "minimap_radar"},
	{HUD_FLAG_BASIC_DEBUG,           "basic_debug"},
	{HUD_FLAG_CHAT_VISIBLE,          "chat"},
	{0, NULL},
};
