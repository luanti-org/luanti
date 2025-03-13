// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <vector>

#include "lua_api/l_base.h"

struct CraftReplacements;

class ModApiCraft : public ModApiBase {
private:
	static int l_register_craft(lua_State *L);
	static int l_get_craft_recipe(lua_State *L);
	static int l_get_all_craft_recipes(lua_State *L);
	static int l_get_craft_result(lua_State *L);
	static int l_clear_craft(lua_State *L);

	static bool readCraftReplacements(lua_State *L, int index,
			CraftReplacements &replacements);
	static bool readCraftRecipeShapeless(lua_State *L, int index,
			std::vector<std::string> &recipe);
	static bool readCraftRecipeShaped(lua_State *L, int index,
			int &width, std::vector<std::string> &recipe);

	static struct EnumString es_CraftMethod[];

public:
	static void Initialize(lua_State *L, int top);
	static void InitializeAsync(lua_State *L, int top);
};
