// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <algorithm>
#include "lua_api/l_env.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_nodemeta.h"
#include "lua_api/l_nodetimer.h"
#include "lua_api/l_noise.h"
#include "lua_api/l_vmanip.h"
#include "lua_api/l_object.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "scripting_server.h"
#include "environment.h"
#include "mapblock.h"
#include "server.h"
#include "nodedef.h"
#include "daynightratio.h"
#include "util/pointedthing.h"
#include "mapgen/treegen.h"
#include "emerge_internal.h"
#include "pathfinder.h"
#include "face_position_cache.h"
#include "remoteplayer.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "util/string.h"
#include "translation.h"
#if CHECK_CLIENT_BUILD()
#include "client/client.h"
#endif

const EnumString ModApiEnvBase::es_ClearObjectsMode[] =
{
	{CLEAR_OBJECTS_MODE_FULL,  "full"},
	{CLEAR_OBJECTS_MODE_QUICK, "quick"},
	{0, NULL},
};

const EnumString ModApiEnvBase::es_BlockStatusType[] =
{
	{ServerEnvironment::BS_UNKNOWN, "unknown"},
	{ServerEnvironment::BS_EMERGING, "emerging"},
	{ServerEnvironment::BS_LOADED,  "loaded"},
	{ServerEnvironment::BS_ACTIVE,  "active"},
	{0, NULL},
};

///////////////////////////////////////////////////////////////////////////////

int LuaRaycast::l_next(lua_State *L)
{
	GET_PLAIN_ENV_PTR;
	ServerEnvironment *senv = dynamic_cast<ServerEnvironment*>(env);

	bool csm = false;
#if CHECK_CLIENT_BUILD()
	csm = getClient(L) != nullptr;
#endif

	LuaRaycast *o = checkObject<LuaRaycast>(L, 1);
	PointedThing pointed;
	for (;;) {
		env->continueRaycast(&o->state, &pointed);
		if (pointed.type != POINTEDTHING_OBJECT)
			break;
		if (!senv)
			break;
		const auto *obj = senv->getActiveObject(pointed.object_id);
		if (obj && !obj->isGone())
			break;
		// skip gone object
	}
	if (pointed.type == POINTEDTHING_NOTHING)
		lua_pushnil(L);
	else
		push_pointed_thing(L, pointed, csm, true);

	return 1;
}

int LuaRaycast::create_object(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	bool objects = true;
	bool liquids = false;
	std::optional<Pointabilities> pointabilities = std::nullopt;

	v3f pos1 = checkFloatPos(L, 1);
	v3f pos2 = checkFloatPos(L, 2);
	if (lua_isboolean(L, 3)) {
		objects = readParam<bool>(L, 3);
	}
	if (lua_isboolean(L, 4)) {
		liquids = readParam<bool>(L, 4);
	}
	if (lua_istable(L, 5)) {
		pointabilities = read_pointabilities(L, 5);
	}

	LuaRaycast *o = new LuaRaycast(core::line3d<f32>(pos1, pos2),
		objects, liquids, pointabilities);

	*(void **) (lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
	return 1;
}

int LuaRaycast::gc_object(lua_State *L)
{
	LuaRaycast *o = *(LuaRaycast **) (lua_touserdata(L, 1));
	delete o;
	return 0;
}

void LuaRaycast::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__call", l_next},
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<LuaRaycast>(L, methods, metamethods);

	lua_register(L, className, create_object);
}

const char LuaRaycast::className[] = "Raycast";
const luaL_Reg LuaRaycast::methods[] =
{
	luamethod(LuaRaycast, next),
	{ 0, 0 }
};

void LuaEmergeAreaCallback(v3s16 blockpos, EmergeAction action, void *param)
{
	ScriptCallbackState *state = (ScriptCallbackState *)param;
	assert(state != NULL);
	assert(state->script != NULL);
	assert(state->refcount > 0);

	// state must be protected by envlock
	Server *server = state->script->getServer();
	Server::EnvAutoLock envlock(server);

	state->refcount--;

	state->script->on_emerge_area_completion(blockpos, action, state);

	if (state->refcount == 0)
		delete state;
}

/* Exported functions */

// set_node(pos, node)
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_set_node(lua_State *L)
{
	GET_ENV_PTR;

	// parameters
	v3s16 pos = read_v3s16(L, 1);
	MapNode n = readnode(L, 2);
	// Do it
	bool succeeded = env->setNode(pos, n);
	lua_pushboolean(L, succeeded);
	return 1;
}

// bulk_set_node([pos1, pos2, ...], node)
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_bulk_set_node(lua_State *L)
{
	GET_ENV_PTR;

	// parameters
	if (!lua_istable(L, 1)) {
		return 0;
	}

	s32 len = lua_objlen(L, 1);
	if (len == 0) {
		lua_pushboolean(L, true);
		return 1;
	}

	MapNode n = readnode(L, 2);

	// Do it
	bool succeeded = true;
	for (s32 i = 1; i <= len; i++) {
		lua_rawgeti(L, 1, i);
		if (!env->setNode(read_v3s16(L, -1), n))
			succeeded = false;
		lua_pop(L, 1);
	}

	lua_pushboolean(L, succeeded);
	return 1;
}

int ModApiEnv::l_add_node(lua_State *L)
{
	return l_set_node(L);
}

// remove_node(pos)
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_remove_node(lua_State *L)
{
	GET_ENV_PTR;

	// parameters
	v3s16 pos = read_v3s16(L, 1);
	// Do it
	bool succeeded = env->removeNode(pos);
	lua_pushboolean(L, succeeded);
	return 1;
}

// swap_node(pos, node)
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_swap_node(lua_State *L)
{
	GET_ENV_PTR;

	// parameters
	v3s16 pos = read_v3s16(L, 1);
	MapNode n = readnode(L, 2);
	// Do it
	bool succeeded = env->swapNode(pos, n);
	lua_pushboolean(L, succeeded);
	return 1;
}

// bulk_swap_node([pos1, pos2, ...], node)
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_bulk_swap_node(lua_State *L)
{
	GET_ENV_PTR;

	luaL_checktype(L, 1, LUA_TTABLE);

	s32 len = lua_objlen(L, 1);

	MapNode n = readnode(L, 2);

	// Do it
	bool succeeded = true;
	for (s32 i = 1; i <= len; i++) {
		lua_rawgeti(L, 1, i);
		if (!env->swapNode(read_v3s16(L, -1), n))
			succeeded = false;
		lua_pop(L, 1);
	}

	lua_pushboolean(L, succeeded);
	return 1;
}

