// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "irr_v3d.h"
#include "mapnode.h"
#include "server/blockmodifier.h"
#include <unordered_set>
#include <vector>

class ServerEnvironment;
class MapBlock;
struct ScriptCallbackState;

/*
	LuaABM
*/

class LuaABM : public ActiveBlockModifier {
private:
	const int m_id;

	std::string m_name;
	std::vector<std::string> m_trigger_contents;
	std::vector<std::string> m_required_neighbors;
	std::vector<std::string> m_without_neighbors;
	float m_trigger_interval;
	u32 m_trigger_chance;
	bool m_simple_catch_up;
	s16 m_min_y;
	s16 m_max_y;

public:
	LuaABM(int id,
			const std::string &name,
			const std::vector<std::string> &trigger_contents,
			const std::vector<std::string> &required_neighbors,
			const std::vector<std::string> &without_neighbors,
			float trigger_interval, u32 trigger_chance, bool simple_catch_up,
			s16 min_y, s16 max_y);

	virtual const std::string &getName() const;
	virtual const std::vector<std::string> &getTriggerContents() const;
	virtual const std::vector<std::string> &getRequiredNeighbors() const;
	virtual const std::vector<std::string> &getWithoutNeighbors() const;
	virtual float getTriggerInterval();
	virtual u32 getTriggerChance();
	virtual bool getSimpleCatchUp();
	virtual s16 getMinY();
	virtual s16 getMaxY();

	virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider);
};

class ScriptApiEnv : virtual public ScriptApiBase
{
public:
	// Called on environment step
	void environment_Step(float dtime);

	// Called after generating a piece of map
	void environment_OnGenerated(v3s16 minp, v3s16 maxp, u32 blockseed);

	// Called on player event
	void player_event(ServerActiveObject *player, const std::string &type);

	// Called after emerge of a block queued from core.emerge_area()
	void on_emerge_area_completion(v3s16 blockpos, int action,
		ScriptCallbackState *state);

	void check_for_falling(v3s16 p);

	// Called after liquid transform changes
	void on_liquid_transformed(const std::vector<std::pair<v3s16, MapNode>> &list);

	// Called after mapblock changes
	void on_mapblocks_changed(const std::unordered_set<v3s16> &set);

	// Determines whether there are any on_mapblocks_changed callbacks
	bool has_on_mapblocks_changed();

	// Initializes environment and loads some definitions from Lua
	void initializeEnvironment(ServerEnvironment *env);

	void triggerABM(int id, v3s16 p, MapNode n,
			u32 active_object_count, u32 active_object_count_wider);

	void triggerLBM(int id, MapBlock *block,
		const std::unordered_set<v3s16> &positions, float dtime_s);

	static LuaABM *readABM(lua_State *L, int abm_index, int id);
private:
	void readABMs();

	void readLBMs();

	// Reads a single or a list of node names into a vector
	static bool read_nodenames(lua_State *L, int idx, std::vector<std::string> &to);
};
