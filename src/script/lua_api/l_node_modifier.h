#pragma once

#include "lua_api/l_base.h"
#include "irr_v3d.h"

class ModApiNodeModifier : public ModApiBase {
private:
	static int l_register_node_modifier_raw(lua_State *L);
public:
	static void Initialize(lua_State *L, int top);
};

class LuaItemStack;
class ServerEnvironment;

// Live reference to modifiers on a node or an owned Lua ItemStack.
class NodeModifiersRef : public ModApiBase {
	LuaItemStack *m_stack = nullptr;
	ServerEnvironment *m_env = nullptr;
	v3s16 m_pos;
	static int l_add(lua_State *L);
	static int l_remove(lua_State *L);
	static int l_set_list(lua_State *L);
	static int l_get_list(lua_State *L);
	static int gc_object(lua_State *L);
	static void create(lua_State *L, NodeModifiersRef *ref);
public:
	explicit NodeModifiersRef(LuaItemStack *stack);
	NodeModifiersRef(v3s16 pos, ServerEnvironment *env) : m_env(env), m_pos(pos) {}
	~NodeModifiersRef();
	DISABLE_CLASS_COPY(NodeModifiersRef)
	bool set(std::vector<std::string> names);
	void pushList(lua_State *L) const;
	static std::string readName(lua_State *L, int index);
	static void readList(lua_State *L, int table, std::vector<std::string> &names);
	static void create(lua_State *L, LuaItemStack *stack);
	static void create(lua_State *L, v3s16 pos, ServerEnvironment *env);
	static void Register(lua_State *L);
	static const char className[];
};
