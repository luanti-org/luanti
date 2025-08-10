// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_node.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "nodedef.h"
#include "server.h"
#include "environment.h"
#include "util/pointedthing.h"


// Should be ordered exactly like enum NodeDrawType in nodedef.h
struct EnumString ScriptApiNode::es_DrawType[] =
	{
		{NDT_NORMAL, "normal"},
		{NDT_AIRLIKE, "airlike"},
		{NDT_LIQUID, "liquid"},
		{NDT_FLOWINGLIQUID, "flowingliquid"},
		{NDT_GLASSLIKE, "glasslike"},
		{NDT_ALLFACES, "allfaces"},
		{NDT_ALLFACES_OPTIONAL, "allfaces_optional"},
		{NDT_TORCHLIKE, "torchlike"},
		{NDT_SIGNLIKE, "signlike"},
		{NDT_PLANTLIKE, "plantlike"},
		{NDT_FENCELIKE, "fencelike"},
		{NDT_RAILLIKE, "raillike"},
		{NDT_NODEBOX, "nodebox"},
		{NDT_GLASSLIKE_FRAMED, "glasslike_framed"},
		{NDT_FIRELIKE, "firelike"},
		{NDT_GLASSLIKE_FRAMED_OPTIONAL, "glasslike_framed_optional"},
		{NDT_MESH, "mesh"},
		{NDT_PLANTLIKE_ROOTED, "plantlike_rooted"},
		{0, NULL},
	};

struct EnumString ScriptApiNode::es_ContentParamType2[] =
	{
		{CPT2_NONE, "none"},
		{CPT2_FULL, "full"},
		{CPT2_FLOWINGLIQUID, "flowingliquid"},
		{CPT2_FACEDIR, "facedir"},
		{CPT2_WALLMOUNTED, "wallmounted"},
		{CPT2_LEVELED, "leveled"},
		{CPT2_DEGROTATE, "degrotate"},
		{CPT2_MESHOPTIONS, "meshoptions"},
		{CPT2_COLOR, "color"},
		{CPT2_COLORED_FACEDIR, "colorfacedir"},
		{CPT2_COLORED_WALLMOUNTED, "colorwallmounted"},
		{CPT2_GLASSLIKE_LIQUID_LEVEL, "glasslikeliquidlevel"},
		{CPT2_COLORED_DEGROTATE, "colordegrotate"},
		{CPT2_4DIR, "4dir"},
		{CPT2_COLORED_4DIR, "color4dir"},
		{0, NULL},
	};

struct EnumString ScriptApiNode::es_LiquidType[] =
	{
		{LIQUID_NONE, "none"},
		{LIQUID_FLOWING, "flowing"},
		{LIQUID_SOURCE, "source"},
		{0, NULL},
	};

struct EnumString ScriptApiNode::es_ContentParamType[] =
	{
		{CPT_NONE, "none"},
		{CPT_LIGHT, "light"},
		{0, NULL},
	};

struct EnumString ScriptApiNode::es_NodeBoxType[] =
	{
		{NODEBOX_REGULAR, "regular"},
		{NODEBOX_FIXED, "fixed"},
		{NODEBOX_WALLMOUNTED, "wallmounted"},
		{NODEBOX_LEVELED, "leveled"},
		{NODEBOX_CONNECTED, "connected"},
		{0, NULL},
	};

struct EnumString ScriptApiNode::es_TextureAlphaMode[] =
	{
		{ALPHAMODE_OPAQUE, "opaque"},
		{ALPHAMODE_CLIP, "clip"},
		{ALPHAMODE_BLEND, "blend"},
		{0, NULL},
	};

bool ScriptApiNode::node_on_punch(v3s16 p, MapNode node,
		ServerActiveObject *puncher, const PointedThing &pointed)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "on_punch", &p))
		return false;

	// Call function
	push_v3s16(L, p);
	pushnode(L, node);
	objectrefGetOrCreate(L, puncher);
	pushPointedThing(pointed);
	PCALL_RES(lua_pcall(L, 4, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
	return true;
}

bool ScriptApiNode::node_on_dig(v3s16 p, MapNode node,
		ServerActiveObject *digger)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "on_dig", &p))
		return false;

	// Call function
	push_v3s16(L, p);
	pushnode(L, node);
	objectrefGetOrCreate(L, digger);
	PCALL_RES(lua_pcall(L, 3, 1, error_handler));

	// nil is treated as true for backwards compat
	bool result = lua_isnil(L, -1) || lua_toboolean(L, -1);

	lua_pop(L, 2);  // Pop error handler and result

	return result;
}

void ScriptApiNode::node_on_construct(v3s16 p, MapNode node)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "on_construct", &p))
		return;

	// Call function
	push_v3s16(L, p);
	PCALL_RES(lua_pcall(L, 1, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}

void ScriptApiNode::node_on_destruct(v3s16 p, MapNode node)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "on_destruct", &p))
		return;

	// Call function
	push_v3s16(L, p);
	PCALL_RES(lua_pcall(L, 1, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}

bool ScriptApiNode::node_on_flood(v3s16 p, MapNode node, MapNode newnode)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "on_flood", &p))
		return false;

	// Call function
	push_v3s16(L, p);
	pushnode(L, node);
	pushnode(L, newnode);
	PCALL_RES(lua_pcall(L, 3, 1, error_handler));
	lua_remove(L, error_handler);
	return readParam<bool>(L, -1, false);
}

void ScriptApiNode::node_after_destruct(v3s16 p, MapNode node)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "after_destruct", &p))
		return;

	// Call function
	push_v3s16(L, p);
	pushnode(L, node);
	PCALL_RES(lua_pcall(L, 2, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}

bool ScriptApiNode::node_on_timer(v3s16 p, MapNode node, f32 elapsed, f32 timeout)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "on_timer", &p))
		return false;

	// Call function
	push_v3s16(L, p);
	lua_pushnumber(L, elapsed);
	pushnode(L, node);
	lua_pushnumber(L, timeout);
	PCALL_RES(lua_pcall(L, 4, 1, error_handler));
	bool ret = readParam<bool>(L, -1, false);
	lua_pop(L, 2); // error handler, return value

	return ret;
}

void ScriptApiNode::node_on_receive_fields(v3s16 p,
		const std::string &formname,
		const StringMap &fields,
		ServerActiveObject *sender)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const NodeDefManager *ndef = getServer()->ndef();

	// If node doesn't exist, we don't know what callback to call
	MapNode node = getEnv()->getMap().getNode(p);
	if (node.getContent() == CONTENT_IGNORE)
		return;

	// Push callback function on stack
	if (!getItemCallback(ndef->get(node).name.c_str(), "on_receive_fields", &p))
		return;

	// Call function
	push_v3s16(L, p);                    // pos
	lua_pushstring(L, formname.c_str()); // formname
	lua_newtable(L);                     // fields
	StringMap::const_iterator it;
	for (it = fields.begin(); it != fields.end(); ++it) {
		const std::string &name = it->first;
		const std::string &value = it->second;
		lua_pushstring(L, name.c_str());
		lua_pushlstring(L, value.c_str(), value.size());
		lua_settable(L, -3);
	}
	objectrefGetOrCreate(L, sender);        // player
	PCALL_RES(lua_pcall(L, 4, 0, error_handler));
	lua_pop(L, 1);  // Pop error handler
}
