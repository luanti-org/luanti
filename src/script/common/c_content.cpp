// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
#include "common/c_content.h"
#include "common/c_converter.h"
#include "common/c_types.h"
#include "nodedef.h"
#include "object_properties.h"
#include "collision.h"
#include "cpp_api/s_node.h"
#include "lua_api/l_object.h"
#include "lua_api/l_item.h"
#include "common/c_internal.h"
#include "server.h"
#include "log.h"
#include "tool.h"
#include "porting.h"
#include "mapgen/mg_schematic.h"
#include "noise.h"
#include "server/player_sao.h"
#include "util/pointedthing.h"
#include "debug.h" // For FATAL_ERROR
#include <SColor.h>
#include <json/json.h>
#include "mapgen/treegen.h"

struct EnumString es_TileAnimationType[] =
{
	{TAT_NONE, "none"},
	{TAT_VERTICAL_FRAMES, "vertical_frames"},
	{TAT_SHEET_2D, "sheet_2d"},
	{0, nullptr},
};

struct EnumString es_ItemType[] =
{
	{ITEM_NONE, "none"},
	{ITEM_NODE, "node"},
	{ITEM_CRAFT, "craft"},
	{ITEM_TOOL, "tool"},
	{0, NULL},
};

struct EnumString es_TouchInteractionMode[] =
{
	{LONG_DIG_SHORT_PLACE, "long_dig_short_place"},
	{SHORT_DIG_LONG_PLACE, "short_dig_long_place"},
	{TouchInteractionMode_USER, "user"},
	{0, NULL},
};

