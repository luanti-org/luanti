// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>


#include "lua_api/l_craft.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_item.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "server.h"
#include "craftdef.h"

struct EnumString ModApiCraft::es_CraftMethod[] =
{
	{CRAFT_METHOD_NORMAL, "normal"},
	{CRAFT_METHOD_COOKING, "cooking"},
	{CRAFT_METHOD_FUEL, "fuel"},
	{0, NULL},
};

// helper for register_craft
bool ModApiCraft::readCraftRecipeShaped(lua_State *L, int index,
		int &width, std::vector<std::string> &recipe)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	if(!lua_istable(L, index))
		return false;

	lua_pushnil(L);
	int rowcount = 0;
	while(lua_next(L, index) != 0){
		int colcount = 0;
		// key at index -2 and value at index -1
		if(!lua_istable(L, -1))
			return false;
		int table2 = lua_gettop(L);
		lua_pushnil(L);
		while(lua_next(L, table2) != 0){
			// key at index -2 and value at index -1
			if(!lua_isstring(L, -1))
				return false;
			recipe.emplace_back(readParam<std::string>(L, -1));
			// removes value, keeps key for next iteration
			lua_pop(L, 1);
			colcount++;
		}
		if(rowcount == 0){
			width = colcount;
		} else {
			if(colcount != width)
				return false;
		}
		// removes value, keeps key for next iteration
		lua_pop(L, 1);
		rowcount++;
	}
	return width != 0;
}

// helper for register_craft
bool ModApiCraft::readCraftRecipeShapeless(lua_State *L, int index,
		std::vector<std::string> &recipe)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	if(!lua_istable(L, index))
		return false;

	lua_pushnil(L);
	while(lua_next(L, index) != 0){
		// key at index -2 and value at index -1
		if(!lua_isstring(L, -1))
			return false;
		recipe.emplace_back(readParam<std::string>(L, -1));
		// removes value, keeps key for next iteration
		lua_pop(L, 1);
	}
	return true;
}