// get_node_raw(x, y, z) -> content, param1, param2, pos_ok
int ModApiEnv::l_get_node_raw(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	v3s16 pos;
	// mirrors the implementation of read_v3s16 (with the exact same rounding)
	{
		if (lua_isnoneornil(L, 1))
			log_deprecated(L, "X position is nil", 1, true);
		if (lua_isnoneornil(L, 2))
			log_deprecated(L, "Y position is nil", 1, true);
		if (lua_isnoneornil(L, 3))
			log_deprecated(L, "Z position is nil", 1, true);
		double x = lua_tonumber(L, 1);
		double y = lua_tonumber(L, 2);
		double z = lua_tonumber(L, 3);
		pos = doubleToInt(v3d(x, y, z), 1.0);
	}
	bool pos_ok;
	MapNode n = env->getMap().getNode(pos, &pos_ok);
	// Return node and pos_ok
	lua_pushinteger(L, n.getContent());
	lua_pushinteger(L, n.getParam1());
	lua_pushinteger(L, n.getParam2());
	lua_pushboolean(L, pos_ok);
	return 4;
}

// get_node_light(pos, timeofday)
// pos = {x=num, y=num, z=num}
// timeofday: nil = current time, 0 = night, 0.5 = day
int ModApiEnv::l_get_node_light(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	// Do it
	v3s16 pos = read_v3s16(L, 1);
	u32 time_of_day = env->getTimeOfDay();
	if(lua_isnumber(L, 2))
		time_of_day = 24000.0 * lua_tonumber(L, 2);
	time_of_day %= 24000;
	u32 dnr = time_to_daynight_ratio(time_of_day, true);

	bool is_position_ok;
	MapNode n = env->getMap().getNode(pos, &is_position_ok);
	if (is_position_ok) {
		const NodeDefManager *ndef = env->getGameDef()->ndef();
		lua_pushinteger(L, n.getLightBlend(dnr, ndef->getLightingFlags(n)));
	} else {
		lua_pushnil(L);
	}
	return 1;
}


// get_natural_light(pos, timeofday)
// pos = {x=num, y=num, z=num}
// timeofday: nil = current time, 0 = night, 0.5 = day
int ModApiEnv::l_get_natural_light(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 pos = read_v3s16(L, 1);

	bool is_position_ok;
	MapNode n = env->getMap().getNode(pos, &is_position_ok);
	if (!is_position_ok)
		return 0;

	// If the daylight is 0, nothing needs to be calculated
	u8 daylight = n.param1 & 0x0f;
	if (daylight == 0) {
		lua_pushinteger(L, 0);
		return 1;
	}

	u32 time_of_day;
	if (lua_isnumber(L, 2)) {
		time_of_day = 24000.0 * lua_tonumber(L, 2);
		time_of_day %= 24000;
	} else {
		time_of_day = env->getTimeOfDay();
	}
	u32 dnr = time_to_daynight_ratio(time_of_day, true);

	// If it's the same as the artificial light, the sunlight needs to be
	// searched for because the value may not emanate from the sun
	if (daylight == n.param1 >> 4)
		daylight = env->findSunlight(pos);

	lua_pushinteger(L, dnr * daylight / 1000);
	return 1;
}

// place_node(pos, node, [placer])
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_place_node(lua_State *L)
{
	GET_ENV_PTR;

	ScriptApiItem *scriptIfaceItem = getScriptApi<ScriptApiItem>(L);
	Server *server = getServer(L);
	const NodeDefManager *ndef = server->ndef();
	IItemDefManager *idef = server->idef();

	v3s16 pos = read_v3s16(L, 1);
	MapNode n = readnode(L, 2);

	// Don't attempt to load non-loaded area as of now
	MapNode n_old = env->getMap().getNode(pos);
	if(n_old.getContent() == CONTENT_IGNORE){
		lua_pushboolean(L, false);
		return 1;
	}

	// Create item to place
	std::optional<ItemStack> item = ItemStack(ndef->get(n).name, 1, 0, idef);
	// Make pointed position
	PointedThing pointed;
	pointed.type = POINTEDTHING_NODE;
	pointed.node_abovesurface = pos;
	pointed.node_undersurface = pos + v3s16(0,-1,0);

	ServerActiveObject *placer = nullptr;

	if (!lua_isnoneornil(L, 3)) {
		ObjectRef *ref = checkObject<ObjectRef>(L, 3);
		placer = ObjectRef::getobject(ref);
	}

	// Place it with a nullptr placer (appears in Lua as nil)
	// or the given ObjectRef
	bool success = scriptIfaceItem->item_OnPlace(item, placer, pointed);
	lua_pushboolean(L, success);
	return 1;
}

// dig_node(pos, [digger])
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_dig_node(lua_State *L)
{
	GET_ENV_PTR;

	ScriptApiNode *scriptIfaceNode = getScriptApi<ScriptApiNode>(L);

	v3s16 pos = read_v3s16(L, 1);

	// Don't attempt to load non-loaded area as of now
	MapNode n = env->getMap().getNode(pos);
	if(n.getContent() == CONTENT_IGNORE){
		lua_pushboolean(L, false);
		return 1;
	}

	ServerActiveObject *digger = nullptr;

	if (!lua_isnoneornil(L, 2)) {
		ObjectRef *ref = checkObject<ObjectRef>(L, 2);
		digger = ObjectRef::getobject(ref);
	}

	// Dig it out with a nullptr digger
	// (appears in Lua as a non-functional ObjectRef)
	// or the given ObjectRef
	bool success = scriptIfaceNode->node_on_dig(pos, n, digger);
	lua_pushboolean(L, success);
	return 1;
}

// punch_node(pos, [puncher])
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_punch_node(lua_State *L)
{
	GET_ENV_PTR;

	ScriptApiNode *scriptIfaceNode = getScriptApi<ScriptApiNode>(L);

	v3s16 pos = read_v3s16(L, 1);

	// Don't attempt to load non-loaded area as of now
	MapNode n = env->getMap().getNode(pos);
	if(n.getContent() == CONTENT_IGNORE){
		lua_pushboolean(L, false);
		return 1;
	}

	ServerActiveObject *puncher = nullptr;

	if (!lua_isnoneornil(L, 2)) {
		ObjectRef *ref = checkObject<ObjectRef>(L, 2);
		puncher = ObjectRef::getobject(ref);
	}

	// Punch it with a nullptr puncher
	// (appears in Lua as a non-functional ObjectRef)
	// or the given ObjectRef
	// TODO: custom PointedThing (requires a converter function)
	bool success = scriptIfaceNode->node_on_punch(pos, n, puncher, PointedThing());
	lua_pushboolean(L, success);
	return 1;
}