/******************************************************************************/
void read_item_definition(lua_State* L, int index,
		const ItemDefinition &default_def, ItemDefinition &def)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	def.name.clear();
	getstringfield(L, index, "name", def.name);

	{
		auto str = getstringfield_default(L, index, "type", "");
		if (!string_to_enum(es_ItemType, def.type, str))
			warningstream << "Item " << def.name
				<< " has unknown type \"" << str << '"' << std::endl;
	}

	getstringfield(L, index, "description", def.description);
	getstringfield(L, index, "short_description", def.short_description);
	getstringfield(L, index, "inventory_image", def.inventory_image);
	getstringfield(L, index, "inventory_overlay", def.inventory_overlay);
	getstringfield(L, index, "wield_image", def.wield_image);
	getstringfield(L, index, "wield_overlay", def.wield_overlay);
	getstringfield(L, index, "palette", def.palette_image);

	// Read item color.
	lua_getfield(L, index, "color");
	read_color(L, -1, &def.color);
	lua_pop(L, 1);

	lua_getfield(L, index, "wield_scale");
	if(lua_istable(L, -1)){
		def.wield_scale = check_v3f(L, -1);
	}
	lua_pop(L, 1);

	int stack_max = getintfield_default(L, index, "stack_max", def.stack_max);
	def.stack_max = rangelim(stack_max, 1, U16_MAX);

	lua_getfield(L, index, "on_use");
	def.usable = lua_isfunction(L, -1);
	lua_pop(L, 1);

	getboolfield(L, index, "liquids_pointable", def.liquids_pointable);

	lua_getfield(L, index, "pointabilities");
	if(lua_istable(L, -1)){
		def.pointabilities = std::make_optional<Pointabilities>(
				read_pointabilities(L, -1));
	}

	lua_getfield(L, index, "tool_capabilities");
	if(lua_istable(L, -1)){
		def.tool_capabilities = new ToolCapabilities(
				read_tool_capabilities(L, -1));
	}
	lua_getfield(L, index, "wear_color");
	if (lua_istable(L, -1)) {
		def.wear_bar_params = read_wear_bar_params(L, -1);
	} else if (lua_isstring(L, -1)) {
		video::SColor color;
		read_color(L, -1, &color);
		def.wear_bar_params = WearBarParams({{0.0f, color}},
				WearBarParams::BLEND_MODE_CONSTANT);
	}

	// If name is "" (hand), ensure there are ToolCapabilities
	// because it will be looked up there whenever any other item has
	// no ToolCapabilities
	if (def.name.empty() && def.tool_capabilities == NULL){
		def.tool_capabilities = new ToolCapabilities();
	}

	lua_getfield(L, index, "groups");
	read_groups(L, -1, def.groups);
	lua_pop(L, 1);

	lua_getfield(L, index, "sounds");
	if (!lua_isnil(L, -1)) {
		luaL_checktype(L, -1, LUA_TTABLE);
		lua_getfield(L, -1, "place");
		read_simplesoundspec(L, -1, def.sound_place);
		lua_pop(L, 1);
		lua_getfield(L, -1, "place_failed");
		read_simplesoundspec(L, -1, def.sound_place_failed);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	// No, this is not a mistake. Item sounds are in "sound", node sounds in "sounds".
	lua_getfield(L, index, "sound");
	if (!lua_isnil(L, -1)) {
		luaL_checktype(L, -1, LUA_TTABLE);
		lua_getfield(L, -1, "punch_use");
		read_simplesoundspec(L, -1, def.sound_use);
		lua_pop(L, 1);
		lua_getfield(L, -1, "punch_use_air");
		read_simplesoundspec(L, -1, def.sound_use_air);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	def.range = getfloatfield_default(L, index, "range", def.range);

	// Client shall immediately place this node when player places the item.
	// Server will update the precise end result a moment later.
	// "" = no prediction
	getstringfield(L, index, "node_placement_prediction",
			def.node_placement_prediction);

	int place_param2;
	if (getintfield(L, index, "place_param2", place_param2))
		def.place_param2 = rangelim(place_param2, 0, U8_MAX);

	getboolfield(L, index, "wallmounted_rotate_vertical", def.wallmounted_rotate_vertical);

	TouchInteraction &inter = def.touch_interaction;
	lua_getfield(L, index, "touch_interaction");
	if (lua_istable(L, -1)) {
		inter.pointed_nothing = (TouchInteractionMode)getenumfield(L, -1, "pointed_nothing",
				es_TouchInteractionMode, inter.pointed_nothing);
		inter.pointed_node = (TouchInteractionMode)getenumfield(L, -1, "pointed_node",
				es_TouchInteractionMode, inter.pointed_node);
		inter.pointed_object = (TouchInteractionMode)getenumfield(L, -1, "pointed_object",
				es_TouchInteractionMode, inter.pointed_object);
	} else if (lua_isstring(L, -1)) {
		int value;
		if (string_to_enum(es_TouchInteractionMode, value, lua_tostring(L, -1))) {
			inter.pointed_nothing = (TouchInteractionMode)value;
			inter.pointed_node    = (TouchInteractionMode)value;
			inter.pointed_object  = (TouchInteractionMode)value;
		}
	} else if (!lua_isnil(L, -1)) {
		throw LuaError("invalid type for 'touch_interaction'");
	}
	lua_pop(L, 1);
}

/******************************************************************************/
void push_item_definition(lua_State *L, const ItemDefinition &i)
{
	lua_newtable(L);
	lua_pushstring(L, i.name.c_str());
	lua_setfield(L, -2, "name");
	lua_pushstring(L, i.description.c_str());
	lua_setfield(L, -2, "description");
}

void push_item_definition_full(lua_State *L, const ItemDefinition &i)
{
	std::string type(enum_to_string(es_ItemType, i.type));

	lua_newtable(L);
	lua_pushstring(L, i.name.c_str());
	lua_setfield(L, -2, "name");
	lua_pushstring(L, i.description.c_str());
	lua_setfield(L, -2, "description");
	if (!i.short_description.empty()) {
		lua_pushstring(L, i.short_description.c_str());
		lua_setfield(L, -2, "short_description");
	}
	lua_pushstring(L, type.c_str());
	lua_setfield(L, -2, "type");
	lua_pushstring(L, i.inventory_image.c_str());
	lua_setfield(L, -2, "inventory_image");
	lua_pushstring(L, i.inventory_overlay.c_str());
	lua_setfield(L, -2, "inventory_overlay");
	lua_pushstring(L, i.wield_image.c_str());
	lua_setfield(L, -2, "wield_image");
	lua_pushstring(L, i.wield_overlay.c_str());
	lua_setfield(L, -2, "wield_overlay");
	lua_pushstring(L, i.palette_image.c_str());
	lua_setfield(L, -2, "palette_image");
	push_ARGB8(L, i.color);
	lua_setfield(L, -2, "color");
	push_v3f(L, i.wield_scale);
	lua_setfield(L, -2, "wield_scale");
	lua_pushinteger(L, i.stack_max);
	lua_setfield(L, -2, "stack_max");
	lua_pushboolean(L, i.usable);
	lua_setfield(L, -2, "usable");
	lua_pushboolean(L, i.liquids_pointable);
	lua_setfield(L, -2, "liquids_pointable");
	if (i.pointabilities) {
		push_pointabilities(L, *i.pointabilities);
		lua_setfield(L, -2, "pointabilities");
	}
	if (i.tool_capabilities) {
		push_tool_capabilities(L, *i.tool_capabilities);
		lua_setfield(L, -2, "tool_capabilities");
	}
	if (i.wear_bar_params.has_value()) {
		push_wear_bar_params(L, *i.wear_bar_params);
		lua_setfield(L, -2, "wear_color");
	}
	push_groups(L, i.groups);
	lua_setfield(L, -2, "groups");
	push_simplesoundspec(L, i.sound_place);
	lua_setfield(L, -2, "sound_place");
	push_simplesoundspec(L, i.sound_place_failed);
	lua_setfield(L, -2, "sound_place_failed");
	lua_pushstring(L, i.node_placement_prediction.c_str());
	lua_setfield(L, -2, "node_placement_prediction");
	lua_pushboolean(L, i.wallmounted_rotate_vertical);
	lua_setfield(L, -2, "wallmounted_rotate_vertical");

	lua_createtable(L, 0, 3);
	const TouchInteraction &inter = i.touch_interaction;
	lua_pushstring(L, enum_to_string(es_TouchInteractionMode, inter.pointed_nothing));
	lua_setfield(L, -2,"pointed_nothing");
	lua_pushstring(L, enum_to_string(es_TouchInteractionMode, inter.pointed_node));
	lua_setfield(L, -2,"pointed_node");
	lua_pushstring(L, enum_to_string(es_TouchInteractionMode, inter.pointed_object));
	lua_setfield(L, -2,"pointed_object");
	lua_setfield(L, -2, "touch_interaction");
}

/******************************************************************************/
const std::array<const char *, 33> object_property_keys = {
	"hp_max",
	"breath_max",
	"physical",
	"collide_with_objects",
	"collisionbox",
	"selectionbox",
	"pointable",
	"visual",
	"mesh",
	"visual_size",
	"textures",
	"colors",
	"spritediv",
	"initial_sprite_basepos",
	"is_visible",
	"makes_footstep_sound",
	"stepheight",
	"eye_height",
	"automatic_rotate",
	"automatic_face_movement_dir",
	"backface_culling",
	"glow",
	"nametag",
	"nametag_color",
	"automatic_face_movement_max_rotation_per_sec",
	"infotext",
	"static_save",
	"wield_item",
	"zoom_fov",
	"use_texture_alpha",
	"shaded",
	"damage_texture_modifier",
	"show_on_minimap",
	// "node" is intentionally not here as it's gated behind `fallback` below!
};

/******************************************************************************/
void read_object_properties(lua_State *L, int index,
		ServerActiveObject *sao, ObjectProperties *prop, IItemDefManager *idef,
		bool fallback)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;
	if (lua_isnil(L, index))
		return;

	luaL_checktype(L, -1, LUA_TTABLE);

	int hp_max = 0;
	if (getintfield(L, -1, "hp_max", hp_max)) {
		prop->hp_max = (u16)rangelim(hp_max, 0, U16_MAX);
		// hp_max = 0 is sometimes used as a hack to keep players dead, only validate for entities
		if (prop->hp_max == 0 && sao->getType() != ACTIVEOBJECT_TYPE_PLAYER)
			throw LuaError("The hp_max property may not be 0 for entities!");

		if (prop->hp_max < sao->getHP()) {
			PlayerHPChangeReason reason(PlayerHPChangeReason::SET_HP_MAX);
			sao->setHP(prop->hp_max, reason);
		}
	}

	if (getintfield(L, -1, "breath_max", prop->breath_max)) {
		if (sao->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			PlayerSAO *player = (PlayerSAO *)sao;
			if (prop->breath_max < player->getBreath())
				player->setBreath(prop->breath_max);
		}
	}
	getboolfield(L, -1, "physical", prop->physical);
	getboolfield(L, -1, "collide_with_objects", prop->collideWithObjects);

	lua_getfield(L, -1, "collisionbox");
	bool collisionbox_defined = lua_istable(L, -1);
	if (collisionbox_defined)
		prop->collisionbox = read_aabb3f(L, -1, 1.0);
	lua_pop(L, 1);

	lua_getfield(L, -1, "selectionbox");
	if (lua_istable(L, -1)) {
		getboolfield(L, -1, "rotate", prop->rotate_selectionbox);
		prop->selectionbox = read_aabb3f(L, -1, 1.0);
	} else if (collisionbox_defined) {
		prop->selectionbox = prop->collisionbox;
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "pointable");
	if(!lua_isnil(L, -1)){
		prop->pointable = read_pointability_type(L, -1);
	}
	lua_pop(L, 1);

	// Don't set if nil
	std::string visual;
	if (getstringfield(L, -1, "visual", visual)) {
		if (!string_to_enum(es_ObjectVisual, prop->visual, visual)) {
			script_log_unique(L, "Unsupported ObjectVisual: " + visual, warningstream);
			prop->visual = OBJECTVISUAL_UNKNOWN;
		}
	}

	getstringfield(L, -1, "mesh", prop->mesh);

	lua_getfield(L, -1, "visual_size");
	if (lua_istable(L, -1)) {
		// Backwards compatibility: Also accept { x = ?, y = ? }
		v2f scale_xy = read_v2f(L, -1);

		f32 scale_z = scale_xy.X;
		lua_getfield(L, -1, "z");
		if (lua_isnumber(L, -1))
			scale_z = lua_tonumber(L, -1);
		lua_pop(L, 1);

		prop->visual_size = v3f(scale_xy.X, scale_xy.Y, scale_z);
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "textures");
	if(lua_istable(L, -1)){
		prop->textures.clear();
		int table = lua_gettop(L);
		lua_pushnil(L);
		while(lua_next(L, table) != 0){
			// key at index -2 and value at index -1
			if(lua_isstring(L, -1))
				prop->textures.emplace_back(lua_tostring(L, -1));
			else
				prop->textures.emplace_back("");
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, -1, "colors");
	if (lua_istable(L, -1)) {
		int table = lua_gettop(L);
		prop->colors.clear();
		for (lua_pushnil(L); lua_next(L, table); lua_pop(L, 1)) {
			video::SColor color(255, 255, 255, 255);
			read_color(L, -1, &color);
			prop->colors.push_back(color);
		}
	}
	lua_pop(L, 1);

	// This hack exists because the name 'node' easily collides with mods own
	// usage (or in this case literally builtin/game/falling.lua).
	if (!fallback) {
		lua_getfield(L, -1, "node");
		if (lua_istable(L, -1)) {
			prop->node = readnode(L, -1);
		}
		lua_pop(L, 1);
	}

	lua_getfield(L, -1, "spritediv");
	if(lua_istable(L, -1))
		prop->spritediv = read_v2s16(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, -1, "initial_sprite_basepos");
	if(lua_istable(L, -1))
		prop->initial_sprite_basepos = read_v2s16(L, -1);
	lua_pop(L, 1);

	getboolfield(L, -1, "is_visible", prop->is_visible);
	getboolfield(L, -1, "makes_footstep_sound", prop->makes_footstep_sound);
	if (getfloatfield(L, -1, "stepheight", prop->stepheight))
		prop->stepheight *= BS;
	getfloatfield(L, -1, "eye_height", prop->eye_height);

	getfloatfield(L, -1, "automatic_rotate", prop->automatic_rotate);
	lua_getfield(L, -1, "automatic_face_movement_dir");
	if (lua_isnumber(L, -1)) {
		prop->automatic_face_movement_dir = true;
		prop->automatic_face_movement_dir_offset = luaL_checknumber(L, -1);
	} else if (lua_isboolean(L, -1)) {
		prop->automatic_face_movement_dir = lua_toboolean(L, -1);
		prop->automatic_face_movement_dir_offset = 0;
	}
	lua_pop(L, 1);
	getfloatfield(L, -1, "automatic_face_movement_max_rotation_per_sec",
		prop->automatic_face_movement_max_rotation_per_sec);

	getboolfield(L, -1, "backface_culling", prop->backface_culling);
	getintfield(L, -1, "glow", prop->glow);

	getstringfield(L, -1, "nametag", prop->nametag);
	lua_getfield(L, -1, "nametag_color");
	if (!lua_isnil(L, -1)) {
		video::SColor color = prop->nametag_color;
		if (read_color(L, -1, &color))
			prop->nametag_color = color;
	}
	lua_pop(L, 1);
	lua_getfield(L, -1, "nametag_bgcolor");
	if (!lua_isnil(L, -1)) {
		if (lua_toboolean(L, -1)) {
			video::SColor color;
			if (read_color(L, -1, &color))
				prop->nametag_bgcolor = color;
		} else {
			prop->nametag_bgcolor = std::nullopt;
		}
	}
	lua_pop(L, 1);

	getstringfield(L, -1, "infotext", prop->infotext);
	getboolfield(L, -1, "static_save", prop->static_save);

	lua_getfield(L, -1, "wield_item");
	if (!lua_isnil(L, -1))
		prop->wield_item = read_item(L, -1, idef).getItemString();
	lua_pop(L, 1);

	getfloatfield(L, -1, "zoom_fov", prop->zoom_fov);
	getboolfield(L, -1, "use_texture_alpha", prop->use_texture_alpha);
	getboolfield(L, -1, "shaded", prop->shaded);
	getboolfield(L, -1, "show_on_minimap", prop->show_on_minimap);

	getstringfield(L, -1, "damage_texture_modifier", prop->damage_texture_modifier);

	// Remember to update object_property_keys above
	// when adding a new property
}

/******************************************************************************/
void push_object_properties(lua_State *L, const ObjectProperties *prop)
{
	lua_newtable(L);
	lua_pushnumber(L, prop->hp_max);
	lua_setfield(L, -2, "hp_max");
	lua_pushnumber(L, prop->breath_max);
	lua_setfield(L, -2, "breath_max");
	lua_pushboolean(L, prop->physical);
	lua_setfield(L, -2, "physical");
	lua_pushboolean(L, prop->collideWithObjects);
	lua_setfield(L, -2, "collide_with_objects");
	push_aabb3f(L, prop->collisionbox);
	lua_setfield(L, -2, "collisionbox");
	push_aabb3f(L, prop->selectionbox);
	lua_pushboolean(L, prop->rotate_selectionbox);
	lua_setfield(L, -2, "rotate");
	lua_setfield(L, -2, "selectionbox");
	push_pointability_type(L, prop->pointable);
	lua_setfield(L, -2, "pointable");
	lua_pushstring(L, enum_to_string(es_ObjectVisual, prop->visual));
	lua_setfield(L, -2, "visual");
	lua_pushlstring(L, prop->mesh.c_str(), prop->mesh.size());
	lua_setfield(L, -2, "mesh");
	push_v3f(L, prop->visual_size);
	lua_setfield(L, -2, "visual_size");

	lua_createtable(L, prop->textures.size(), 0);
	u16 i = 1;
	for (const std::string &texture : prop->textures) {
		lua_pushlstring(L, texture.c_str(), texture.size());
		lua_rawseti(L, -2, i++);
	}
	lua_setfield(L, -2, "textures");

	lua_createtable(L, prop->colors.size(), 0);
	i = 1;
	for (const video::SColor &color : prop->colors) {
		push_ARGB8(L, color);
		lua_rawseti(L, -2, i++);
	}
	lua_setfield(L, -2, "colors");

	pushnode(L, prop->node);
	lua_setfield(L, -2, "node");
	push_v2s16(L, prop->spritediv);
	lua_setfield(L, -2, "spritediv");
	push_v2s16(L, prop->initial_sprite_basepos);
	lua_setfield(L, -2, "initial_sprite_basepos");
	lua_pushboolean(L, prop->is_visible);
	lua_setfield(L, -2, "is_visible");
	lua_pushboolean(L, prop->makes_footstep_sound);
	lua_setfield(L, -2, "makes_footstep_sound");
	lua_pushnumber(L, prop->stepheight / BS);
	lua_setfield(L, -2, "stepheight");
	lua_pushnumber(L, prop->eye_height);
	lua_setfield(L, -2, "eye_height");
	lua_pushnumber(L, prop->automatic_rotate);
	lua_setfield(L, -2, "automatic_rotate");
	if (prop->automatic_face_movement_dir)
		lua_pushnumber(L, prop->automatic_face_movement_dir_offset);
	else
		lua_pushboolean(L, false);
	lua_setfield(L, -2, "automatic_face_movement_dir");
	lua_pushnumber(L, prop->automatic_face_movement_max_rotation_per_sec);
	lua_setfield(L, -2, "automatic_face_movement_max_rotation_per_sec");
	lua_pushboolean(L, prop->backface_culling);
	lua_setfield(L, -2, "backface_culling");
	lua_pushnumber(L, prop->glow);
	lua_setfield(L, -2, "glow");
	lua_pushlstring(L, prop->nametag.c_str(), prop->nametag.size());
	lua_setfield(L, -2, "nametag");
	push_ARGB8(L, prop->nametag_color);
	lua_setfield(L, -2, "nametag_color");
	if (prop->nametag_bgcolor) {
		push_ARGB8(L, prop->nametag_bgcolor.value());
		lua_setfield(L, -2, "nametag_bgcolor");
	} else {
		lua_pushboolean(L, false);
		lua_setfield(L, -2, "nametag_bgcolor");
	}
	lua_pushlstring(L, prop->infotext.c_str(), prop->infotext.size());
	lua_setfield(L, -2, "infotext");
	lua_pushboolean(L, prop->static_save);
	lua_setfield(L, -2, "static_save");
	lua_pushlstring(L, prop->wield_item.c_str(), prop->wield_item.size());
	lua_setfield(L, -2, "wield_item");
	lua_pushnumber(L, prop->zoom_fov);
	lua_setfield(L, -2, "zoom_fov");
	lua_pushboolean(L, prop->use_texture_alpha);
	lua_setfield(L, -2, "use_texture_alpha");
	lua_pushboolean(L, prop->shaded);
	lua_setfield(L, -2, "shaded");
	lua_pushlstring(L, prop->damage_texture_modifier.c_str(), prop->damage_texture_modifier.size());
	lua_setfield(L, -2, "damage_texture_modifier");
	lua_pushboolean(L, prop->show_on_minimap);
	lua_setfield(L, -2, "show_on_minimap");

	// Remember to update object_property_keys above
	// when adding a new property
}

/******************************************************************************/
TileDef read_tiledef(lua_State *L, int index, u8 drawtype, bool special)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	TileDef tiledef;

	bool default_tiling = true;
	bool default_culling = true;
	switch (drawtype) {
		case NDT_PLANTLIKE:
		case NDT_FIRELIKE:
			default_tiling = false;
			[[fallthrough]];
		case NDT_MESH:
		case NDT_LIQUID:
			default_culling = false;
			break;
		case NDT_PLANTLIKE_ROOTED:
			default_tiling = !special;
			default_culling = !special;
			break;
		default:
			break;
	}

	if(lua_isstring(L, index)){
		// "default_lava.png"
		tiledef.name = lua_tostring(L, index);
		tiledef.tileable_vertical = default_tiling;
		tiledef.tileable_horizontal = default_tiling;
		tiledef.backface_culling = default_culling;
	}
	else if(lua_istable(L, index))
	{
		// name="default_lava.png"
		tiledef.name.clear();
		getstringfield(L, index, "name", tiledef.name);
		if (getstringfield(L, index, "image", tiledef.name)) {
			log_deprecated(L, "Field \"image\" on TileDef is deprecated, "
					"use \"name\" instead.", 2);
		}

		tiledef.backface_culling = getboolfield_default(
			L, index, "backface_culling", default_culling);
		tiledef.tileable_horizontal = getboolfield_default(
			L, index, "tileable_horizontal", default_tiling);
		tiledef.tileable_vertical = getboolfield_default(
			L, index, "tileable_vertical", default_tiling);
		std::string align_style;
		if (getstringfield(L, index, "align_style", align_style)) {
			if (align_style == "user")
				tiledef.align_style = ALIGN_STYLE_USER_DEFINED;
			else if (align_style == "world")
				tiledef.align_style = ALIGN_STYLE_WORLD;
			else
				tiledef.align_style = ALIGN_STYLE_NODE;
		}
		tiledef.scale = getintfield_default(L, index, "scale", 0);
		// color = ...
		lua_getfield(L, index, "color");
		tiledef.has_color = read_color(L, -1, &tiledef.color);
		lua_pop(L, 1);
		// animation = {}
		lua_getfield(L, index, "animation");
		tiledef.animation = read_animation_definition(L, -1);
		lua_pop(L, 1);
	} else if (!lua_isnil(L, index)) {
		// TODO: should be an error
		errorstream << "TileDef: Invalid type! (expected string or table)" << std::endl;
	}

	return tiledef;
}

