#pragma once

#include "lua_api/l_base.h"

class ModApiNodeModifier : public ModApiBase {
private:
	static int l_register_node_modifier_raw(lua_State *L);
public:
	static void Initialize(lua_State *L, int top);
};