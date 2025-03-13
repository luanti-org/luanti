// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "cpp_api/s_entity.h"
#include "cpp_api/s_internal.h"
#include "log.h"
#include "object_properties.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "server.h"

bool ScriptApiEntity::luaentity_Add(u16 id, const char *name)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.registered_entities[name]
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "registered_entities");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushstring(L, name);
	lua_gettable(L, -2);
	// Should be a table, which we will use as a prototype
	//luaL_checktype(L, -1, LUA_TTABLE);
	if (lua_type(L, -1) != LUA_TTABLE){
		errorstream<<"LuaEntity name \""<<name<<"\" not defined"<<std::endl;
		return false;
	}
	int prototype_table = lua_gettop(L);
	//dump2(L, "prototype_table");

	// Create entity object
	lua_newtable(L);
	int object = lua_gettop(L);

	// Set object metatable
	lua_pushvalue(L, prototype_table);
	lua_setmetatable(L, -2);

	// Add object reference
	// This should be userdata with metatable ObjectRef
	push_objectRef(L, id);
	luaL_checktype(L, -1, LUA_TUSERDATA);
	luaL_checkudata(L, -1, "ObjectRef");
	lua_setfield(L, -2, "object");

	// core.luaentities[id] = object
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "luaentities");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushnumber(L, id); // Push id
	lua_pushvalue(L, object); // Copy object to top of stack
	lua_settable(L, -3);

	return true;
}

void ScriptApiEntity::luaentity_Activate(u16 id,
		const std::string &staticdata, u32 dtime_s)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get core.luaentities[id]
	luaentity_get(L, id);
	int object = lua_gettop(L);

	// Get on_activate function
	lua_getfield(L, -1, "on_activate");
	if (!lua_isnil(L, -1)) {
		luaL_checktype(L, -1, LUA_TFUNCTION);
		lua_pushvalue(L, object); // self
		lua_pushlstring(L, staticdata.c_str(), staticdata.size());
		lua_pushinteger(L, dtime_s);

		setOriginFromTable(object);
		PCALL_RES(lua_pcall(L, 3, 0, error_handler));
	} else {
		lua_pop(L, 1);
	}
	lua_pop(L, 2); // Pop object and error handler
}

void ScriptApiEntity::luaentity_Deactivate(u16 id, bool removal)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get the entity
	luaentity_get(L, id);
	int object = lua_gettop(L);

	// Get on_deactivate
	lua_getfield(L, -1, "on_deactivate");
	if (!lua_isnil(L, -1)) {
		luaL_checktype(L, -1, LUA_TFUNCTION);
		lua_pushvalue(L, object);
		lua_pushboolean(L, removal);
		setOriginFromTable(object);
		PCALL_RES(lua_pcall(L, 2, 0, error_handler));
	} else {
		lua_pop(L, 1);
	}
	lua_pop(L, 2); // Pop object and error handler
}

void ScriptApiEntity::luaentity_Remove(u16 id)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.luaentities table
	lua_getglobal(L, "core");
	lua_getfield(L, -1, "luaentities");
	luaL_checktype(L, -1, LUA_TTABLE);
	int objectstable = lua_gettop(L);

	// Set luaentities[id] = nil
	lua_pushnumber(L, id); // Push id
	lua_pushnil(L);
	lua_settable(L, objectstable);

	lua_pop(L, 2); // pop luaentities, core
}

std::string ScriptApiEntity::luaentity_GetStaticdata(u16 id)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get core.luaentities[id]
	luaentity_get(L, id);
	int object = lua_gettop(L);

	// Get get_staticdata function
	lua_getfield(L, -1, "get_staticdata");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 2); // Pop entity and  get_staticdata
		return "";
	}
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushvalue(L, object); // self

	setOriginFromTable(object);
	PCALL_RES(lua_pcall(L, 1, 1, error_handler));

	lua_remove(L, object);
	lua_remove(L, error_handler);

	size_t len = 0;
	const char *s = lua_tolstring(L, -1, &len);
	lua_pop(L, 1); // Pop static data
	return std::string(s, len);
}

void ScriptApiEntity::logDeprecationForExistingProperties(lua_State *L, int index, const std::string &name)
{
	if (deprecation_warned_init_properties.find(name) != deprecation_warned_init_properties.end())
		return;

	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	if (!lua_istable(L, index))
		return;

	for (const char *key : object_property_keys) {
		lua_getfield(L, index, key);
		bool exists = !lua_isnil(L, -1);
		lua_pop(L, 1);

		if (exists) {
			std::ostringstream os;

			os << "Reading initial object properties directly from an entity definition is deprecated, "
				<< "move it to the 'initial_properties' table instead. "
				<< "(Property '" << key << "' in entity '" << name << "')" << std::endl;

			log_deprecated(L, os.str(), -1);

			deprecation_warned_init_properties.insert(name);
			break;
		}
	}
}