/******************************************************************************/
void read_content_features(lua_State *L, ContentFeatures &f, int index)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	/* Cache existence of some callbacks */
	lua_getfield(L, index, "on_construct");
	f.has_on_construct = !lua_isnil(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, index, "on_destruct");
	f.has_on_destruct = !lua_isnil(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, index, "after_destruct");
	f.has_after_destruct = !lua_isnil(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, index, "on_rightclick");
	f.rightclickable = lua_isfunction(L, -1);
	lua_pop(L, 1);

	/* Name */
	getstringfield(L, index, "name", f.name);

	/* Groups */
	lua_getfield(L, index, "groups");
	read_groups(L, -1, f.groups);
	lua_pop(L, 1);

	/* Visual definition */

	{
		auto str = getstringfield_default(L, index, "drawtype", "");
		if (!string_to_enum(ScriptApiNode::es_DrawType, f.drawtype, str))
			warningstream << "Node " << f.name
				<< " has unknown drawtype \"" << str << '"' << std::endl;
	}

	getfloatfield(L, index, "visual_scale", f.visual_scale);

	/* Meshnode model filename */
	getstringfield(L, index, "mesh", f.mesh);

	// tiles = {}
	lua_getfield(L, index, "tiles");
	if(lua_istable(L, -1)){
		int table = lua_gettop(L);
		lua_pushnil(L);
		int i = 0;
		while(lua_next(L, table) != 0){
			// Read tiledef from value
			f.tiledef[i] = read_tiledef(L, -1, f.drawtype, false);
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
			i++;
			if(i==6){
				lua_pop(L, 1);
				break;
			}
		}
		// Copy last value to all remaining textures
		if(i >= 1){
			TileDef lasttile = f.tiledef[i-1];
			while(i < 6){
				f.tiledef[i] = lasttile;
				i++;
			}
		}
	}
	lua_pop(L, 1);

	// overlay_tiles = {}
	lua_getfield(L, index, "overlay_tiles");
	if (lua_istable(L, -1)) {
		int table = lua_gettop(L);
		lua_pushnil(L);
		int i = 0;
		while (lua_next(L, table) != 0) {
			// Read tiledef from value
			f.tiledef_overlay[i] = read_tiledef(L, -1, f.drawtype, false);
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
			i++;
			if (i == 6) {
				lua_pop(L, 1);
				break;
			}
		}
		// Copy last value to all remaining textures
		if (i >= 1) {
			TileDef lasttile = f.tiledef_overlay[i - 1];
			while (i < 6) {
				f.tiledef_overlay[i] = lasttile;
				i++;
			}
		}
	}
	lua_pop(L, 1);

	// special_tiles = {}
	lua_getfield(L, index, "special_tiles");
	if(lua_istable(L, -1)){
		int table = lua_gettop(L);
		lua_pushnil(L);
		int i = 0;
		while(lua_next(L, table) != 0){
			// Read tiledef from value
			f.tiledef_special[i] = read_tiledef(L, -1, f.drawtype, true);
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
			i++;
			if(i==CF_SPECIAL_COUNT){
				lua_pop(L, 1);
				break;
			}
		}
	}
	lua_pop(L, 1);

	/* alpha & use_texture_alpha */
	// This is a bit complicated due to compatibility

	f.setDefaultAlphaMode();

	{
		int alpha;
		if (getintfield(L, index, "alpha", alpha)) {
			log_deprecated(L, "Field \"alpha\" on node " + f.name + " is obsolete, "
					"only limited compatibility provided; "
					"replaced by \"use_texture_alpha\".", 2);
			if (alpha != 255)
				f.alpha = ALPHAMODE_BLEND;
		}
	}

	lua_getfield(L, index, "use_texture_alpha");
	if (lua_isboolean(L, -1)) {
		log_deprecated(L, "Field \"use_texture_alpha\" on node " + f.name + ": "
				"Boolean values are deprecated; use the new choices instead.", 2);
		if (lua_toboolean(L, -1))
			f.alpha = (f.drawtype == NDT_NORMAL) ? ALPHAMODE_CLIP : ALPHAMODE_BLEND;
	} else if (check_field_or_nil(L, -1, LUA_TSTRING, "use_texture_alpha")) {
		string_to_enum(ScriptApiNode::es_TextureAlphaMode, f.alpha, lua_tostring(L, -1));
	}
	lua_pop(L, 1);

	/* Other stuff */

	lua_getfield(L, index, "color");
	read_color(L, -1, &f.color);
	lua_pop(L, 1);

	getstringfield(L, index, "palette", f.palette_name);

	lua_getfield(L, index, "post_effect_color");
	read_color(L, -1, &f.post_effect_color);
	lua_pop(L, 1);

	getboolfield(L, index, "post_effect_color_shaded", f.post_effect_color_shaded);

	{
		auto str = getstringfield_default(L, index, "paramtype", "");
		if (!string_to_enum(ScriptApiNode::es_ContentParamType, f.param_type, str))
			warningstream << "Node " << f.name
				<< " has unknown paramtype \"" << str << '"' << std::endl;
	}
	{
		auto str = getstringfield_default(L, index, "paramtype2", "");
		if (!string_to_enum(ScriptApiNode::es_ContentParamType2, f.param_type_2, str))
			warningstream << "Node " << f.name
				<< " has unknown paramtype2 \"" << str << '"' << std::endl;
	}

	if (!f.palette_name.empty() &&
			!(f.param_type_2 == CPT2_COLOR ||
			f.param_type_2 == CPT2_COLORED_FACEDIR ||
			f.param_type_2 == CPT2_COLORED_WALLMOUNTED ||
			f.param_type_2 == CPT2_COLORED_DEGROTATE ||
			f.param_type_2 == CPT2_COLORED_4DIR))
		warningstream << "Node " << f.name.c_str()
			<< " has a palette, but not a suitable paramtype2." << std::endl;

	// True for all ground-like things like stone and mud, false for eg. trees
	getboolfield(L, index, "is_ground_content", f.is_ground_content);
	f.light_propagates = (f.param_type == CPT_LIGHT);
	getboolfield(L, index, "sunlight_propagates", f.sunlight_propagates);
	// This is used for collision detection.
	// Also for general solidness queries.
	getboolfield(L, index, "walkable", f.walkable);

	// Player can point to these, point through or it is blocking
	lua_getfield(L, index, "pointable");
	if(!lua_isnil(L, -1)){
		f.pointable = read_pointability_type(L, -1);
	}
	lua_pop(L, 1);

	// Player can dig these
	getboolfield(L, index, "diggable", f.diggable);
	// Player can climb these
	getboolfield(L, index, "climbable", f.climbable);
	// Player can build on these
	getboolfield(L, index, "buildable_to", f.buildable_to);
	// Liquids flow into and replace node
	getboolfield(L, index, "floodable", f.floodable);
	// Whether the node is non-liquid, source liquid or flowing liquid
	{
		auto str = getstringfield_default(L, index, "liquidtype", "");
		if (!string_to_enum(ScriptApiNode::es_LiquidType, f.liquid_type, str))
			warningstream << "Node " << f.name
				<< " has unknown liquidtype \"" << str << '"' << std::endl;
	}
	// If the content is liquid, this is the flowing version of the liquid.
	getstringfield(L, index, "liquid_alternative_flowing",
			f.liquid_alternative_flowing);
	// If the content is liquid, this is the source version of the liquid.
	getstringfield(L, index, "liquid_alternative_source",
			f.liquid_alternative_source);
	// Viscosity for fluid flow, ranging from 1 to 7, with
	// 1 giving almost instantaneous propagation and 7 being
	// the slowest possible
	f.liquid_viscosity = getintfield_default(L, index,
			"liquid_viscosity", f.liquid_viscosity);
	// If move_resistance is not set explicitly,
	// move_resistance is equal to liquid_viscosity
	f.move_resistance = f.liquid_viscosity;
	f.liquid_range = getintfield_default(L, index,
			"liquid_range", f.liquid_range);
	f.leveled = getintfield_default(L, index, "leveled", f.leveled);
	f.leveled_max = getintfield_default(L, index,
			"leveled_max", f.leveled_max);

	getboolfield(L, index, "liquid_renewable", f.liquid_renewable);
	f.drowning = getintfield_default(L, index,
			"drowning", f.drowning);
	// Amount of light the node emits
	f.light_source = getintfield_default(L, index,
			"light_source", f.light_source);
	if (f.light_source > LIGHT_MAX) {
		warningstream << "Node " << f.name.c_str()
			<< " had greater light_source than " << LIGHT_MAX
			<< ", it was reduced." << std::endl;
		f.light_source = LIGHT_MAX;
	}
	f.damage_per_second = getintfield_default(L, index,
			"damage_per_second", f.damage_per_second);

	lua_getfield(L, index, "node_box");
	if(lua_istable(L, -1))
		f.node_box = read_nodebox(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, index, "connects_to");
	if (lua_istable(L, -1)) {
		int table = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, table) != 0) {
			// Value at -1
			f.connects_to.emplace_back(lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, index, "connect_sides");
	if (lua_istable(L, -1)) {
		int table = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, table) != 0) {
			// Value at -1
			std::string_view side(lua_tostring(L, -1));
			// Note faces are flipped to make checking easier
			if (side == "top")
				f.connect_sides |= 2;
			else if (side == "bottom")
				f.connect_sides |= 1;
			else if (side == "front")
				f.connect_sides |= 16;
			else if (side == "left")
				f.connect_sides |= 32;
			else if (side == "back")
				f.connect_sides |= 4;
			else if (side == "right")
				f.connect_sides |= 8;
			else
				warningstream << "Unknown value for \"connect_sides\": "
					<< side << std::endl;
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, index, "selection_box");
	if(lua_istable(L, -1))
		f.selection_box = read_nodebox(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, index, "collision_box");
	if(lua_istable(L, -1))
		f.collision_box = read_nodebox(L, -1);
	lua_pop(L, 1);

	f.waving = getintfield_default(L, index,
			"waving", f.waving);

	// Set to true if paramtype used to be 'facedir_simple'
	getboolfield(L, index, "legacy_facedir_simple", f.legacy_facedir_simple);
	// Set to true if wall_mounted used to be set to true
	getboolfield(L, index, "legacy_wallmounted", f.legacy_wallmounted);

	// Sound table
	lua_getfield(L, index, "sounds");
	if(lua_istable(L, -1)){
		lua_getfield(L, -1, "footstep");
		read_simplesoundspec(L, -1, f.sound_footstep);
		lua_pop(L, 1);
		lua_getfield(L, -1, "dig");
		read_simplesoundspec(L, -1, f.sound_dig);
		lua_pop(L, 1);
		lua_getfield(L, -1, "dug");
		read_simplesoundspec(L, -1, f.sound_dug);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	// Node immediately placed by client when node is dug
	getstringfield(L, index, "node_dig_prediction",
		f.node_dig_prediction);

	// How much the node slows down players, ranging from 1 to 7,
	// the higher, the slower.
	f.move_resistance = getintfield_default(L, index,
			"move_resistance", f.move_resistance);

	// Whether e.g. players in this node will have liquid movement physics
	lua_getfield(L, index, "liquid_move_physics");
	if(lua_isboolean(L, -1)) {
		f.liquid_move_physics = lua_toboolean(L, -1);
	} else if(lua_isnil(L, -1)) {
		f.liquid_move_physics = f.liquid_type != LIQUID_NONE;
	} else {
		// TODO: should be an error
		errorstream << "Field \"liquid_move_physics\": Invalid type!" << std::endl;
	}
	lua_pop(L, 1);
}

