// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_env.h"
#include "cpp_api/s_internal.h"
#include "common/c_converter.h"
#include "log.h"
#include "environment.h"
#include "mapgen/mapgen.h"
#include "lua_api/l_env.h"
#include "server.h"
#include "scripting_server.h"
#include "script/common/c_content.h"

/*
	LuaABM & LuaLBM
*/

class LuaABM : public ActiveBlockModifier {
private:
	const int m_id;

	std::vector<std::string> m_trigger_contents;
	std::vector<std::string> m_required_neighbors;
	std::vector<std::string> m_without_neighbors;
	float m_trigger_interval;
	u32 m_trigger_chance;
	bool m_simple_catch_up;
	s16 m_min_y;
	s16 m_max_y;
public:
	LuaABM(int id,
			const std::vector<std::string> &trigger_contents,
			const std::vector<std::string> &required_neighbors,
			const std::vector<std::string> &without_neighbors,
			float trigger_interval, u32 trigger_chance, bool simple_catch_up,
			s16 min_y, s16 max_y):
		m_id(id),
		m_trigger_contents(trigger_contents),
		m_required_neighbors(required_neighbors),
		m_without_neighbors(without_neighbors),
		m_trigger_interval(trigger_interval),
		m_trigger_chance(trigger_chance),
		m_simple_catch_up(simple_catch_up),
		m_min_y(min_y),
		m_max_y(max_y)
	{
	}
	virtual const std::vector<std::string> &getTriggerContents() const
	{
		return m_trigger_contents;
	}
	virtual const std::vector<std::string> &getRequiredNeighbors() const
	{
		return m_required_neighbors;
	}
	virtual const std::vector<std::string> &getWithoutNeighbors() const
	{
		return m_without_neighbors;
	}
	virtual float getTriggerInterval()
	{
		return m_trigger_interval;
	}
	virtual u32 getTriggerChance()
	{
		return m_trigger_chance;
	}
	virtual bool getSimpleCatchUp()
	{
		return m_simple_catch_up;
	}
	virtual s16 getMinY()
	{
		return m_min_y;
	}
	virtual s16 getMaxY()
	{
		return m_max_y;
	}

	virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider)
	{
		auto *script = env->getScriptIface();
		script->triggerABM(m_id, p, n, active_object_count, active_object_count_wider);
	}
};

class LuaLBM : public LoadingBlockModifierDef
{
private:
	int m_id;
public:
	LuaLBM(int id,
			const std::vector<std::string> &trigger_contents,
			const std::string &name, bool run_at_every_load):
		m_id(id)
	{
		this->run_at_every_load = run_at_every_load;
		this->trigger_contents = trigger_contents;
		this->name = name;
	}

	virtual void trigger(ServerEnvironment *env, MapBlock *block,
		const std::unordered_set<v3s16> &positions, float dtime_s)
	{
		auto *script = env->getScriptIface();
		script->triggerLBM(m_id, block, positions, dtime_s);
	}
};

/*
	ScriptApiEnv
*/

