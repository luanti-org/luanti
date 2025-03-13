// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 red-001 <red-001@outlook.ie>

#pragma once

#include "lua_api/l_base.h"

class ModApiParticlesLocal : public ModApiBase
{
private:
	static int l_add_particle(lua_State *L);
	static int l_add_particlespawner(lua_State *L);
	static int l_delete_particlespawner(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