void push_content_features(lua_State *L, const ContentFeatures &c)
{
	std::string paramtype(enum_to_string(ScriptApiNode::es_ContentParamType, c.param_type));
	std::string paramtype2(enum_to_string(ScriptApiNode::es_ContentParamType2, c.param_type_2));
	std::string drawtype(enum_to_string(ScriptApiNode::es_DrawType, c.drawtype));
	std::string liquid_type(enum_to_string(ScriptApiNode::es_LiquidType, c.liquid_type));

	/* Missing "tiles" because I don't see a usecase (at least not yet). */

	lua_newtable(L);
	lua_pushboolean(L, c.has_on_construct);
	lua_setfield(L, -2, "has_on_construct");
	lua_pushboolean(L, c.has_on_destruct);
	lua_setfield(L, -2, "has_on_destruct");
	lua_pushboolean(L, c.has_after_destruct);
	lua_setfield(L, -2, "has_after_destruct");
	lua_pushstring(L, c.name.c_str());
	lua_setfield(L, -2, "name");
	push_groups(L, c.groups);
	lua_setfield(L, -2, "groups");
	lua_pushstring(L, paramtype.c_str());
	lua_setfield(L, -2, "paramtype");
	lua_pushstring(L, paramtype2.c_str());
	lua_setfield(L, -2, "paramtype2");
	lua_pushstring(L, drawtype.c_str());
	lua_setfield(L, -2, "drawtype");
	if (!c.mesh.empty()) {
		lua_pushstring(L, c.mesh.c_str());
		lua_setfield(L, -2, "mesh");
	}
#if CHECK_CLIENT_BUILD()
	push_ARGB8(L, c.minimap_color);       // I know this is not set-able w/ register_node,
	lua_setfield(L, -2, "minimap_color"); // but the people need to know!
#endif
	lua_pushnumber(L, c.visual_scale);
	lua_setfield(L, -2, "visual_scale");
	lua_pushnumber(L, c.alpha);
	lua_setfield(L, -2, "alpha");
	if (!c.palette_name.empty()) {
		push_ARGB8(L, c.color);
		lua_setfield(L, -2, "color");

		lua_pushstring(L, c.palette_name.c_str());
		lua_setfield(L, -2, "palette_name");

		push_palette(L, c.palette);
		lua_setfield(L, -2, "palette");
	}
	lua_pushnumber(L, c.waving);
	lua_setfield(L, -2, "waving");
	lua_pushnumber(L, c.connect_sides);
	lua_setfield(L, -2, "connect_sides");

	lua_createtable(L, c.connects_to.size(), 0);
	u16 i = 1;
	for (const std::string &it : c.connects_to) {
		lua_pushlstring(L, it.c_str(), it.size());
		lua_rawseti(L, -2, i++);
	}
	lua_setfield(L, -2, "connects_to");

	push_ARGB8(L, c.post_effect_color);
	lua_setfield(L, -2, "post_effect_color");
	lua_pushboolean(L, c.post_effect_color_shaded);
	lua_setfield(L, -2, "post_effect_color_shaded");
	lua_pushnumber(L, c.leveled);
	lua_setfield(L, -2, "leveled");
	lua_pushnumber(L, c.leveled_max);
	lua_setfield(L, -2, "leveled_max");
	lua_pushboolean(L, c.sunlight_propagates);
	lua_setfield(L, -2, "sunlight_propagates");
	lua_pushnumber(L, c.light_source);
	lua_setfield(L, -2, "light_source");
	lua_pushboolean(L, c.is_ground_content);
	lua_setfield(L, -2, "is_ground_content");
	lua_pushboolean(L, c.walkable);
	lua_setfield(L, -2, "walkable");
	push_pointability_type(L, c.pointable);
	lua_setfield(L, -2, "pointable");
	lua_pushboolean(L, c.diggable);
	lua_setfield(L, -2, "diggable");
	lua_pushboolean(L, c.climbable);
	lua_setfield(L, -2, "climbable");
	lua_pushboolean(L, c.buildable_to);
	lua_setfield(L, -2, "buildable_to");
	lua_pushboolean(L, c.rightclickable);
	lua_setfield(L, -2, "rightclickable");
	lua_pushnumber(L, c.damage_per_second);
	lua_setfield(L, -2, "damage_per_second");
	if (c.isLiquid()) {
		lua_pushstring(L, liquid_type.c_str());
		lua_setfield(L, -2, "liquid_type");
		lua_pushstring(L, c.liquid_alternative_flowing.c_str());
		lua_setfield(L, -2, "liquid_alternative_flowing");
		lua_pushstring(L, c.liquid_alternative_source.c_str());
		lua_setfield(L, -2, "liquid_alternative_source");
		lua_pushnumber(L, c.liquid_viscosity);
		lua_setfield(L, -2, "liquid_viscosity");
		lua_pushboolean(L, c.liquid_renewable);
		lua_setfield(L, -2, "liquid_renewable");
		lua_pushnumber(L, c.liquid_range);
		lua_setfield(L, -2, "liquid_range");
	}
	lua_pushnumber(L, c.drowning);
	lua_setfield(L, -2, "drowning");
	lua_pushboolean(L, c.floodable);
	lua_setfield(L, -2, "floodable");
	push_nodebox(L, c.node_box);
	lua_setfield(L, -2, "node_box");
	push_nodebox(L, c.selection_box);
	lua_setfield(L, -2, "selection_box");
	push_nodebox(L, c.collision_box);
	lua_setfield(L, -2, "collision_box");
	lua_newtable(L);
	push_simplesoundspec(L, c.sound_footstep);
	lua_setfield(L, -2, "sound_footstep");
	push_simplesoundspec(L, c.sound_dig);
	lua_setfield(L, -2, "sound_dig");
	push_simplesoundspec(L, c.sound_dug);
	lua_setfield(L, -2, "sound_dug");
	lua_setfield(L, -2, "sounds");
	lua_pushboolean(L, c.legacy_facedir_simple);
	lua_setfield(L, -2, "legacy_facedir_simple");
	lua_pushboolean(L, c.legacy_wallmounted);
	lua_setfield(L, -2, "legacy_wallmounted");
	lua_pushstring(L, c.node_dig_prediction.c_str());
	lua_setfield(L, -2, "node_dig_prediction");
	lua_pushnumber(L, c.move_resistance);
	lua_setfield(L, -2, "move_resistance");
	lua_pushboolean(L, c.liquid_move_physics);
	lua_setfield(L, -2, "liquid_move_physics");
}

/******************************************************************************/
void push_nodebox(lua_State *L, const NodeBox &box)
{
	lua_newtable(L);
	switch (box.type)
	{
		case NODEBOX_REGULAR:
			lua_pushstring(L, "regular");
			lua_setfield(L, -2, "type");
			break;
		case NODEBOX_LEVELED:
		case NODEBOX_FIXED:
			lua_pushstring(L, "fixed");
			lua_setfield(L, -2, "type");
			push_aabb3f_vector(L, box.fixed);
			lua_setfield(L, -2, "fixed");
			break;
		case NODEBOX_WALLMOUNTED:
			lua_pushstring(L, "wallmounted");
			lua_setfield(L, -2, "type");
			push_aabb3f(L, box.wall_top);
			lua_setfield(L, -2, "wall_top");
			push_aabb3f(L, box.wall_bottom);
			lua_setfield(L, -2, "wall_bottom");
			push_aabb3f(L, box.wall_side);
			lua_setfield(L, -2, "wall_side");
			break;
		case NODEBOX_CONNECTED: {
			lua_pushstring(L, "connected");
			lua_setfield(L, -2, "type");
			const auto &c = box.getConnected();
			push_aabb3f_vector(L, c.connect_top);
			lua_setfield(L, -2, "connect_top");
			push_aabb3f_vector(L, c.connect_bottom);
			lua_setfield(L, -2, "connect_bottom");
			push_aabb3f_vector(L, c.connect_front);
			lua_setfield(L, -2, "connect_front");
			push_aabb3f_vector(L, c.connect_back);
			lua_setfield(L, -2, "connect_back");
			push_aabb3f_vector(L, c.connect_left);
			lua_setfield(L, -2, "connect_left");
			push_aabb3f_vector(L, c.connect_right);
			lua_setfield(L, -2, "connect_right");
			// half the boxes are missing here?
			break;
		}
		default:
			FATAL_ERROR("Invalid box.type");
			break;
	}
}

/******************************************************************************/
void push_palette(lua_State *L, const std::vector<video::SColor> *palette)
{
	lua_createtable(L, palette->size(), 0);
	int newTable = lua_gettop(L);
	int index = 1;
	for (auto iter = palette->begin(); iter != palette->end(); ++iter) {
		push_ARGB8(L, (*iter));
		lua_rawseti(L, newTable, index);
		index++;
	}
}