void ScriptApiEnv::environment_OnGenerated(v3s16 minp, v3s16 maxp,
	u32 blockseed)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_generateds
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_generateds");
	// Call callbacks
	push_v3s16(L, minp);
	push_v3s16(L, maxp);
	lua_pushnumber(L, blockseed);
	runCallbacks(3, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiEnv::environment_Step(float dtime)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_globalsteps
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_globalsteps");
	// Call callbacks
	lua_pushnumber(L, dtime);
	runCallbacks(1, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiEnv::player_event(ServerActiveObject *player, const std::string &type)
{
	SCRIPTAPI_PRECHECKHEADER

	if (player == NULL)
		return;

	// Get core.registered_playerevents
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_playerevents");

	// Call callbacks
	objectrefGetOrCreate(L, player);   // player
	lua_pushstring(L,type.c_str()); // event type
	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiEnv::initializeEnvironment(ServerEnvironment *env)
{
	SCRIPTAPI_PRECHECKHEADER

	assert(env);
	verbosestream << "ScriptApiEnv: Environment initialized" << std::endl;
	setEnv(env);

	readABMs();
	readLBMs();
}

// Reads a single or a list of node names into a vector
bool ScriptApiEnv::read_nodenames(lua_State *L, int idx, std::vector<std::string> &to)
{
	if (lua_istable(L, idx)) {
		const int table = idx < 0 ? (lua_gettop(L) + idx + 1) : idx;
		lua_pushnil(L);
		while (lua_next(L, table)) {
			// key at index -2 and value at index -1
			luaL_checktype(L, -1, LUA_TSTRING);
			to.emplace_back(readParam<std::string>(L, -1));
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
		}
	} else if (lua_isstring(L, idx)) {
		to.emplace_back(readParam<std::string>(L, idx));
	} else {
		return false;
	}
	return true;
}

void ScriptApiEnv::readABMs()
{
	SCRIPTAPI_PRECHECKHEADER
	auto *env = reinterpret_cast<ServerEnvironment*>(getEnv());

	// Get core.registered_abms
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_abms");
	int registered_abms = lua_gettop(L);

	if (!lua_istable(L, registered_abms)) {
		lua_pop(L, 1);
		throw LuaError("core.registered_abms was not a lua table, as expected.");
	}
	lua_pushnil(L);
	while (lua_next(L, registered_abms)) {
		// key at index -2 and value at index -1
		int id = lua_tonumber(L, -2);
		int current_abm = lua_gettop(L);

		std::vector<std::string> trigger_contents;
		lua_getfield(L, current_abm, "nodenames");
		read_nodenames(L, -1, trigger_contents);
		lua_pop(L, 1);

		std::vector<std::string> required_neighbors;
		lua_getfield(L, current_abm, "neighbors");
		read_nodenames(L, -1, required_neighbors);
		lua_pop(L, 1);

		std::vector<std::string> without_neighbors;
		lua_getfield(L, current_abm, "without_neighbors");
		read_nodenames(L, -1, without_neighbors);
		lua_pop(L, 1);

		float trigger_interval = 10.0;
		getfloatfield(L, current_abm, "interval", trigger_interval);

		int trigger_chance = 50;
		getintfield(L, current_abm, "chance", trigger_chance);

		bool simple_catch_up = true;
		getboolfield(L, current_abm, "catch_up", simple_catch_up);

		s16 min_y = INT16_MIN;
		getintfield(L, current_abm, "min_y", min_y);

		s16 max_y = INT16_MAX;
		getintfield(L, current_abm, "max_y", max_y);

		lua_getfield(L, current_abm, "action");
		luaL_checktype(L, current_abm + 1, LUA_TFUNCTION);
		lua_pop(L, 1);

		LuaABM *abm = new LuaABM(id, trigger_contents, required_neighbors,
			without_neighbors, trigger_interval, trigger_chance,
			simple_catch_up, min_y, max_y);

		env->addActiveBlockModifier(abm);

		// removes value, keeps key for next iteration
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

void ScriptApiEnv::readLBMs()
{
	SCRIPTAPI_PRECHECKHEADER
	auto *env = reinterpret_cast<ServerEnvironment*>(getEnv());

	// Get core.registered_lbms
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_lbms");
	int registered_lbms = lua_gettop(L);

	if (!lua_istable(L, registered_lbms)) {
		lua_pop(L, 1);
		throw LuaError("core.registered_lbms was not a lua table, as expected.");
	}

	lua_pushnil(L);
	while (lua_next(L, registered_lbms)) {
		// key at index -2 and value at index -1
		int id = lua_tonumber(L, -2);
		int current_lbm = lua_gettop(L);

		std::vector<std::string> trigger_contents;
		lua_getfield(L, current_lbm, "nodenames");
		read_nodenames(L, -1, trigger_contents);
		lua_pop(L, 1);

		std::string name;
		getstringfield(L, current_lbm, "name", name);

		bool run_at_every_load = getboolfield_default(L, current_lbm,
			"run_at_every_load", false);

		LuaLBM *lbm = new LuaLBM(id, trigger_contents, name,
			run_at_every_load);

		env->addLoadingBlockModifierDef(lbm);

		// removes value, keeps key for next iteration
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

void ScriptApiEnv::on_emerge_area_completion(
	v3s16 blockpos, int action, ScriptCallbackState *state)
{
	Server *server = getServer();

	// This function should be executed with envlock held.
	// The caller (LuaEmergeAreaCallback in src/script/lua_api/l_env.cpp)
	// should have obtained the lock.
	// Note that the order of these locks is important!  Envlock must *ALWAYS*
	// be acquired before attempting to acquire scriptlock, or else ServerThread
	// will try to acquire scriptlock after it already owns envlock, thus
	// deadlocking EmergeThread and ServerThread

	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, state->callback_ref);
	luaL_checktype(L, -1, LUA_TFUNCTION);

	push_v3s16(L, blockpos);
	lua_pushinteger(L, action);
	lua_pushinteger(L, state->refcount);
	lua_rawgeti(L, LUA_REGISTRYINDEX, state->args_ref);

	setOriginDirect(state->origin.c_str());

	try {
		PCALL_RES(lua_pcall(L, 4, 0, error_handler));
	} catch (LuaError &e) {
		// Note: don't throw here, we still need to run the cleanup code below
		server->setAsyncFatalError(e);
	}

	lua_pop(L, 1); // Pop error handler

	if (state->refcount == 0) {
		luaL_unref(L, LUA_REGISTRYINDEX, state->callback_ref);
		luaL_unref(L, LUA_REGISTRYINDEX, state->args_ref);
	}
}

void ScriptApiEnv::check_for_falling(v3s16 p)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "check_for_falling");
	luaL_checktype(L, -1, LUA_TFUNCTION);
	push_v3s16(L, p);
	PCALL_RES(lua_pcall(L, 1, 0, error_handler));
}

void ScriptApiEnv::on_liquid_transformed(
	const std::vector<std::pair<v3s16, MapNode>> &list)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_liquid_transformed
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_liquid_transformed");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_remove(L, -2);

	// Skip converting list and calling hook if there are
	// no registered callbacks.
	if(lua_objlen(L, -1) < 1) return;

	// Convert the list to a pos array and a node array for lua
	int index = 1;
	lua_createtable(L, list.size(), 0);
	lua_createtable(L, list.size(), 0);
	for(auto &p : list) {
		lua_pushnumber(L, index);
		push_v3s16(L, p.first);
		lua_rawset(L, -4);
		lua_pushnumber(L, index++);
		pushnode(L, p.second);
		lua_rawset(L, -3);
	}

	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

void ScriptApiEnv::on_mapblocks_changed(const std::unordered_set<v3s16> &set)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_mapblocks_changed
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_mapblocks_changed");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_remove(L, -2);

	// Convert the set to a set of position hashes
	lua_createtable(L, 0, set.size());
	for(const v3s16 &p : set) {
		lua_pushnumber(L, hash_node_position(p));
		lua_pushboolean(L, true);
		lua_rawset(L, -3);
	}
	lua_pushinteger(L, set.size());

	runCallbacks(2, RUN_CALLBACKS_MODE_FIRST);
}

bool ScriptApiEnv::has_on_mapblocks_changed()
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_on_mapblocks_changed
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_on_mapblocks_changed");
	luaL_checktype(L, -1, LUA_TTABLE);
	return lua_objlen(L, -1) > 0;
}

void ScriptApiEnv::triggerABM(int id, v3s16 p, MapNode n,
		u32 active_object_count, u32 active_object_count_wider)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get registered_abms
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_abms");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_remove(L, -2); // Remove core

	// Get registered_abms[m_id]
	lua_pushinteger(L, id);
	lua_gettable(L, -2);
	FATAL_ERROR_IF(lua_isnil(L, -1), "Entry with given id not found in registered_abms table");
	lua_remove(L, -2); // Remove registered_abms

	setOriginFromTable(-1);

	// Call action
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, "action");
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_remove(L, -2); // Remove registered_abms[m_id]
	push_v3s16(L, p);
	pushnode(L, n);
	lua_pushnumber(L, active_object_count);
	lua_pushnumber(L, active_object_count_wider);

	int result = lua_pcall(L, 4, 0, error_handler);
	if (result)
		scriptError(result, "LuaABM::trigger");

	lua_pop(L, 1); // Pop error handler
}

void ScriptApiEnv::triggerLBM(int id, MapBlock *block,
		const std::unordered_set<v3s16> &positions, float dtime_s)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	const v3s16 pos_of_block = block->getPosRelative();

	// Get core.run_lbm
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "run_lbm");
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_remove(L, -2); // Remove core

	// Call it
	lua_pushinteger(L, id);
	lua_createtable(L, positions.size(), 0);
	int i = 1;
	for (auto &p : positions) {
		push_v3s16(L, pos_of_block + p);
		lua_rawseti(L, -2, i++);
	}
	lua_pushnumber(L, dtime_s);

	int result = lua_pcall(L, 3, 0, error_handler);
	if (result)
		scriptError(result, "LuaLBM::trigger");

	lua_pop(L, 1); // Pop error handler
}
