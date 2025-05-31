// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <unordered_map>

#include "irr_v3d.h"
#include "mapnode.h"

class ServerEnvironment;
class ServerMap;
class MapBlock;
class IGameDef;

/*
	ABMs
*/

class ActiveBlockModifier
{
public:
	ActiveBlockModifier() = default;
	virtual ~ActiveBlockModifier() = default;

	// Set of contents to trigger on
	virtual const std::vector<std::string> &getTriggerContents() const = 0;
	// Set of required neighbors (trigger doesn't happen if none are found)
	// Empty = do not check neighbors
	virtual const std::vector<std::string> &getRequiredNeighbors() const = 0;
	// Set of without neighbors (trigger doesn't happen if any are found)
	// Empty = do not check neighbors
	virtual const std::vector<std::string> &getWithoutNeighbors() const = 0;
	// Trigger interval in seconds
	virtual float getTriggerInterval() = 0;
	// Random chance of (1 / return value), 0 is disallowed
	virtual u32 getTriggerChance() = 0;
	// Whether to modify chance to simulate time lost by an unnattended block
	virtual bool getSimpleCatchUp() = 0;
	// get min Y for apply abm
	virtual s16 getMinY() = 0;
	// get max Y for apply abm
	virtual s16 getMaxY() = 0;
	// This is called usually at interval for 1/chance of the nodes
	virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n){};
	virtual void trigger(ServerEnvironment *env, v3s16 p, MapNode n,
		u32 active_object_count, u32 active_object_count_wider){};
};

struct ABMWithState
{
	ActiveBlockModifier *abm;
	float timer = 0.0f;

	ABMWithState(ActiveBlockModifier *abm_);
};

struct ActiveABM; // hidden

class ABMHandler
{
	ServerEnvironment *m_env;
	// vector index = content_t
	std::vector<std::vector<ActiveABM>*> m_aabms;

public:
	ABMHandler(std::vector<ABMWithState> &abms,
		float dtime_s, ServerEnvironment *env,
		bool use_timers);
	~ABMHandler();

	// Find out how many objects the given block and its neighbors contain.
	// Returns the number of objects in the block, and also in 'wider' the
	// number of objects in the block and all its neighbors. The latter
	// may be an estimate if any neighbors are unloaded.
	static u32 countObjects(MapBlock *block, ServerMap * map, u32 &wider);

	void apply(MapBlock *block, int &blocks_scanned, int &abms_run, int &blocks_cached);
};

/*
	LBMs
*/

#define LBM_NAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyz0123456789_:"

struct LoadingBlockModifierDef
{
	// Set of contents to trigger on
	std::vector<std::string> trigger_contents;
	std::string name;
	bool run_at_every_load = false;

	virtual ~LoadingBlockModifierDef() = default;

	/// @brief Called to invoke LBM
	/// @param env environment
	/// @param block the block in question
	/// @param positions set of node positions (block-relative!)
	/// @param dtime_s game time since last deactivation
	virtual void trigger(ServerEnvironment *env, MapBlock *block,
		const std::unordered_set<v3s16> &positions, float dtime_s) {};
};

class LBMContentMapping
{
public:
	typedef std::vector<LoadingBlockModifierDef*> lbm_vector;
	typedef std::unordered_map<content_t, lbm_vector> lbm_map;

	LBMContentMapping() = default;
	void addLBM(LoadingBlockModifierDef *lbm_def, IGameDef *gamedef);
	const lbm_map::mapped_type *lookup(content_t c) const;
	const lbm_vector &getList() const { return lbm_list; }
	bool empty() const { return lbm_list.empty(); }

	// This struct owns the LBM pointers.
	~LBMContentMapping();
	DISABLE_CLASS_COPY(LBMContentMapping);
	ALLOW_CLASS_MOVE(LBMContentMapping);

private:
	lbm_vector lbm_list;
	lbm_map map;
};

class LBMManager
{
public:
	LBMManager() = default;
	~LBMManager();

	// Don't call this after loadIntroductionTimes() ran.
	void addLBMDef(LoadingBlockModifierDef *lbm_def);

	/// @param now current game time
	void loadIntroductionTimes(const std::string &times,
		IGameDef *gamedef, u32 now);

	// Don't call this before loadIntroductionTimes() ran.
	std::string createIntroductionTimesString();

	// Don't call this before loadIntroductionTimes() ran.
	void applyLBMs(ServerEnvironment *env, MapBlock *block,
			u32 stamp, float dtime_s);

	// Warning: do not make this std::unordered_map, order is relevant here
	typedef std::map<u32, LBMContentMapping> lbm_lookup_map;

private:
	// Once we set this to true, we can only query,
	// not modify
	bool m_query_mode = false;

	// For m_query_mode == false:
	// The key of the map is the LBM def's name.
	std::unordered_map<std::string, LoadingBlockModifierDef *> m_lbm_defs;

	// For m_query_mode == true:
	// The key of the map is the LBM def's first introduction time.
	lbm_lookup_map m_lbm_lookup;

	/// @return map of LBM name -> timestamp
	static std::unordered_map<std::string, u32>
	parseIntroductionTimesString(const std::string &times);

	// Returns an iterator to the LBMs that were introduced
	// after the given time. This is guaranteed to return
	// valid values for everything
	lbm_lookup_map::const_iterator getLBMsIntroducedAfter(u32 time)
	{ return m_lbm_lookup.lower_bound(time); }
};
