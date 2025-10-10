// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "lua_api/l_base.h"
#include "irr_v3d.h"

typedef u16 biome_t;  // copy from mg_biome.h to avoid an unnecessary include

class MMVManip;
class BiomeGen;
class Mapgen;
class EmergeManager;

class ModApiMapgen : public ModApiBase
{
	friend class LuaVoxelManip;
private:
	// get_biome_id(biomename)
	// returns the biome id as used in biomemap and returned by 'get_biome_data()'
	static int l_get_biome_id(lua_State *L);

	// get_biome_name(biome_id)
	// returns the biome name string
	static int l_get_biome_name(lua_State *L);

	// get_heat(pos)
	// returns the heat at the position
	static int l_get_heat(lua_State *L);

	// get_humidity(pos)
	// returns the humidity at the position
	static int l_get_humidity(lua_State *L);

	// get_biome_data(pos)
	// returns a table containing the biome id, heat and humidity at the position
	static int l_get_biome_data(lua_State *L);

	// get_mapgen_object(objectname)
	// returns the requested object used during map generation
	static int l_get_mapgen_object(lua_State *L);

	// get_spawn_level(x = num, z = num)
	static int l_get_spawn_level(lua_State *L);

	// get_mapgen_params()
	// returns the currently active map generation parameter set
	static int l_get_mapgen_params(lua_State *L);

	// set_mapgen_params(params)
	// set mapgen parameters
	static int l_set_mapgen_params(lua_State *L);

	// get_mapgen_edges([mapgen_limit[, chunksize]])
	static int l_get_mapgen_edges(lua_State *L);

	// get_mapgen_chunksize()
	static int l_get_mapgen_chunksize(lua_State *L);

	// get_seed([add])
	static int l_get_seed(lua_State *L);

	// get_mapgen_setting(name)
	static int l_get_mapgen_setting(lua_State *L);

	// set_mapgen_setting(name, value, override_meta)
	static int l_set_mapgen_setting(lua_State *L);

	// get_mapgen_setting_noiseparams(name)
	static int l_get_mapgen_setting_noiseparams(lua_State *L);

	// set_mapgen_setting_noiseparams(name, value, override_meta)
	static int l_set_mapgen_setting_noiseparams(lua_State *L);

	// set_noiseparam_defaults(name, noiseparams, set_default)
	static int l_set_noiseparams(lua_State *L);

	// get_noiseparam_defaults(name)
	static int l_get_noiseparams(lua_State *L);

	// set_gen_notify(flags, {deco_ids}, {ud_ids})
	static int l_set_gen_notify(lua_State *L);

	// get_gen_notify()
	static int l_get_gen_notify(lua_State *L);

	// save_gen_notify(ud_id, data)
	static int l_save_gen_notify(lua_State *L);

	// get_decoration_id(decoration_name)
	// returns the decoration ID as used in gennotify
	static int l_get_decoration_id(lua_State *L);

	// register_biome({lots of stuff})
	static int l_register_biome(lua_State *L);

	// register_decoration({lots of stuff})
	static int l_register_decoration(lua_State *L);

	// register_ore({lots of stuff})
	static int l_register_ore(lua_State *L);

	// register_schematic({schematic}, replacements={})
	static int l_register_schematic(lua_State *L);

	// clear_registered_biomes()
	static int l_clear_registered_biomes(lua_State *L);

	// clear_registered_decorations()
	static int l_clear_registered_decorations(lua_State *L);

	// clear_registered_schematics()
	static int l_clear_registered_schematics(lua_State *L);

	// generate_ores(vm, p1, p2)
	static int l_generate_ores(lua_State *L);

	// generate_decorations(vm, p1, p2)
	static int l_generate_decorations(lua_State *L);

	// clear_registered_ores
	static int l_clear_registered_ores(lua_State *L);

	// create_schematic(p1, p2, probability_list, filename)
	static int l_create_schematic(lua_State *L);

	// place_schematic(p, schematic, rotation,
	//     replacements, force_placement, flagstring)
	static int l_place_schematic(lua_State *L);

	// place_schematic_on_vmanip(vm, p, schematic, rotation,
	//     replacements, force_placement, flagstring)
	static int l_place_schematic_on_vmanip(lua_State *L);

	// spawn_tree_on_vmanip(vmanip, pos, treedef)
	static int l_spawn_tree_on_vmanip(lua_State *L);

	// serialize_schematic(schematic, format, options={...})
	static int l_serialize_schematic(lua_State *L);

	// read_schematic(schematic, options={...})
	static int l_read_schematic(lua_State *L);

	// Foreign implementations
	/*
	 * In this case the API functions belong to LuaVoxelManip (so l_vmanip.cpp),
	 * but the implementations are so deeply connected to mapgen-related code
	 * that they are better off being here.
	 */

	static int update_liquids(lua_State *L, MMVManip *vm);

	static int calc_lighting(lua_State *L, MMVManip *vm,
			v3s16 pmin, v3s16 pmax, bool propagate_shadow);

	static int set_lighting(lua_State *L, MMVManip *vm,
			v3s16 pmin, v3s16 pmax, u8 light);

	// Helpers

	// get a read-only(!) EmergeManager
	static const EmergeManager *getEmergeManager(lua_State *L);
	// get the thread-local or global BiomeGen (still read-only)
	static const BiomeGen *getBiomeGen(lua_State *L);
	// get the thread-local mapgen
	static Mapgen *getMapgen(lua_State *L);

public:
	static void Initialize(lua_State *L, int top);
	static void InitializeEmerge(lua_State *L, int top);

	static struct EnumString es_BiomeTerrainType[];
	static struct EnumString es_DecorationType[];
	static struct EnumString es_MapgenObject[];
	static struct EnumString es_OreType[];
	static struct EnumString es_Rotation[];
	static struct EnumString es_SchematicFormatType[];
	static struct EnumString es_NodeResolveMethod[];
};
