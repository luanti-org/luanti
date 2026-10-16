#include "lua_api/l_node_modifier.h"
#include "lua_api/l_internal.h"
#include "common/c_content.h"
#include "common/c_converter.h"
#include "node_modifier.h"
#include "server.h"
#include "util/string.h"

static void read_node_modifier(lua_State *L, NodeModifier &nmod, int index)
{
	if (index < 0)
		index = lua_gettop(L) + 1 + index;

	bool is_present = getintfield(L, index, "light_source", nmod.m_light_source);
	if (is_present) {
		nmod.m_modified_props_mask |= MP_LightSource;
		if (nmod.m_light_source > LIGHT_MAX) {
			warningstream << "Node " << nmod.m_name.c_str()
				<< " had greater light_source than " << LIGHT_MAX
				<< ", it was reduced." << std::endl;
			nmod.m_light_source = LIGHT_MAX;
		}
	}
}

int ModApiNodeModifier::l_register_node_modifier_raw(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;
	luaL_checktype(L, 1, LUA_TTABLE);
	int table = 1;

	auto *nmod = getServer(L)->getWritableNodeModifierManager();

	std::string name;
	lua_getfield(L, table, "name");
	luaL_checktype(L, -1, LUA_TSTRING);
	name = readParam<std::string>(L, -1);

	if (!isValidModScopedName(name))
		throw LuaError("Node modifier name must have the format modname:name "
				"with letters, digits and underscores (lowercase modname)");

	NodeModifier mod(name);
	read_node_modifier(L, mod, table);
	u16 inserted_id = nmod->add(std::move(mod));
	if (inserted_id == MODIFIER_IGNORE) {
		warningstream << "Failed to register node modifier " << name << "." << std::endl;
	}

	return 0;
}

void ModApiNodeModifier::Initialize(lua_State *L, int top)
{
	API_FCT(register_node_modifier_raw);
}