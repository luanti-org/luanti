// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>
// Copyright (C) 2025 grorp

#pragma once

#include "lua_api/l_base.h"

class ModApiClientCommon : public ModApiBase
{
private:
	// show_formspec(name, formspec)
	static int l_show_formspec(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
};