/******************************************************************************/
void read_server_sound_params(lua_State *L, int index,
		ServerPlayingSound &params)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	if (lua_istable(L, index)) {
		// Functional overlap: this may modify SimpleSoundSpec contents
		getfloatfield(L, index, "fade", params.spec.fade);
		getfloatfield(L, index, "pitch", params.spec.pitch);
		getfloatfield(L, index, "start_time", params.spec.start_time);
		getboolfield(L, index, "loop", params.spec.loop);

		getfloatfield(L, index, "gain", params.gain);

		// Handle positional information
		getstringfield(L, index, "to_player", params.to_player);
		lua_getfield(L, index, "pos");
		if(!lua_isnil(L, -1)){
			v3f p = read_v3f(L, -1)*BS;
			params.pos = p;
			params.type = SoundLocation::Position;
		}
		lua_pop(L, 1);
		lua_getfield(L, index, "object");
		if(!lua_isnil(L, -1)){
			ObjectRef *ref = ModApiBase::checkObject<ObjectRef>(L, -1);
			ServerActiveObject *sao = ObjectRef::getobject(ref);
			if(sao){
				params.object = sao->getId();
				params.type = SoundLocation::Object;
			}
		}
		lua_pop(L, 1);
		params.max_hear_distance = BS*getfloatfield_default(L, index,
				"max_hear_distance", params.max_hear_distance/BS);
		getstringfield(L, index, "exclude_player", params.exclude_player);
	}
}

/******************************************************************************/
void read_simplesoundspec(lua_State *L, int index, SoundSpec &spec)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;
	if (lua_isnil(L, index))
		return;

	if (lua_istable(L, index)) {
		getstringfield(L, index, "name", spec.name);
		getfloatfield(L, index, "gain", spec.gain);
		getfloatfield(L, index, "fade", spec.fade);
		getfloatfield(L, index, "pitch", spec.pitch);
	} else if (lua_isstring(L, index)) {
		spec.name = lua_tostring(L, index);
	}
}

void push_simplesoundspec(lua_State *L, const SoundSpec &spec)
{
	lua_createtable(L, 0, 3);
	lua_pushstring(L, spec.name.c_str());
	lua_setfield(L, -2, "name");
	lua_pushnumber(L, spec.gain);
	lua_setfield(L, -2, "gain");
	lua_pushnumber(L, spec.fade);
	lua_setfield(L, -2, "fade");
	lua_pushnumber(L, spec.pitch);
	lua_setfield(L, -2, "pitch");
}

/******************************************************************************/
NodeBox read_nodebox(lua_State *L, int index)
{
	NodeBox nodebox;
	if (lua_isnil(L, -1))
		return nodebox;

	luaL_checktype(L, -1, LUA_TTABLE);

	nodebox.type = (NodeBoxType)getenumfield(L, index, "type",
			ScriptApiNode::es_NodeBoxType, NODEBOX_REGULAR);

#define NODEBOXREAD(n, s){ \
		lua_getfield(L, index, (s)); \
		if (lua_istable(L, -1)) \
			(n) = read_aabb3f(L, -1, BS); \
		lua_pop(L, 1); \
	}

#define NODEBOXREADVEC(n, s) \
	lua_getfield(L, index, (s)); \
	if (lua_istable(L, -1)) \
		(n) = read_aabb3f_vector(L, -1, BS); \
	lua_pop(L, 1);

	NODEBOXREADVEC(nodebox.fixed, "fixed");
	NODEBOXREAD(nodebox.wall_top, "wall_top");
	NODEBOXREAD(nodebox.wall_bottom, "wall_bottom");
	NODEBOXREAD(nodebox.wall_side, "wall_side");

	if (nodebox.type == NODEBOX_CONNECTED) {
		auto &c = nodebox.getConnected();
		NODEBOXREADVEC(c.connect_top, "connect_top");
		NODEBOXREADVEC(c.connect_bottom, "connect_bottom");
		NODEBOXREADVEC(c.connect_front, "connect_front");
		NODEBOXREADVEC(c.connect_left, "connect_left");
		NODEBOXREADVEC(c.connect_back, "connect_back");
		NODEBOXREADVEC(c.connect_right, "connect_right");
		NODEBOXREADVEC(c.disconnected_top, "disconnected_top");
		NODEBOXREADVEC(c.disconnected_bottom, "disconnected_bottom");
		NODEBOXREADVEC(c.disconnected_front, "disconnected_front");
		NODEBOXREADVEC(c.disconnected_left, "disconnected_left");
		NODEBOXREADVEC(c.disconnected_back, "disconnected_back");
		NODEBOXREADVEC(c.disconnected_right, "disconnected_right");
		NODEBOXREADVEC(c.disconnected, "disconnected");
		NODEBOXREADVEC(c.disconnected_sides, "disconnected_sides");
	}

	return nodebox;
}

/******************************************************************************/
MapNode readnode(lua_State *L, int index)
{
	lua_pushvalue(L, index);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_READ_NODE);
	lua_insert(L, -2);
	lua_call(L, 1, 3);
	content_t content = lua_tointeger(L, -3);
	u8 param1 = lua_tointeger(L, -2);
	u8 param2 = lua_tointeger(L, -1);
	lua_pop(L, 3);
	return MapNode(content, param1, param2);
}

/******************************************************************************/
void pushnode(lua_State *L, const MapNode &n)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PUSH_NODE);
	lua_pushinteger(L, n.getContent());
	lua_pushinteger(L, n.getParam1());
	lua_pushinteger(L, n.getParam2());
	lua_call(L, 3, 1);
}

/******************************************************************************/
int getenumfield(lua_State *L, int table,
		const char *fieldname, const EnumString *spec, int default_)
{
	int result = default_;
	string_to_enum(spec, result,
			getstringfield_default(L, table, fieldname, ""));
	return result;
}

/******************************************************************************/
ItemStack read_item(lua_State* L, int index, IItemDefManager *idef)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	if (lua_isnil(L, index)) {
		return ItemStack();
	}

	if (lua_isuserdata(L, index)) {
		// Convert from LuaItemStack
		LuaItemStack *o = ModApiBase::checkObject<LuaItemStack>(L, index);
		return o->getItem();
	}

	if (lua_isstring(L, index)) {
		// Convert from itemstring
		std::string itemstring = lua_tostring(L, index);
		try
		{
			ItemStack item;
			item.deSerialize(itemstring, idef);
			return item;
		}
		catch(SerializationError &e)
		{
			warningstream<<"unable to create item from itemstring"
					<<": "<<itemstring<<std::endl;
			return ItemStack();
		}
	}
	else if(lua_istable(L, index))
	{
		// Convert from table
		std::string name = getstringfield_default(L, index, "name", "");
		int count = getintfield_default(L, index, "count", 1);
		int wear = getintfield_default(L, index, "wear", 0);

		ItemStack istack(name, count, wear, idef);

		// BACKWARDS COMPATIBLITY
		std::string value = getstringfield_default(L, index, "metadata", "");
		istack.metadata.setString("", value);

		// Get meta
		lua_getfield(L, index, "meta");
		int fieldstable = lua_gettop(L);
		if (lua_istable(L, fieldstable)) {
			lua_pushnil(L);
			while (lua_next(L, fieldstable) != 0) {
				// key at index -2 and value at index -1
				std::string key = lua_tostring(L, -2);
				size_t value_len;
				const char *value_cs = lua_tolstring(L, -1, &value_len);
				std::string value(value_cs, value_len);
				istack.metadata.setString(key, value);
				lua_pop(L, 1); // removes value, keeps key for next iteration
			}
		}

		return istack;
	} else {
		throw LuaError("Expecting itemstack, itemstring, table or nil");
	}
}

/******************************************************************************/
void push_tool_capabilities(lua_State *L,
		const ToolCapabilities &toolcap)
{
	lua_newtable(L);
	setfloatfield(L, -1, "full_punch_interval", toolcap.full_punch_interval);
	setintfield(L, -1, "max_drop_level", toolcap.max_drop_level);
	setintfield(L, -1, "punch_attack_uses", toolcap.punch_attack_uses);
		// Create groupcaps table
		lua_newtable(L);
		// For each groupcap
		for (const auto &gc_it : toolcap.groupcaps) {
			// Create groupcap table
			lua_newtable(L);
			const std::string &name = gc_it.first;
			const ToolGroupCap &groupcap = gc_it.second;
			// Create subtable "times"
			lua_newtable(L);
			for (auto time : groupcap.times) {
				lua_pushinteger(L, time.first);
				lua_pushnumber(L, time.second);
				lua_settable(L, -3);
			}
			// Set subtable "times"
			lua_setfield(L, -2, "times");
			// Set simple parameters
			setintfield(L, -1, "maxlevel", groupcap.maxlevel);
			setintfield(L, -1, "uses", groupcap.uses);
			// Insert groupcap table into groupcaps table
			lua_setfield(L, -2, name.c_str());
		}
		// Set groupcaps table
		lua_setfield(L, -2, "groupcaps");
		//Create damage_groups table
		lua_newtable(L);
		// For each damage group
		for (const auto &damageGroup : toolcap.damageGroups) {
			// Create damage group table
			lua_pushinteger(L, damageGroup.second);
			lua_setfield(L, -2, damageGroup.first.c_str());
		}
		lua_setfield(L, -2, "damage_groups");
}

/******************************************************************************/
void push_wear_bar_params(lua_State *L,
		const WearBarParams &params)
{
	lua_newtable(L);
	setstringfield(L, -1, "blend", enum_to_string(WearBarParams::es_BlendMode, params.blend));

	lua_newtable(L);
	for (const std::pair<const f32, const video::SColor> item: params.colorStops) {
		lua_pushnumber(L, item.first); // key
		push_ARGB8(L, item.second);
		lua_rawset(L, -3);
	}
	lua_setfield(L, -2, "color_stops");
}

/******************************************************************************/
void push_inventory_list(lua_State *L, const InventoryList &invlist)
{
	push_items(L, invlist.getItems());
}

/******************************************************************************/
void push_inventory_lists(lua_State *L, const Inventory &inv)
{
	const auto &lists = inv.getLists();
	lua_createtable(L, 0, lists.size());
	for(const InventoryList *list : lists) {
		const std::string &name = list->getName();
		lua_pushlstring(L, name.c_str(), name.size());
		push_inventory_list(L, *list);
		lua_rawset(L, -3);
	}
}

/******************************************************************************/
void read_inventory_list(lua_State *L, int tableindex,
		Inventory *inv, const char *name, IGameDef *gdef, int forcesize)
{
	if(tableindex < 0)
		tableindex = lua_gettop(L) + 1 + tableindex;

	// If nil, delete list
	if(lua_isnil(L, tableindex)){
		inv->deleteList(name);
		return;
	}

	// Get Lua-specified items to insert into the list
	std::vector<ItemStack> items = read_items(L, tableindex, gdef);
	size_t listsize = (forcesize >= 0) ? forcesize : items.size();

	// Create or resize/clear list
	InventoryList *invlist = inv->addList(name, listsize);
	if (!invlist) {
		luaL_error(L, "inventory list: cannot create list named '%s'", name);
		return;
	}

	for (size_t i = 0; i < items.size(); ++i) {
		if (i == listsize)
			break; // Truncate provided list of items
		invlist->changeItem(i, items[i]);
	}
}

