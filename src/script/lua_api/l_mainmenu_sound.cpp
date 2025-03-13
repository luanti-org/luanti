// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "l_mainmenu_sound.h"
#include "l_internal.h"
#include "common/c_content.h"
#include "gui/guiEngine.h"

/* ModApiMainMenuSound */

// sound_play(spec, loop)
int ModApiMainMenuSound::l_sound_play(lua_State *L)
{
	SoundSpec spec;
	read_simplesoundspec(L, 1, spec);
	spec.loop = readParam<bool>(L, 2);

	ISoundManager &sound_manager = *getGuiEngine(L)->m_sound_manager;

	sound_handle_t handle = sound_manager.allocateId(2);
	sound_manager.playSound(handle, spec);

	MainMenuSoundHandle::create(L, handle);

	return 1;
}

void ModApiMainMenuSound::Initialize(lua_State *L, int top)
{
	API_FCT(sound_play);
}

/* MainMenuSoundHandle */

MainMenuSoundHandle *MainMenuSoundHandle::checkobject(lua_State *L, int narg)
{
	luaL_checktype(L, narg, LUA_TUSERDATA);
	void *ud = luaL_checkudata(L, narg, className);
	if (!ud)
		luaL_typerror(L, narg, className);
	return *(MainMenuSoundHandle**)ud; // unbox pointer
}

int MainMenuSoundHandle::gc_object(lua_State *L)
{
	MainMenuSoundHandle *o = *(MainMenuSoundHandle **)(lua_touserdata(L, 1));
	if (getGuiEngine(L) && getGuiEngine(L)->m_sound_manager)
		getGuiEngine(L)->m_sound_manager->freeId(o->m_handle);
	delete o;
	return 0;
}

// :stop()
int MainMenuSoundHandle::l_stop(lua_State *L)
{
	MainMenuSoundHandle *o = checkobject(L, 1);
	getGuiEngine(L)->m_sound_manager->stopSound(o->m_handle);
	return 0;
}

void MainMenuSoundHandle::create(lua_State *L, sound_handle_t handle)
{
	MainMenuSoundHandle *o = new MainMenuSoundHandle(handle);
	*(void **)(lua_newuserdata(L, sizeof(void *))) = o;
	luaL_getmetatable(L, className);
	lua_setmetatable(L, -2);
}

void MainMenuSoundHandle::Register(lua_State *L)
{
	lua_newtable(L);
	int methodtable = lua_gettop(L);
	luaL_newmetatable(L, className);
	int metatable = lua_gettop(L);

	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);  // hide metatable from Lua getmetatable()

	lua_pushliteral(L, "__index");
	lua_pushvalue(L, methodtable);
	lua_settable(L, metatable);

	lua_pushliteral(L, "__gc");
	lua_pushcfunction(L, gc_object);
	lua_settable(L, metatable);

	lua_pop(L, 1);  // drop metatable

	luaL_register(L, nullptr, methods);  // fill methodtable
	lua_pop(L, 1);  // drop methodtable
}

const char MainMenuSoundHandle::className[] = "MainMenuSoundHandle";
const luaL_Reg MainMenuSoundHandle::methods[] = {
	luamethod(MainMenuSoundHandle, stop),
	{0,0}
};
