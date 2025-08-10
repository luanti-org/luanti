// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_nodetimer.h"
#include "lua_api/l_internal.h"
#include "serverenvironment.h"
#include "map.h"


int NodeTimerRef::gc_object(lua_State *L) {
	NodeTimerRef *o = *(NodeTimerRef **)(lua_touserdata(L, 1));
	delete o;
	return 0;
}

int NodeTimerRef::l_set(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	NodeTimerRef *o = checkObject<NodeTimerRef>(L, 1);
	f32 t = readParam<float>(L,2);
	f32 e = readParam<float>(L,3);
	o->m_map->setNodeTimer(NodeTimer(t, e, o->m_p));
	return 0;
}

int NodeTimerRef::l_start(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	NodeTimerRef *o = checkObject<NodeTimerRef>(L, 1);
	f32 t = readParam<float>(L,2);
	o->m_map->setNodeTimer(NodeTimer(t, 0, o->m_p));
	return 0;
}

int NodeTimerRef::l_stop(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	NodeTimerRef *o = checkObject<NodeTimerRef>(L, 1);
	o->m_map->removeNodeTimer(o->m_p);
	return 0;
}

int NodeTimerRef::l_is_started(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	NodeTimerRef *o = checkObject<NodeTimerRef>(L, 1);
	NodeTimer t = o->m_map->getNodeTimer(o->m_p);
	lua_pushboolean(L,(t.timeout != 0));
	return 1;
}

int NodeTimerRef::l_get_timeout(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	NodeTimerRef *o = checkObject<NodeTimerRef>(L, 1);
	NodeTimer t = o->m_map->getNodeTimer(o->m_p);
	lua_pushnumber(L,t.timeout);
	return 1;
}

int NodeTimerRef::l_get_elapsed(lua_State *L)
{
	MAP_LOCK_REQUIRED;
	NodeTimerRef *o = checkObject<NodeTimerRef>(L, 1);
	NodeTimer t = o->m_map->getNodeTimer(o->m_p);
	lua_pushnumber(L,t.elapsed);
	return 1;
}

// Creates an NodeTimerRef and leaves it on top of stack
// Not callable from Lua; all references are created on the C side.
void NodeTimerRef::create(lua_State *L, v3s16 p, ServerMap *map)
{
	NodeTimerRef *o = new NodeTimerRef(p, map);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

void NodeTimerRef::Register(lua_State *L)
{
	static const luaL_Reg metamethods[] = {
		{"__gc", gc_object},
		{0, 0}
	};
	registerClass<NodeTimerRef>(L, methods, metamethods);

	// Cannot be created from Lua
	//lua_register(L, className, create_object);
}

const char NodeTimerRef::className[] = "NodeTimerRef";
const luaL_Reg NodeTimerRef::methods[] = {
	luamethod(NodeTimerRef, start),
	luamethod(NodeTimerRef, set),
	luamethod(NodeTimerRef, stop),
	luamethod(NodeTimerRef, is_started),
	luamethod(NodeTimerRef, get_timeout),
	luamethod(NodeTimerRef, get_elapsed),
	{0,0}
};