// helper for register_craft
bool ModApiCraft::readCraftReplacements(lua_State *L, int index,
		CraftReplacements &replacements)
{
	if(index < 0)
		index = lua_gettop(L) + 1 + index;

	if(!lua_istable(L, index))
		return false;

	lua_pushnil(L);
	while(lua_next(L, index) != 0){
		// key at index -2 and value at index -1
		if(!lua_istable(L, -1))
			return false;
		lua_rawgeti(L, -1, 1);
		if(!lua_isstring(L, -1))
			return false;
		std::string replace_from = readParam<std::string>(L, -1);
		lua_pop(L, 1);
		lua_rawgeti(L, -1, 2);
		if(!lua_isstring(L, -1))
			return false;
		std::string replace_to = readParam<std::string>(L, -1);
		lua_pop(L, 1);
		replacements.pairs.emplace_back(replace_from, replace_to);
		// removes value, keeps key for next iteration
		lua_pop(L, 1);
	}
	return true;
}
// register_craft({output=item, recipe={{item00,item10},{item01,item11}})
int ModApiCraft::l_register_craft(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	//infostream<<"register_craft"<<std::endl;
	luaL_checktype(L, 1, LUA_TTABLE);
	int table = 1;

	// Get the writable craft definition manager from the server
	IWritableCraftDefManager *craftdef =
			getServer(L)->getWritableCraftDefManager();

	std::string type = getstringfield_default(L, table, "type", "shaped");

	/*
		CraftDefinitionShaped
	*/
	if(type == "shaped"){
		std::string output = getstringfield_default(L, table, "output", "");
		if (output.empty())
			throw LuaError("Crafting definition is missing an output");

		int width = 0;
		std::vector<std::string> recipe;
		lua_getfield(L, table, "recipe");
		if(lua_isnil(L, -1))
			throw LuaError("Crafting definition is missing a recipe"
					" (output=\"" + output + "\")");
		if(!readCraftRecipeShaped(L, -1, width, recipe))
			throw LuaError("Invalid crafting recipe"
					" (output=\"" + output + "\")");

		CraftReplacements replacements;
		lua_getfield(L, table, "replacements");
		if(!lua_isnil(L, -1))
		{
			if(!readCraftReplacements(L, -1, replacements))
				throw LuaError("Invalid replacements"
						" (output=\"" + output + "\")");
		}

		CraftDefinition *def = new CraftDefinitionShaped(
				output, width, recipe, replacements);
		craftdef->registerCraft(def, getServer(L));
	}
	/*
		CraftDefinitionShapeless
	*/
	else if(type == "shapeless"){
		std::string output = getstringfield_default(L, table, "output", "");
		if (output.empty())
			throw LuaError("Crafting definition (shapeless)"
					" is missing an output");

		std::vector<std::string> recipe;
		lua_getfield(L, table, "recipe");
		if(lua_isnil(L, -1))
			throw LuaError("Crafting definition (shapeless)"
					" is missing a recipe"
					" (output=\"" + output + "\")");
		if(!readCraftRecipeShapeless(L, -1, recipe))
			throw LuaError("Invalid crafting recipe"
					" (output=\"" + output + "\")");

		CraftReplacements replacements;
		lua_getfield(L, table, "replacements");
		if(!lua_isnil(L, -1))
		{
			if(!readCraftReplacements(L, -1, replacements))
				throw LuaError("Invalid replacements"
						" (output=\"" + output + "\")");
		}

		CraftDefinition *def = new CraftDefinitionShapeless(
				output, recipe, replacements);
		craftdef->registerCraft(def, getServer(L));
	}
	/*
		CraftDefinitionToolRepair
	*/
	else if(type == "toolrepair"){
		float additional_wear = getfloatfield_default(L, table,
				"additional_wear", 0.0);

		CraftDefinition *def = new CraftDefinitionToolRepair(
				additional_wear);
		craftdef->registerCraft(def, getServer(L));
	}
	/*
		CraftDefinitionCooking
	*/
	else if(type == "cooking"){
		std::string output = getstringfield_default(L, table, "output", "");
		if (output.empty())
			throw LuaError("Crafting definition (cooking)"
					" is missing an output");

		std::string recipe = getstringfield_default(L, table, "recipe", "");
		if (recipe.empty())
			throw LuaError("Crafting definition (cooking)"
					" is missing a recipe"
					" (output=\"" + output + "\")");

		float cooktime = getfloatfield_default(L, table, "cooktime", 3.0);

		CraftReplacements replacements;
		lua_getfield(L, table, "replacements");
		if(!lua_isnil(L, -1))
		{
			if(!readCraftReplacements(L, -1, replacements))
				throw LuaError("Invalid replacements"
						" (cooking output=\"" + output + "\")");
		}

		CraftDefinition *def = new CraftDefinitionCooking(
				output, recipe, cooktime, replacements);
		craftdef->registerCraft(def, getServer(L));
	}
	/*
		CraftDefinitionFuel
	*/
	else if(type == "fuel"){
		std::string recipe = getstringfield_default(L, table, "recipe", "");
		if (recipe.empty())
			throw LuaError("Crafting definition (fuel)"
					" is missing a recipe");

		float burntime = getfloatfield_default(L, table, "burntime", 1.0);

		CraftReplacements replacements;
		lua_getfield(L, table, "replacements");
		if(!lua_isnil(L, -1))
		{
			if(!readCraftReplacements(L, -1, replacements))
				throw LuaError("Invalid replacements"
						" (fuel recipe=\"" + recipe + "\")");
		}

		CraftDefinition *def = new CraftDefinitionFuel(
				recipe, burntime, replacements);
		craftdef->registerCraft(def, getServer(L));
	}
	else
	{
		throw LuaError("Unknown crafting definition type: \"" + type + "\"");
	}

	lua_pop(L, 1);
	return 0; /* number of results */
}

