// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "irr_v3d.h"
#include <unordered_set>

struct ObjectProperties;
struct ToolCapabilities;
struct collisionMoveResult;

class ScriptApiEntity
		: virtual public ScriptApiBase
{
public:
	bool luaentity_Add(u16 id, const char *name);
	void luaentity_Activate(u16 id,
			const std::string &staticdata, u32 dtime_s);
	void luaentity_Deactivate(u16 id, bool removal);
	void luaentity_Remove(u16 id);
	std::string luaentity_GetStaticdata(u16 id);
	void luaentity_GetProperties(u16 id,
			ServerActiveObject *self, ObjectProperties *prop, const std::string &entity_name);
	void luaentity_Step(u16 id, float dtime,
		const collisionMoveResult *moveresult);
	bool luaentity_Punch(u16 id,
			ServerActiveObject *puncher, float time_from_last_punch,
			const ToolCapabilities *toolcap, v3f dir, s32 damage);
	bool luaentity_on_death(u16 id, ServerActiveObject *killer);
	void luaentity_Rightclick(u16 id, ServerActiveObject *clicker);
	void luaentity_on_attach_child(u16 id, ServerActiveObject *child);
	void luaentity_on_detach_child(u16 id, ServerActiveObject *child);
	void luaentity_on_detach(u16 id, ServerActiveObject *parent);
private:
	bool luaentity_run_simple_callback(u16 id, ServerActiveObject *sao,
		const char *field);

	void logDeprecationForExistingProperties(lua_State *L, int index, const std::string &name);

	/** Stores names of entities that already caused a deprecation warning due to
	 * properties being outside of initial_properties. If an entity's name is in here,
	 * it won't cause any more of those deprecation warnings. */
	std::unordered_set<std::string> deprecation_warned_init_properties;
};