// get_node_max_level(pos)
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_get_node_max_level(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	v3s16 pos = read_v3s16(L, 1);
	MapNode n = env->getMap().getNode(pos);
	lua_pushnumber(L, n.getMaxLevel(env->getGameDef()->ndef()));
	return 1;
}

// get_node_level(pos)
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_get_node_level(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	v3s16 pos = read_v3s16(L, 1);
	MapNode n = env->getMap().getNode(pos);
	lua_pushnumber(L, n.getLevel(env->getGameDef()->ndef()));
	return 1;
}

// set_node_level(pos, level)
// pos = {x=num, y=num, z=num}
// level: 0..63
int ModApiEnv::l_set_node_level(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 pos = read_v3s16(L, 1);
	u8 level = 1;
	if(lua_isnumber(L, 2))
		level = lua_tonumber(L, 2);
	MapNode n = env->getMap().getNode(pos);
	lua_pushnumber(L, n.setLevel(env->getGameDef()->ndef(), level));
	env->swapNode(pos, n);
	return 1;
}

// add_node_level(pos, level)
// pos = {x=num, y=num, z=num}
// level: -127..127
int ModApiEnv::l_add_node_level(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 pos = read_v3s16(L, 1);
	s16 level = 1;
	if(lua_isnumber(L, 2))
		level = lua_tonumber(L, 2);
	MapNode n = env->getMap().getNode(pos);
	lua_pushnumber(L, n.addLevel(env->getGameDef()->ndef(), level));
	env->swapNode(pos, n);
	return 1;
}

// get_node_boxes(box_type, pos, [node]) -> table
// box_type = string
// pos = {x=num, y=num, z=num}
// node = {name=string, param1=num, param2=num} or nil
int ModApiEnv::l_get_node_boxes(lua_State *L)
{
	GET_ENV_PTR;

	std::string box_type = luaL_checkstring(L, 1);
	v3s16 pos = read_v3s16(L, 2);
	MapNode n;
	if (lua_istable(L, 3))
		n = readnode(L, 3);
	else
		n = env->getMap().getNode(pos);

	u8 neighbors = n.getNeighbors(pos, &env->getMap());
	const NodeDefManager *ndef = env->getGameDef()->ndef();

	std::vector<aabb3f> boxes;
	if (box_type == "node_box")
		n.getNodeBoxes(ndef, &boxes, neighbors);
	else if (box_type == "collision_box")
		n.getCollisionBoxes(ndef, &boxes, neighbors);
	else if (box_type == "selection_box")
		n.getSelectionBoxes(ndef, &boxes, neighbors);
	else
		luaL_error(L, "get_node_boxes: box_type is invalid. Allowed values: \"node_box\", \"collision_box\", \"selection_box\"");

	push_aabb3f_vector(L, boxes, BS);

	return 1;
}

// find_nodes_with_meta(pos1, pos2)
int ModApiEnv::l_find_nodes_with_meta(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	std::vector<v3s16> positions = env->getMap().findNodesWithMetadata(
		check_v3s16(L, 1), check_v3s16(L, 2));

	lua_createtable(L, positions.size(), 0);
	for (size_t i = 0; i != positions.size(); i++) {
		push_v3s16(L, positions[i]);
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}

// get_meta(pos)
int ModApiEnv::l_get_meta(lua_State *L)
{
	GET_ENV_PTR;

	// Do it
	v3s16 p = read_v3s16(L, 1);
	NodeMetaRef::create(L, p, env);
	return 1;
}

// get_node_timer(pos)
int ModApiEnv::l_get_node_timer(lua_State *L)
{
	GET_ENV_PTR;

	// Do it
	v3s16 p = read_v3s16(L, 1);
	NodeTimerRef::create(L, p, &env->getServerMap());
	return 1;
}

// add_entity(pos, entityname, [staticdata]) -> ObjectRef or nil
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_add_entity(lua_State *L)
{
	GET_ENV_PTR;

	v3f pos = checkFloatPos(L, 1);
	const char *name = luaL_checkstring(L, 2);
	std::string staticdata = readParam<std::string>(L, 3, "");

	std::unique_ptr<ServerActiveObject> obj_u =
			std::make_unique<LuaEntitySAO>(env, pos, name, staticdata);
	auto obj = obj_u.get();
	int objectid = env->addActiveObject(std::move(obj_u));
	// If failed to add, return nothing (reads as nil)
	if (objectid == 0)
		return 0;

	// If already deleted (can happen in on_activate), return nil
	if (obj->isGone())
		return 0;
	getScriptApiBase(L)->objectrefGetOrCreate(L, obj);
	return 1;
}

// add_item(pos, itemstack or itemstring or table) -> ObjectRef or nil
// pos = {x=num, y=num, z=num}
int ModApiEnv::l_add_item(lua_State *L)
{
	GET_ENV_PTR;

	// pos
	//v3f pos = checkFloatPos(L, 1);
	// item
	ItemStack item = read_item(L, 2,getServer(L)->idef());
	if(item.empty() || !item.isKnown(getServer(L)->idef()))
		return 0;

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Use spawn_item to spawn a __builtin:item
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "spawn_item");
	lua_remove(L, -2); // Remove core
	if(lua_isnil(L, -1))
		return 0;
	lua_pushvalue(L, 1);
	lua_pushstring(L, item.getItemString().c_str());

	PCALL_RESL(L, lua_pcall(L, 2, 1, error_handler));

	lua_remove(L, error_handler);
	return 1;
}

// get_connected_players()
int ModApiEnv::l_get_connected_players(lua_State *L)
{
	ServerEnvironment *env = (ServerEnvironment *) getEnv(L);
	if (!env) {
		log_deprecated(L, "Calling get_connected_players() at mod load time"
				" is deprecated");
		lua_createtable(L, 0, 0);
		return 1;
	}

	lua_createtable(L, env->getPlayerCount(), 0);
	u32 i = 0;
	for (RemotePlayer *player : env->getPlayers()) {
		PlayerSAO *sao = player->getPlayerSAO();
		if (sao && !sao->isGone()) {
			getScriptApiBase(L)->objectrefGetOrCreate(L, sao);
			lua_rawseti(L, -2, ++i);
		}
	}
	return 1;
}

