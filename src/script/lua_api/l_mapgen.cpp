// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_mapgen.h"
#include "lua_api/l_internal.h"
#include "lua_api/l_vmanip.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "cpp_api/s_security.h"
#include "util/serialize.h"
#include "server.h"
#include "environment.h"
#include "emerge_internal.h"
#include "mapgen/mg_biome.h"
#include "mapgen/mg_ore.h"
#include "mapgen/mg_decoration.h"
#include "mapgen/mg_schematic.h"
#include "mapgen/mapgen_v5.h"
#include "mapgen/mapgen_v7.h"
#include "mapgen/treegen.h"
#include "filesys.h"
#include "settings.h"
#include "log.h"

struct EnumString ModApiMapgen::es_BiomeTerrainType[] =
{
	{BIOMETYPE_NORMAL, "normal"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_DecorationType[] =
{
	{DECO_SIMPLE,    "simple"},
	{DECO_SCHEMATIC, "schematic"},
	{DECO_LSYSTEM,   "lsystem"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_MapgenObject[] =
{
	{MGOBJ_VMANIP,    "voxelmanip"},
	{MGOBJ_HEIGHTMAP, "heightmap"},
	{MGOBJ_BIOMEMAP,  "biomemap"},
	{MGOBJ_HEATMAP,   "heatmap"},
	{MGOBJ_HUMIDMAP,  "humiditymap"},
	{MGOBJ_GENNOTIFY, "gennotify"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_OreType[] =
{
	{ORE_SCATTER, "scatter"},
	{ORE_SHEET,   "sheet"},
	{ORE_PUFF,    "puff"},
	{ORE_BLOB,    "blob"},
	{ORE_VEIN,    "vein"},
	{ORE_STRATUM, "stratum"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_Rotation[] =
{
	{ROTATE_0,    "0"},
	{ROTATE_90,   "90"},
	{ROTATE_180,  "180"},
	{ROTATE_270,  "270"},
	{ROTATE_RAND, "random"},
	{0, NULL},
};

struct EnumString ModApiMapgen::es_SchematicFormatType[] =
{
	{SCHEM_FMT_HANDLE, "handle"},
	{SCHEM_FMT_MTS,    "mts"},
	{SCHEM_FMT_LUA,    "lua"},
	{0, NULL},
};

ObjDef *get_objdef(lua_State *L, int index, const ObjDefManager *objmgr);

Biome *get_or_load_biome(lua_State *L, int index,
	BiomeManager *biomemgr);
Biome *read_biome_def(lua_State *L, int index, const NodeDefManager *ndef);
size_t get_biome_list(lua_State *L, int index,
	BiomeManager *biomemgr, std::unordered_set<biome_t> *biome_id_list);

Schematic *get_or_load_schematic(lua_State *L, int index,
	SchematicManager *schemmgr, StringMap *replace_names);
Schematic *load_schematic(lua_State *L, int index, const NodeDefManager *ndef,
	StringMap *replace_names);
Schematic *load_schematic_from_def(lua_State *L, int index,
	const NodeDefManager *ndef, StringMap *replace_names);
bool read_schematic_def(lua_State *L, int index,
	Schematic *schem, std::vector<std::string> *names);

bool read_deco_simple(lua_State *L, DecoSimple *deco);
bool read_deco_schematic(lua_State *L, SchematicManager *schemmgr, DecoSchematic *deco);
bool read_deco_lsystem(lua_State *L, const NodeDefManager *ndef, DecoLSystem *deco);


///////////////////////////////////////////////////////////////////////////////

ObjDef *get_objdef(lua_State *L, int index, const ObjDefManager *objmgr)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	// If a number, assume this is a handle to an object def
	if (lua_isnumber(L, index))
		return objmgr->get(lua_tointeger(L, index));

	// If a string, assume a name is given instead
	if (lua_isstring(L, index))
		return objmgr->getByName(lua_tostring(L, index));

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

Schematic *get_or_load_schematic(lua_State *L, int index,
	SchematicManager *schemmgr, StringMap *replace_names)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	Schematic *schem = (Schematic *)get_objdef(L, index, schemmgr);
	if (schem)
		return schem;

	schem = load_schematic(L, index, schemmgr->getNodeDef(),
		replace_names);
	if (!schem)
		return NULL;

	if (schemmgr->add(schem) == OBJDEF_INVALID_HANDLE) {
		delete schem;
		return NULL;
	}

	return schem;
}


Schematic *load_schematic(lua_State *L, int index, const NodeDefManager *ndef,
	StringMap *replace_names)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	Schematic *schem = NULL;

	if (lua_istable(L, index)) {
		schem = load_schematic_from_def(L, index, ndef,
			replace_names);
		if (!schem) {
			delete schem;
			return NULL;
		}
	} else if (lua_isnumber(L, index)) {
		return NULL;
	} else if (lua_isstring(L, index)) {
		schem = SchematicManager::create(SCHEMATIC_NORMAL);

		std::string filepath = lua_tostring(L, index);
		if (!fs::IsPathAbsolute(filepath))
			filepath = ModApiBase::getCurrentModPath(L) + DIR_DELIM + filepath;

		if (!schem->loadSchematicFromFile(filepath, ndef,
				replace_names)) {
			delete schem;
			return NULL;
		}
	}

	return schem;
}


Schematic *load_schematic_from_def(lua_State *L, int index,
	const NodeDefManager *ndef, StringMap *replace_names)
{
	Schematic *schem = SchematicManager::create(SCHEMATIC_NORMAL);

	if (!read_schematic_def(L, index, schem, &schem->m_nodenames)) {
		delete schem;
		return NULL;
	}

	size_t num_nodes = schem->m_nodenames.size();

	schem->m_nnlistsizes.push_back(num_nodes);

	if (replace_names) {
		for (size_t i = 0; i != num_nodes; i++) {
			StringMap::iterator it = replace_names->find(schem->m_nodenames[i]);
			if (it != replace_names->end())
				schem->m_nodenames[i] = it->second;
		}
	}

	if (ndef)
		ndef->pendNodeResolve(schem);

	return schem;
}


bool read_schematic_def(lua_State *L, int index,
	Schematic *schem, std::vector<std::string> *names)
{
	if (!lua_istable(L, index))
		return false;

	//// Get schematic size
	lua_getfield(L, index, "size");
	v3s16 size = check_v3s16(L, -1);
	lua_pop(L, 1);

	schem->size = size;

	//// Get schematic data
	lua_getfield(L, index, "data");
	luaL_checktype(L, -1, LUA_TTABLE);

	u32 numnodes = size.X * size.Y * size.Z;
	schem->schemdata = new MapNode[numnodes];

	size_t names_base = names->size();
	std::unordered_map<std::string, content_t> name_id_map;

	u32 i = 0;
	for (lua_pushnil(L); lua_next(L, -2); i++, lua_pop(L, 1)) {
		if (i >= numnodes)
			continue;

		//// Read name
		std::string name;
		if (!getstringfield(L, -1, "name", name))
			throw LuaError("Schematic data definition with missing name field");

		//// Read param1/prob
		u8 param1;
		if (!getintfield(L, -1, "param1", param1) &&
			!getintfield(L, -1, "prob", param1))
			param1 = MTSCHEM_PROB_ALWAYS_OLD;

		//// Read param2
		u8 param2 = getintfield_default(L, -1, "param2", 0);

		//// Find or add new nodename-to-ID mapping
		auto it = name_id_map.find(name);
		content_t name_index;
		if (it != name_id_map.end()) {
			name_index = it->second;
		} else {
			name_index = names->size() - names_base;
			name_id_map[name] = name_index;
			names->push_back(name);
		}

		//// Perform probability/force_place fixup on param1
		param1 >>= 1;
		if (getboolfield_default(L, -1, "force_place", false))
			param1 |= MTSCHEM_FORCE_PLACE;

		//// Actually set the node in the schematic
		schem->schemdata[i] = MapNode(name_index, param1, param2);
	}

	if (i != numnodes) {
		errorstream << "read_schematic_def: incorrect number of "
			"nodes provided in raw schematic data (got " << i <<
			", expected " << numnodes << ")." << std::endl;
		return false;
	}

	//// Get Y-slice probability values (if present)
	schem->slice_probs = new u8[size.Y];
	for (i = 0; i != (u32) size.Y; i++)
		schem->slice_probs[i] = MTSCHEM_PROB_ALWAYS;

	lua_getfield(L, index, "yslice_prob");
	if (lua_istable(L, -1)) {
		for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
			u16 ypos;
			if (!getintfield(L, -1, "ypos", ypos) || (ypos >= size.Y) ||
				!getintfield(L, -1, "prob", schem->slice_probs[ypos]))
				continue;

			schem->slice_probs[ypos] >>= 1;
		}
	}

	return true;
}


void read_schematic_replacements(lua_State *L, int index, StringMap *replace_names)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	lua_pushnil(L);
	while (lua_next(L, index)) {
		std::string replace_from;
		std::string replace_to;

		if (lua_istable(L, -1)) { // Old {{"x", "y"}, ...} format
			lua_rawgeti(L, -1, 1);
			if (!lua_isstring(L, -1))
				throw LuaError("schematics: replace_from field is not a string");
			replace_from = lua_tostring(L, -1);
			lua_pop(L, 1);

			lua_rawgeti(L, -1, 2);
			if (!lua_isstring(L, -1))
				throw LuaError("schematics: replace_to field is not a string");
			replace_to = lua_tostring(L, -1);
			lua_pop(L, 1);
		} else { // New {x = "y", ...} format
			if (!lua_isstring(L, -2))
				throw LuaError("schematics: replace_from field is not a string");
			replace_from = lua_tostring(L, -2);
			if (!lua_isstring(L, -1))
				throw LuaError("schematics: replace_to field is not a string");
			replace_to = lua_tostring(L, -1);
		}

		replace_names->insert(std::make_pair(replace_from, replace_to));
		lua_pop(L, 1);
	}
}

///////////////////////////////////////////////////////////////////////////////

Biome *get_or_load_biome(lua_State *L, int index, BiomeManager *biomemgr)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	Biome *biome = (Biome *)get_objdef(L, index, biomemgr);
	if (biome)
		return biome;

	biome = read_biome_def(L, index, biomemgr->getNodeDef());
	if (!biome)
		return NULL;

	if (biomemgr->add(biome) == OBJDEF_INVALID_HANDLE) {
		delete biome;
		return NULL;
	}

	return biome;
}


Biome *read_biome_def(lua_State *L, int index, const NodeDefManager *ndef)
{
	if (!lua_istable(L, index))
		return NULL;

	BiomeType biometype = (BiomeType)getenumfield(L, index, "type",
		ModApiMapgen::es_BiomeTerrainType, BIOMETYPE_NORMAL);
	Biome *b = BiomeManager::create(biometype);

	getstringfield(L, index, "name", b->name);
	getintfield(L,    index, "depth_top",       b->depth_top);
	getintfield(L,    index, "depth_filler",    b->depth_filler);
	getintfield(L,    index, "depth_water_top", b->depth_water_top);
	getintfield(L,    index, "depth_riverbed",  b->depth_riverbed);
	getfloatfield(L,  index, "heat_point",      b->heat_point);
	getfloatfield(L,  index, "humidity_point",  b->humidity_point);
	getintfield(L,    index, "vertical_blend",  b->vertical_blend);
	getfloatfield(L,  index, "weight",          b->weight);

	b->min_pos = getv3s16field_default(L, index, "min_pos", b->min_pos);
	getintfield(L, index, "y_min", b->min_pos.Y);
	b->max_pos = getv3s16field_default(L, index, "max_pos", b->max_pos);
	getintfield(L, index, "y_max", b->max_pos.Y);

	std::vector<std::string> &nn = b->m_nodenames;
	nn.push_back(getstringfield_default(L, index, "node_top",           ""));
	nn.push_back(getstringfield_default(L, index, "node_filler",        ""));
	nn.push_back(getstringfield_default(L, index, "node_stone",         ""));
	nn.push_back(getstringfield_default(L, index, "node_water_top",     ""));
	nn.push_back(getstringfield_default(L, index, "node_water",         ""));
	nn.push_back(getstringfield_default(L, index, "node_river_water",   ""));
	nn.push_back(getstringfield_default(L, index, "node_riverbed",      ""));
	nn.push_back(getstringfield_default(L, index, "node_dust",          ""));

	size_t nnames = getstringlistfield(L, index, "node_cave_liquid", &nn);
	// If no cave liquids defined, set list to "ignore" to trigger old hardcoded
	// cave liquid behavior.
	if (nnames == 0) {
		nn.emplace_back("ignore");
		nnames = 1;
	}
	b->m_nnlistsizes.push_back(nnames);

	nn.push_back(getstringfield_default(L, index, "node_dungeon",       ""));
	nn.push_back(getstringfield_default(L, index, "node_dungeon_alt",   ""));
	nn.push_back(getstringfield_default(L, index, "node_dungeon_stair", ""));
	ndef->pendNodeResolve(b);

	return b;
}


size_t get_biome_list(lua_State *L, int index,
	BiomeManager *biomemgr, std::unordered_set<biome_t> *biome_id_list)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	if (lua_isnil(L, index))
		return 0;

	bool is_single = true;
	if (lua_istable(L, index)) {
		lua_getfield(L, index, "name");
		is_single = !lua_isnil(L, -1);
		lua_pop(L, 1);
	}

	if (is_single) {
		Biome *biome = get_or_load_biome(L, index, biomemgr);
		if (!biome) {
			warningstream << "get_biome_list: failed to get biome '"
				<< (lua_isstring(L, index) ? lua_tostring(L, index) : "")
				<< "'." << std::endl;
			return 1;
		}

		biome_id_list->insert(biome->index);
		return 0;
	}

	// returns number of failed resolutions
	size_t fail_count = 0;

	for (lua_pushnil(L); lua_next(L, index); lua_pop(L, 1)) {
		Biome *biome = get_or_load_biome(L, -1, biomemgr);
		if (!biome) {
			fail_count++;
			warningstream << "get_biome_list: failed to get biome '"
				<< (lua_isstring(L, -1) ? lua_tostring(L, -1) : "")
				<< "'" << std::endl;
			continue;
		}

		biome_id_list->insert(biome->index);
	}

	return fail_count;
}

///////////////////////////////////////////////////////////////////////////////

// get_biome_id(biomename)
// returns the biome id as used in biomemap and returned by 'get_biome_data()'
int ModApiMapgen::l_get_biome_id(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const char *biome_str = luaL_checkstring(L, 1);

	const BiomeManager *bmgr = getEmergeManager(L)->getBiomeManager();
	if (!bmgr)
		return 0;

	const Biome *biome = (Biome *)bmgr->getByName(biome_str);
	if (!biome || biome->index == OBJDEF_INVALID_INDEX)
		return 0;

	lua_pushinteger(L, biome->index);

	return 1;
}


// get_biome_name(biome_id)
// returns the biome name string
int ModApiMapgen::l_get_biome_name(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	int biome_id = luaL_checkinteger(L, 1);

	const BiomeManager *bmgr = getEmergeManager(L)->getBiomeManager();
	if (!bmgr)
		return 0;

	const Biome *b = (Biome *)bmgr->getRaw(biome_id);
	lua_pushstring(L, b->name.c_str());

	return 1;
}


// get_heat(pos)
// returns the heat at the position
int ModApiMapgen::l_get_heat(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	v3s16 pos = read_v3s16(L, 1);

	const BiomeGen *biomegen = getBiomeGen(L);
	if (!biomegen || biomegen->getType() != BIOMEGEN_ORIGINAL)
		return 0;

	float heat = ((BiomeGenOriginal*) biomegen)->calcHeatAtPoint(pos);

	lua_pushnumber(L, heat);

	return 1;
}


// get_humidity(pos)
// returns the humidity at the position
int ModApiMapgen::l_get_humidity(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	v3s16 pos = read_v3s16(L, 1);

	const BiomeGen *biomegen = getBiomeGen(L);
	if (!biomegen || biomegen->getType() != BIOMEGEN_ORIGINAL)
		return 0;

	float humidity = ((BiomeGenOriginal*) biomegen)->calcHumidityAtPoint(pos);

	lua_pushnumber(L, humidity);

	return 1;
}


// get_biome_data(pos)
// returns a table containing the biome id, heat and humidity at the position
int ModApiMapgen::l_get_biome_data(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	v3s16 pos = read_v3s16(L, 1);

	const BiomeGen *biomegen = getBiomeGen(L);
	if (!biomegen)
		return 0;

	if (biomegen->getType() == BIOMEGEN_ORIGINAL) {
		float heat = ((BiomeGenOriginal*) biomegen)->calcHeatAtPoint(pos);
		float humidity = ((BiomeGenOriginal*) biomegen)->calcHumidityAtPoint(pos);
		const Biome *biome = ((BiomeGenOriginal*) biomegen)->calcBiomeFromNoise(heat, humidity, pos);
		if (!biome || biome->index == OBJDEF_INVALID_INDEX)
			return 0;

		lua_newtable(L);

		lua_pushinteger(L, biome->index);
		lua_setfield(L, -2, "biome");

		lua_pushnumber(L, heat);
		lua_setfield(L, -2, "heat");

		lua_pushnumber(L, humidity);
		lua_setfield(L, -2, "humidity");

	} else {
		const Biome *biome = biomegen->calcBiomeAtPoint(pos);
		if (!biome || biome->index == OBJDEF_INVALID_INDEX)
			return 0;

		lua_newtable(L);

		lua_pushinteger(L, biome->index);
		lua_setfield(L, -2, "biome");
	}

	return 1;
}


// get_mapgen_object(objectname)
// returns the requested object used during map generation
int ModApiMapgen::l_get_mapgen_object(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const char *mgobjstr = lua_tostring(L, 1);

	int mgobjint;
	if (!string_to_enum(es_MapgenObject, mgobjint, mgobjstr ? mgobjstr : ""))
		return 0;

	enum MapgenObject mgobj = (MapgenObject)mgobjint;

	Mapgen *mg = getMapgen(L);
	if (!mg)
		throw LuaError("Must only be called in a mapgen thread!");

	size_t maplen = mg->csize.X * mg->csize.Z;

	switch (mgobj) {
	case MGOBJ_VMANIP: {
		MMVManip *vm = mg->vm;

		// VoxelManip object
		LuaVoxelManip::create(L, vm, true);

		// emerged min pos
		push_v3s16(L, vm->m_area.MinEdge);

		// emerged max pos
		push_v3s16(L, vm->m_area.MaxEdge);

		return 3;
	}
	case MGOBJ_HEIGHTMAP: {
		if (!mg->heightmap)
			return 0;

		lua_createtable(L, maplen, 0);
		for (size_t i = 0; i != maplen; i++) {
			lua_pushinteger(L, mg->heightmap[i]);
			lua_rawseti(L, -2, i + 1);
		}

		return 1;
	}
	case MGOBJ_BIOMEMAP: {
		if (!mg->biomegen)
			return 0;

		lua_createtable(L, maplen, 0);
		for (size_t i = 0; i != maplen; i++) {
			lua_pushinteger(L, mg->biomegen->biomemap[i]);
			lua_rawseti(L, -2, i + 1);
		}

		return 1;
	}
	case MGOBJ_HEATMAP: {
		if (!mg->biomegen || mg->biomegen->getType() != BIOMEGEN_ORIGINAL)
			return 0;

		BiomeGenOriginal *bg = (BiomeGenOriginal *)mg->biomegen;

		lua_createtable(L, maplen, 0);
		for (size_t i = 0; i != maplen; i++) {
			lua_pushnumber(L, bg->heatmap[i]);
			lua_rawseti(L, -2, i + 1);
		}

		return 1;
	}

	case MGOBJ_HUMIDMAP: {
		if (!mg->biomegen || mg->biomegen->getType() != BIOMEGEN_ORIGINAL)
			return 0;

		BiomeGenOriginal *bg = (BiomeGenOriginal *)mg->biomegen;

		lua_createtable(L, maplen, 0);
		for (size_t i = 0; i != maplen; i++) {
			lua_pushnumber(L, bg->humidmap[i]);
			lua_rawseti(L, -2, i + 1);
		}

		return 1;
	}
	case MGOBJ_GENNOTIFY: {
		std::map<std::string, std::vector<v3s16>> event_map;
		mg->gennotify.getEvents(event_map);

		lua_createtable(L, 0, event_map.size());
		for (auto it = event_map.begin(); it != event_map.end(); ++it) {
			lua_createtable(L, it->second.size(), 0);

			for (size_t j = 0; j != it->second.size(); j++) {
				push_v3s16(L, it->second[j]);
				lua_rawseti(L, -2, j + 1);
			}

			lua_setfield(L, -2, it->first.c_str());
		}

		// push user-defined data
		auto &custom_map = mg->gennotify.getCustomData();

		lua_createtable(L, 0, custom_map.size());
		lua_getglobal(L, "core");
		lua_getfield(L, -1, "deserialize");
		lua_remove(L, -2); // remove 'core'
		for (const auto &it : custom_map) {
			lua_pushvalue(L, -1); // deserialize func
			lua_pushlstring(L, it.second.c_str(), it.second.size());
			lua_pushboolean(L, true);
			lua_call(L, 2, 1);

			lua_setfield(L, -3, it.first.c_str()); // put into table
		}
		lua_pop(L, 1); // remove func
		lua_setfield(L, -2, "custom"); // put into top-level table

		return 1;
	}
	}

	return 0;
}


// get_spawn_level(x = num, z = num)
int ModApiMapgen::l_get_spawn_level(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	s16 x = luaL_checkinteger(L, 1);
	s16 z = luaL_checkinteger(L, 2);

	EmergeManager *emerge = getServer(L)->getEmergeManager();
	int spawn_level = emerge->getSpawnLevelAtPoint(v2s16(x, z));
	// Unsuitable spawn point
	if (spawn_level == MAX_MAP_GENERATION_LIMIT)
		return 0;

	// 'findSpawnPos()' in server.cpp adds at least 1
	lua_pushinteger(L, spawn_level + 1);

	return 1;
}


// get_seed([add])
int ModApiMapgen::l_get_seed(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	// This exists to
	// 1. not duplicate the truncation logic from Mapgen::Mapgen() once more
	// 2. because I don't trust myself to do it correctly in Lua

	auto *emerge = getEmergeManager(L);
	if (!emerge || !emerge->mgparams)
		return 0;

	int add = 0;
	if (lua_isnumber(L, 1))
		add = luaL_checkint(L, 1);

	s32 seed = (s32)emerge->mgparams->seed;
	seed += add;

	lua_pushinteger(L, seed);
	return 1;
}


int ModApiMapgen::l_get_mapgen_params(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	log_deprecated(L, "get_mapgen_params is deprecated; "
		"use get_mapgen_setting instead");

	std::string value;

	const MapSettingsManager *settingsmgr =
		getEmergeManager(L)->map_settings_mgr;

	lua_newtable(L);

	settingsmgr->getMapSetting("mg_name", &value);
	lua_pushstring(L, value.c_str());
	lua_setfield(L, -2, "mgname");

	settingsmgr->getMapSetting("seed", &value);
	u64 seed = from_string<u64>(value);
	lua_pushinteger(L, seed);
	lua_setfield(L, -2, "seed");

	settingsmgr->getMapSetting("water_level", &value);
	lua_pushinteger(L, stoi(value, -32768, 32767));
	lua_setfield(L, -2, "water_level");

	settingsmgr->getMapSetting("chunksize", &value);
	lua_pushinteger(L, stoi(value, -32768, 32767));
	lua_setfield(L, -2, "chunksize");

	settingsmgr->getMapSetting("mg_flags", &value);
	lua_pushstring(L, value.c_str());
	lua_setfield(L, -2, "flags");

	return 1;
}


// set_mapgen_params(params)
// set mapgen parameters
int ModApiMapgen::l_set_mapgen_params(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	log_deprecated(L, "set_mapgen_params is deprecated; "
		"use set_mapgen_setting instead");

	if (!lua_istable(L, 1))
		return 0;

	MapSettingsManager *settingsmgr =
		getServer(L)->getEmergeManager()->map_settings_mgr;

	lua_getfield(L, 1, "mgname");
	if (lua_isstring(L, -1))
		settingsmgr->setMapSetting("mg_name", readParam<std::string>(L, -1), true);

	lua_getfield(L, 1, "seed");
	if (lua_isnumber(L, -1))
		settingsmgr->setMapSetting("seed", readParam<std::string>(L, -1), true);

	lua_getfield(L, 1, "water_level");
	if (lua_isnumber(L, -1))
		settingsmgr->setMapSetting("water_level", readParam<std::string>(L, -1), true);

	lua_getfield(L, 1, "chunksize");
	if (lua_isnumber(L, -1))
		settingsmgr->setMapSetting("chunksize", readParam<std::string>(L, -1), true);

	lua_getfield(L, 1, "flags");
	if (lua_isstring(L, -1))
		settingsmgr->setMapSetting("mg_flags", readParam<std::string>(L, -1), true);

	return 0;
}

// get_mapgen_edges([mapgen_limit[, chunksize]])
int ModApiMapgen::l_get_mapgen_edges(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const MapSettingsManager *settingsmgr =
		getEmergeManager(L)->map_settings_mgr;

	// MapSettingsManager::makeMapgenParams cannot be used here because it would
	// make mapgen settings immutable from then on. Mapgen settings should stay
	// mutable until after mod loading ends.

	s16 mapgen_limit;
	if (lua_isnumber(L, 1)) {
		 mapgen_limit = lua_tointeger(L, 1);
	} else {
		std::string mapgen_limit_str;
		settingsmgr->getMapSetting("mapgen_limit", &mapgen_limit_str);
		mapgen_limit = stoi(mapgen_limit_str, 0, MAX_MAP_GENERATION_LIMIT);
	}

	s16 chunksize;
	if (lua_isnumber(L, 2)) {
		chunksize = lua_tointeger(L, 2);
	} else {
		std::string chunksize_str;
		settingsmgr->getMapSetting("chunksize", &chunksize_str);
		chunksize = stoi(chunksize_str, 1, 10);
	}

	std::pair<s16, s16> edges = get_mapgen_edges(mapgen_limit, chunksize);
	push_v3s16(L, v3s16(1, 1, 1) * edges.first);
	push_v3s16(L, v3s16(1, 1, 1) * edges.second);
	return 2;
}

// get_mapgen_chunksize()
int ModApiMapgen::l_get_mapgen_chunksize(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const MapSettingsManager *settingsmgr = getEmergeManager(L)->map_settings_mgr;

	// MapSettingsManager::makeMapgenParams cannot be used here because it would
	// make mapgen settings immutable from then on. Mapgen settings should stay
	// mutable until after mod loading ends.

	std::string chunksize_str;
	settingsmgr->getMapSetting("chunksize", &chunksize_str);
	s16 chunksize = stoi(chunksize_str, 1, 10);

	push_v3s16(L, {chunksize, chunksize, chunksize});
	return 1;
}

// get_mapgen_setting(name)
int ModApiMapgen::l_get_mapgen_setting(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string value;
	const MapSettingsManager *settingsmgr =
		getEmergeManager(L)->map_settings_mgr;

	const char *name = luaL_checkstring(L, 1);
	if (!settingsmgr->getMapSetting(name, &value))
		return 0;

	lua_pushstring(L, value.c_str());
	return 1;
}

// get_mapgen_setting_noiseparams(name)
int ModApiMapgen::l_get_mapgen_setting_noiseparams(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	NoiseParams np;
	const MapSettingsManager *settingsmgr =
		getEmergeManager(L)->map_settings_mgr;

	const char *name = luaL_checkstring(L, 1);
	if (!settingsmgr->getNoiseParams(name, &np))
		return 0;

	push_noiseparams(L, &np);
	return 1;
}

// set_mapgen_setting(name, value, override_meta)
// set mapgen config values
int ModApiMapgen::l_set_mapgen_setting(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	MapSettingsManager *settingsmgr =
		getServer(L)->getEmergeManager()->map_settings_mgr;

	const char *name   = luaL_checkstring(L, 1);
	const char *value  = luaL_checkstring(L, 2);
	bool override_meta = readParam<bool>(L, 3, false);

	if (!settingsmgr->setMapSetting(name, value, override_meta)) {
		errorstream << "set_mapgen_setting: cannot set '"
			<< name << "' after initialization" << std::endl;
	}

	return 0;
}


// set_mapgen_setting_noiseparams(name, noiseparams, set_default)
// set mapgen config values for noise parameters
int ModApiMapgen::l_set_mapgen_setting_noiseparams(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	MapSettingsManager *settingsmgr =
		getServer(L)->getEmergeManager()->map_settings_mgr;

	const char *name = luaL_checkstring(L, 1);

	NoiseParams np;
	if (!read_noiseparams(L, 2, &np)) {
		errorstream << "set_mapgen_setting_noiseparams: cannot set '" << name
			<< "'; invalid noiseparams table" << std::endl;
		return 0;
	}

	bool override_meta = readParam<bool>(L, 3, false);

	if (!settingsmgr->setMapSettingNoiseParams(name, &np, override_meta)) {
		errorstream << "set_mapgen_setting_noiseparams: cannot set '"
			<< name << "' after initialization" << std::endl;
	}

	return 0;
}


// set_noiseparams(name, noiseparams, set_default)
// set global config values for noise parameters
int ModApiMapgen::l_set_noiseparams(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const char *name = luaL_checkstring(L, 1);

	NoiseParams np;
	if (!read_noiseparams(L, 2, &np)) {
		errorstream << "set_noiseparams: cannot set '" << name
			<< "'; invalid noiseparams table" << std::endl;
		return 0;
	}

	bool set_default = !lua_isboolean(L, 3) || readParam<bool>(L, 3);

	Settings::getLayer(set_default ? SL_DEFAULTS : SL_GLOBAL)->setNoiseParams(name, np);

	return 0;
}


// get_noiseparams(name)
int ModApiMapgen::l_get_noiseparams(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string name = luaL_checkstring(L, 1);

	NoiseParams np;
	if (!g_settings->getNoiseParams(name, np))
		return 0;

	push_noiseparams(L, &np);
	return 1;
}


// set_gen_notify(flags, {deco_ids}, {custom_ids})
int ModApiMapgen::l_set_gen_notify(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	u32 flags = 0, flagmask = 0;
	EmergeManager *emerge = getServer(L)->getEmergeManager();

	if (read_flags(L, 1, flagdesc_gennotify, &flags, &flagmask)) {
		emerge->gen_notify_on &= ~flagmask;
		emerge->gen_notify_on |= flags;
	}

	if (lua_istable(L, 2)) {
		lua_pushnil(L);
		while (lua_next(L, 2)) {
			if (lua_isnumber(L, -1))
				emerge->gen_notify_on_deco_ids.insert((u32)lua_tonumber(L, -1));
			lua_pop(L, 1);
		}
	}

	if (lua_istable(L, 3)) {
		lua_pushnil(L);
		while (lua_next(L, 3)) {
			emerge->gen_notify_on_custom.insert(readParam<std::string>(L, -1));
			lua_pop(L, 1);
		}
	}

	// Clear sets if relevant flag disabled
	if ((emerge->gen_notify_on & (1 << GENNOTIFY_DECORATION)) == 0)
		emerge->gen_notify_on_deco_ids.clear();
	if ((emerge->gen_notify_on & (1 << GENNOTIFY_CUSTOM)) == 0)
		emerge->gen_notify_on_custom.clear();

	return 0;
}


// get_gen_notify()
// returns flagstring, {deco_ids}, {custom_ids})
int ModApiMapgen::l_get_gen_notify(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto *emerge = getEmergeManager(L);

	push_flags_string(L, flagdesc_gennotify, emerge->gen_notify_on,
		emerge->gen_notify_on);

	lua_createtable(L, emerge->gen_notify_on_deco_ids.size(), 0);
	int i = 1;
	for (u32 id : emerge->gen_notify_on_deco_ids) {
		lua_pushinteger(L, id);
		lua_rawseti(L, -2, i++);
	}

	lua_createtable(L, emerge->gen_notify_on_custom.size(), 0);
	int j = 1;
	for (const auto &id : emerge->gen_notify_on_custom) {
		lua_pushstring(L, id.c_str());
		lua_rawseti(L, -2, j++);
	}

	return 3;
}


// save_gen_notify(custom_id, data) [in emerge thread]
int ModApiMapgen::l_save_gen_notify(lua_State *L)
{
	auto *emerge = getEmergeThread(L);

	std::string key = readParam<std::string>(L, 1);

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "serialize");
	lua_remove(L, -2); // remove 'core'
	lua_pushvalue(L, 2);
	lua_call(L, 1, 1);
	std::string val = readParam<std::string>(L, -1);
	lua_pop(L, 1);

	bool set = emerge->getMapgen()->gennotify.setCustom(key, val);

	lua_pushboolean(L, set);
	return 1;
}


// get_decoration_id(decoration_name)
// returns the decoration ID as used in gennotify
int ModApiMapgen::l_get_decoration_id(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const char *deco_str = luaL_checkstring(L, 1);
	if (!deco_str)
		return 0;

	const DecorationManager *dmgr =
		getEmergeManager(L)->getDecorationManager();
	if (!dmgr)
		return 0;

	Decoration *deco = (Decoration *)dmgr->getByName(deco_str);

	if (!deco)
		return 0;

	lua_pushinteger(L, deco->index);

	return 1;
}


// register_biome({lots of stuff})
int ModApiMapgen::l_register_biome(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	int index = 1;
	luaL_checktype(L, index, LUA_TTABLE);

	const NodeDefManager *ndef = getServer(L)->getNodeDefManager();
	BiomeManager *bmgr = getServer(L)->getEmergeManager()->getWritableBiomeManager();

	Biome *biome = read_biome_def(L, index, ndef);
	if (!biome)
		return 0;

	ObjDefHandle handle = bmgr->add(biome);
	if (handle == OBJDEF_INVALID_HANDLE) {
		delete biome;
		return 0;
	}

	lua_pushinteger(L, handle);
	return 1;
}


// register_decoration({lots of stuff})
int ModApiMapgen::l_register_decoration(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	int index = 1;
	luaL_checktype(L, index, LUA_TTABLE);

	const NodeDefManager *ndef      = getServer(L)->getNodeDefManager();
	EmergeManager *emerge = getServer(L)->getEmergeManager();
	DecorationManager *decomgr = emerge->getWritableDecorationManager();
	BiomeManager *biomemgr     = emerge->getWritableBiomeManager();
	SchematicManager *schemmgr = emerge->getWritableSchematicManager();

	enum DecorationType decotype = (DecorationType)getenumfield(L, index,
				"deco_type", es_DecorationType, -1);

	Decoration *deco = decomgr->create(decotype);
	if (!deco) {
		errorstream << "register_decoration: decoration placement type "
			<< decotype << " not implemented" << std::endl;
		return 0;
	}

	deco->name           = getstringfield_default(L, index, "name", "");
	deco->fill_ratio     = getfloatfield_default(L, index, "fill_ratio", 0.02);
	deco->y_min          = getintfield_default(L, index, "y_min", -31000);
	deco->y_max          = getintfield_default(L, index, "y_max", 31000);
	deco->nspawnby       = getintfield_default(L, index, "num_spawn_by", -1);
	deco->place_offset_y = getintfield_default(L, index, "place_offset_y", 0);
	deco->check_offset   = getintfield_default(L, index, "check_offset", -1);
	deco->sidelen        = getintfield_default(L, index, "sidelen", 8);
	if (deco->sidelen <= 0) {
		errorstream << "register_decoration: sidelen must be "
			"greater than 0" << std::endl;
		delete deco;
		return 0;
	}

	//// Get node name(s) to place decoration on
	size_t nread = getstringlistfield(L, index, "place_on", &deco->m_nodenames);
	deco->m_nnlistsizes.push_back(nread);

	//// Get decoration flags
	getflagsfield(L, index, "flags", flagdesc_deco, &deco->flags, NULL);

	//// Get NoiseParams to define how decoration is placed
	lua_getfield(L, index, "noise_params");
	if (read_noiseparams(L, -1, &deco->np))
		deco->flags |= DECO_USE_NOISE;
	lua_pop(L, 1);

	//// Get biomes associated with this decoration (if any)
	lua_getfield(L, index, "biomes");
	if (get_biome_list(L, -1, biomemgr, &deco->biomes))
		infostream << "register_decoration: couldn't get all biomes " << std::endl;
	lua_pop(L, 1);

	//// Get node name(s) to 'spawn by'
	size_t nnames = getstringlistfield(L, index, "spawn_by", &deco->m_nodenames);
	deco->m_nnlistsizes.push_back(nnames);
	if (nnames == 0 && deco->nspawnby != -1) {
		errorstream << "register_decoration: no spawn_by nodes defined,"
			" but num_spawn_by specified" << std::endl;
	}
	if (deco->check_offset < -1 || deco->check_offset > 1) {
		delete deco;
		luaL_error(L, "register_decoration: check_offset out of range!  Allowed values: [-1, 0, 1]");
	}

	//// Handle decoration type-specific parameters
	bool success = false;
	switch (decotype) {
	case DECO_SIMPLE:
		success = read_deco_simple(L, (DecoSimple *)deco);
		break;
	case DECO_SCHEMATIC:
		success = read_deco_schematic(L, schemmgr, (DecoSchematic *)deco);
		break;
	case DECO_LSYSTEM:
		success = read_deco_lsystem(L, ndef, (DecoLSystem *)deco);
		break;
	}

	if (!success) {
		delete deco;
		return 0;
	}

	ndef->pendNodeResolve(deco);

	ObjDefHandle handle = decomgr->add(deco);
	if (handle == OBJDEF_INVALID_HANDLE) {
		delete deco;
		return 0;
	}

	lua_pushinteger(L, handle);
	return 1;
}


bool read_deco_simple(lua_State *L, DecoSimple *deco)
{
	int index = 1;
	int param2;
	int param2_max;

	deco->deco_height     = getintfield_default(L, index, "height", 1);
	deco->deco_height_max = getintfield_default(L, index, "height_max", 0);

	if (deco->deco_height <= 0) {
		errorstream << "register_decoration: simple decoration height"
			" must be greater than 0" << std::endl;
		return false;
	}

	size_t nnames = getstringlistfield(L, index, "decoration", &deco->m_nodenames);
	deco->m_nnlistsizes.push_back(nnames);

	if (nnames == 0) {
		errorstream << "register_decoration: no decoration nodes "
			"defined" << std::endl;
		return false;
	}

	param2 = getintfield_default(L, index, "param2", 0);
	param2_max = getintfield_default(L, index, "param2_max", 0);

	if (param2 < 0 || param2 > 255 || param2_max < 0 || param2_max > 255) {
		errorstream << "register_decoration: param2 or param2_max out of bounds (0-255)"
			<< std::endl;
		return false;
	}

	deco->deco_param2 = (u8)param2;
	deco->deco_param2_max = (u8)param2_max;

	return true;
}


bool read_deco_schematic(lua_State *L, SchematicManager *schemmgr, DecoSchematic *deco)
{
	int index = 1;

	deco->rotation = (Rotation)getenumfield(L, index, "rotation",
		ModApiMapgen::es_Rotation, ROTATE_0);

	StringMap replace_names;
	lua_getfield(L, index, "replacements");
	if (lua_istable(L, -1))
		read_schematic_replacements(L, -1, &replace_names);
	lua_pop(L, 1);

	lua_getfield(L, index, "schematic");
	Schematic *schem = get_or_load_schematic(L, -1, schemmgr, &replace_names);
	lua_pop(L, 1);

	deco->schematic = schem;
	return schem != NULL;
}

bool read_deco_lsystem(lua_State *L, const NodeDefManager *ndef, DecoLSystem *deco)
{
	deco->tree_def = std::make_shared<treegen::TreeDef>();

	lua_getfield(L, 1, "treedef");
	bool has_def = read_tree_def(L, -1, ndef, *(deco->tree_def));
	lua_pop(L, 1);

	return has_def;
}


// register_ore({lots of stuff})
int ModApiMapgen::l_register_ore(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	int index = 1;
	luaL_checktype(L, index, LUA_TTABLE);

	const NodeDefManager *ndef = getServer(L)->getNodeDefManager();
	EmergeManager *emerge = getServer(L)->getEmergeManager();
	BiomeManager *bmgr    = emerge->getWritableBiomeManager();
	OreManager *oremgr    = emerge->getWritableOreManager();

	int oretype_int;
	std::string oretype_string = getstringfield_default(L, index, "ore_type", "nil");
	if (!string_to_enum(es_OreType, oretype_int, oretype_string)) {
		throw LuaError("register_ore: unknown oretype \"" + oretype_string + "\"");
	}
	enum OreType oretype = (OreType) oretype_int;

	std::unique_ptr<Ore> ore(oremgr->create(oretype));
	ore->name           = getstringfield_default(L, index, "name", "");
	ore->ore_param2     = (u8)getintfield_default(L, index, "ore_param2", 0);
	ore->clust_scarcity = getintfield_default(L, index, "clust_scarcity", 1);
	ore->clust_num_ores = getintfield_default(L, index, "clust_num_ores", 1);
	ore->clust_size     = getintfield_default(L, index, "clust_size", 0);
	ore->noise          = nullptr;
	ore->flags          = 0;

	//// Get noise_threshold
	{
		float nthresh;
		if (getfloatfield(L, index, "noise_threshold", nthresh)) {
		} else if (getfloatfield(L, index, "noise_threshhold", nthresh)) {
			log_deprecated(L, "Field \"noise_threshhold\" on ore " + ore->name +
					" is deprecated, use \"noise_threshold\" instead.", 2);
		} else {
			nthresh = 0;
		}
		ore->nthresh = nthresh;
	}

	//// Get y_min/y_max

	{
		int ymin;
		if (getintfield(L, index, "y_min", ymin)) {
		} else if (getintfield(L, index, "height_min", ymin)) {
			log_deprecated(L, "Field \"height_min\" on ore " + ore->name +
					" is deprecated, use \"y_min\" instead.", 2);
		} else {
			ymin = -31000;
		}
		ore->y_min = ymin;
	}

	{
		int ymax;
		if (getintfield(L, index, "y_max", ymax)) {
		} else if (getintfield(L, index, "height_max", ymax)) {
			log_deprecated(L, "Field \"height_max\" on ore " + ore->name +
					" is deprecated, use \"y_max\" instead.", 2);
		} else {
			ymax = 31000;
		}
		ore->y_max = ymax;
	}

	if (ore->clust_scarcity <= 0 || ore->clust_num_ores <= 0) {
		errorstream << "register_ore: clust_scarcity and clust_num_ores"
			"must be greater than 0" << std::endl;
		return 0;
	}

	//// Get flags
	getflagsfield(L, index, "flags", flagdesc_ore, &ore->flags, nullptr);

	//// Get biomes associated with this decoration (if any)
	lua_getfield(L, index, "biomes");
	if (get_biome_list(L, -1, bmgr, &ore->biomes))
		infostream << "register_ore: couldn't get all biomes " << std::endl;
	lua_pop(L, 1);

	//// Get noise parameters if needed
	lua_getfield(L, index, "noise_params");
	if (read_noiseparams(L, -1, &ore->np)) {
		ore->flags |= OREFLAG_USE_NOISE;
	} else if (ore->needs_noise) {
		log_deprecated(L, "register_ore: ore type requires 'noise_params'"
				" but it is not specified, falling back to defaults", 2);
	}
	lua_pop(L, 1);

	//// Get type-specific parameters
	switch (oretype) {
		case ORE_SHEET: {
			OreSheet *oresheet = (OreSheet *)ore.get();

			oresheet->column_height_min = getintfield_default(L, index,
				"column_height_min", 1);
			oresheet->column_height_max = getintfield_default(L, index,
				"column_height_max", ore->clust_size);
			oresheet->column_midpoint_factor = getfloatfield_default(L, index,
				"column_midpoint_factor", 0.5f);

			break;
		}
		case ORE_PUFF: {
			OrePuff *orepuff = (OrePuff *)ore.get();

			lua_getfield(L, index, "np_puff_top");
			read_noiseparams(L, -1, &orepuff->np_puff_top);
			lua_pop(L, 1);

			lua_getfield(L, index, "np_puff_bottom");
			read_noiseparams(L, -1, &orepuff->np_puff_bottom);
			lua_pop(L, 1);

			break;
		}
		case ORE_VEIN: {
			OreVein *orevein = (OreVein *)ore.get();

			orevein->random_factor = getfloatfield_default(L, index,
				"random_factor", 1.f);

			break;
		}
		case ORE_STRATUM: {
			OreStratum *orestratum = (OreStratum *)ore.get();

			lua_getfield(L, index, "np_stratum_thickness");
			if (read_noiseparams(L, -1, &orestratum->np_stratum_thickness))
				ore->flags |= OREFLAG_USE_NOISE2;
			lua_pop(L, 1);

			orestratum->stratum_thickness = getintfield_default(L, index,
				"stratum_thickness", 8);

			break;
		}
		default:
			break;
	}

	ObjDefHandle handle = oremgr->add(ore.get());
	if (handle == OBJDEF_INVALID_HANDLE) {
		return 0;
	}

	ore->m_nodenames.push_back(getstringfield_default(L, index, "ore", ""));

	size_t nnames = getstringlistfield(L, index, "wherein", &ore->m_nodenames);
	ore->m_nnlistsizes.push_back(nnames);

	ndef->pendNodeResolve(ore.get());

	// We passed ownership of the ore object to oremgr earlier.
	ore.release();

	lua_pushinteger(L, handle);
	return 1;
}


// register_schematic({schematic}, replacements={})
int ModApiMapgen::l_register_schematic(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	SchematicManager *schemmgr =
		getServer(L)->getEmergeManager()->getWritableSchematicManager();

	StringMap replace_names;
	if (lua_istable(L, 2))
		read_schematic_replacements(L, 2, &replace_names);

	Schematic *schem = load_schematic(L, 1, schemmgr->getNodeDef(),
		&replace_names);
	if (!schem)
		return 0;

	ObjDefHandle handle = schemmgr->add(schem);
	if (handle == OBJDEF_INVALID_HANDLE) {
		delete schem;
		return 0;
	}

	lua_pushinteger(L, handle);
	return 1;
}


// clear_registered_biomes()
int ModApiMapgen::l_clear_registered_biomes(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	BiomeManager *bmgr =
		getServer(L)->getEmergeManager()->getWritableBiomeManager();
	bmgr->clear();
	return 0;
}


// clear_registered_decorations()
int ModApiMapgen::l_clear_registered_decorations(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	DecorationManager *dmgr =
		getServer(L)->getEmergeManager()->getWritableDecorationManager();
	dmgr->clear();
	return 0;
}


// clear_registered_ores()
int ModApiMapgen::l_clear_registered_ores(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	OreManager *omgr =
		getServer(L)->getEmergeManager()->getWritableOreManager();
	omgr->clear();
	return 0;
}


// clear_registered_schematics()
int ModApiMapgen::l_clear_registered_schematics(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	SchematicManager *smgr =
		getServer(L)->getEmergeManager()->getWritableSchematicManager();
	smgr->clear();
	return 0;
}


// generate_ores(vm, p1, p2)
int ModApiMapgen::l_generate_ores(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto *emerge = getEmergeManager(L);
	if (!emerge || !emerge->mgparams)
		return 0;

	OreManager *oremgr;
	if (auto mg = getMapgen(L))
		oremgr = mg->m_emerge->oremgr;
	else
		oremgr = emerge->oremgr;

	Mapgen mg;
	// Intentionally truncates to s32, see Mapgen::Mapgen()
	mg.seed = (s32)emerge->mgparams->seed;
	mg.vm   = checkObject<LuaVoxelManip>(L, 1)->vm;
	mg.ndef = emerge->ndef;

	v3s16 pmin = lua_istable(L, 2) ? check_v3s16(L, 2) :
			mg.vm->m_area.MinEdge + v3s16(1,1,1) * MAP_BLOCKSIZE;
	v3s16 pmax = lua_istable(L, 3) ? check_v3s16(L, 3) :
			mg.vm->m_area.MaxEdge - v3s16(1,1,1) * MAP_BLOCKSIZE;
	sortBoxVerticies(pmin, pmax);

	u32 blockseed = Mapgen::getBlockSeed(pmin, mg.seed);

	oremgr->placeAllOres(&mg, blockseed, pmin, pmax);

	return 0;
}


// generate_decorations(vm, p1, p2, use_mapgen_biomes)
int ModApiMapgen::l_generate_decorations(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	auto *emerge = getEmergeManager(L);
	if (!emerge || !emerge->mgparams)
		return 0;

	DecorationManager *decomgr;
	Mapgen mg, *mgp = nullptr;
	bool use_mapgen_biomes = readParam<bool>(L, 4, false);
	MMVManip *oldvm = nullptr, *vm = checkObject<LuaVoxelManip>(L, 1)->vm;
	if (auto mg = getMapgen(L)) {
		decomgr = mg->m_emerge->decomgr;
		if (use_mapgen_biomes) {
			mgp = mg;
			oldvm = mgp->vm;
			if (!oldvm) {
				goto no_vm;
			}
		}
	} else {
		if (use_mapgen_biomes) {
			no_vm:
			throw LuaError("use_mapgen_biomes specified outside a "
				       "map generation context");
		}
		decomgr = emerge->decomgr;
	}
	if (!mgp) {
		mgp = &mg;
		// Intentionally truncates to s32, see Mapgen::Mapgen()
		mg.seed = (s32)emerge->mgparams->seed;
		mg.ndef = emerge->ndef;
	}

	const v3s16 default_pmin = vm->m_area.MinEdge + MAP_BLOCKSIZE,
				default_pmax = vm->m_area.MaxEdge - MAP_BLOCKSIZE;
	v3s16 pmin = lua_istable(L, 2) ? check_v3s16(L, 2) : default_pmin;
	v3s16 pmax = lua_istable(L, 3) ? check_v3s16(L, 3) : default_pmax;
	sortBoxVerticies(pmin, pmax);
	if (use_mapgen_biomes) {
		assert(oldvm);
		const v3s16 required_pmin = oldvm->m_area.MinEdge + MAP_BLOCKSIZE,
			required_pmax = oldvm->m_area.MaxEdge - MAP_BLOCKSIZE;
		if (pmin != required_pmin || pmax != required_pmax)
			throw LuaError("use_mapgen_biomes requires extents matching chunk area");
	}

	u32 blockseed = Mapgen::getBlockSeed(pmin, mg.seed);

	mgp->vm = vm;
	try {
		decomgr->placeAllDecos(mgp, blockseed, pmin, pmax);
	} catch (...) {
		mgp->vm = oldvm;
		throw;
	}
	mgp->vm = oldvm;

	return 0;
}


// create_schematic(p1, p2, probability_list, filename, y_slice_prob_list)
int ModApiMapgen::l_create_schematic(lua_State *L)
{
	GET_ENV_PTR;

	const NodeDefManager *ndef = getServer(L)->getNodeDefManager();

	const char *filename = luaL_checkstring(L, 4);
	CHECK_SECURE_PATH(L, filename, true);

	Schematic schem;

	v3s16 p1 = check_v3s16(L, 1);
	v3s16 p2 = check_v3s16(L, 2);
	sortBoxVerticies(p1, p2);

	std::vector<std::pair<v3s16, u8> > prob_list;
	if (lua_istable(L, 3)) {
		lua_pushnil(L);
		while (lua_next(L, 3)) {
			if (lua_istable(L, -1)) {
				lua_getfield(L, -1, "pos");
				v3s16 pos = check_v3s16(L, -1);
				lua_pop(L, 1);

				u8 prob = getintfield_default(L, -1, "prob", MTSCHEM_PROB_ALWAYS);
				prob_list.emplace_back(pos, prob);
			}

			lua_pop(L, 1);
		}
	}

	std::vector<std::pair<s16, u8> > slice_prob_list;
	if (lua_istable(L, 5)) {
		lua_pushnil(L);
		while (lua_next(L, 5)) {
			if (lua_istable(L, -1)) {
				s16 ypos = getintfield_default(L, -1, "ypos", 0);
				u8 prob  = getintfield_default(L, -1, "prob", MTSCHEM_PROB_ALWAYS);
				slice_prob_list.emplace_back(ypos, prob);
			}

			lua_pop(L, 1);
		}
	}

	if (!schem.getSchematicFromMap(&env->getMap(), p1, p2)) {
		errorstream << "create_schematic: failed to get schematic "
			"from map" << std::endl;
		return 0;
	}

	schem.applyProbabilities(p1, &prob_list, &slice_prob_list);

	schem.saveSchematicToFile(filename, ndef);
	actionstream << "create_schematic: saved schematic file '"
		<< filename << "'." << std::endl;

	lua_pushboolean(L, true);
	return 1;
}


// place_schematic(p, schematic, rotation,
//     replacements, force_placement, flagstring)
int ModApiMapgen::l_place_schematic(lua_State *L)
{
	GET_ENV_PTR;

	ServerMap *map = &(env->getServerMap());
	SchematicManager *schemmgr = getServer(L)->getEmergeManager()->schemmgr;

	//// Read position
	v3s16 p = check_v3s16(L, 1);

	//// Read rotation
	int rot = ROTATE_0;
	std::string enumstr = readParam<std::string>(L, 3, "");
	if (!enumstr.empty())
		string_to_enum(es_Rotation, rot, enumstr);

	//// Read force placement
	bool force_placement = true;
	if (lua_isboolean(L, 5))
		force_placement = readParam<bool>(L, 5);

	//// Read node replacements
	StringMap replace_names;
	if (lua_istable(L, 4))
		read_schematic_replacements(L, 4, &replace_names);

	//// Read schematic
	Schematic *schem = get_or_load_schematic(L, 2, schemmgr, &replace_names);
	if (!schem) {
		errorstream << "place_schematic: failed to get schematic" << std::endl;
		return 0;
	}

	//// Read flags
	u32 flags = 0;
	read_flags(L, 6, flagdesc_deco, &flags, NULL);

	schem->placeOnMap(map, p, flags, (Rotation)rot, force_placement);

	lua_pushboolean(L, true);
	return 1;
}


// place_schematic_on_vmanip(vm, p, schematic, rotation,
//     replacements, force_placement, flagstring)
int ModApiMapgen::l_place_schematic_on_vmanip(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	SchematicManager *schemmgr;
	if (auto mg = getMapgen(L))
		schemmgr = mg->m_emerge->schemmgr;
	else
		schemmgr = getServer(L)->getEmergeManager()->schemmgr;

	//// Read VoxelManip object
	MMVManip *vm = checkObject<LuaVoxelManip>(L, 1)->vm;

	//// Read position
	v3s16 p = check_v3s16(L, 2);

	//// Read rotation
	int rot = ROTATE_0;
	std::string enumstr = readParam<std::string>(L, 4, "");
	if (!enumstr.empty())
		string_to_enum(es_Rotation, rot, std::string(enumstr));

	//// Read force placement
	bool force_placement = true;
	if (lua_isboolean(L, 6))
		force_placement = readParam<bool>(L, 6);

	//// Read node replacements
	StringMap replace_names;
	if (lua_istable(L, 5))
		read_schematic_replacements(L, 5, &replace_names);

	//// Read schematic
	Schematic *schem = get_or_load_schematic(L, 3, schemmgr, &replace_names);
	if (!schem) {
		errorstream << "place_schematic: failed to get schematic" << std::endl;
		return 0;
	}

	//// Read flags
	u32 flags = 0;
	read_flags(L, 7, flagdesc_deco, &flags, NULL);

	bool schematic_did_fit = schem->placeOnVManip(
		vm, p, flags, (Rotation)rot, force_placement);

	lua_pushboolean(L, schematic_did_fit);
	return 1;
}

// spawn_tree_on_vmanip(vmanip, pos, treedef)
int ModApiMapgen::l_spawn_tree_on_vmanip(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	MMVManip *vm = checkObject<LuaVoxelManip>(L, 1)->vm;
	v3s16 p0 = read_v3s16(L, 2);
	treegen::TreeDef tree_def;
	const NodeDefManager *ndef = getGameDef(L)->ndef();
	if (!read_tree_def(L, 3, ndef, tree_def))
		return 0;

	treegen::error e = treegen::make_ltree(*vm, p0, tree_def);
	if (e != treegen::SUCCESS) {
		throw LuaError("spawn_tree_on_vmanip(): " + treegen::error_to_string(e));
	}

	lua_pushboolean(L, true);
	return 1;
}


// serialize_schematic(schematic, format, options={...})
int ModApiMapgen::l_serialize_schematic(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const SchematicManager *schemmgr = getEmergeManager(L)->getSchematicManager();

	//// Read options
	bool use_comments = getboolfield_default(L, 3, "lua_use_comments", false);
	u32 indent_spaces = getintfield_default(L, 3, "lua_num_indent_spaces", 0);

	//// Get schematic
	bool was_loaded = false;
	const Schematic *schem = (Schematic *)get_objdef(L, 1, schemmgr);
	if (!schem) {
		schem = load_schematic(L, 1, NULL, NULL);
		was_loaded = true;
	}
	if (!schem) {
		errorstream << "serialize_schematic: failed to get schematic" << std::endl;
		return 0;
	}

	//// Read format of definition to save as
	int schem_format = SCHEM_FMT_MTS;
	std::string enumstr = readParam<std::string>(L, 2, "");
	if (!enumstr.empty())
		string_to_enum(es_SchematicFormatType, schem_format, enumstr);

	//// Serialize to binary string
	std::ostringstream os(std::ios_base::binary);
	switch (schem_format) {
	case SCHEM_FMT_MTS:
		schem->serializeToMts(&os);
		break;
	case SCHEM_FMT_LUA:
		schem->serializeToLua(&os, use_comments, indent_spaces);
		break;
	default:
		return 0;
	}

	if (was_loaded)
		delete schem;

	std::string ser = os.str();
	lua_pushlstring(L, ser.c_str(), ser.length());
	return 1;
}

// read_schematic(schematic, options={...})
int ModApiMapgen::l_read_schematic(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	const SchematicManager *schemmgr = getEmergeManager(L)->getSchematicManager();
	const NodeDefManager *ndef = getGameDef(L)->ndef();

	//// Read options
	std::string write_yslice = getstringfield_default(L, 2, "write_yslice_prob", "all");

	//// Get schematic
	bool was_loaded = false;
	Schematic *schem = (Schematic *)get_objdef(L, 1, schemmgr);
	if (!schem) {
		schem = load_schematic(L, 1, NULL, NULL);
		was_loaded = true;
	}
	if (!schem) {
		errorstream << "read_schematic: failed to get schematic" << std::endl;
		return 0;
	}
	lua_pop(L, 2);

	//// Create the Lua table
	u32 numnodes = schem->size.X * schem->size.Y * schem->size.Z;
	bool resolve_done = schem->isResolveDone();
	const std::vector<std::string> &names = schem->m_nodenames;

	lua_createtable(L, 0, (write_yslice == "none") ? 2 : 3);

	// Create the size field
	push_v3s16(L, schem->size);
	lua_setfield(L, 1, "size");

	// Create the yslice_prob field
	if (write_yslice != "none") {
		lua_createtable(L, schem->size.Y, 0);
		for (u16 y = 0; y != schem->size.Y; ++y) {
			u8 probability = schem->slice_probs[y] & MTSCHEM_PROB_MASK;
			if (probability < MTSCHEM_PROB_ALWAYS || write_yslice != "low") {
				lua_createtable(L, 0, 2);
				lua_pushinteger(L, y);
				lua_setfield(L, 3, "ypos");
				lua_pushinteger(L, probability * 2);
				lua_setfield(L, 3, "prob");
				lua_rawseti(L, 2, y + 1);
			}
		}
		lua_setfield(L, 1, "yslice_prob");
	}

	// Create the data field
	lua_createtable(L, numnodes, 0); // data table
	for (u32 i = 0; i < numnodes; ++i) {
		MapNode node = schem->schemdata[i];
		const std::string &name =
				resolve_done ? ndef->get(node.getContent()).name : names[node.getContent()];
		u8 probability   = node.param1 & MTSCHEM_PROB_MASK;
		bool force_place = node.param1 & MTSCHEM_FORCE_PLACE;
		lua_createtable(L, 0, force_place ? 4 : 3);
		lua_pushstring(L, name.c_str());
		lua_setfield(L, 3, "name");
		lua_pushinteger(L, probability * 2);
		lua_setfield(L, 3, "prob");
		lua_pushinteger(L, node.param2);
		lua_setfield(L, 3, "param2");
		if (force_place) {
			lua_pushboolean(L, 1);
			lua_setfield(L, 3, "force_place");
		}
		lua_rawseti(L, 2, i + 1);
	}
	lua_setfield(L, 1, "data");

	if (was_loaded)
		delete schem;

	return 1;
}

int ModApiMapgen::update_liquids(lua_State *L, MMVManip *vm)
{
	UniqueQueue<v3s16> *trans_liquid;
	if (auto emerge = getEmergeThread(L)) {
		trans_liquid = emerge->m_trans_liquid;
	} else {
		GET_ENV_PTR;
		trans_liquid = &env->getServerMap().m_transforming_liquid;
	}
	assert(trans_liquid);

	const NodeDefManager *ndef = getGameDef(L)->ndef();

	Mapgen mg;
	mg.vm   = vm;
	mg.ndef = ndef;

	mg.updateLiquid(trans_liquid, vm->m_area.MinEdge, vm->m_area.MaxEdge);
	return 0;
}

int ModApiMapgen::calc_lighting(lua_State *L, MMVManip *vm,
		v3s16 pmin, v3s16 pmax, bool propagate_shadow)
{
	const NodeDefManager *ndef = getGameDef(L)->ndef();
	auto emerge = getEmergeManager(L);

	assert(vm->m_area.contains(VoxelArea(pmin, pmax)));

	Mapgen mg;
	mg.vm          = vm;
	mg.ndef        = ndef;
	mg.water_level = emerge->mgparams->water_level;

	mg.calcLighting(pmin, pmax, vm->m_area.MinEdge, vm->m_area.MaxEdge,
		propagate_shadow);
	return 0;
}

int ModApiMapgen::set_lighting(lua_State *L, MMVManip *vm,
		v3s16 pmin, v3s16 pmax, u8 light)
{
	assert(vm->m_area.contains(VoxelArea(pmin, pmax)));

	Mapgen mg;
	mg.vm = vm;

	mg.setLighting(light, pmin, pmax);
	return 0;
}

const EmergeManager *ModApiMapgen::getEmergeManager(lua_State *L)
{
	auto emerge = getEmergeThread(L);
	if (emerge)
		return emerge->getEmergeManager();
	return getServer(L)->getEmergeManager();
}

const BiomeGen *ModApiMapgen::getBiomeGen(lua_State *L)
{
	// path 1: we're in the emerge environment
	auto emerge = getEmergeThread(L);
	if (emerge)
		return emerge->getMapgen()->m_emerge->biomegen;
	// path 2: we're in the server environment
	auto manager = getServer(L)->getEmergeManager();
	return manager->getBiomeGen();
}

Mapgen *ModApiMapgen::getMapgen(lua_State *L)
{
	// path 1
	auto emerge = getEmergeThread(L);
	if (emerge)
		return emerge->getMapgen();
	// path 2
	return getServer(L)->getEmergeManager()->getCurrentMapgen();
}

void ModApiMapgen::Initialize(lua_State *L, int top)
{
	API_FCT(get_biome_id);
	API_FCT(get_biome_name);
	API_FCT(get_heat);
	API_FCT(get_humidity);
	API_FCT(get_biome_data);
	API_FCT(get_mapgen_object);
	API_FCT(get_spawn_level);

	API_FCT(get_mapgen_params);
	API_FCT(set_mapgen_params);
	API_FCT(get_mapgen_edges);
	API_FCT(get_mapgen_chunksize);
	API_FCT(get_mapgen_setting);
	API_FCT(set_mapgen_setting);
	API_FCT(get_mapgen_setting_noiseparams);
	API_FCT(set_mapgen_setting_noiseparams);
	API_FCT(set_noiseparams);
	API_FCT(get_noiseparams);
	API_FCT(set_gen_notify);
	API_FCT(get_gen_notify);
	API_FCT(get_decoration_id);

	API_FCT(register_biome);
	API_FCT(register_decoration);
	API_FCT(register_ore);
	API_FCT(register_schematic);

	API_FCT(clear_registered_biomes);
	API_FCT(clear_registered_decorations);
	API_FCT(clear_registered_ores);
	API_FCT(clear_registered_schematics);

	API_FCT(generate_ores);
	API_FCT(generate_decorations);
	API_FCT(create_schematic);
	API_FCT(place_schematic);
	API_FCT(place_schematic_on_vmanip);
	API_FCT(spawn_tree_on_vmanip);
	API_FCT(serialize_schematic);
	API_FCT(read_schematic);
}

void ModApiMapgen::InitializeEmerge(lua_State *L, int top)
{
	API_FCT(get_biome_id);
	API_FCT(get_biome_name);
	API_FCT(get_heat);
	API_FCT(get_humidity);
	API_FCT(get_biome_data);
	API_FCT(get_mapgen_object);

	API_FCT(get_seed);
	API_FCT(get_mapgen_params);
	API_FCT(get_mapgen_edges);
	API_FCT(get_mapgen_chunksize);
	API_FCT(get_mapgen_setting);
	API_FCT(get_mapgen_setting_noiseparams);
	API_FCT(get_noiseparams);
	API_FCT(get_gen_notify);
	API_FCT(get_decoration_id);
	API_FCT(save_gen_notify);

	API_FCT(generate_ores);
	API_FCT(generate_decorations);
	API_FCT(place_schematic_on_vmanip);
	API_FCT(spawn_tree_on_vmanip);
	API_FCT(serialize_schematic);
	API_FCT(read_schematic);
}
