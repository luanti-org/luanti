// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <set>
#include <utility>

#include "activeobject.h"
#include "environment.h"
#include "servermap.h"
#include "settings.h"
#include "server/activeobjectmgr.h"
#include "util/numeric.h"
#include "util/metricsbackend.h"

class IGameDef;
struct GameParams;
class RemotePlayer;
class PlayerDatabase;
class AuthDatabase;
class PlayerSAO;
class ServerEnvironment;
class ActiveBlockModifier;
struct StaticObject;
class ServerActiveObject;
class Server;
class ServerScripting;
enum AccessDeniedCode : u8;
typedef u16 session_t;

/*
	{Active, Loading} block modifier interface.

	These are fed into ServerEnvironment at initialization time;
	ServerEnvironment handles deleting them.
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

	// Returns an iterator to the LBMs that were introduced
	// after the given time. This is guaranteed to return
	// valid values for everything
	lbm_lookup_map::const_iterator getLBMsIntroducedAfter(u32 time)
	{ return m_lbm_lookup.lower_bound(time); }
};

/*
	List of active blocks, used by ServerEnvironment
*/

class ActiveBlockList
{
public:
	void update(std::vector<PlayerSAO*> &active_players,
		s16 active_block_range,
		s16 active_object_range,
		std::set<v3s16> &blocks_removed,
		std::set<v3s16> &blocks_added,
		std::set<v3s16> &extra_blocks_added);

	bool contains(v3s16 p) const {
		return (m_list.find(p) != m_list.end());
	}

	auto size() const {
		return m_list.size();
	}

	void clear() {
		m_list.clear();
	}

	void remove(v3s16 p) {
		m_list.erase(p);
		m_abm_list.erase(p);
	}

	std::set<v3s16> m_list;
	std::set<v3s16> m_abm_list;
	// list of blocks that are always active, not modified by this class
	std::set<v3s16> m_forceloaded_list;
};

/*
	ServerEnvironment::m_on_mapblocks_changed_receiver
*/
struct OnMapblocksChangedReceiver : public MapEventReceiver {
	std::unordered_set<v3s16> modified_blocks;
	bool receiving = false;

	void onMapEditEvent(const MapEditEvent &event) override;
};

/*
	Operation mode for ServerEnvironment::clearObjects()
*/
enum ClearObjectsMode {
	// Load and go through every mapblock, clearing objects
		CLEAR_OBJECTS_MODE_FULL,

	// Clear objects immediately in loaded mapblocks;
	// clear objects in unloaded mapblocks only when the mapblocks are next activated.
		CLEAR_OBJECTS_MODE_QUICK,
};

class ServerEnvironment final : public Environment
{
public:
	ServerEnvironment(std::unique_ptr<ServerMap> map, Server *server, MetricsBackend *mb);
	~ServerEnvironment();

	void init();

	Map & getMap();

	ServerMap & getServerMap();

	//TODO find way to remove this fct!
	ServerScripting* getScriptIface()
	{ return m_script; }

	Server *getGameDef()
	{ return m_server; }

	float getSendRecommendedInterval()
	{ return m_recommended_send_interval; }

	// Save players
	void saveLoadedPlayers(bool force = false);
	void savePlayer(RemotePlayer *player);
	PlayerSAO *loadPlayer(RemotePlayer *player, bool *new_player, session_t peer_id,
		bool is_singleplayer);
	void addPlayer(RemotePlayer *player);
	void removePlayer(RemotePlayer *player);
	bool removePlayerFromDatabase(const std::string &name);

	/*
		Save and load time of day and game timer
	*/
	void saveMeta();
	void loadMeta();

	u32 addParticleSpawner(float exptime);
	u32 addParticleSpawner(float exptime, u16 attached_id);
	void deleteParticleSpawner(u32 id, bool remove_from_object = true);

	/*
		External ActiveObject interface
		-------------------------------------------
	*/

	ServerActiveObject* getActiveObject(u16 id)
	{
		return m_ao_manager.getActiveObject(id);
	}