// get_player_by_name(name)
int ModApiEnv::l_get_player_by_name(lua_State *L)
{
	GET_ENV_PTR;

	// Do it
	const char *name = luaL_checkstring(L, 1);
	RemotePlayer *player = env->getPlayer(name);
	if (!player)
		return 0;
	PlayerSAO *sao = player->getPlayerSAO();
	if (!sao || sao->isGone())
		return 0;
	// Put player on stack
	getScriptApiBase(L)->objectrefGetOrCreate(L, sao);
	return 1;
}

// get_objects_inside_radius(pos, radius)
int ModApiEnv::l_get_objects_inside_radius(lua_State *L)
{
	GET_ENV_PTR;
	ScriptApiBase *script = getScriptApiBase(L);

	// Do it
	v3f pos = checkFloatPos(L, 1);
	float radius = readParam<float>(L, 2) * BS;
	std::vector<ServerActiveObject *> objs;

	auto include_obj_cb = [](ServerActiveObject *obj){ return !obj->isGone(); };
	env->getObjectsInsideRadius(objs, pos, radius, include_obj_cb);

	int i = 0;
	lua_createtable(L, objs.size(), 0);
	for (const auto obj : objs) {
		// Insert object reference into table
		script->objectrefGetOrCreate(L, obj);
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

// get_objects_in_area(pos, minp, maxp)
int ModApiEnv::l_get_objects_in_area(lua_State *L)
{
	GET_ENV_PTR;
	ScriptApiBase *script = getScriptApiBase(L);

	v3f minp = read_v3f(L, 1) * BS;
	v3f maxp = read_v3f(L, 2) * BS;
	aabb3f box(minp, maxp);
	box.repair();
	std::vector<ServerActiveObject *> objs;

	auto include_obj_cb = [](ServerActiveObject *obj){ return !obj->isGone(); };
	env->getObjectsInArea(objs, box, include_obj_cb);

	int i = 0;
	lua_createtable(L, objs.size(), 0);
	for (const auto obj : objs) {
		// Insert object reference into table
		script->objectrefGetOrCreate(L, obj);
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

// set_timeofday(val)
// val = 0...1
int ModApiEnv::l_set_timeofday(lua_State *L)
{
	GET_ENV_PTR;

	// Do it
	float timeofday_f = readParam<float>(L, 1);
	luaL_argcheck(L, timeofday_f >= 0.0f && timeofday_f <= 1.0f, 1,
		"value must be between 0 and 1");
	int timeofday_mh = (int)(timeofday_f * 24000.0f);
	// This should be set directly in the environment but currently
	// such changes aren't immediately sent to the clients, so call
	// the server instead.
	//env->setTimeOfDay(timeofday_mh);
	getServer(L)->setTimeOfDay(timeofday_mh);
	return 0;
}

// get_timeofday() -> 0...1
int ModApiEnv::l_get_timeofday(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	lua_pushnumber(L, env->getTimeOfDayF());
	return 1;
}

// get_day_count() -> int
int ModApiEnv::l_get_day_count(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	lua_pushnumber(L, env->getDayCount());
	return 1;
}

// get_gametime()
int ModApiEnv::l_get_gametime(lua_State *L)
{
	GET_ENV_PTR;

	lua_pushnumber(L, env->getGameTime());
	return 1;
}

void ModApiEnvBase::collectNodeIds(lua_State *L, int idx, const NodeDefManager *ndef,
	std::vector<content_t> &filter)
{
	if (lua_istable(L, idx)) {
		lua_pushnil(L);
		while (lua_next(L, idx) != 0) {
			// key at index -2 and value at index -1
			luaL_checktype(L, -1, LUA_TSTRING);
			ndef->getIds(readParam<std::string>(L, -1), filter);
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	} else if (lua_isstring(L, idx)) {
		ndef->getIds(readParam<std::string>(L, idx), filter);
	}
}

template <typename F>
int ModApiEnvBase::findNodeNear(lua_State *L, v3s16 pos, int radius,
		const std::vector<content_t> &filter, int start_radius, F &&getNode)
{
	for (int d = start_radius; d <= radius; d++) {
		const std::vector<v3s16> &list = FacePositionCache::getFacePositions(d);
		for (const v3s16 &i : list) {
			v3s16 p = pos + i;
			content_t c = getNode(p).getContent();
			if (CONTAINS(filter, c)) {
				push_v3s16(L, p);
				return 1;
			}
		}
	}
	return 0;
}

// find_node_near(pos, radius, nodenames, [search_center]) -> pos or nil
// nodenames: eg. {"ignore", "group:tree"} or "default:dirt"
int ModApiEnv::l_find_node_near(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	const NodeDefManager *ndef = env->getGameDef()->ndef();
	Map &map = env->getMap();

	v3s16 pos = read_v3s16(L, 1);
	int radius = luaL_checkinteger(L, 2);
	std::vector<content_t> filter;
	collectNodeIds(L, 3, ndef, filter);

	int start_radius = (lua_isboolean(L, 4) && readParam<bool>(L, 4)) ? 0 : 1;

#if CHECK_CLIENT_BUILD()
	// Client API limitations
	if (Client *client = getClient(L))
		radius = client->CSMClampRadius(pos, radius);
#endif

	auto getNode = [&map] (v3s16 p) -> MapNode {
		return map.getNode(p);
	};
	return findNodeNear(L, pos, radius, filter, start_radius, getNode);
}

void ModApiEnvBase::checkArea(v3s16 &minp, v3s16 &maxp)
{
	auto volume = VoxelArea(minp, maxp).getVolume();
	if (volume > MAX_WORKING_VOLUME) {
		throw LuaError("Area volume exceeds allowed value of " + std::to_string(MAX_WORKING_VOLUME));
	}

	// Clamp to map range to avoid problems
#define CLAMP(arg) core::clamp(arg, (s16)-MAX_MAP_GENERATION_LIMIT, (s16)MAX_MAP_GENERATION_LIMIT)
	minp = v3s16(CLAMP(minp.X), CLAMP(minp.Y), CLAMP(minp.Z));
	maxp = v3s16(CLAMP(maxp.X), CLAMP(maxp.Y), CLAMP(maxp.Z));
#undef CLAMP
}

template <typename F>
int ModApiEnvBase::findNodesInArea(lua_State *L, const NodeDefManager *ndef,
		const std::vector<content_t> &filter, bool grouped, F &&iterate)
{
	if (grouped) {
		// create the table we will be returning
		lua_createtable(L, 0, filter.size());
		int base = lua_gettop(L);

		// create one table for each filter
		std::vector<u32> idx;
		idx.resize(filter.size());
		for (u32 i = 0; i < filter.size(); i++)
			lua_newtable(L);

		iterate([&](v3s16 p, MapNode n) -> bool {
			content_t c = n.getContent();

			auto it = std::find(filter.begin(), filter.end(), c);
			if (it != filter.end()) {
				// Calculate index of the table and append the position
				u32 filt_index = it - filter.begin();
				push_v3s16(L, p);
				lua_rawseti(L, base + 1 + filt_index, ++idx[filt_index]);
			}

			return true;
		});

		// last filter table is at top of stack
		u32 i = filter.size();
		while (i --> 0) {
			if (idx[i] == 0) {
				// No such node found -> drop the empty table
				lua_pop(L, 1);
			} else {
				// This node was found -> put table into the return table
				lua_setfield(L, base, ndef->get(filter[i]).name.c_str());
			}
		}

		assert(lua_gettop(L) == base);
		return 1;
	} else {
		std::vector<u32> individual_count;
		individual_count.resize(filter.size());

		lua_newtable(L);
		u32 i = 0;
		iterate([&](v3s16 p, MapNode n) -> bool {
			content_t c = n.getContent();

			auto it = std::find(filter.begin(), filter.end(), c);
			if (it != filter.end()) {
				push_v3s16(L, p);
				lua_rawseti(L, -2, ++i);

				u32 filt_index = it - filter.begin();
				individual_count[filt_index]++;
			}

			return true;
		});

		lua_createtable(L, 0, filter.size());
		for (u32 i = 0; i < filter.size(); i++) {
			lua_pushinteger(L, individual_count[i]);
			lua_setfield(L, -2, ndef->get(filter[i]).name.c_str());
		}
		return 2;
	}
}

// find_nodes_in_area(minp, maxp, nodenames, [grouped])
int ModApiEnv::l_find_nodes_in_area(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	v3s16 minp = read_v3s16(L, 1);
	v3s16 maxp = read_v3s16(L, 2);
	sortBoxVerticies(minp, maxp);

	const NodeDefManager *ndef = env->getGameDef()->ndef();
	Map &map = env->getMap();

#if CHECK_CLIENT_BUILD()
	if (Client *client = getClient(L)) {
		minp = client->CSMClampPos(minp);
		maxp = client->CSMClampPos(maxp);
	}
#endif

	checkArea(minp, maxp);

	std::vector<content_t> filter;
	collectNodeIds(L, 3, ndef, filter);

	bool grouped = lua_isboolean(L, 4) && readParam<bool>(L, 4);

	auto iterate = [&] (auto &&callback) {
		map.forEachNodeInArea(minp, maxp, callback);
	};
	return findNodesInArea(L, ndef, filter, grouped, iterate);
}

template <typename F>
int ModApiEnvBase::findNodesInAreaUnderAir(lua_State *L, v3s16 minp, v3s16 maxp,
	const std::vector<content_t> &filter, F &&getNode)
{
	lua_newtable(L);
	u32 i = 0;
	v3s16 p;
	for (p.X = minp.X; p.X <= maxp.X; p.X++)
	for (p.Z = minp.Z; p.Z <= maxp.Z; p.Z++) {
		p.Y = minp.Y;
		content_t c = getNode(p).getContent();
		for (; p.Y <= maxp.Y; p.Y++) {
			v3s16 psurf(p.X, p.Y + 1, p.Z);
			content_t csurf = getNode(psurf).getContent();
			if (c != CONTENT_AIR && csurf == CONTENT_AIR &&
					CONTAINS(filter, c)) {
				push_v3s16(L, p);
				lua_rawseti(L, -2, ++i);
			}
			c = csurf;
		}
	}
	return 1;
}

// find_nodes_in_area_under_air(minp, maxp, nodenames) -> list of positions
// nodenames: e.g. {"ignore", "group:tree"} or "default:dirt"
int ModApiEnv::l_find_nodes_in_area_under_air(lua_State *L)
{
	/* TODO: A similar but generalized (and therefore slower) version of this
	 * function could be created -- e.g. find_nodes_in_area_under -- which
	 * would accept a node name or list of names that the "above node" should be.
	 */
	GET_PLAIN_ENV_PTR;

	v3s16 minp = read_v3s16(L, 1);
	v3s16 maxp = read_v3s16(L, 2);
	sortBoxVerticies(minp, maxp);

	const NodeDefManager *ndef = env->getGameDef()->ndef();
	Map &map = env->getMap();

#if CHECK_CLIENT_BUILD()
	if (Client *client = getClient(L)) {
		minp = client->CSMClampPos(minp);
		maxp = client->CSMClampPos(maxp);
	}
#endif

	checkArea(minp, maxp);

	std::vector<content_t> filter;
	collectNodeIds(L, 3, ndef, filter);

	auto getNode = [&map] (v3s16 p) -> MapNode {
		return map.getNode(p);
	};
	return findNodesInAreaUnderAir(L, minp, maxp, filter, getNode);
}

// get_value_noise(seeddiff, octaves, persistence, scale)
// returns world-specific ValueNoise
int ModApiEnv::l_get_value_noise(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	NoiseParams params;

	if (lua_istable(L, 1)) {
		read_noiseparams(L, 1, &params);
	} else {
		params.seed    = luaL_checkint(L, 1);
		params.octaves = luaL_checkint(L, 2);
		params.persist = readParam<float>(L, 3);
		params.spread  = v3f(1, 1, 1) * readParam<float>(L, 4);
	}

	params.seed += (int)env->getServerMap().getSeed();

	LuaValueNoise *n = new LuaValueNoise(&params);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = n;
	luaL_getmetatable(L, "ValueNoise");
	lua_setmetatable(L, -2);
	return 1;
}

// get_value_noise_map(noiseparams, size)
// returns world-specific ValueNoiseMap
int ModApiEnv::l_get_value_noise_map(lua_State *L)
{
	GET_ENV_PTR_NO_MAP_LOCK;

	NoiseParams np;
	if (!read_noiseparams(L, 1, &np))
		return 0;
	v3s16 size = read_v3s16(L, 2);

	s32 seed = (s32)(env->getServerMap().getSeed());
	LuaValueNoiseMap *n = new LuaValueNoiseMap(&np, seed, size);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = n;
	luaL_getmetatable(L, "ValueNoiseMap");
	lua_setmetatable(L, -2);
	return 1;
}

// get_voxel_manip()
// returns voxel manipulator
int ModApiEnv::l_get_voxel_manip(lua_State *L)
{
	return LuaVoxelManip::create_object(L);
}

// clear_objects([options])
// clear all objects in the environment
// where options = {mode = "full" or "quick"}
int ModApiEnv::l_clear_objects(lua_State *L)
{
	GET_ENV_PTR;

	ClearObjectsMode mode = CLEAR_OBJECTS_MODE_QUICK;
	if (lua_istable(L, 1)) {
		mode = (ClearObjectsMode)getenumfield(L, 1, "mode",
			ModApiEnv::es_ClearObjectsMode, mode);
	}

	env->clearObjects(mode);
	return 0;
}

// line_of_sight(pos1, pos2) -> true/false, pos
int ModApiEnv::l_line_of_sight(lua_State *L)
{
	GET_PLAIN_ENV_PTR;

	// read position 1 from lua
	v3f pos1 = checkFloatPos(L, 1);
	// read position 2 from lua
	v3f pos2 = checkFloatPos(L, 2);

	v3s16 p;

	bool success = env->line_of_sight(pos1, pos2, &p);
	lua_pushboolean(L, success);
	if (!success) {
		push_v3s16(L, p);
		return 2;
	}
	return 1;
}

// fix_light(p1, p2)
int ModApiEnv::l_fix_light(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 blockpos1 = getContainerPos(read_v3s16(L, 1), MAP_BLOCKSIZE);
	v3s16 blockpos2 = getContainerPos(read_v3s16(L, 2), MAP_BLOCKSIZE);
	ServerMap &map = env->getServerMap();
	std::map<v3s16, MapBlock *> modified_blocks;
	bool success = true;
	v3s16 blockpos;
	for (blockpos.X = blockpos1.X; blockpos.X <= blockpos2.X; blockpos.X++)
	for (blockpos.Y = blockpos1.Y; blockpos.Y <= blockpos2.Y; blockpos.Y++)
	for (blockpos.Z = blockpos1.Z; blockpos.Z <= blockpos2.Z; blockpos.Z++) {
		success = success & map.repairBlockLight(blockpos, &modified_blocks);
	}
	if (!modified_blocks.empty()) {
		MapEditEvent event;
		event.type = MEET_OTHER;
		event.setModifiedBlocks(modified_blocks);

		map.dispatchEvent(event);
	}
	lua_pushboolean(L, success);

	return 1;
}

int ModApiEnv::l_raycast(lua_State *L)
{
	return LuaRaycast::create_object(L);
}

// load_area(p1, [p2])
// load mapblocks in area p1..p2, but do not generate map
int ModApiEnv::l_load_area(lua_State *L)
{
	GET_ENV_PTR;

	Map *map = &(env->getMap());
	v3s16 bp1 = getNodeBlockPos(check_v3s16(L, 1));
	if (!lua_istable(L, 2)) {
		map->emergeBlock(bp1);
	} else {
		v3s16 bp2 = getNodeBlockPos(check_v3s16(L, 2));
		sortBoxVerticies(bp1, bp2);
		for (s16 z = bp1.Z; z <= bp2.Z; z++)
		for (s16 y = bp1.Y; y <= bp2.Y; y++)
		for (s16 x = bp1.X; x <= bp2.X; x++) {
			map->emergeBlock(v3s16(x, y, z));
		}
	}

	return 0;
}

// emerge_area(p1, p2, [callback, context])
// emerge mapblocks in area p1..p2, calls callback with context upon completion
int ModApiEnv::l_emerge_area(lua_State *L)
{
	GET_ENV_PTR;

	EmergeCompletionCallback callback = NULL;
	ScriptCallbackState *state = NULL;

	EmergeManager *emerge = getServer(L)->getEmergeManager();

	v3s16 bpmin = getNodeBlockPos(read_v3s16(L, 1));
	v3s16 bpmax = getNodeBlockPos(read_v3s16(L, 2));
	sortBoxVerticies(bpmin, bpmax);

	size_t num_blocks = VoxelArea(bpmin, bpmax).getVolume();
	if (num_blocks == 0)
		return 0;

	if (lua_isfunction(L, 3)) {
		callback = LuaEmergeAreaCallback;

		lua_pushvalue(L, 3);
		int callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

		lua_pushvalue(L, 4);
		int args_ref = luaL_ref(L, LUA_REGISTRYINDEX);

		state = new ScriptCallbackState;
		state->script       = getServer(L)->getScriptIface();
		state->callback_ref = callback_ref;
		state->args_ref     = args_ref;
		state->refcount     = num_blocks;
		state->origin       = getScriptApiBase(L)->getOrigin();
	}

	for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
	for (s16 y = bpmin.Y; y <= bpmax.Y; y++)
	for (s16 x = bpmin.X; x <= bpmax.X; x++) {
		emerge->enqueueBlockEmergeEx(v3s16(x, y, z), PEER_ID_INEXISTENT,
			BLOCK_EMERGE_ALLOW_GEN | BLOCK_EMERGE_FORCE_QUEUE, callback, state);
	}

	return 0;
}

// delete_area(p1, p2)
// delete mapblocks in area p1..p2
int ModApiEnv::l_delete_area(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 bpmin = getNodeBlockPos(read_v3s16(L, 1));
	v3s16 bpmax = getNodeBlockPos(read_v3s16(L, 2));
	sortBoxVerticies(bpmin, bpmax);

	ServerMap &map = env->getServerMap();

	MapEditEvent event;
	event.type = MEET_OTHER;

	bool success = true;
	for (s16 z = bpmin.Z; z <= bpmax.Z; z++)
	for (s16 y = bpmin.Y; y <= bpmax.Y; y++)
	for (s16 x = bpmin.X; x <= bpmax.X; x++) {
		v3s16 bp(x, y, z);
		if (map.deleteBlock(bp)) {
			env->setStaticForActiveObjectsInBlock(bp, false);
			event.modified_blocks.push_back(bp);
		} else {
			success = false;
		}
	}

	map.dispatchEvent(event);
	lua_pushboolean(L, success);
	return 1;
}

// find_path(pos1, pos2, searchdistance,
//     max_jump, max_drop, algorithm) -> table containing path
int ModApiEnv::l_find_path(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 pos1                  = read_v3s16(L, 1);
	v3s16 pos2                  = read_v3s16(L, 2);
	unsigned int searchdistance = luaL_checkint(L, 3);
	unsigned int max_jump       = luaL_checkint(L, 4);
	unsigned int max_drop       = luaL_checkint(L, 5);
	PathAlgorithm algo          = PA_PLAIN_NP;
	if (!lua_isnoneornil(L, 6)) {
		std::string algorithm = luaL_checkstring(L,6);

		if (algorithm == "A*")
			algo = PA_PLAIN;

		if (algorithm == "Dijkstra")
			algo = PA_DIJKSTRA;
	}

	std::vector<v3s16> path = get_path(&env->getServerMap(), env->getGameDef()->ndef(), pos1, pos2,
		searchdistance, max_jump, max_drop, algo);

	if (!path.empty()) {
		lua_createtable(L, path.size(), 0);
		int top = lua_gettop(L);
		unsigned int index = 1;
		for (const v3s16 &i : path) {
			lua_pushnumber(L,index);
			push_v3s16(L, i);
			lua_settable(L, top);
			index++;
		}
		return 1;
	}

	return 0;
}

// spawn_tree(pos, treedef)
int ModApiEnv::l_spawn_tree(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 p0 = read_v3s16(L, 1);

	treegen::TreeDef tree_def;
	const NodeDefManager *ndef = env->getGameDef()->ndef();

	if (!read_tree_def(L, 2, ndef, tree_def))
		return 0;

	ServerMap *map = &env->getServerMap();
	treegen::error e;
	if ((e = treegen::spawn_ltree (map, p0, tree_def)) != treegen::SUCCESS) {
		throw LuaError("spawn_tree(): " + treegen::error_to_string(e));
	}

	lua_pushboolean(L, true);
	return 1;
}

// transforming_liquid_add(pos)
int ModApiEnv::l_transforming_liquid_add(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 p0 = read_v3s16(L, 1);
	env->getServerMap().transforming_liquid_add(p0);
	return 1;
}

// forceload_block(blockpos)
// blockpos = {x=num, y=num, z=num}
int ModApiEnv::l_forceload_block(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 blockpos = read_v3s16(L, 1);
	env->getForceloadedBlocks()->insert(blockpos);
	return 0;
}

// compare_block_status(nodepos)
int ModApiEnv::l_compare_block_status(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 nodepos = check_v3s16(L, 1);
	std::string condition_s = luaL_checkstring(L, 2);
	auto status = env->getBlockStatus(getNodeBlockPos(nodepos));

	int condition_i = -1;
	if (!string_to_enum(es_BlockStatusType, condition_i, condition_s))
		return 0; // Unsupported

	lua_pushboolean(L, status >= condition_i);
	return 1;
}


// forceload_free_block(blockpos)
// blockpos = {x=num, y=num, z=num}
int ModApiEnv::l_forceload_free_block(lua_State *L)
{
	GET_ENV_PTR;

	v3s16 blockpos = read_v3s16(L, 1);
	env->getForceloadedBlocks()->erase(blockpos);
	return 0;
}

// get_translated_string(lang_code, string)
int ModApiEnv::l_get_translated_string(lua_State * L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string lang_code = luaL_checkstring(L, 1);
	std::string string = luaL_checkstring(L, 2);

	auto *translations = getServer(L)->getTranslationLanguage(lang_code);
	string = wide_to_utf8(translate_string(utf8_to_wide(string), translations));
	lua_pushstring(L, string.c_str());
	return 1;
}

void ModApiEnv::Initialize(lua_State *L, int top)
{
	API_FCT(set_node);
	API_FCT(bulk_set_node);
	API_FCT(add_node);
	API_FCT(swap_node);
	API_FCT(bulk_swap_node);
	API_FCT(add_item);
	API_FCT(remove_node);
	API_FCT(get_node_raw);
	API_FCT(get_node_light);
	API_FCT(get_natural_light);
	API_FCT(place_node);
	API_FCT(dig_node);
	API_FCT(punch_node);
	API_FCT(get_node_max_level);
	API_FCT(get_node_level);
	API_FCT(set_node_level);
	API_FCT(add_node_level);
	API_FCT(get_node_boxes);
	API_FCT(add_entity);
	API_FCT(find_nodes_with_meta);
	API_FCT(get_meta);
	API_FCT(get_node_timer);
	API_FCT(get_connected_players);
	API_FCT(get_player_by_name);
	API_FCT(get_objects_in_area);
	API_FCT(get_objects_inside_radius);
	API_FCT(set_timeofday);
	API_FCT(get_timeofday);
	API_FCT(get_gametime);
	API_FCT(get_day_count);
	API_FCT(find_node_near);
	API_FCT(find_nodes_in_area);
	API_FCT(find_nodes_in_area_under_air);
	API_FCT(fix_light);
	API_FCT(load_area);
	API_FCT(emerge_area);
	API_FCT(delete_area);
	API_FCT(get_value_noise);
	API_FCT(get_value_noise_map);
	API_FCT(get_voxel_manip);
	API_FCT(clear_objects);
	API_FCT(spawn_tree);
	API_FCT(find_path);
	API_FCT(line_of_sight);
	API_FCT(raycast);
	API_FCT(transforming_liquid_add);
	API_FCT(forceload_block);
	API_FCT(forceload_free_block);
	API_FCT(compare_block_status);
	API_FCT(get_translated_string);
}

void ModApiEnv::InitializeClient(lua_State *L, int top)
{
	API_FCT(get_node_light);
	API_FCT(get_timeofday);
	API_FCT(get_node_max_level);
	API_FCT(get_node_level);
	API_FCT(find_nodes_with_meta);
	API_FCT(find_node_near);
	API_FCT(find_nodes_in_area);
	API_FCT(find_nodes_in_area_under_air);
	API_FCT(line_of_sight);
	API_FCT(raycast);
}

#define GET_VM_PTR               \
	MMVManip *vm = getVManip(L); \
	if (!vm)                     \
		return 0

// get_node_or_nil(pos)
int ModApiEnvVM::l_get_node_or_nil(lua_State *L)
{
	GET_VM_PTR;

	v3s16 pos = read_v3s16(L, 1);
	if (vm->exists(pos))
		pushnode(L, vm->getNodeRefUnsafe(pos));
	else
		lua_pushnil(L);
	return 1;
}

// get_node_max_level(pos)
int ModApiEnvVM::l_get_node_max_level(lua_State *L)
{
	GET_VM_PTR;

	v3s16 pos = read_v3s16(L, 1);
	MapNode n = vm->getNodeNoExNoEmerge(pos);
	lua_pushnumber(L, n.getMaxLevel(getGameDef(L)->ndef()));
	return 1;
}

// get_node_level(pos)
int ModApiEnvVM::l_get_node_level(lua_State *L)
{
	GET_VM_PTR;

	v3s16 pos = read_v3s16(L, 1);
	MapNode n = vm->getNodeNoExNoEmerge(pos);
	lua_pushnumber(L, n.getLevel(getGameDef(L)->ndef()));
	return 1;
}

// set_node_level(pos, level)
int ModApiEnvVM::l_set_node_level(lua_State *L)
{
	GET_VM_PTR;

	v3s16 pos = read_v3s16(L, 1);
	u8 level = 1;
	if (lua_isnumber(L, 2))
		level = lua_tonumber(L, 2);
	MapNode n = vm->getNodeNoExNoEmerge(pos);
	lua_pushnumber(L, n.setLevel(getGameDef(L)->ndef(), level));
	vm->setNodeNoEmerge(pos, n);
	return 1;
}

// add_node_level(pos, level)
int ModApiEnvVM::l_add_node_level(lua_State *L)
{
	GET_VM_PTR;

	v3s16 pos = read_v3s16(L, 1);
	u8 level = 1;
	if (lua_isnumber(L, 2))
		level = lua_tonumber(L, 2);
	MapNode n = vm->getNodeNoExNoEmerge(pos);
	lua_pushnumber(L, n.addLevel(getGameDef(L)->ndef(), level));
	vm->setNodeNoEmerge(pos, n);
	return 1;
}

// find_node_near(pos, radius, nodenames, [search_center])
int ModApiEnvVM::l_find_node_near(lua_State *L)
{
	GET_VM_PTR;

	const NodeDefManager *ndef = getGameDef(L)->ndef();

	v3s16 pos = read_v3s16(L, 1);
	int radius = luaL_checkinteger(L, 2);
	std::vector<content_t> filter;
	collectNodeIds(L, 3, ndef, filter);
	int start_radius = (lua_isboolean(L, 4) && readParam<bool>(L, 4)) ? 0 : 1;

	auto getNode = [&vm] (v3s16 p) -> MapNode {
		return vm->getNodeNoExNoEmerge(p);
	};
	return findNodeNear(L, pos, radius, filter, start_radius, getNode);
}

// find_nodes_in_area(minp, maxp, nodenames, [grouped])
int ModApiEnvVM::l_find_nodes_in_area(lua_State *L)
{
	GET_VM_PTR;

	const NodeDefManager *ndef = getGameDef(L)->ndef();

	v3s16 minp = read_v3s16(L, 1);
	v3s16 maxp = read_v3s16(L, 2);
	sortBoxVerticies(minp, maxp);

	checkArea(minp, maxp);
	// avoid the loop going out-of-bounds
	{
		VoxelArea cropped = VoxelArea(minp, maxp).intersect(vm->m_area);
		minp = cropped.MinEdge;
		maxp = cropped.MaxEdge;
	}

	std::vector<content_t> filter;
	collectNodeIds(L, 3, ndef, filter);

	bool grouped = lua_isboolean(L, 4) && readParam<bool>(L, 4);

	auto iterate = [&] (auto callback) {
		for (s16 z = minp.Z; z <= maxp.Z; z++)
		for (s16 y = minp.Y; y <= maxp.Y; y++) {
			u32 vi = vm->m_area.index(minp.X, y, z);
			for (s16 x = minp.X; x <= maxp.X; x++) {
				v3s16 pos(x, y, z);
				MapNode n = vm->m_data[vi];
				if (!callback(pos, n))
					return;
				++vi;
			}
		}
	};
	return findNodesInArea(L, ndef, filter, grouped, iterate);
}

// find_nodes_in_area_under_air(minp, maxp, nodenames)
int ModApiEnvVM::l_find_nodes_in_area_under_air(lua_State *L)
{
	GET_VM_PTR;

	const NodeDefManager *ndef = getGameDef(L)->ndef();

	v3s16 minp = read_v3s16(L, 1);
	v3s16 maxp = read_v3s16(L, 2);
	sortBoxVerticies(minp, maxp);
	checkArea(minp, maxp);

	std::vector<content_t> filter;
	collectNodeIds(L, 3, ndef, filter);

	auto getNode = [&vm] (v3s16 p) -> MapNode {
		return vm->getNodeNoExNoEmerge(p);
	};
	return findNodesInAreaUnderAir(L, minp, maxp, filter, getNode);
}

// spawn_tree(pos, treedef)
int ModApiEnvVM::l_spawn_tree(lua_State *L)
{
	GET_VM_PTR;

	const NodeDefManager *ndef = getGameDef(L)->ndef();

	v3s16 p0 = read_v3s16(L, 1);

	treegen::TreeDef tree_def;
	if (!read_tree_def(L, 2, ndef, tree_def))
		return 0;

	treegen::error e;
	if ((e = treegen::make_ltree(*vm, p0, tree_def)) != treegen::SUCCESS) {
		throw LuaError("spawn_tree(): " + treegen::error_to_string(e));
	}

	lua_pushboolean(L, true);
	return 1;
}

MMVManip *ModApiEnvVM::getVManip(lua_State *L)
{
	auto emerge = getEmergeThread(L);
	if (emerge)
		return emerge->getMapgen()->vm;
	return nullptr;
}

void ModApiEnvVM::InitializeEmerge(lua_State *L, int top)
{
	// other, more trivial functions are in builtin/emerge/env.lua
	API_FCT(get_node_or_nil);
	API_FCT(get_node_max_level);
	API_FCT(get_node_level);
	API_FCT(set_node_level);
	API_FCT(add_node_level);
	API_FCT(find_node_near);
	API_FCT(find_nodes_in_area);
	API_FCT(find_nodes_in_area_under_air);
	API_FCT(spawn_tree);
}

#undef GET_VM_PTR