/******************************************************************************/
struct TileAnimationParams read_animation_definition(lua_State *L, int index)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	struct TileAnimationParams anim;
	anim.type = TAT_NONE;
	if (!lua_istable(L, index))
		return anim;

	anim.type = (TileAnimationType)
		getenumfield(L, index, "type", es_TileAnimationType,
		TAT_NONE);
	if (anim.type == TAT_VERTICAL_FRAMES) {
		// {type="vertical_frames", aspect_w=16, aspect_h=16, length=2.0}
		anim.vertical_frames.aspect_w =
			getintfield_default(L, index, "aspect_w", 16);
		anim.vertical_frames.aspect_h =
			getintfield_default(L, index, "aspect_h", 16);
		anim.vertical_frames.length =
			getfloatfield_default(L, index, "length", 1.0);
	} else if (anim.type == TAT_SHEET_2D) {
		// {type="sheet_2d", frames_w=5, frames_h=3, frame_length=0.5}
		getintfield(L, index, "frames_w",
			anim.sheet_2d.frames_w);
		getintfield(L, index, "frames_h",
			anim.sheet_2d.frames_h);
		getfloatfield(L, index, "frame_length",
			anim.sheet_2d.frame_length);
	}

	return anim;
}

/******************************************************************************/
ToolCapabilities read_tool_capabilities(
		lua_State *L, int table)
{
	ToolCapabilities toolcap;
	getfloatfield(L, table, "full_punch_interval", toolcap.full_punch_interval);
	getintfield(L, table, "max_drop_level", toolcap.max_drop_level);
	getintfield(L, table, "punch_attack_uses", toolcap.punch_attack_uses);
	lua_getfield(L, table, "groupcaps");
	if(lua_istable(L, -1)){
		int table_groupcaps = lua_gettop(L);
		lua_pushnil(L);
		while(lua_next(L, table_groupcaps) != 0){
			// key at index -2 and value at index -1
			std::string groupname = luaL_checkstring(L, -2);
			if(lua_istable(L, -1)){
				int table_groupcap = lua_gettop(L);
				// This will be created
				ToolGroupCap groupcap;
				// Read simple parameters
				getintfield(L, table_groupcap, "maxlevel", groupcap.maxlevel);
				getintfield(L, table_groupcap, "uses", groupcap.uses);
				// DEPRECATED: maxwear
				float maxwear = 0;
				if (getfloatfield(L, table_groupcap, "maxwear", maxwear)){
					if (maxwear != 0)
						groupcap.uses = 1.0/maxwear;
					else
						groupcap.uses = 0;
					warningstream << "Field \"maxwear\" is deprecated; "
							<< "replace with uses=1/maxwear" << std::endl;
					infostream << script_get_backtrace(L) << std::endl;
				}
				// Read "times" table
				lua_getfield(L, table_groupcap, "times");
				if(lua_istable(L, -1)){
					int table_times = lua_gettop(L);
					lua_pushnil(L);
					while(lua_next(L, table_times) != 0){
						// key at index -2 and value at index -1
						int rating = luaL_checkinteger(L, -2);
						float time = luaL_checknumber(L, -1);
						groupcap.times.emplace_back(rating, time);
						// removes value, keeps key for next iteration
						lua_pop(L, 1);
					}
				}
				lua_pop(L, 1);
				// Insert groupcap into toolcap
				toolcap.groupcaps[groupname] = groupcap;
			}
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, table, "damage_groups");
	if(lua_istable(L, -1)){
		int table_damage_groups = lua_gettop(L);
		lua_pushnil(L);
		while(lua_next(L, table_damage_groups) != 0){
			// key at index -2 and value at index -1
			std::string groupname = luaL_checkstring(L, -2);
			u16 value = luaL_checkinteger(L, -1);
			toolcap.damageGroups[groupname] = value;
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
	return toolcap;
}

/******************************************************************************/
PointabilityType read_pointability_type(lua_State *L, int index)
{
	if (lua_isboolean(L, index)) {
		if (lua_toboolean(L, index))
			return PointabilityType::POINTABLE;
		else
			return PointabilityType::POINTABLE_NOT;
	} else {
		const char* s = luaL_checkstring(L, index);
		if (s && !strcmp(s, "blocking")) {
			return PointabilityType::POINTABLE_BLOCKING;
		}
	}
	throw LuaError("Invalid pointable type.");
}

/******************************************************************************/
Pointabilities read_pointabilities(lua_State *L, int index)
{
	Pointabilities pointabilities;

	lua_getfield(L, index, "nodes");
	if(lua_istable(L, -1)){
		int ti = lua_gettop(L);
		lua_pushnil(L);
		while(lua_next(L, ti) != 0) {
			// key at index -2 and value at index -1
			std::string name = luaL_checkstring(L, -2);

			// handle groups
			if (str_starts_with(name, "group:")) {
				pointabilities.node_groups[name.substr(6)] = read_pointability_type(L, -1);
			} else {
				pointabilities.nodes[name] = read_pointability_type(L, -1);
			}

			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, index, "objects");
	if(lua_istable(L, -1)){
		int ti = lua_gettop(L);
		lua_pushnil(L);
		while(lua_next(L, ti) != 0) {
			// key at index -2 and value at index -1
			std::string name = luaL_checkstring(L, -2);

			// handle groups
			if (str_starts_with(name, "group:")) {
				pointabilities.object_groups[name.substr(6)] = read_pointability_type(L, -1);
			} else {
				pointabilities.objects[name] = read_pointability_type(L, -1);
			}

			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	return pointabilities;
}

/******************************************************************************/
void push_pointability_type(lua_State *L, PointabilityType pointable)
{
	switch(pointable)
	{
	case PointabilityType::POINTABLE:
		lua_pushboolean(L, true);
		break;
	case PointabilityType::POINTABLE_NOT:
		lua_pushboolean(L, false);
		break;
	case PointabilityType::POINTABLE_BLOCKING:
		lua_pushliteral(L, "blocking");
		break;
	}
}

/******************************************************************************/
void push_pointabilities(lua_State *L, const Pointabilities &pointabilities)
{
	// pointabilities table
	lua_newtable(L);

	if (!pointabilities.nodes.empty() || !pointabilities.node_groups.empty()) {
		// Create and fill table
		lua_newtable(L);
		for (const auto &entry : pointabilities.nodes) {
			push_pointability_type(L, entry.second);
			lua_setfield(L, -2, entry.first.c_str());
		}
		for (const auto &entry : pointabilities.node_groups) {
			push_pointability_type(L, entry.second);
			lua_setfield(L, -2, ("group:" + entry.first).c_str());
		}
		lua_setfield(L, -2, "nodes");
	}

	if (!pointabilities.objects.empty() || !pointabilities.object_groups.empty()) {
		// Create and fill table
		lua_newtable(L);
		for (const auto &entry : pointabilities.objects) {
			push_pointability_type(L, entry.second);
			lua_setfield(L, -2, entry.first.c_str());
		}
		for (const auto &entry : pointabilities.object_groups) {
			push_pointability_type(L, entry.second);
			lua_setfield(L, -2, ("group:" + entry.first).c_str());
		}
		lua_setfield(L, -2, "objects");
	}
}

/******************************************************************************/
WearBarParams read_wear_bar_params(
		lua_State *L, int stack_idx)
{
	if (lua_isstring(L, stack_idx)) {
		video::SColor color;
		read_color(L, stack_idx, &color);
		return WearBarParams(color);
	}

	if (!lua_istable(L, stack_idx))
		throw LuaError("Expected wear bar color table or colorstring");

	lua_getfield(L, stack_idx, "color_stops");
	if (!check_field_or_nil(L, -1, LUA_TTABLE, "color_stops"))
		throw LuaError("color_stops must be a table");

	std::map<f32, video::SColor> colorStops;
	// color stops table is on the stack
	int table_values = lua_gettop(L);
	lua_pushnil(L);
	while (lua_next(L, table_values) != 0) {
		// key at index -2 and value at index -1 within table_values
		f32 point = luaL_checknumber(L, -2);
		if (point < 0 || point > 1)
			throw LuaError("Wear bar color stop key out of range");
		video::SColor color;
		read_color(L, -1, &color);
		colorStops.emplace(point, color);

		// removes value, keeps key for next iteration
		lua_pop(L, 1);
	}
	lua_pop(L, 1); // pop color stops table

	auto blend = WearBarParams::BLEND_MODE_CONSTANT;
	lua_getfield(L, stack_idx, "blend");
	if (check_field_or_nil(L, -1, LUA_TSTRING, "blend")) {
		if (!string_to_enum(WearBarParams::es_BlendMode, blend, lua_tostring(L, -1)))
			throw LuaError("Invalid wear bar color blend mode");
	}
	lua_pop(L, 1);

	return WearBarParams(colorStops, blend);
}

/******************************************************************************/
void push_dig_params(lua_State *L,const DigParams &params)
{
	lua_createtable(L, 0, 3);
	setboolfield(L, -1, "diggable", params.diggable);
	setfloatfield(L, -1, "time", params.time);
	setintfield(L, -1, "wear", params.wear);
}

/******************************************************************************/
void push_hit_params(lua_State *L,const HitParams &params)
{
	lua_createtable(L, 0, 3);
	setintfield(L, -1, "hp", params.hp);
	setintfield(L, -1, "wear", params.wear);
}

/******************************************************************************/

bool getflagsfield(lua_State *L, int table, const char *fieldname,
	const FlagDesc *flagdesc, u32 *flags, u32 *flagmask)
{
	lua_getfield(L, table, fieldname);

	bool success = read_flags(L, -1, flagdesc, flags, flagmask);

	lua_pop(L, 1);

	return success;
}

bool read_flags(lua_State *L, int index, const FlagDesc *flagdesc,
	u32 *flags, u32 *flagmask)
{
	if (lua_isstring(L, index)) {
		std::string flagstr = lua_tostring(L, index);
		*flags = readFlagString(flagstr, flagdesc, flagmask);
	} else if (lua_istable(L, index)) {
		*flags = read_flags_table(L, index, flagdesc, flagmask);
	} else {
		return false;
	}

	return true;
}

u32 read_flags_table(lua_State *L, int table, const FlagDesc *flagdesc, u32 *flagmask)
{
	u32 flags = 0, mask = 0;
	char fnamebuf[64] = "no";

	for (int i = 0; flagdesc[i].name; i++) {
		bool result;

		if (getboolfield(L, table, flagdesc[i].name, result)) {
			mask |= flagdesc[i].flag;
			if (result)
				flags |= flagdesc[i].flag;
		}

		strlcpy(fnamebuf + 2, flagdesc[i].name, sizeof(fnamebuf) - 2);
		if (getboolfield(L, table, fnamebuf, result))
			mask |= flagdesc[i].flag;
	}

	if (flagmask)
		*flagmask = mask;

	return flags;
}

void push_flags_string(lua_State *L, const FlagDesc *flagdesc, u32 flags, u32 flagmask)
{
	std::string flagstring = writeFlagString(flags, flagdesc, flagmask);
	lua_pushlstring(L, flagstring.c_str(), flagstring.size());
}

/******************************************************************************/
/* Lua Stored data!                                                           */
/******************************************************************************/

/******************************************************************************/
void read_groups(lua_State *L, int index, ItemGroupList &result)
{
	if (lua_isnil(L, index))
		return;

	luaL_checktype(L, index, LUA_TTABLE);

	result.clear();
	lua_pushnil(L);
	if (index < 0)
		index -= 1;
	while (lua_next(L, index) != 0) {
		// key at index -2 and value at index -1
		std::string name = luaL_checkstring(L, -2);
		int rating = luaL_checkinteger(L, -1);
		// zero rating indicates not in the group
		if (rating != 0)
			result[name] = rating;
		// removes value, keeps key for next iteration
		lua_pop(L, 1);
	}
}

/******************************************************************************/
void push_groups(lua_State *L, const ItemGroupList &groups)
{
	lua_createtable(L, 0, groups.size());
	for (const auto &group : groups) {
		lua_pushinteger(L, group.second);
		lua_setfield(L, -2, group.first.c_str());
	}
}

/******************************************************************************/
void push_items(lua_State *L, const std::vector<ItemStack> &items)
{
	lua_createtable(L, items.size(), 0);
	for (u32 i = 0; i != items.size(); i++) {
		LuaItemStack::create(L, items[i]);
		lua_rawseti(L, -2, i + 1);
	}
}

/******************************************************************************/
std::vector<ItemStack> read_items(lua_State *L, int index, IGameDef *gdef)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	std::vector<ItemStack> items;
	luaL_checktype(L, index, LUA_TTABLE);
	lua_pushnil(L);
	while (lua_next(L, index)) {
		s32 key = luaL_checkinteger(L, -2);
		if (key < 1) {
			throw LuaError("Invalid inventory list index");
		}
		if (items.size() < (u32) key) {
			items.resize(key);
		}
		items[key - 1] = read_item(L, -1, gdef->idef());
		lua_pop(L, 1);
	}
	return items;
}

/******************************************************************************/
void luaentity_get(lua_State *L, u16 id)
{
	// Get luaentities[i]
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "luaentities");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushinteger(L, id);
	lua_gettable(L, -2);
	lua_remove(L, -2); // Remove luaentities
	lua_remove(L, -2); // Remove core
}

/******************************************************************************/
bool read_noiseparams(lua_State *L, int index, NoiseParams *np)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	if (!lua_istable(L, index))
		return false;

	getfloatfield(L, index, "offset",      np->offset);
	getfloatfield(L, index, "scale",       np->scale);
	getfloatfield(L, index, "persist",     np->persist);
	getfloatfield(L, index, "persistence", np->persist);
	getfloatfield(L, index, "lacunarity",  np->lacunarity);
	getintfield(L,   index, "seed",        np->seed);
	getintfield(L,   index, "octaves",     np->octaves);

	u32 flags    = 0;
	u32 flagmask = 0;
	np->flags = getflagsfield(L, index, "flags", flagdesc_noiseparams,
		&flags, &flagmask) ? flags : NOISE_FLAG_DEFAULTS;

	lua_getfield(L, index, "spread");
	np->spread  = read_v3f(L, -1);
	lua_pop(L, 1);

	return true;
}

void push_noiseparams(lua_State *L, NoiseParams *np)
{
	lua_newtable(L);
	setfloatfield(L, -1, "offset",      np->offset);
	setfloatfield(L, -1, "scale",       np->scale);
	setfloatfield(L, -1, "persist",     np->persist);
	setfloatfield(L, -1, "persistence", np->persist);
	setfloatfield(L, -1, "lacunarity",  np->lacunarity);
	setintfield(  L, -1, "seed",        np->seed);
	setintfield(  L, -1, "octaves",     np->octaves);

	push_flags_string(L, flagdesc_noiseparams, np->flags,
		np->flags);
	lua_setfield(L, -2, "flags");

	push_v3f(L, np->spread);
	lua_setfield(L, -2, "spread");
}

bool read_tree_def(lua_State *L, int idx, const NodeDefManager *ndef,
		treegen::TreeDef &tree_def)
{
	std::string trunk, leaves, fruit;
	if (!lua_istable(L, idx))
		return false;

	getstringfield(L, idx, "axiom", tree_def.initial_axiom);
	getstringfield(L, idx, "rules_a", tree_def.rules_a);
	getstringfield(L, idx, "rules_b", tree_def.rules_b);
	getstringfield(L, idx, "rules_c", tree_def.rules_c);
	getstringfield(L, idx, "rules_d", tree_def.rules_d);
	getstringfield(L, idx, "trunk", trunk);
	tree_def.m_nodenames.push_back(trunk);
	getstringfield(L, idx, "leaves", leaves);
	tree_def.m_nodenames.push_back(leaves);
	tree_def.leaves2_chance = 0;
	getstringfield(L, idx, "leaves2", leaves);
	if (!leaves.empty()) {
		getintfield(L, idx, "leaves2_chance", tree_def.leaves2_chance);
		if (tree_def.leaves2_chance)
			tree_def.m_nodenames.push_back(leaves);
	}
	getintfield(L, idx, "angle", tree_def.angle);
	getintfield(L, idx, "iterations", tree_def.iterations);
	if (!getintfield(L, idx, "random_level", tree_def.iterations_random_level))
		tree_def.iterations_random_level = 0;
	getstringfield(L, idx, "trunk_type", tree_def.trunk_type);
	getboolfield(L, idx, "thin_branches", tree_def.thin_branches);
	tree_def.fruit_chance = 0;
	fruit = "air";
	getstringfield(L, idx, "fruit", fruit);
	if (!fruit.empty())
		getintfield(L, idx, "fruit_chance", tree_def.fruit_chance);
	tree_def.m_nodenames.push_back(fruit);
	tree_def.explicit_seed = getintfield(L, idx, "seed", tree_def.seed);

	// Resolves the node IDs for trunk, leaves, leaves2 and fruit at runtime,
	// when tree_def.resolveNodeNames will be called.
	ndef->pendNodeResolve(&tree_def);

	return true;
}

/******************************************************************************/

// Returns depth of json value tree
static int push_json_value_getdepth(const Json::Value &value)
{
	if (!value.isArray() && !value.isObject())
		return 1;

	int maxdepth = 0;
	for (const auto &it : value)
		maxdepth = std::max(push_json_value_getdepth(it), maxdepth);
	return maxdepth + 1;
}
// Recursive function to convert JSON --> Lua table
static bool push_json_value_helper(lua_State *L, const Json::Value &value,
		int nullindex)
{
	switch(value.type()) {
		case Json::nullValue:
		default:
			lua_pushvalue(L, nullindex);
			break;
		case Json::intValue:
		case Json::uintValue:
		case Json::realValue:
			// push everything as a double since Lua integers may be too small
			lua_pushnumber(L, value.asDouble());
			break;
		case Json::stringValue: {
			const auto &str = value.asString();
			lua_pushlstring(L, str.c_str(), str.size());
			break;
		}
		case Json::booleanValue:
			lua_pushboolean(L, value.asInt());
			break;
		case Json::arrayValue:
			lua_createtable(L, value.size(), 0);
			for (auto it = value.begin(); it != value.end(); ++it) {
				push_json_value_helper(L, *it, nullindex);
				lua_rawseti(L, -2, it.index() + 1);
			}
			break;
		case Json::objectValue:
			lua_createtable(L, 0, value.size());
			for (auto it = value.begin(); it != value.end(); ++it) {
#if JSONCPP_VERSION_HEXA >= 0x01060000 /* 1.6.0 */
				const auto &str = it.name();
				lua_pushlstring(L, str.c_str(), str.size());
#else
				const char *str = it.memberName();
				lua_pushstring(L, str ? str : "");
#endif
				push_json_value_helper(L, *it, nullindex);
				lua_rawset(L, -3);
			}
			break;
	}
	return true;
}

// converts JSON --> Lua table; returns false if lua stack limit exceeded
// nullindex: Lua stack index of value to use in place of JSON null
bool push_json_value(lua_State *L, const Json::Value &value, int nullindex)
{
	if(nullindex < 0)
		nullindex = lua_gettop(L) + 1 + nullindex;

	int depth = push_json_value_getdepth(value);

	// The maximum number of Lua stack slots used at each recursion level
	// of push_json_value_helper is 2, so make sure there a depth * 2 slots
	if (lua_checkstack(L, depth * 2))
		return push_json_value_helper(L, value, nullindex);

	return false;
}

// Converts Lua table --> JSON
void read_json_value(lua_State *L, Json::Value &root, int index, u16 max_depth)
{
	if (max_depth == 0)
		throw SerializationError("depth exceeds MAX_JSON_DEPTH");

	int type = lua_type(L, index);
	if (type == LUA_TBOOLEAN) {
		root = (bool) lua_toboolean(L, index);
	} else if (type == LUA_TNUMBER) {
		root = lua_tonumber(L, index);
	} else if (type == LUA_TSTRING) {
		size_t len;
		const char *str = lua_tolstring(L, index, &len);
		root = std::string(str, len);
	} else if (type == LUA_TTABLE) {
		// Reserve two slots for key and value.
		lua_checkstack(L, 2);
		lua_pushnil(L);
		while (lua_next(L, index)) {
			// Key is at -2 and value is at -1
			Json::Value value;
			read_json_value(L, value, lua_gettop(L), max_depth - 1);

			Json::ValueType roottype = root.type();
			int keytype = lua_type(L, -1);
			if (keytype == LUA_TNUMBER) {
				lua_Number key = lua_tonumber(L, -1);
				if (roottype != Json::nullValue && roottype != Json::arrayValue) {
					throw SerializationError("Can't mix array and object values in JSON");
				} else if (key < 1) {
					throw SerializationError("Can't use zero-based or negative indexes in JSON");
				} else if (floor(key) != key) {
					throw SerializationError("Can't use indexes with a fractional part in JSON");
				}
				root[(Json::ArrayIndex) key - 1] = value;
			} else if (keytype == LUA_TSTRING) {
				if (roottype != Json::nullValue && roottype != Json::objectValue) {
					throw SerializationError("Can't mix array and object values in JSON");
				}
				root[lua_tostring(L, -1)] = value;
			} else {
				throw SerializationError("Lua key to convert to JSON is not a string or number");
			}
		}
	} else if (type == LUA_TNIL) {
		root = Json::nullValue;
	} else {
		throw SerializationError("Can only store booleans, numbers, strings, objects, arrays, and null in JSON");
	}
	lua_pop(L, 1); // Pop value
}

void push_pointed_thing(lua_State *L, const PointedThing &pointed, bool csm,
	bool hitpoint)
{
	lua_newtable(L);
	if (pointed.type == POINTEDTHING_NODE) {
		lua_pushstring(L, "node");
		lua_setfield(L, -2, "type");
		push_v3s16(L, pointed.node_undersurface);
		lua_setfield(L, -2, "under");
		push_v3s16(L, pointed.node_abovesurface);
		lua_setfield(L, -2, "above");
	} else if (pointed.type == POINTEDTHING_OBJECT) {
		lua_pushstring(L, "object");
		lua_setfield(L, -2, "type");

		if (csm) {
			lua_pushinteger(L, pointed.object_id);
			lua_setfield(L, -2, "id");
		} else {
			push_objectRef(L, pointed.object_id);
			lua_setfield(L, -2, "ref");
		}
	} else {
		lua_pushstring(L, "nothing");
		lua_setfield(L, -2, "type");
	}
	if (hitpoint && (pointed.type != POINTEDTHING_NOTHING)) {
		push_v3f(L, pointed.intersection_point / BS); // convert to node coords
		lua_setfield(L, -2, "intersection_point");
		push_v3f(L, pointed.intersection_normal);
		lua_setfield(L, -2, "intersection_normal");
		lua_pushinteger(L, pointed.box_id + 1); // change to Lua array index
		lua_setfield(L, -2, "box_id");
	}
}

void push_objectRef(lua_State *L, const u16 id)
{
	assert(id != 0);
	// Get core.object_refs[i]
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "object_refs");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushinteger(L, id);
	lua_gettable(L, -2);
	assert(!lua_isnoneornil(L, -1));
	lua_remove(L, -2); // object_refs
	lua_remove(L, -2); // core
}

void read_hud_element(lua_State *L, HudElement *elem)
{
	std::string type_string;
	bool has_type = getstringfield(L, 2, "type", type_string);

	// Handle deprecated hud_elem_type
	std::string deprecated_type_string;
	if (getstringfield(L, 2, "hud_elem_type", deprecated_type_string)) {
		if (has_type && deprecated_type_string != type_string) {
			log_deprecated(L, "Ambiguous HUD element fields \"type\" and \"hud_elem_type\", "
					"\"type\" will be used.", 1, true);
		} else {
			has_type = true;
			type_string = deprecated_type_string;
			log_deprecated(L, "Deprecated \"hud_elem_type\" field, use \"type\" instead.",
					1, true);
		}
	}

	int type_enum;
	if (has_type && string_to_enum(es_HudElementType, type_enum, type_string))
		elem->type = static_cast<HudElementType>(type_enum);
	else
		elem->type = HUD_ELEM_TEXT;

	lua_getfield(L, 2, "position");
	elem->pos = lua_istable(L, -1) ? read_v2f(L, -1) : v2f();
	lua_pop(L, 1);

	lua_getfield(L, 2, "scale");
	elem->scale = lua_istable(L, -1) ? read_v2f(L, -1) : v2f();
	lua_pop(L, 1);

	lua_getfield(L, 2, "size");
	elem->size = lua_istable(L, -1) ? read_v2s32(L, -1) : v2s32();
	lua_pop(L, 1);

	elem->name    = getstringfield_default(L, 2, "name", "");
	elem->text    = getstringfield_default(L, 2, "text", "");
	elem->number  = getintfield_default(L, 2, "number", 0);
	if (elem->type == HUD_ELEM_WAYPOINT) {
		// Waypoints reuse the item field to store precision,
		// item = precision + 1 and item = 0 <=> precision = 10 for backwards compatibility.
		int precision = getintfield_default(L, 2, "precision", 10);
		if (precision < 0)
			throw LuaError("Waypoint precision must be non-negative");
		elem->item = precision + 1;
	} else {
		elem->item = getintfield_default(L, 2, "item", 0);
	}
	elem->dir     = getintfield_default(L, 2, "direction", 0);
	elem->z_index = MYMAX(S16_MIN, MYMIN(S16_MAX,
			getintfield_default(L, 2, "z_index", 0)));
	elem->text2   = getstringfield_default(L, 2, "text2", "");

	// Deprecated, only for compatibility's sake
	if (elem->dir == 0)
		elem->dir = getintfield_default(L, 2, "dir", 0);

	lua_getfield(L, 2, "alignment");
	if (lua_istable(L, -1))
		elem->align = read_v2f(L, -1);
	else
		elem->align = elem->type == HUD_ELEM_INVENTORY ? v2f(1.0f, 1.0f) : v2f();
	lua_pop(L, 1);

	lua_getfield(L, 2, "offset");
	elem->offset = lua_istable(L, -1) ? read_v2f(L, -1) : v2f();
	lua_pop(L, 1);

	lua_getfield(L, 2, "world_pos");
	elem->world_pos = lua_istable(L, -1) ? read_v3f(L, -1) : v3f();
	lua_pop(L, 1);

	elem->style = getintfield_default(L, 2, "style", 0);

	/* check for known deprecated element usage */
	if ((elem->type  == HUD_ELEM_STATBAR) && (elem->size == v2s32()))
		log_deprecated(L,"Deprecated usage of statbar without size!");
}

void push_hud_element(lua_State *L, HudElement *elem)
{
	lua_newtable(L);

	lua_pushstring(L, enum_to_string(es_HudElementType, elem->type));
	lua_setfield(L, -2, "type");

	push_v2f(L, elem->pos);
	lua_setfield(L, -2, "position");

	lua_pushstring(L, elem->name.c_str());
	lua_setfield(L, -2, "name");

	push_v2f(L, elem->scale);
	lua_setfield(L, -2, "scale");

	lua_pushstring(L, elem->text.c_str());
	lua_setfield(L, -2, "text");

	lua_pushnumber(L, elem->number);
	lua_setfield(L, -2, "number");

	if (elem->type == HUD_ELEM_WAYPOINT) {
		// Waypoints reuse the item field to store precision,
		// item = precision + 1 and item = 0 <=> precision = 10 for backwards compatibility.
		// See `Hud::drawLuaElements`, case `HUD_ELEM_WAYPOINT`.
		lua_pushnumber(L, (elem->item == 0) ? 10 : (elem->item - 1));
		lua_setfield(L, -2, "precision");
	}
	// push the item field for waypoints as well for backwards compatibility
	lua_pushnumber(L, elem->item);
	lua_setfield(L, -2, "item");

	lua_pushnumber(L, elem->dir);
	lua_setfield(L, -2, "direction");

	push_v2f(L, elem->offset);
	lua_setfield(L, -2, "offset");

	push_v2f(L, elem->align);
	lua_setfield(L, -2, "alignment");

	push_v2s32(L, elem->size);
	lua_setfield(L, -2, "size");

	// Deprecated, only for compatibility's sake
	lua_pushnumber(L, elem->dir);
	lua_setfield(L, -2, "dir");

	push_v3f(L, elem->world_pos);
	lua_setfield(L, -2, "world_pos");

	lua_pushnumber(L, elem->z_index);
	lua_setfield(L, -2, "z_index");

	lua_pushstring(L, elem->text2.c_str());
	lua_setfield(L, -2, "text2");

	lua_pushinteger(L, elem->style);
	lua_setfield(L, -2, "style");
}

bool read_hud_change(lua_State *L, HudElementStat &stat, HudElement *elem, void **value)
{
	std::string statstr = lua_tostring(L, 3);
	if (!string_to_enum(es_HudElementStat, stat, statstr)) {
		script_log_unique(L, "Unknown HUD stat type: " + statstr, warningstream);
		return false;
	}

	switch (stat) {
		case HUD_STAT_POS:
			elem->pos = read_v2f(L, 4);
			*value = &elem->pos;
			break;
		case HUD_STAT_NAME:
			elem->name = luaL_checkstring(L, 4);
			*value = &elem->name;
			break;
		case HUD_STAT_SCALE:
			elem->scale = read_v2f(L, 4);
			*value = &elem->scale;
			break;
		case HUD_STAT_TEXT:
			elem->text = luaL_checkstring(L, 4);
			*value = &elem->text;
			break;
		case HUD_STAT_NUMBER:
			elem->number = luaL_checknumber(L, 4);
			*value = &elem->number;
			break;
		case HUD_STAT_ITEM:
			elem->item = luaL_checknumber(L, 4);
			if (elem->type == HUD_ELEM_WAYPOINT && statstr == "precision")
				elem->item++;
			*value = &elem->item;
			break;
		case HUD_STAT_DIR:
			elem->dir = luaL_checknumber(L, 4);
			*value = &elem->dir;
			break;
		case HUD_STAT_ALIGN:
			elem->align = read_v2f(L, 4);
			*value = &elem->align;
			break;
		case HUD_STAT_OFFSET:
			elem->offset = read_v2f(L, 4);
			*value = &elem->offset;
			break;
		case HUD_STAT_WORLD_POS:
			elem->world_pos = read_v3f(L, 4);
			*value = &elem->world_pos;
			break;
		case HUD_STAT_SIZE:
			elem->size = read_v2s32(L, 4);
			*value = &elem->size;
			break;
		case HUD_STAT_Z_INDEX:
			elem->z_index = MYMAX(S16_MIN, MYMIN(S16_MAX, luaL_checknumber(L, 4)));
			*value = &elem->z_index;
			break;
		case HUD_STAT_TEXT2:
			elem->text2 = luaL_checkstring(L, 4);
			*value = &elem->text2;
			break;
		case HUD_STAT_STYLE:
			elem->style = luaL_checknumber(L, 4);
			*value = &elem->style;
			break;
		case HudElementStat_END:
			return false;
			break;
	}

	return true;
}

/******************************************************************************/

// Indices must match values in `enum CollisionType` exactly!!
static const char *collision_type_str[] = {
	"node",
	"object",
};

// Indices must match values in `enum CollisionAxis` exactly!!
static const char *collision_axis_str[] = {
	"x",
	"y",
	"z",
};

void push_collision_move_result(lua_State *L, const collisionMoveResult &res)
{
	// use faster Lua helper if possible
	if (res.collisions.size() == 1 && res.collisions.front().type == COLLISION_NODE) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PUSH_MOVERESULT1);
		const auto &c = res.collisions.front();
		lua_pushboolean(L, res.touching_ground);
		lua_pushboolean(L, res.collides);
		lua_pushboolean(L, res.standing_on_object);
		assert(c.axis != COLLISION_AXIS_NONE);
		lua_pushinteger(L, static_cast<int>(c.axis));
		lua_pushinteger(L, c.node_p.X);
		lua_pushinteger(L, c.node_p.Y);
		lua_pushinteger(L, c.node_p.Z);
		for (v3f v : {c.new_pos / BS, c.old_speed / BS, c.new_speed / BS}) {
			lua_pushnumber(L, v.X);
			lua_pushnumber(L, v.Y);
			lua_pushnumber(L, v.Z);
		}
		lua_call(L, 3 + 1 + 3 + 3 * 3, 1);
		return;
	}

	lua_createtable(L, 0, 4);

	setboolfield(L, -1, "touching_ground", res.touching_ground);
	setboolfield(L, -1, "collides", res.collides);
	setboolfield(L, -1, "standing_on_object", res.standing_on_object);

	/* collisions */
	lua_createtable(L, res.collisions.size(), 0);
	int i = 1;
	for (const auto &c : res.collisions) {
		lua_createtable(L, 0, 6);

		lua_pushstring(L, collision_type_str[c.type]);
		lua_setfield(L, -2, "type");

		assert(c.axis != COLLISION_AXIS_NONE);
		lua_pushstring(L, collision_axis_str[c.axis]);
		lua_setfield(L, -2, "axis");

		if (c.type == COLLISION_NODE) {
			push_v3s16(L, c.node_p);
			lua_setfield(L, -2, "node_pos");
		} else if (c.type == COLLISION_OBJECT) {
			push_objectRef(L, c.object->getId());
			lua_setfield(L, -2, "object");
		}

		push_v3f(L, c.new_pos / BS);
		lua_setfield(L, -2, "new_pos");

		push_v3f(L, c.old_speed / BS);
		lua_setfield(L, -2, "old_velocity");

		push_v3f(L, c.new_speed / BS);
		lua_setfield(L, -2, "new_velocity");

		lua_rawseti(L, -2, i++);
	}
	lua_setfield(L, -2, "collisions");
	/**/
}


void push_mod_spec(lua_State *L, const ModSpec &spec, bool include_unsatisfied)
{
	lua_newtable(L);

	lua_pushstring(L, spec.name.c_str());
	lua_setfield(L, -2, "name");

	lua_pushstring(L, spec.author.c_str());
	lua_setfield(L, -2, "author");

	lua_pushinteger(L, spec.release);
	lua_setfield(L, -2, "release");

	lua_pushstring(L, spec.desc.c_str());
	lua_setfield(L, -2, "description");

	lua_pushstring(L, spec.path.c_str());
	lua_setfield(L, -2, "path");

	lua_pushstring(L, spec.virtual_path.c_str());
	lua_setfield(L, -2, "virtual_path");

	lua_newtable(L);
	int i = 1;
	for (const auto &dep : spec.unsatisfied_depends) {
		lua_pushstring(L, dep.c_str());
		lua_rawseti(L, -2, i++);
	}
	lua_setfield(L, -2, "unsatisfied_depends");
}