// clear_craft({[output=item], [recipe={{item00,item10},{item01,item11}}])
int ModApiCraft::l_clear_craft(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	luaL_checktype(L, 1, LUA_TTABLE);
	int table = 1;

	IWritableCraftDefManager *craftdef =
			getServer(L)->getWritableCraftDefManager();

	std::string output = getstringfield_default(L, table, "output", "");
	std::string type = getstringfield_default(L, table, "type", "shaped");

	if (!output.empty()) {
		CraftOutput c_output(output, 0);
		if (craftdef->clearCraftsByOutput(c_output, getServer(L))) {
			lua_pushboolean(L, true);
			return 1;
		}

		warningstream << "No craft recipe known for output '" << output
				<< "'" << std::endl;
		lua_pushboolean(L, false);
		return 1;
	}

	std::vector<std::string> recipe;
	int width = 0;
	CraftMethod method = CRAFT_METHOD_NORMAL;

	if (type == "shaped") {
		lua_getfield(L, table, "recipe");
		if (lua_isnil(L, -1))
			throw LuaError("Either output or recipe has to be defined");
		if (!readCraftRecipeShaped(L, -1, width, recipe))
			throw LuaError("Invalid crafting recipe");
	} else if (type == "shapeless") {
		lua_getfield(L, table, "recipe");
		if (lua_isnil(L, -1))
			throw LuaError("Either output or recipe has to be defined");
		if (!readCraftRecipeShapeless(L, -1, recipe))
			throw LuaError("Invalid crafting recipe");
	} else if (type == "cooking") {
		method = CRAFT_METHOD_COOKING;
		std::string rec = getstringfield_default(L, table, "recipe", "");
		if (rec.empty())
			throw LuaError("Crafting definition (cooking) is missing a recipe");
		recipe.push_back(rec);
	} else if (type == "fuel") {
		method = CRAFT_METHOD_FUEL;
		std::string rec = getstringfield_default(L, table, "recipe", "");
		if (rec.empty())
			throw LuaError("Crafting definition (fuel) is missing a recipe");
		recipe.push_back(rec);
	} else {
		throw LuaError("Unknown crafting definition type: \"" + type + "\"");
	}

	std::vector<ItemStack> items;
	items.reserve(recipe.size());
	for (const auto &item : recipe) {
		items.emplace_back(item, 1, 0, getServer(L)->idef());
	}
	CraftInput input(method, width, items);

	if (!craftdef->clearCraftsByInput(input, getServer(L))) {
		warningstream << "No craft recipe matches input (type: " << type
				<< ", items: [";
		for (size_t i = 0; i < items.size(); ++i) {
			warningstream << "'" << items[i].name << "'";
			if (i != items.size() - 1)
				warningstream << ", ";
		}
		warningstream << "])" << std::endl;
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

// get_craft_result(input)
int ModApiCraft::l_get_craft_result(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	IGameDef *gdef = getGameDef(L);

	const int input_i = 1;
	std::string method_s = getstringfield_default(L, input_i, "method", "normal");
	enum CraftMethod method = (CraftMethod)getenumfield(L, input_i, "method",
				es_CraftMethod, CRAFT_METHOD_NORMAL);
	int width = 1;
	lua_getfield(L, input_i, "width");
	if(lua_isnumber(L, -1))
		width = luaL_checkinteger(L, -1);
	lua_pop(L, 1);
	lua_getfield(L, input_i, "items");
	std::vector<ItemStack> items = read_items(L, -1, gdef);
	lua_pop(L, 1); // items

	ICraftDefManager *cdef = gdef->cdef();
	CraftInput input(method, width, items);
	CraftOutput output;
	std::vector<ItemStack> output_replacements;
	bool got = cdef->getCraftResult(input, output, output_replacements, true, gdef);
	lua_newtable(L); // output table
	if (got) {
		ItemStack item;
		item.deSerialize(output.item, gdef->idef());
		LuaItemStack::create(L, item);
		lua_setfield(L, -2, "item");
		setintfield(L, -1, "time", output.time);
		push_items(L, output_replacements);
		lua_setfield(L, -2, "replacements");
	} else {
		LuaItemStack::create(L, ItemStack());
		lua_setfield(L, -2, "item");
		setintfield(L, -1, "time", 0);
		lua_newtable(L);
		lua_setfield(L, -2, "replacements");
	}
	lua_newtable(L); // decremented input table
	lua_pushstring(L, method_s.c_str());
	lua_setfield(L, -2, "method");
	lua_pushinteger(L, width);
	lua_setfield(L, -2, "width");
	push_items(L, input.items);
	lua_setfield(L, -2, "items");
	return 2;
}


static void push_craft_recipe(lua_State *L, IGameDef *gdef,
		const CraftDefinition *recipe,
		const CraftOutput &tmpout)
{
	CraftInput input = recipe->getInput(tmpout, gdef);
	CraftOutput output = recipe->getOutput(input, gdef);

	lua_newtable(L); // items
	auto iter = input.items.begin();
	for (u16 j = 1; iter != input.items.end(); ++iter, j++) {
		if (iter->empty())
			continue;
		lua_pushstring(L, iter->name.c_str());
		lua_rawseti(L, -2, j);
	}
	lua_setfield(L, -2, "items");
	setintfield(L, -1, "width", input.width);

	std::string method_s;
	switch (input.method) {
	case CRAFT_METHOD_NORMAL:
		method_s = "normal";
		break;
	case CRAFT_METHOD_COOKING:
		method_s = "cooking";
		break;
	case CRAFT_METHOD_FUEL:
		method_s = "fuel";
		break;
	default:
		method_s = "unknown";
	}
	lua_pushstring(L, method_s.c_str());
	lua_setfield(L, -2, "method");

	// Deprecated, only for compatibility's sake
	lua_pushstring(L, method_s.c_str());
	lua_setfield(L, -2, "type");

	lua_pushstring(L, output.item.c_str());
	lua_setfield(L, -2, "output");
}

static void push_craft_recipes(lua_State *L, IGameDef *gdef,
		const std::vector<CraftDefinition*> &recipes,
		const CraftOutput &output)
{
	if (recipes.empty()) {
		lua_pushnil(L);
		return;
	}

	lua_createtable(L, recipes.size(), 0);

	auto it = recipes.begin();
	for (unsigned i = 0; it != recipes.end(); ++it) {
		lua_newtable(L);
		push_craft_recipe(L, gdef, *it, output);
		lua_rawseti(L, -2, ++i);
	}
}


// get_craft_recipe(result item)
int ModApiCraft::l_get_craft_recipe(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string item = luaL_checkstring(L, 1);
	IGameDef *gdef = getGameDef(L);
	CraftOutput output(item, 0);
	auto recipes = gdef->cdef()->getCraftRecipes(output, gdef, 1);

	lua_createtable(L, 1, 0);

	if (recipes.empty()) {
		lua_pushnil(L);
		lua_setfield(L, -2, "items");
		setintfield(L, -1, "width", 0);
		return 1;
	}
	push_craft_recipe(L, gdef, recipes[0], output);
	return 1;
}

// get_all_craft_recipes(result item)
int ModApiCraft::l_get_all_craft_recipes(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string item = luaL_checkstring(L, 1);
	IGameDef *gdef = getGameDef(L);
	CraftOutput output(item, 0);
	auto recipes = gdef->cdef()->getCraftRecipes(output, gdef);

	push_craft_recipes(L, gdef, recipes, output);
	return 1;
}

void ModApiCraft::Initialize(lua_State *L, int top)
{
	API_FCT(get_all_craft_recipes);
	API_FCT(get_craft_recipe);
	API_FCT(get_craft_result);
	API_FCT(register_craft);
	API_FCT(clear_craft);
}

void ModApiCraft::InitializeAsync(lua_State *L, int top)
{
	// all read-only functions
	API_FCT(get_all_craft_recipes);
	API_FCT(get_craft_recipe);
	API_FCT(get_craft_result);
}