	/*
		Add an active object to the environment.
		Environment handles deletion of object.
		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	u16 addActiveObject(std::unique_ptr<ServerActiveObject> object);

	void invalidateActiveObjectObserverCaches();

	/*
		Find out what new objects have been added to
		inside a radius around a position
	*/
	void getAddedActiveObjects(PlayerSAO *playersao, s16 radius,
		s16 player_radius,
		const std::set<u16> &current_objects,
		std::vector<u16> &added_objects);

	/*
		Find out what new objects have been removed from
		inside a radius around a position
	*/
	void getRemovedActiveObjects(PlayerSAO *playersao, s16 radius,
		s16 player_radius,
		const std::set<u16> &current_objects,
		std::vector<std::pair<bool /* gone? */, u16>> &removed_objects);

	/*
		Get the next message emitted by some active object.
		Returns false if no messages are available, true otherwise.
	*/
	bool getActiveObjectMessage(ActiveObjectMessage *dest);

	virtual void getSelectedActiveObjects(
		const core::line3d<f32> &shootline_on_map,
		std::vector<PointedThing> &objects,
		const std::optional<Pointabilities> &pointabilities
	);

	/*
		Activate objects and dynamically modify for the dtime determined
		from timestamp and additional_dtime
	*/
	void activateBlock(MapBlock *block, u32 additional_dtime=0);

	/*
		{Active,Loading}BlockModifiers
		-------------------------------------------
	*/

	void addActiveBlockModifier(ActiveBlockModifier *abm);
	void addLoadingBlockModifierDef(LoadingBlockModifierDef *lbm);

	/*
		Other stuff
		-------------------------------------------
	*/

	// Script-aware node setters
	bool setNode(v3s16 p, const MapNode &n);
	bool removeNode(v3s16 p);
	bool swapNode(v3s16 p, const MapNode &n);

	// Find the daylight value at pos with a Depth First Search
	u8 findSunlight(v3s16 pos) const;

	// Find all active objects inside a radius around a point
	void getObjectsInsideRadius(std::vector<ServerActiveObject *> &objects, const v3f &pos, float radius,
			std::function<bool(ServerActiveObject *obj)> include_obj_cb)
	{
		return m_ao_manager.getObjectsInsideRadius(pos, radius, objects, include_obj_cb);
	}

	// Find all active objects inside a box
	void getObjectsInArea(std::vector<ServerActiveObject *> &objects, const aabb3f &box,
			std::function<bool(ServerActiveObject *obj)> include_obj_cb)
	{
		return m_ao_manager.getObjectsInArea(box, objects, include_obj_cb);
	}

	// Clear objects, loading and going through every MapBlock
	void clearObjects(ClearObjectsMode mode);

	// to be called before destructor
	void deactivateBlocksAndObjects();

	// This makes stuff happen
	void step(f32 dtime);

	u32 getGameTime() const { return m_game_time; }

	void reportMaxLagEstimate(float f) { m_max_lag_estimate = f; }
	float getMaxLagEstimate() const { return m_max_lag_estimate; }

	std::set<v3s16>* getForceloadedBlocks() { return &m_active_blocks.m_forceloaded_list; }

	// Sorted by how ready a mapblock is
	enum BlockStatus {
		BS_UNKNOWN,
		BS_EMERGING,
		BS_LOADED,
		BS_ACTIVE // always highest value
	};
	BlockStatus getBlockStatus(v3s16 blockpos);

	// Sets the static object status all the active objects in the specified block
	// This is only really needed for deleting blocks from the map
	void setStaticForActiveObjectsInBlock(v3s16 blockpos,
		bool static_exists, v3s16 static_block=v3s16(0,0,0));

	RemotePlayer *getPlayer(const session_t peer_id);
	RemotePlayer *getPlayer(const std::string &name, bool match_invalid_peer = false);
	const std::vector<RemotePlayer *> getPlayers() const { return m_players; }
	u32 getPlayerCount() const { return m_players.size(); }

	static bool migratePlayersDatabase(const GameParams &game_params,
			const Settings &cmd_args);

	AuthDatabase *getAuthDatabase() { return m_auth_database; }
	static bool migrateAuthDatabase(const GameParams &game_params,
			const Settings &cmd_args);
private:

	/**
	 * called if env_meta.txt doesn't exist (e.g. new world)
	 */
	void loadDefaultMeta();