void ScriptApiEntity::luaentity_GetProperties(u16 id,
		ServerActiveObject *self, ObjectProperties *prop, const std::string &entity_name)
{
	SCRIPTAPI_PRECHECKHEADER

	// Get core.luaentities[id]
	luaentity_get(L, id);

	// Set default values that differ from ObjectProperties defaults
	prop->hp_max = 10;

	auto *idef = getServer()->idef();

	// Deprecated: read object properties directly
	// TODO: this should be changed to not read the legacy place
	// if `initial_properties` exists!
	logDeprecationForExistingProperties(L, -1, entity_name);
	read_object_properties(L, -1, self, prop, idef, true);

	// Read initial_properties
	lua_getfield(L, -1, "initial_properties");
	read_object_properties(L, -1, self, prop, idef);
	lua_pop(L, 1);
}

void ScriptApiEntity::luaentity_Step(u16 id, float dtime,
	const collisionMoveResult *moveresult)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get core.luaentities[id]
	luaentity_get(L, id);
	int object = lua_gettop(L);
	// State: object is at top of stack
	// Get step function
	lua_getfield(L, -1, "on_step");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 2); // Pop on_step and entity
		return;
	}
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushvalue(L, object); // self
	lua_pushnumber(L, dtime); // dtime
	/* moveresult */
	if (moveresult)
		push_collision_move_result(L, *moveresult);
	else
		lua_pushnil(L);

	setOriginFromTable(object);
	PCALL_RES(lua_pcall(L, 3, 0, error_handler));

	lua_pop(L, 2); // Pop object and error handler
}

// Calls entity:on_punch(ObjectRef puncher, time_from_last_punch,
//                       tool_capabilities, direction, damage)
bool ScriptApiEntity::luaentity_Punch(u16 id,
		ServerActiveObject *puncher, float time_from_last_punch,
		const ToolCapabilities *toolcap, v3f dir, s32 damage)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get core.luaentities[id]
	luaentity_get(L,id);
	int object = lua_gettop(L);
	// State: object is at top of stack
	// Get function
	lua_getfield(L, -1, "on_punch");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 2); // Pop on_punch and entity
		return false;
	}
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushvalue(L, object);  // self
	if (puncher)
		objectrefGetOrCreate(L, puncher);  // Puncher reference
	else
		lua_pushnil(L);
	lua_pushnumber(L, time_from_last_punch);
	push_tool_capabilities(L, *toolcap);
	push_v3f(L, dir);
	lua_pushnumber(L, damage);

	setOriginFromTable(object);
	PCALL_RES(lua_pcall(L, 6, 1, error_handler));

	bool retval = readParam<bool>(L, -1);
	lua_pop(L, 2); // Pop object and error handler
	return retval;
}

// Calls entity[field](ObjectRef self, ObjectRef sao)
bool ScriptApiEntity::luaentity_run_simple_callback(u16 id,
	ServerActiveObject *sao, const char *field)
{
	SCRIPTAPI_PRECHECKHEADER

	int error_handler = PUSH_ERROR_HANDLER(L);

	// Get core.luaentities[id]
	luaentity_get(L, id);
	int object = lua_gettop(L);
	// State: object is at top of stack
	// Get function
	lua_getfield(L, -1, field);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 2); // Pop callback field and entity
		return false;
	}
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushvalue(L, object);  // self
	if (sao)
		objectrefGetOrCreate(L, sao);  // sao reference
	else
		lua_pushnil(L);

	setOriginFromTable(object);
	PCALL_RES(lua_pcall(L, 2, 1, error_handler));

	bool retval = readParam<bool>(L, -1);
	lua_pop(L, 2); // Pop object and error handler
	return retval;
}

bool ScriptApiEntity::luaentity_on_death(u16 id, ServerActiveObject *killer)
{
	return luaentity_run_simple_callback(id, killer, "on_death");
}

// Calls entity:on_rightclick(ObjectRef clicker)
void ScriptApiEntity::luaentity_Rightclick(u16 id, ServerActiveObject *clicker)
{
	luaentity_run_simple_callback(id, clicker, "on_rightclick");
}

void ScriptApiEntity::luaentity_on_attach_child(u16 id, ServerActiveObject *child)
{
	luaentity_run_simple_callback(id, child, "on_attach_child");
}

void ScriptApiEntity::luaentity_on_detach_child(u16 id, ServerActiveObject *child)
{
	luaentity_run_simple_callback(id, child, "on_detach_child");
}

void ScriptApiEntity::luaentity_on_detach(u16 id, ServerActiveObject *parent)
{
	luaentity_run_simple_callback(id, parent, "on_detach");
}