	static PlayerDatabase *openPlayerDatabase(const std::string &name,
			const std::string &savedir, const Settings &conf);
	static AuthDatabase *openAuthDatabase(const std::string &name,
			const std::string &savedir, const Settings &conf);
	/*
		Internal ActiveObject interface
		-------------------------------------------
	*/

	/*
		Add an active object to the environment.

		Called by addActiveObject.

		Object may be deleted by environment immediately.
		If id of object is 0, assigns a free id to it.
		Returns the id of the object.
		Returns 0 if not added and thus deleted.
	*/
	u16 addActiveObjectRaw(std::unique_ptr<ServerActiveObject> object,
			const StaticObject *from_static, u32 dtime_s);

	/*
		Remove all objects that satisfy (isGone() && m_known_by_count==0)
	*/
	void removeRemovedObjects();

	/*
		Convert stored objects from block to active
	*/
	void activateObjects(MapBlock *block, u32 dtime_s);

	/*
		Convert objects that are not in active blocks to static.

		If m_known_by_count != 0, active object is not deleted, but static
		data is still updated.

		If force_delete is set, active object is deleted nevertheless. It
		shall only be set so in the destructor of the environment.
	*/
	void deactivateFarObjects(bool force_delete);

	/*
		A few helpers used by the three above methods
	*/
	void deleteStaticFromBlock(
			ServerActiveObject *obj, u16 id, u32 mod_reason, bool no_emerge);
	bool saveStaticToBlock(v3s16 blockpos, u16 store_id,
			ServerActiveObject *obj, const StaticObject &s_obj, u32 mod_reason);

	void processActiveObjectRemove(ServerActiveObject *obj);

	/*
		Member variables
	*/

	// The map
	std::unique_ptr<ServerMap> m_map;
	// Lua state
	ServerScripting* m_script;
	// Server definition
	Server *m_server;
	// Active Object Manager
	server::ActiveObjectMgr m_ao_manager;
	// on_mapblocks_changed map event receiver
	OnMapblocksChangedReceiver m_on_mapblocks_changed_receiver;
	// Outgoing network message buffer for active objects
	std::queue<ActiveObjectMessage> m_active_object_messages;
	// Some timers
	float m_send_recommended_timer = 0.0f;
	IntervalLimiter m_object_management_interval;
	// List of active blocks
	ActiveBlockList m_active_blocks;
	int m_fast_active_block_divider = 1;
	IntervalLimiter m_active_blocks_mgmt_interval;
	IntervalLimiter m_active_block_modifier_interval;
	IntervalLimiter m_active_blocks_nodemetadata_interval;
	// Whether the variables below have been read from file yet
	bool m_meta_loaded = false;
	// Time from the beginning of the game in seconds.
	// Incremented in step().
	u32 m_game_time = 0;
	// A helper variable for incrementing the latter
	float m_game_time_fraction_counter = 0.0f;
	// Time of last clearObjects call (game time).
	// When a mapblock older than this is loaded, its objects are cleared.
	u32 m_last_clear_objects_time = 0;
	// Active block modifiers
	std::vector<ABMWithState> m_abms;
	LBMManager m_lbm_mgr;
	// An interval for generally sending object positions and stuff
	float m_recommended_send_interval = 0.1f;
	// Estimate for general maximum lag as determined by server.
	// Can raise to high values like 15s with eg. map generation mods.
	float m_max_lag_estimate = 0.1f;

	// peer_ids in here should be unique, except that there may be many 0s
	std::vector<RemotePlayer*> m_players;

	PlayerDatabase *m_player_database = nullptr;
	AuthDatabase *m_auth_database = nullptr;

	// Particles
	IntervalLimiter m_particle_management_interval;
	std::unordered_map<u32, float> m_particle_spawners;
	u32 m_particle_spawners_id_last_used = 0;
	std::unordered_map<u32, u16> m_particle_spawner_attachments;

	// Environment metrics
	MetricCounterPtr m_step_time_counter;
	MetricGaugePtr m_active_block_gauge;
	MetricGaugePtr m_active_object_gauge;

	std::unique_ptr<ServerActiveObject> createSAO(ActiveObjectType type, v3f pos,
			const std::string &data);
};
