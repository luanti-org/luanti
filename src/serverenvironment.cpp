// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2017 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <algorithm>
#include <stack>
#include <utility>
#include "serverenvironment.h"
#include "irr_aabb3d.h"
#include "settings.h"
#include "log.h"
#include "mapblock.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "gamedef.h"
#include "porting.h"
#include "profiler.h"
#include "raycast.h"
#include "remoteplayer.h"
#include "scripting_server.h"
#include "server.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/basic_macros.h"
#include "util/pointedthing.h"
#include "threading/mutex_auto_lock.h"
#include "filesys.h"
#include "gameparams.h"
#include "database/database-dummy.h"
#include "database/database-files.h"
#include "database/database-sqlite3.h"
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif
#if USE_LEVELDB
#include "database/database-leveldb.h"
#endif
#include "irrlicht_changes/printing.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"

// A number that is much smaller than the timeout for particle spawners should/could ever be
#define PARTICLE_SPAWNER_NO_EXPIRY -1024.f

static constexpr s16 ACTIVE_OBJECT_RESAVE_DISTANCE_SQ = sqr(3);

static constexpr u32 BLOCK_RESAVE_TIMESTAMP_DIFF = 60; // in units of game time


/*
	ActiveBlockList
*/

static void fillRadiusBlock(v3s16 p0, s16 r, std::set<v3s16> &list)
{
	v3s16 p;
	for(p.X=p0.X-r; p.X<=p0.X+r; p.X++)
	for(p.Y=p0.Y-r; p.Y<=p0.Y+r; p.Y++)
	for(p.Z=p0.Z-r; p.Z<=p0.Z+r; p.Z++) {
		// limit to a sphere
		if (p.getDistanceFrom(p0) <= r) {
			list.insert(p);
		}
	}
}

static void fillViewConeBlock(v3s16 p0,
	const s16 r,
	const v3f camera_pos,
	const v3f camera_dir,
	const float camera_fov,
	std::set<v3s16> &list)
{
	v3s16 p;
	const s16 r_nodes = r * BS * MAP_BLOCKSIZE;
	for (p.X = p0.X - r; p.X <= p0.X+r; p.X++)
	for (p.Y = p0.Y - r; p.Y <= p0.Y+r; p.Y++)
	for (p.Z = p0.Z - r; p.Z <= p0.Z+r; p.Z++) {
		if (isBlockInSight(p, camera_pos, camera_dir, camera_fov, r_nodes)) {
			list.insert(p);
		}
	}
}

void ActiveBlockList::update(std::vector<PlayerSAO*> &active_players,
	s16 active_block_range,
	s16 active_object_range,
	std::set<v3s16> &blocks_removed,
	std::set<v3s16> &blocks_added,
	std::set<v3s16> &extra_blocks_added)
{
	/*
		Create the new list
	*/
	std::set<v3s16> newlist = m_forceloaded_list;
	std::set<v3s16> extralist;
	for (const PlayerSAO *playersao : active_players) {
		v3s16 pos = getNodeBlockPos(floatToInt(playersao->getBasePosition(), BS));
		fillRadiusBlock(pos, active_block_range, newlist);

		s16 player_ao_range = std::min(active_object_range, playersao->getWantedRange());
		// only do this if this would add blocks
		if (player_ao_range > active_block_range) {
			v3f camera_dir = v3f(0,0,1);
			camera_dir.rotateYZBy(playersao->getLookPitch());
			camera_dir.rotateXZBy(playersao->getRotation().Y);
			if (playersao->getCameraInverted())
				camera_dir = -camera_dir;
			fillViewConeBlock(pos,
				player_ao_range,
				playersao->getEyePosition(),
				camera_dir,
				playersao->getFov(),
				extralist);
		}
	}

	m_abm_list = newlist;

	// 1. Find out which blocks on the new list are not on the old list
	std::set_difference(newlist.begin(), newlist.end(), m_list.begin(), m_list.end(),
			std::inserter(blocks_added, blocks_added.end()));

	// 2. remove duplicate blocks from the extra list
	for (v3s16 p : newlist) {
		extralist.erase(p);
	}

	// 3. Find out which blocks on the extra list are not on the old list
	std::set_difference(extralist.begin(), extralist.end(), m_list.begin(), m_list.end(),
			std::inserter(extra_blocks_added, extra_blocks_added.end()));

	// 4. make sure newlist has all new block
	newlist.insert(extralist.begin(), extralist.end());

	// 5. Find out which blocks on the old list are not on the new + extra list
	std::set_difference(m_list.begin(), m_list.end(), newlist.begin(), newlist.end(),
			std::inserter(blocks_removed, blocks_removed.end()));

	/*
		Do some least-effort sanity checks to hopefully catch code bugs.
	*/
	assert(newlist.size() >= extralist.size());
	assert(newlist.size() >= m_abm_list.size());
	assert(blocks_removed.size() <= m_list.size());
	if (!blocks_added.empty()) {
		assert(newlist.count(*blocks_added.begin()) > 0);
		assert(blocks_removed.count(*blocks_added.begin()) == 0);
	}
	if (!extra_blocks_added.empty()) {
		assert(newlist.count(*extra_blocks_added.begin()) > 0);
		assert(extralist.count(*extra_blocks_added.begin()) > 0);
		assert(blocks_added.count(*extra_blocks_added.begin()) == 0);
	}
	if (!blocks_removed.empty()) {
		assert(newlist.count(*blocks_removed.begin()) == 0);
		assert(extralist.count(*blocks_removed.begin()) == 0);
		assert(m_list.count(*blocks_removed.begin()) > 0);
	}

	// Update m_list
	m_list = std::move(newlist);
}

/*
	OnMapblocksChangedReceiver
*/

void OnMapblocksChangedReceiver::onMapEditEvent(const MapEditEvent &event)
{
	assert(receiving);
	for (const v3s16 &p : event.modified_blocks) {
		modified_blocks.insert(p);
	}
}

/*
	ServerEnvironment
*/

ServerEnvironment::ServerEnvironment(std::unique_ptr<ServerMap> map,
		Server *server, MetricsBackend *mb):
	Environment(server),
	m_map(std::move(map)),
	m_script(server->getScriptIface()),
	m_server(server)
{
	m_cache_active_block_mgmt_interval = g_settings->getFloat("active_block_mgmt_interval");
	m_cache_abm_interval = rangelim(g_settings->getFloat("abm_interval"), 0.1f, 30);
	m_cache_nodetimer_interval = rangelim(g_settings->getFloat("nodetimer_interval"), 0.1f, 1);
	m_cache_abm_time_budget = g_settings->getFloat("abm_time_budget");

	m_step_time_counter = mb->addCounter(
		"minetest_env_step_time", "Time spent in environment step (in microseconds)");

	m_active_block_gauge = mb->addGauge(
		"minetest_env_active_blocks", "Number of active blocks");

	m_active_object_gauge = mb->addGauge(
		"minetest_env_active_objects", "Number of active objects");
}

void ServerEnvironment::init()
{
	// Determine which database backend to use
	const std::string world_path = m_server->getWorldPath();
	const std::string conf_path = world_path + DIR_DELIM "world.mt";
	Settings conf;

	std::string player_backend_name = "sqlite3";
	std::string auth_backend_name = "sqlite3";

	bool succeeded = conf.readConfigFile(conf_path.c_str());

	// If we open world.mt read the backend configurations.
	if (succeeded) {
		// Check that the world's blocksize matches the compiled MAP_BLOCKSIZE
		u16 blocksize = 16;
		conf.getU16NoEx("blocksize", blocksize);
		if (blocksize != MAP_BLOCKSIZE) {
			throw BaseException("The map's blocksize is not supported.");
		}

		// Read those values before setting defaults
		bool player_backend_exists = conf.exists("player_backend");
		bool auth_backend_exists = conf.exists("auth_backend");

		// player backend is not set, assume it's legacy file backend.
		if (!player_backend_exists) {
			// fall back to files
			conf.set("player_backend", "files");
			player_backend_name = "files";

			if (!conf.updateConfigFile(conf_path.c_str())) {
				errorstream << "ServerEnvironment::ServerEnvironment(): "
						<< "Failed to update world.mt!" << std::endl;
			}
		} else {
			conf.getNoEx("player_backend", player_backend_name);
		}

		// auth backend is not set, assume it's legacy file backend.
		if (!auth_backend_exists) {
			conf.set("auth_backend", "files");
			auth_backend_name = "files";

			if (!conf.updateConfigFile(conf_path.c_str())) {
				errorstream << "ServerEnvironment::ServerEnvironment(): "
						<< "Failed to update world.mt!" << std::endl;
			}
		} else {
			conf.getNoEx("auth_backend", auth_backend_name);
		}
	}

	if (player_backend_name == "files") {
		warningstream << "/!\\ You are using old player file backend. "
				<< "This backend is deprecated and will be removed in a future release /!\\"
				<< std::endl << "Switching to SQLite3 or PostgreSQL is advised, "
				<< "please read https://docs.luanti.org/for-server-hosts/database-backends." << std::endl;
	}

	if (auth_backend_name == "files") {
		warningstream << "/!\\ You are using old auth file backend. "
				<< "This backend is deprecated and will be removed in a future release /!\\"
				<< std::endl << "Switching to SQLite3 is advised, "
				<< "please read https://docs.luanti.org/for-server-hosts/database-backends." << std::endl;
	}

	m_player_database = openPlayerDatabase(player_backend_name, world_path, conf);
	m_auth_database = openAuthDatabase(auth_backend_name, world_path, conf);

	if (m_map && m_script->has_on_mapblocks_changed()) {
		m_map->addEventReceiver(&m_on_mapblocks_changed_receiver);
		m_on_mapblocks_changed_receiver.receiving = true;
	}
}

void ServerEnvironment::deactivateBlocksAndObjects()
{
	// Clear active block list.
	// This makes the next one delete all active objects.
	m_active_blocks.clear();

	deactivateFarObjects(true);
}

ServerEnvironment::~ServerEnvironment()
{
	assert(m_active_blocks.size() == 0); // deactivateBlocksAndObjects does this

	// Drop/delete map
	m_map.reset();

	// Delete ActiveBlockModifiers
	for (ABMWithState &m_abm : m_abms) {
		delete m_abm.abm;
	}

	// Deallocate players
	for (RemotePlayer *m_player : m_players) {
		delete m_player;
	}

	delete m_player_database;
	delete m_auth_database;
}

Map & ServerEnvironment::getMap()
{
	return *m_map;
}

ServerMap & ServerEnvironment::getServerMap()
{
	return *m_map;
}

RemotePlayer *ServerEnvironment::getPlayer(const session_t peer_id)
{
	for (RemotePlayer *player : m_players) {
		if (player->getPeerId() == peer_id)
			return player;
	}
	return NULL;
}

RemotePlayer *ServerEnvironment::getPlayer(const std::string &name, bool match_invalid_peer)
{
	for (RemotePlayer *player : m_players) {
		if (player->getName() != name)
			continue;

		if (match_invalid_peer || player->getPeerId() != PEER_ID_INEXISTENT)
			return player;
		break;
	}
	return nullptr;
}

void ServerEnvironment::addPlayer(RemotePlayer *player)
{
	/*
		Check that peer_ids are unique.
		Also check that names are unique.
		Exception: there can be multiple players with peer_id=0
	*/
	// If peer id is non-zero, it has to be unique.
	if (player->getPeerId() != PEER_ID_INEXISTENT)
		FATAL_ERROR_IF(getPlayer(player->getPeerId()) != NULL, "Peer id not unique");
	// Name has to be unique.
	FATAL_ERROR_IF(getPlayer(player->getName()) != NULL, "Player name not unique");
	// Add.
	m_players.push_back(player);
}

void ServerEnvironment::removePlayer(RemotePlayer *player)
{
	for (auto it = m_players.begin(); it != m_players.end(); ++it) {
		if ((*it) == player) {
			delete *it;
			m_players.erase(it);
			return;
		}
	}
}

bool ServerEnvironment::removePlayerFromDatabase(const std::string &name)
{
	return m_player_database->removePlayer(name);
}

void ServerEnvironment::saveLoadedPlayers(bool force)
{
	for (RemotePlayer *player : m_players) {
		if (force || player->checkModified() || (player->getPlayerSAO() &&
				player->getPlayerSAO()->getMeta().isModified())) {
			try {
				m_player_database->savePlayer(player);
			} catch (DatabaseException &e) {
				errorstream << "Failed to save player " << player->getName() << " exception: "
					<< e.what() << std::endl;
				throw;
			}
		}
	}
}

void ServerEnvironment::savePlayer(RemotePlayer *player)
{
	try {
		m_player_database->savePlayer(player);
	} catch (DatabaseException &e) {
		errorstream << "Failed to save player " << player->getName() << " exception: "
			<< e.what() << std::endl;
		throw;
	}
}

std::unique_ptr<PlayerSAO> ServerEnvironment::loadPlayer(RemotePlayer *player, session_t peer_id)
{
	auto playersao = std::make_unique<PlayerSAO>(this, player, peer_id, m_server->isSingleplayer());
	// Create player if it doesn't exist
	if (!m_player_database->loadPlayer(player, playersao.get())) {
		playersao->setNewPlayer();

		// Set player position
		infostream << "Server: Finding spawn place for player \""
			<< player->getName() << "\"" << std::endl;
		playersao->setBasePosition(m_server->findSpawnPos());

		// Make sure the player is saved
		player->setModified(true);
	} else {
		// If the player exists, ensure that they respawn inside legal bounds
		// This fixes an assert crash when the player can't be added
		// to the environment
		if (objectpos_over_limit(playersao->getBasePosition())) {
			actionstream << "Respawn position for player \""
				<< player->getName() << "\" outside limits, resetting" << std::endl;
			playersao->setBasePosition(m_server->findSpawnPos());
		}
	}

	// Update active blocks quickly for a bit so objects in those blocks appear on the client
	m_fast_active_block_divider = 10;

	return playersao;
}

void ServerEnvironment::saveMeta()
{
	if (!m_meta_loaded)
		return;

	std::string path = m_server->getWorldPath() + DIR_DELIM "env_meta.txt";

	// Open file and serialize
	std::ostringstream ss(std::ios_base::binary);

	Settings args("EnvArgsEnd");
	args.setU64("game_time", m_game_time);
	args.setU64("time_of_day", getTimeOfDay());
	args.setU64("last_clear_objects_time", m_last_clear_objects_time);
	args.setU64("lbm_introduction_times_version", 1);
	args.set("lbm_introduction_times",
		m_lbm_mgr.createIntroductionTimesString());
	args.setU64("day_count", m_day_count);
	args.writeLines(ss);

	if(!fs::safeWriteToFile(path, ss.str()))
	{
		infostream<<"ServerEnvironment::saveMeta(): Failed to write "
			<<path<<std::endl;
		throw SerializationError("Couldn't save env meta");
	}
}

void ServerEnvironment::loadMeta()
{
	SANITY_CHECK(!m_meta_loaded);
	m_meta_loaded = true;

	// This has nothing to do with this method but it's nice to know
	infostream << "ServerEnvironment: " << m_abms.size() << " ABMs are registered" << std::endl;

	std::string path = m_server->getWorldPath() + DIR_DELIM "env_meta.txt";

	// If file doesn't exist, load default environment metadata
	if (!fs::PathExists(path)) {
		loadDefaultMeta();
		return;
	}

	infostream << "ServerEnvironment: Loading environment metadata from file" << std::endl;

	// Open file and deserialize
	auto is = open_ifstream(path.c_str(), true);
	if (!is.good())
		throw SerializationError("Couldn't load env meta");

	Settings args("EnvArgsEnd");
	if (!args.parseConfigLines(is)) {
		throw SerializationError("ServerEnvironment::loadMeta(): "
			"EnvArgsEnd not found!");
	}

	try {
		m_game_time = args.getU64("game_time");
	} catch (SettingNotFoundException &e) {
		// Getting this is crucial, otherwise timestamps are useless
		throw SerializationError("Couldn't read game_time from env meta");
	}

	setTimeOfDay(args.exists("time_of_day") ?
		// if it's missing for some reason, set early morning
		args.getU64("time_of_day") : 5250);

	m_last_clear_objects_time = args.exists("last_clear_objects_time") ?
		// If missing, do as if clearObjects was never called
		args.getU64("last_clear_objects_time") : 0;

	m_day_count = args.exists("day_count") ? args.getU32("day_count") : 0;

	std::string lbm_introduction_times;
	try {
		u32 ver = args.getU32("lbm_introduction_times_version");
		if (ver == 1) {
			lbm_introduction_times = args.get("lbm_introduction_times");
		} else {
			warningstream << "ServerEnvironment::loadMeta(): Unsupported"
				<< " introduction time version " << ver << std::endl;
		}
	} catch (SettingNotFoundException &e) {
		// No problem, this is expected. Just continue with an empty string
	}
	m_lbm_mgr.loadIntroductionTimes(lbm_introduction_times, m_server, m_game_time);
}

/**
 * called if env_meta.txt doesn't exist (e.g. new world)
 */
void ServerEnvironment::loadDefaultMeta()
{
	infostream << "ServerEnvironment: Using default environment metadata"
		<< std::endl;
	m_lbm_mgr.loadIntroductionTimes("", m_server, m_game_time);
}

void ServerEnvironment::forceActivateBlock(MapBlock *block)
{
	assert(block);
	if (m_active_blocks.add(block->getPos()))
		activateBlock(block);
	m_active_block_gauge->set(m_active_blocks.size());
}

void ServerEnvironment::activateBlock(MapBlock *block)
{
	// Reset usage timer immediately, otherwise a block that becomes active
	// again at around the same time as it would normally be unloaded will
	// get unloaded incorrectly. (I think this still leaves a small possibility
	// of a race condition between this and server::AsyncRunStep, which only
	// some kind of synchronisation will fix, but it at least reduces the window
	// of opportunity for it to break from seconds to nanoseconds)
	block->resetUsageTimer();

	// Get time difference
	u32 dtime_s = 0;
	u32 stamp = block->getTimestamp();
	if (m_game_time > stamp && stamp != BLOCK_TIMESTAMP_UNDEFINED)
		dtime_s = m_game_time - stamp;

	// Remove stored static objects if clearObjects was called since block's timestamp
	// Note that non-generated blocks may still have stored static objects
	if (stamp != BLOCK_TIMESTAMP_UNDEFINED && stamp < m_last_clear_objects_time) {
		block->m_static_objects.clearStored();
		// do not set changed flag to avoid unnecessary mapblock writes
	}

	// Set current time as timestamp
	block->setTimestampNoChangedFlag(m_game_time);

	// Activate stored objects
	activateObjects(block, dtime_s);
	if (block->isOrphan())
		return;

	/* Handle LoadingBlockModifiers */
	m_lbm_mgr.applyLBMs(this, block, stamp, (float)dtime_s);
	if (block->isOrphan())
		return;

	// Run node timers
	block->step((float)dtime_s, [&](v3s16 p, MapNode n, NodeTimer t) -> bool {
		return m_script->node_on_timer(p, n, t.elapsed, t.timeout);
	});
}

void ServerEnvironment::addActiveBlockModifier(ActiveBlockModifier *abm)
{
	m_abms.emplace_back(abm);
}

void ServerEnvironment::addLoadingBlockModifierDef(LoadingBlockModifierDef *lbm)
{
	m_lbm_mgr.addLBMDef(lbm);
}

bool ServerEnvironment::setNode(v3s16 p, const MapNode &n)
{
	const NodeDefManager *ndef = m_server->ndef();
	MapNode n_old = m_map->getNode(p);

	const ContentFeatures &cf_old = ndef->get(n_old);

	// Call destructor
	if (cf_old.has_on_destruct)
		m_script->node_on_destruct(p, n_old);

	// Replace node
	if (!m_map->addNodeWithEvent(p, n))
		return false;

	// Update active VoxelManipulator if a mapgen thread
	m_map->updateVManip(p);

	// Call post-destructor
	if (cf_old.has_after_destruct)
		m_script->node_after_destruct(p, n_old);

	// Retrieve node content features
	// if new node is same as old, reuse old definition to prevent a lookup
	const ContentFeatures &cf_new = n_old == n ? cf_old : ndef->get(n);

	// Call constructor
	if (cf_new.has_on_construct)
		m_script->node_on_construct(p, n);

	return true;
}

bool ServerEnvironment::removeNode(v3s16 p)
{
	const NodeDefManager *ndef = m_server->ndef();
	MapNode n_old = m_map->getNode(p);

	// Call destructor
	if (ndef->get(n_old).has_on_destruct)
		m_script->node_on_destruct(p, n_old);

	// Replace with air
	// This is slightly optimized compared to addNodeWithEvent(air)
	if (!m_map->removeNodeWithEvent(p))
		return false;

	// Update active VoxelManipulator if a mapgen thread
	m_map->updateVManip(p);

	// Call post-destructor
	if (ndef->get(n_old).has_after_destruct)
		m_script->node_after_destruct(p, n_old);

	// Air doesn't require constructor
	return true;
}

bool ServerEnvironment::swapNode(v3s16 p, const MapNode &n)
{
	if (!m_map->addNodeWithEvent(p, n, false))
		return false;

	// Update active VoxelManipulator if a mapgen thread
	m_map->updateVManip(p);

	return true;
}

u8 ServerEnvironment::findSunlight(v3s16 pos) const
{
	// Directions for neighboring nodes with specified order
	static const v3s16 dirs[] = {
		v3s16(-1, 0, 0), v3s16(1, 0, 0), v3s16(0, 0, -1), v3s16(0, 0, 1),
		v3s16(0, -1, 0), v3s16(0, 1, 0)
	};

	const NodeDefManager *ndef = m_server->ndef();

	// found_light remembers the highest known sunlight value at pos
	u8 found_light = 0;

	struct stack_entry {
		v3s16 pos;
		s16 dist;
	};
	std::stack<stack_entry> stack;
	stack.push({pos, 0});

	std::unordered_map<s64, s8> dists;
	dists[MapDatabase::getBlockAsInteger(pos)] = 0;

	while (!stack.empty()) {
		struct stack_entry e = stack.top();
		stack.pop();

		v3s16 currentPos = e.pos;
		s8 dist = e.dist + 1;

		for (const v3s16& off : dirs) {
			v3s16 neighborPos = currentPos + off;
			s64 neighborHash = MapDatabase::getBlockAsInteger(neighborPos);

			// Do not walk neighborPos multiple times unless the distance to the start
			// position is shorter
			auto it = dists.find(neighborHash);
			if (it != dists.end() && dist >= it->second)
				continue;

			// Position to walk
			bool is_position_ok;
			MapNode node = m_map->getNode(neighborPos, &is_position_ok);
			if (!is_position_ok) {
				// This happens very rarely because the map at currentPos is loaded
				m_map->emergeBlock(neighborPos, false);
				node = m_map->getNode(neighborPos, &is_position_ok);
				if (!is_position_ok)
					continue; // not generated
			}

			const ContentFeatures &def = ndef->get(node);
			if (!def.sunlight_propagates) {
				// Do not test propagation here again
				dists[neighborHash] = -1;
				continue;
			}

			// Sunlight could have come from here
			dists[neighborHash] = dist;
			u8 daylight = node.param1 & 0x0f;

			// In the special case where sunlight shines from above and thus
			// does not decrease with upwards distance, daylight is always
			// bigger than nightlight, which never reaches 15
			int possible_finlight = daylight - dist;
			if (possible_finlight <= found_light) {
				// Light from here cannot make a brighter light at currentPos than
				// found_light
				continue;
			}

			u8 nightlight = node.param1 >> 4;
			if (daylight > nightlight) {
				// Found a valid daylight
				found_light = possible_finlight;
			} else {
				// Sunlight may be darker, so walk the neighbors
				stack.push({neighborPos, dist});
			}
		}
	}
	return found_light;
}

void ServerEnvironment::clearObjects(ClearObjectsMode mode)
{
	infostream << "ServerEnvironment::clearObjects(): "
		<< "Removing all active objects" << std::endl;
	auto cb_removal = [this] (ServerActiveObject *obj, u16 id) {
		if (obj->getType() == ACTIVEOBJECT_TYPE_PLAYER)
			return false;

		// Delete static object if block is loaded
		deleteStaticFromBlock(obj, id, MOD_REASON_CLEAR_ALL_OBJECTS, true);
		obj->markForRemoval();

		// If known by some client, don't delete immediately
		if (obj->m_known_by_count > 0)
			return false;

		processActiveObjectRemove(obj);

		// Delete active object
		return true;
	};

	m_ao_manager.clearIf(cb_removal);

	// Get list of loaded blocks
	std::vector<v3s16> loaded_blocks;
	infostream << "ServerEnvironment::clearObjects(): "
		<< "Listing all loaded blocks" << std::endl;
	m_map->listAllLoadedBlocks(loaded_blocks);
	infostream << "ServerEnvironment::clearObjects(): "
		<< "Done listing all loaded blocks: "
		<< loaded_blocks.size()<<std::endl;

	// Get list of loadable blocks
	std::vector<v3s16> loadable_blocks;
	if (mode == CLEAR_OBJECTS_MODE_FULL) {
		infostream << "ServerEnvironment::clearObjects(): "
			<< "Listing all loadable blocks" << std::endl;
		m_map->listAllLoadableBlocks(loadable_blocks);
		infostream << "ServerEnvironment::clearObjects(): "
			<< "Done listing all loadable blocks: "
			<< loadable_blocks.size() << std::endl;
	} else {
		loadable_blocks = loaded_blocks;
	}

	actionstream << "ServerEnvironment::clearObjects(): "
		<< "Now clearing objects in " << loadable_blocks.size()
		<< " blocks" << std::endl;

	// Grab a reference on each loaded block to avoid unloading it
	for (v3s16 p : loaded_blocks) {
		MapBlock *block = m_map->getBlockNoCreateNoEx(p);
		assert(block != NULL);
		block->refGrab();
	}

	// Remove objects in all loadable blocks
	u32 unload_interval = U32_MAX;
	if (mode == CLEAR_OBJECTS_MODE_FULL) {
		unload_interval = g_settings->getS32("max_clearobjects_extra_loaded_blocks");
		unload_interval = MYMAX(unload_interval, 1);
	}
	u32 report_interval = loadable_blocks.size() / 10;
	u32 num_blocks_checked = 0;
	u32 num_blocks_cleared = 0;
	u32 num_objs_cleared = 0;
	for (auto i = loadable_blocks.begin();
		i != loadable_blocks.end(); ++i) {
		v3s16 p = *i;
		MapBlock *block = m_map->emergeBlock(p, false);
		if (!block) {
			errorstream << "ServerEnvironment::clearObjects(): "
				<< "Failed to emerge block " << p << std::endl;
			continue;
		}

		u32 num_cleared = block->clearObjects();
		if (num_cleared > 0) {
			num_objs_cleared += num_cleared;
			num_blocks_cleared++;
		}
		num_blocks_checked++;

		if (report_interval != 0 &&
			num_blocks_checked % report_interval == 0) {
			float percent = 100.0 * (float)num_blocks_checked /
				loadable_blocks.size();
			actionstream << "ServerEnvironment::clearObjects(): "
				<< "Cleared " << num_objs_cleared << " objects"
				<< " in " << num_blocks_cleared << " blocks ("
				<< percent << "%)" << std::endl;
		}
		if (num_blocks_checked % unload_interval == 0) {
			m_map->unloadUnreferencedBlocks();
		}
	}
	m_map->unloadUnreferencedBlocks();

	// Drop references that were added above
	for (v3s16 p : loaded_blocks) {
		MapBlock *block = m_map->getBlockNoCreateNoEx(p);
		assert(block);
		block->refDrop();
	}

	m_last_clear_objects_time = m_game_time;

	actionstream << "ServerEnvironment::clearObjects(): "
		<< "Finished: Cleared " << num_objs_cleared << " objects"
		<< " in " << num_blocks_cleared << " blocks" << std::endl;
}

void ServerEnvironment::step(float dtime)
{
	ScopeProfiler sp2(g_profiler, "ServerEnv::step()", SPT_AVG);
	const auto start_time = porting::getTimeUs();

	/* Step time of day */
	stepTimeOfDay(dtime);

	// Update this one
	// NOTE: This is kind of funny on a singleplayer game, but doesn't
	// really matter that much.
	static thread_local const float server_step =
			g_settings->getFloat("dedicated_server_step");
	m_recommended_send_interval = server_step;

	/*
		Increment game time
	*/
	{
		m_game_time_fraction_counter += dtime;
		u32 inc_i = (u32)m_game_time_fraction_counter;
		m_game_time += inc_i;
		m_game_time_fraction_counter -= (float)inc_i;
	}

	/*
		Manage active block list
	*/
	if (m_active_blocks_mgmt_interval.step(dtime, m_cache_active_block_mgmt_interval / m_fast_active_block_divider)) {
		ScopeProfiler sp(g_profiler, "ServerEnv: update active blocks", SPT_AVG);

		/*
			Get player block positions
		*/
		std::vector<PlayerSAO*> players;
		players.reserve(m_players.size());
		for (RemotePlayer *player : m_players) {
			// Ignore disconnected players
			if (player->getPeerId() == PEER_ID_INEXISTENT)
				continue;

			PlayerSAO *playersao = player->getPlayerSAO();
			assert(playersao);

			players.push_back(playersao);
		}

		/*
			Update list of active blocks, collecting changes
		*/
		// use active_object_send_range_blocks since that is max distance
		// for active objects sent the client anyway
		static thread_local const s16 active_object_range =
				g_settings->getS16("active_object_send_range_blocks");
		static thread_local const s16 active_block_range =
				g_settings->getS16("active_block_range");
		std::set<v3s16> blocks_removed;
		std::set<v3s16> blocks_added;
		std::set<v3s16> extra_blocks_added;
		m_active_blocks.update(players, active_block_range, active_object_range,
			blocks_removed, blocks_added, extra_blocks_added);

		/*
			Handle removed blocks
		*/

		// Convert active objects that are no more in active blocks to static
		deactivateFarObjects(false);

		for (const v3s16 &p: blocks_removed) {
			MapBlock *block = m_map->getBlockNoCreateNoEx(p);
			if (!block)
				continue;

			// Set current time as timestamp (and let it set ChangedFlag)
			block->setTimestamp(m_game_time);
		}

		/*
			Handle added blocks
		*/

		for (const v3s16 &p: blocks_added) {
			MapBlock *block = m_map->getBlockOrEmerge(p, true);
			if (!block) {
				// TODO: The blocks removed here will only be picked up again
				// on the next cycle. To minimize the latency of objects being
				// activated we could remember the blocks pending activating
				// and activate them instantly as soon as they're loaded.
				m_active_blocks.remove(p);
				continue;
			}

			activateBlock(block);
		}

		for (const v3s16 &p: extra_blocks_added) {
			// only activate if the block is already loaded
			MapBlock *block = m_map->getBlockNoCreateNoEx(p);
			if (!block) {
				m_active_blocks.remove(p);
				continue;
			}

			activateBlock(block);
		}

		// Some blocks may be removed again by the code above so do this here
		m_active_block_gauge->set(m_active_blocks.size());

		if (m_fast_active_block_divider > 1)
			--m_fast_active_block_divider;
	}

	/*
		Mess around in active blocks
	*/
	if (m_active_blocks_nodemetadata_interval.step(dtime, m_cache_nodetimer_interval)) {
		ScopeProfiler sp(g_profiler, "ServerEnv: Run node timers", SPT_AVG);

		// FIXME: this is not actually correct, because the block may have been
		// activated just moments ago. In practice the intervnal is very small
		// so this doesn't really matter.
		const float dtime = m_cache_nodetimer_interval;

		for (const v3s16 &p: m_active_blocks.m_list) {
			MapBlock *block = m_map->getBlockNoCreateNoEx(p);
			if (!block)
				continue;

			// Reset block usage timer
			block->resetUsageTimer();

			// Set current time as timestamp
			block->setTimestampNoChangedFlag(m_game_time);
			// If the block timestamp has changed considerably, mark it to be
			// re-saved. We do this even if there were no actual data changes
			// for the sake of LBMs.
			if (block->getTimestamp() > block->getDiskTimestamp()
				+ BLOCK_RESAVE_TIMESTAMP_DIFF) {
				block->raiseModified(MOD_STATE_WRITE_AT_UNLOAD,
					MOD_REASON_BLOCK_EXPIRED);
			}

			// Run node timers
			block->step(dtime, [&](v3s16 p, MapNode n, NodeTimer t) -> bool {
				return m_script->node_on_timer(p, n, t.elapsed, t.timeout);
			});
		}
	}

	if (m_active_block_modifier_interval.step(dtime, m_cache_abm_interval)) {
		ScopeProfiler sp(g_profiler, "SEnv: modify in blocks avg per interval", SPT_AVG);
		TimeTaker timer("modify in active blocks per interval");

		// Shuffle to prevent persistent artifacts of ordering
		std::shuffle(m_abms.begin(), m_abms.end(), MyRandGenerator());

		// Initialize handling of ActiveBlockModifiers
		// TODO: reinitializing this state every time is probably not efficient?
		ABMHandler abmhandler(m_abms, m_cache_abm_interval, this, true);

		int blocks_scanned = 0;
		int abms_run = 0;
		int blocks_cached = 0;

		std::vector<v3s16> output(m_active_blocks.m_abm_list.size());

		// Shuffle the active blocks so that each block gets an equal chance
		// of having its ABMs run.
		std::copy(m_active_blocks.m_abm_list.begin(), m_active_blocks.m_abm_list.end(), output.begin());
		std::shuffle(output.begin(), output.end(), MyRandGenerator());

		int i = 0;
		// determine the time budget for ABMs
		u32 max_time_ms = m_cache_abm_interval * 1000 * m_cache_abm_time_budget;
		for (const v3s16 &p : output) {
			MapBlock *block = m_map->getBlockNoCreateNoEx(p);
			if (!block)
				continue;

			i++;

			// Set current time as timestamp
			block->setTimestampNoChangedFlag(m_game_time);

			/* Handle ActiveBlockModifiers */
			abmhandler.apply(block, blocks_scanned, abms_run, blocks_cached);

			u32 time_ms = timer.getTimerTime();

			if (time_ms > max_time_ms) {
				warningstream << "active block modifiers took "
					  << time_ms << "ms (processed " << i << " of "
					  << output.size() << " active blocks)" << std::endl;
				break;
			}
		}
		g_profiler->avg("ServerEnv: active blocks", m_active_blocks.m_abm_list.size());
		g_profiler->avg("ServerEnv: active blocks cached", blocks_cached);
		g_profiler->avg("ServerEnv: active blocks scanned for ABMs", blocks_scanned);
		g_profiler->avg("ServerEnv: ABMs run", abms_run);

		timer.stop(true);
	}

	/*
		Step script environment (run global on_step())
	*/
	m_script->environment_Step(dtime);

	m_script->stepAsync();

	/*
		Step active objects
	*/
	{
		ScopeProfiler sp(g_profiler, "ServerEnv: Run SAO::step()", SPT_AVG);

		// This helps the objects to send data at the same time
		bool send_recommended = false;
		m_send_recommended_timer += dtime;
		if (m_send_recommended_timer > getSendRecommendedInterval()) {
			m_send_recommended_timer -= getSendRecommendedInterval();
			send_recommended = true;
		}

		u32 object_count = 0;

		auto cb_state = [&](ServerActiveObject *obj) {
			if (obj->isGone())
				return;
			object_count++;

			// Step object
			obj->step(dtime, send_recommended);
			// Read messages from object
			obj->dumpAOMessagesToQueue(m_active_object_messages);
		};
		m_ao_manager.step(dtime, cb_state);

		m_active_object_gauge->set(object_count);
	}

	/*
		Manage active objects
	*/
	if (m_object_management_interval.step(dtime, 0.5)) {
		removeRemovedObjects();
	}

	/*
		Manage particle spawner expiration
	*/
	if (m_particle_management_interval.step(dtime, 1.0)) {
		for (auto i = m_particle_spawners.begin(); i != m_particle_spawners.end(); ) {
			// non expiring spawners
			if (i->second == PARTICLE_SPAWNER_NO_EXPIRY) {
				++i;
				continue;
			}

			i->second -= 1.0f;
			if (i->second <= 0.f)
				i = m_particle_spawners.erase(i);
			else
				++i;
		}
	}

	// Send outdated player inventories
	for (RemotePlayer *player : m_players) {
		if (player->getPeerId() == PEER_ID_INEXISTENT)
			continue;

		if (player->inventory.checkModified())
			m_server->SendInventory(player, true);
	}

	// Send outdated detached inventories
	if (!m_players.empty())
		m_server->sendDetachedInventories(PEER_ID_INEXISTENT, true);

	// Notify mods of modified mapblocks
	if (m_on_mapblocks_changed_receiver.receiving &&
			!m_on_mapblocks_changed_receiver.modified_blocks.empty()) {
		std::unordered_set<v3s16> modified_blocks;
		std::swap(modified_blocks, m_on_mapblocks_changed_receiver.modified_blocks);
		m_script->on_mapblocks_changed(modified_blocks);
	}

	const auto end_time = porting::getTimeUs();
	m_step_time_counter->increment(end_time - start_time);
}

ServerEnvironment::BlockStatus ServerEnvironment::getBlockStatus(v3s16 blockpos)
{
	if (m_active_blocks.contains(blockpos))
		return BS_ACTIVE;

	const MapBlock *block = m_map->getBlockNoCreateNoEx(blockpos);
	if (block)
		return BS_LOADED;

	if (m_map->isBlockInQueue(blockpos))
		return BS_EMERGING;

	return BS_UNKNOWN;
}

u32 ServerEnvironment::addParticleSpawner(float exptime)
{
	// Timers with lifetime 0 do not expire
	float time = exptime > 0.f ? exptime : PARTICLE_SPAWNER_NO_EXPIRY;

	u32 free_id = m_particle_spawners_id_last_used;
	do {
		free_id++;
		if (free_id == m_particle_spawners_id_last_used)
			return 0; // full
	} while (free_id == 0 || m_particle_spawners.find(free_id) != m_particle_spawners.end());

	m_particle_spawners_id_last_used = free_id;
	m_particle_spawners[free_id] = time;
	return free_id;
}

u32 ServerEnvironment::addParticleSpawner(float exptime, u16 attached_id)
{
	u32 id = addParticleSpawner(exptime);
	m_particle_spawner_attachments[id] = attached_id;
	if (ServerActiveObject *obj = getActiveObject(attached_id)) {
		obj->attachParticleSpawner(id);
	}
	return id;
}

void ServerEnvironment::deleteParticleSpawner(u32 id, bool remove_from_object)
{
	m_particle_spawners.erase(id);
	const auto &it = m_particle_spawner_attachments.find(id);
	if (it != m_particle_spawner_attachments.end()) {
		u16 obj_id = it->second;
		ServerActiveObject *sao = getActiveObject(obj_id);
		if (sao != NULL && remove_from_object) {
			sao->detachParticleSpawner(id);
		}
		m_particle_spawner_attachments.erase(id);
	}
}

u16 ServerEnvironment::addActiveObject(std::unique_ptr<ServerActiveObject> object)
{
	assert(object);	// Pre-condition
	m_added_objects++;
	u16 id = addActiveObjectRaw(std::move(object), nullptr, 0);
	return id;
}

void ServerEnvironment::invalidateActiveObjectObserverCaches()
{
	m_ao_manager.invalidateActiveObjectObserverCaches();
}

/*
	Finds out what new objects have been added to
	inside a radius around a position
*/
void ServerEnvironment::getAddedActiveObjects(PlayerSAO *playersao, s16 radius,
	s16 player_radius,
	const std::set<u16> &current_objects,
	std::vector<u16> &added_objects)
{
	f32 radius_f = radius * BS;
	f32 player_radius_f = player_radius * BS;

	if (player_radius_f < 0.0f)
		player_radius_f = 0.0f;

	if (!playersao->isEffectivelyObservedBy(playersao->getPlayer()->getName()))
		throw ModError("Player does not observe itself");

	m_ao_manager.getAddedActiveObjectsAroundPos(
		playersao->getBasePosition(), playersao->getPlayer()->getName(),
		radius_f, player_radius_f,
		current_objects, added_objects);
}

/*
	Finds out what objects have been removed from
	inside a radius around a position
*/
void ServerEnvironment::getRemovedActiveObjects(PlayerSAO *playersao, s16 radius,
	s16 player_radius,
	const std::set<u16> &current_objects,
	std::vector<std::pair<bool /* gone? */, u16>> &removed_objects)
{
	f32 radius_f = radius * BS;
	f32 player_radius_f = player_radius * BS;

	if (player_radius_f < 0)
		player_radius_f = 0;

	const std::string &player_name = playersao->getPlayer()->getName();

	if (!playersao->isEffectivelyObservedBy(player_name))
		throw ModError("Player does not observe itself");

	/*
		Go through current_objects; object is removed if:
		- object is not found in m_active_objects (this is actually an
		  error condition; objects should be removed only after all clients
		  have been informed about removal), or
		- object is to be removed or deactivated, or
		- object is too far away, or
		- object is marked as not observable by the client
	*/
	for (u16 id : current_objects) {
		ServerActiveObject *object = getActiveObject(id);

		if (!object) {
			warningstream << FUNCTION_NAME << ": found NULL object id="
				<< (int)id << std::endl;
			removed_objects.emplace_back(true, id);
			continue;
		}

		if (object->isGone()) {
			removed_objects.emplace_back(true, id);
			continue;
		}

		f32 distance_f = object->getBasePosition().getDistanceFrom(playersao->getBasePosition());
		bool in_range = object->getType() == ACTIVEOBJECT_TYPE_PLAYER
			? distance_f <= player_radius_f || player_radius_f == 0
			: distance_f <= radius_f;

		if (!in_range || !object->isEffectivelyObservedBy(player_name))
			removed_objects.emplace_back(false, id); // out of range or not observed anymore
	}
}

void ServerEnvironment::setStaticForActiveObjectsInBlock(
	v3s16 blockpos, bool static_exists, v3s16 static_block)
{
	MapBlock *block = m_map->getBlockNoCreateNoEx(blockpos);
	if (!block)
		return;

	for (auto &so_it : block->m_static_objects.getAllActives()) {
		// Get the ServerActiveObject counterpart to this StaticObject
		ServerActiveObject *sao = m_ao_manager.getActiveObject(so_it.first);
		if (!sao) {
			// If this ever happens, there must be some kind of nasty bug.
			errorstream << "ServerEnvironment::setStaticForObjectsInBlock(): "
				"Object from MapBlock::m_static_objects::m_active not found "
				"in m_active_objects";
			continue;
		}

		sao->m_static_exists = static_exists;
		sao->m_static_block  = static_block;
	}
}

bool ServerEnvironment::getActiveObjectMessage(ActiveObjectMessage *dest)
{
	if (m_active_object_messages.empty())
		return false;

	*dest = std::move(m_active_object_messages.front());
	m_active_object_messages.pop();
	return true;
}

void ServerEnvironment::getSelectedActiveObjects(
	const core::line3d<f32> &shootline_on_map,
	std::vector<PointedThing> &objects,
	const std::optional<Pointabilities> &pointabilities)
{
	const v3f line_vector = shootline_on_map.getVector();

	auto process = [&] (ServerActiveObject *obj) -> bool {
		if (obj->isGone())
			return false;
		aabb3f selection_box{{0.0f, 0.0f, 0.0f}};
		if (!obj->getSelectionBox(&selection_box))
			return false;

		v3f pos = obj->getBasePosition();
		v3f rel_pos = shootline_on_map.start - pos;

		v3f current_intersection;
		v3f current_normal;
		v3f current_raw_normal;

		ObjectProperties *props = obj->accessObjectProperties();
		bool collision;
		UnitSAO* usao = dynamic_cast<UnitSAO*>(obj);
		if (props->rotate_selectionbox && usao != nullptr) {
			collision = boxLineCollision(selection_box, usao->getTotalRotation(),
				rel_pos, line_vector, &current_intersection, &current_normal, &current_raw_normal);
		} else {
			collision = boxLineCollision(selection_box, rel_pos, line_vector,
				&current_intersection, &current_normal);
			current_raw_normal = current_normal;
		}
		if (!collision)
			return false;

		PointabilityType pointable;
		if (pointabilities) {
			if (LuaEntitySAO* lsao = dynamic_cast<LuaEntitySAO*>(obj)) {
				pointable = pointabilities->matchObject(lsao->getName(),
						usao->getArmorGroups()).value_or(props->pointable);
			} else if (PlayerSAO* psao = dynamic_cast<PlayerSAO*>(obj)) {
				pointable = pointabilities->matchPlayer(psao->getArmorGroups()).value_or(
						props->pointable);
			} else {
				pointable = props->pointable;
			}
		} else {
			pointable = props->pointable;
		}
		if (pointable != PointabilityType::POINTABLE_NOT) {
			current_intersection += pos;
			f32 d_sq = (current_intersection - shootline_on_map.start).getLengthSQ();
			objects.emplace_back(
				(s16) obj->getId(), current_intersection, current_normal,
				current_raw_normal, d_sq, pointable);
		}
		return false;
	};

	aabb3f search_area(shootline_on_map.start, shootline_on_map.end);
	search_area.repair();
	search_area.MinEdge -= 5 * BS;
	search_area.MaxEdge += 5 * BS;

	// Use "logic in callback" pattern to avoid useless vector filling
	std::vector<ServerActiveObject*> tmp;
	getObjectsInArea(tmp, search_area, process);
}

/*
	************ Private methods *************
*/

u16 ServerEnvironment::addActiveObjectRaw(std::unique_ptr<ServerActiveObject> object_u,
	const StaticObject *from_static, u32 dtime_s)
{
	auto object = object_u.get();
	if (!m_ao_manager.registerObject(std::move(object_u))) {
		return 0;
	}

	// Register reference in scripting api (must be done before post-init)
	m_script->addObjectReference(object);
	// Post-initialize object
	// Note that this can change the value of isStaticAllowed() in case of LuaEntitySAO
	object->addedToEnvironment(dtime_s);

	// Activate object
	if (object->m_static_exists)
	{
		sanity_check(from_static);
		/*
		 * Note: Don't check isStaticAllowed() here. If an object has static data
		 * when it shouldn't, we still need to activate it so the static data
		 * can be properly removed.
		 */
		auto blockpos = object->m_static_block;
		MapBlock *block = m_map->emergeBlock(blockpos);
		if (block) {
			block->m_static_objects.setActive(object->getId(), *from_static);
		} else {
			warningstream << "ServerEnvironment::addActiveObjectRaw(): "
				<< "object was supposed to be in block " << blockpos
				<< ", but this block disappeared." << std::endl;
			object->m_static_exists = false;
		}
	}
	// Add static data to block
	else if (object->isStaticAllowed())
	{
		v3f objectpos = object->getBasePosition();
		StaticObject s_obj(object, objectpos);
		// Add to the block where the object is located in
		v3s16 blockpos = getNodeBlockPos(floatToInt(objectpos, BS));
		MapBlock *block = m_map->emergeBlock(blockpos);
		if (block) {
			block->m_static_objects.setActive(object->getId(), s_obj);
			object->m_static_exists = true;
			object->m_static_block = blockpos;

			block->raiseModified(MOD_STATE_WRITE_NEEDED,
				MOD_REASON_ADD_ACTIVE_OBJECT_RAW);
		} else {
			v3s16 p = floatToInt(objectpos, BS);
			errorstream << "ServerEnvironment::addActiveObjectRaw(): "
				<< "could not emerge block " << p << " for storing id="
				<< object->getId() << " statically" << std::endl;
			// clean in case of error
			object->markForRemoval();
			processActiveObjectRemove(object);
			m_ao_manager.removeObject(object->getId());
			return 0;
		}
	}

	return object->getId();
}

/*
	Remove objects that satisfy (isGone() && m_known_by_count==0)
*/
void ServerEnvironment::removeRemovedObjects()
{
	ScopeProfiler sp(g_profiler, "ServerEnvironment::removeRemovedObjects()", SPT_AVG);

	auto clear_cb = [this](ServerActiveObject *obj, u16 id) {
		/*
			We will handle objects marked for removal or deactivation
		*/
		if (!obj->isGone())
			return false;

		/*
			Delete static data from block if removed
		*/
		if (obj->isPendingRemoval())
			deleteStaticFromBlock(obj, id, MOD_REASON_REMOVE_OBJECTS_REMOVE, false);

		// If still known by clients, don't actually remove. On some future
		// invocation this will be 0, which is when removal will continue.
		if (obj->m_known_by_count > 0)
			return false;

		/*
			Move static data from active to stored if deactivated
		*/
		if (!obj->isPendingRemoval() && obj->m_static_exists) {
			if (MapBlock *block = m_map->emergeBlock(obj->m_static_block, false)) {
				if (!block->storeActiveObject(id)) {
					warningstream << "ServerEnvironment::removeRemovedObjects(): "
							<< "id=" << id << " m_static_exists=true but "
							<< "static data doesn't actually exist in "
							<< obj->m_static_block << std::endl;
				}
			} else {
				infostream << "Failed to emerge block from which an object to "
						<< "be deactivated was loaded from. id=" << id << std::endl;
			}
		}

		processActiveObjectRemove(obj);

		// Delete
		return true;
	};

	m_ao_manager.clearIf(clear_cb);
}

static void print_hexdump(std::ostream &o, const std::string &data)
{
	const int linelength = 16;
	for (int l = 0;; l++) {
		int i0 = linelength * l;
		bool at_end = false;
		int thislinelength = linelength;
		if (i0 + thislinelength > (int)data.size()) {
			thislinelength = data.size() - i0;
			at_end = true;
		}
		for (int di = 0; di < linelength; di++) {
			int i = i0 + di;
			char buf[4];
			if (di < thislinelength)
				porting::mt_snprintf(buf, sizeof(buf), "%.2x ", data[i]);
			else
				porting::mt_snprintf(buf, sizeof(buf), "   ");
			o << buf;
		}
		o << " ";
		for (int di = 0; di < thislinelength; di++) {
			int i = i0 + di;
			if (data[i] >= 32)
				o << data[i];
			else
				o << ".";
		}
		o << std::endl;
		if (at_end)
			break;
	}
}

std::unique_ptr<ServerActiveObject> ServerEnvironment::createSAO(ActiveObjectType type,
		v3f pos, const std::string &data)
{
	switch (type) {
		case ACTIVEOBJECT_TYPE_LUAENTITY:
			return std::make_unique<LuaEntitySAO>(this, pos, data);
		default:
			warningstream << "ServerActiveObject: No factory for type=" << type << std::endl;
	}
	return nullptr;
}

/*
	Convert stored objects from blocks near the players to active.
*/
void ServerEnvironment::activateObjects(MapBlock *block, u32 dtime_s)
{
	if (block == NULL)
		return;

	if (!block->onObjectsActivation())
		return;

	// Activate stored objects
	std::vector<StaticObject> new_stored;
	for (const StaticObject &s_obj : block->m_static_objects.getAllStored()) {
		// Create an active object from the data
		std::unique_ptr<ServerActiveObject> obj =
				createSAO((ActiveObjectType)s_obj.type, s_obj.pos, s_obj.data);
		// If couldn't create object, store static data back.
		if (!obj) {
			errorstream << "ServerEnvironment::activateObjects(): "
				<< "failed to create active object from static object "
				<< "in block " << block->getPos()
				<< " type=" << (int)s_obj.type << " data:" << std::endl;
			print_hexdump(verbosestream, s_obj.data);

			new_stored.push_back(s_obj);
			continue;
		}

		obj->m_static_exists = true;
		obj->m_static_block = block->getPos();

		// This will also add the object to the active static list
		bool ok = addActiveObjectRaw(std::move(obj), &s_obj, dtime_s) != 0;
		if (ok) {
			verbosestream << "ServerEnvironment::activateObjects(): "
				<< "activated static object pos=" << (s_obj.pos / BS)
				<< " type=" << (int)s_obj.type << std::endl;
		}

		// callbacks could invalidate this block
		if (block->isOrphan())
			return;
	}

	// Clear stored list
	block->m_static_objects.clearStored();
	// Add leftover failed stuff to stored list
	for (const StaticObject &s_obj : new_stored) {
		block->m_static_objects.pushStored(s_obj);
	}

	/*
		Note: Block hasn't really been modified here.
		The objects have just been activated and moved from the stored
		static list to the active static list.
		As such, the block is essentially the same.
		Thus, do not call block->raiseModified(MOD_STATE_WRITE_NEEDED).
		Otherwise there would be a huge amount of unnecessary I/O.
	*/
}

/*
	Convert objects that are not standing inside active blocks to static.

	If m_known_by_count != 0, active object is not deleted, but static
	data is still updated.

	If force_delete is set, active object is deleted nevertheless. It
	shall only be set so in the destructor of the environment.

	If block wasn't generated (not in memory or on disk),
*/
void ServerEnvironment::deactivateFarObjects(const bool _force_delete)
{
	auto cb_deactivate = [this, _force_delete](ServerActiveObject *obj, u16 id) {
		// force_delete might be overridden per object
		bool force_delete = _force_delete;

		// Do not deactivate if disallowed
		if (!force_delete && !obj->shouldUnload())
			return false;

		// removeRemovedObjects() is responsible for these
		if (!force_delete && obj->isGone())
			return false;

		const v3f &objectpos = obj->getBasePosition();

		// The block in which the object resides in
		v3s16 blockpos_o = getNodeBlockPos(floatToInt(objectpos, BS));

		// If object's static data is stored in a deactivated block or it has moved a bunch
		// then re-save to the block in which the object is now located in.
		// This only applies if the object is in a currently active block, since deactivating
		// is handled by the code further below.
		if (!force_delete && obj->isStaticAllowed() && obj->m_static_exists &&
		   m_active_blocks.contains(blockpos_o) &&
		   (!m_active_blocks.contains(obj->m_static_block) ||
		   blockpos_o.getDistanceFromSQ(obj->m_static_block) >= ACTIVE_OBJECT_RESAVE_DISTANCE_SQ)) {

			// Delete from block where object was located
			deleteStaticFromBlock(obj, id, MOD_REASON_STATIC_DATA_REMOVED, false);

			StaticObject s_obj(obj, objectpos);
			// Save to block where object is located
			saveStaticToBlock(blockpos_o, id, obj, s_obj, MOD_REASON_STATIC_DATA_ADDED);

			return false;
		}

		// If block is still active, don't remove
		bool still_active = obj->isStaticAllowed() ?
			m_active_blocks.contains(blockpos_o) :
			getMap().getBlockNoCreateNoEx(blockpos_o) != nullptr;
		if (!force_delete && still_active)
			return false;

		// If known by some client, don't immediately delete.
		bool pending_delete = (obj->m_known_by_count > 0 && !force_delete);

		verbosestream << "ServerEnvironment::deactivateFarObjects(): "
					  << "deactivating object id=" << id << " on inactive block "
					  << blockpos_o << (pending_delete ? " (pending)" : "")
					  << std::endl;

		/*
			Update the static data
		*/
		if (obj->isStaticAllowed()) {
			// Create new static object
			StaticObject s_obj(obj, objectpos);

			bool stays_in_same_block = false;
			bool data_changed = true;

			// Check if static data has changed considerably
			if (obj->m_static_exists) {
				if (obj->m_static_block == blockpos_o)
					stays_in_same_block = true;

				if (MapBlock *block = m_map->emergeBlock(obj->m_static_block, false)) {
					const auto n = block->m_static_objects.getAllActives().find(id);
					if (n != block->m_static_objects.getAllActives().end()) {
						StaticObject static_old = n->second;

						float save_movem = obj->getMinimumSavedMovement();

						if (static_old.data == s_obj.data &&
							(static_old.pos - objectpos).getLength() < save_movem)
							data_changed = false;
					} else {
						warningstream << "ServerEnvironment::deactivateFarObjects(): "
								<< "id=" << id << " m_static_exists=true but "
								<< "static data doesn't actually exist in "
								<< obj->m_static_block << std::endl;
					}
				}
			}

			/*
				While changes are always saved, blocks are only marked as modified
				if the object has moved or different staticdata. (see above)
			*/
			bool shall_be_written = (!stays_in_same_block || data_changed);
			u32 reason = shall_be_written ? MOD_REASON_STATIC_DATA_CHANGED : MOD_REASON_UNKNOWN;

			// Delete old static object
			deleteStaticFromBlock(obj, id, reason, false);

			// Add to the block where the object is located in
			v3s16 blockpos = getNodeBlockPos(floatToInt(objectpos, BS));
			u16 store_id = pending_delete ? id : 0;
			if (!saveStaticToBlock(blockpos, store_id, obj, s_obj, reason))
				force_delete = true;
		} else {
			// If the object has static data but shouldn't we need to get rid of it.
			deleteStaticFromBlock(obj, id, MOD_REASON_STATIC_DATA_REMOVED, false);
		}

		// Regardless of what happens to the object at this point, deactivate it first.
		// This ensures that LuaEntity on_deactivate is always called.
		obj->markForDeactivation();

		// If known by some client, don't delete yet.
		if (pending_delete && !force_delete)
			return false;

		processActiveObjectRemove(obj);

		// Delete active object
		return true;
	};

	m_ao_manager.clearIf(cb_deactivate);
}

void ServerEnvironment::deleteStaticFromBlock(
		ServerActiveObject *obj, u16 id, u32 mod_reason, bool no_emerge)
{
	if (!obj->m_static_exists)
		return;

	MapBlock *block;
	if (no_emerge)
		block = m_map->getBlockNoCreateNoEx(obj->m_static_block);
	else
		block = m_map->emergeBlock(obj->m_static_block, false);
	if (!block) {
		if (!no_emerge)
			errorstream << "ServerEnv: Failed to emerge block " << obj->m_static_block
					<< " when deleting static data of object from it. id=" << id << std::endl;
		return;
	}

	block->m_static_objects.remove(id);
	if (mod_reason != MOD_REASON_UNKNOWN) // Do not mark as modified if requested
		block->raiseModified(MOD_STATE_WRITE_NEEDED, mod_reason);

	obj->m_static_exists = false;
}

bool ServerEnvironment::saveStaticToBlock(
		v3s16 blockpos, u16 store_id,
		ServerActiveObject *obj, const StaticObject &s_obj,
		u32 mod_reason)
{
	MapBlock *block = nullptr;
	try {
		block = m_map->emergeBlock(blockpos);
	} catch (InvalidPositionException &e) {
		// Handled via NULL pointer
		// NOTE: emergeBlock's failure is usually determined by it
		//       actually returning NULL
	}

	if (!block) {
		errorstream << "ServerEnv: Failed to emerge block " << obj->m_static_block
				<< " when saving static data of object to it. id=" << store_id << std::endl;
		return false;
	}

	if (!block->saveStaticObject(store_id, s_obj, mod_reason))
		return false;

	obj->m_static_exists = true;
	obj->m_static_block = blockpos;

	return true;
}

void ServerEnvironment::processActiveObjectRemove(ServerActiveObject *obj)
{
	// markForRemoval or markForDeactivation should have been called before
	// Not because it's strictly necessary but because the Lua callback is
	// bound to that.
	assert(obj->isGone());

	// Tell the object about removal
	obj->removingFromEnvironment();
	// Deregister in scripting api
	m_script->removeObjectReference(obj);
}

std::vector<std::string> ServerEnvironment::getPlayerDatabaseBackends()
{
	std::vector<std::string> ret;
	ret.emplace_back("sqlite3");
	ret.emplace_back("dummy");
#if USE_POSTGRESQL
	ret.emplace_back("postgresql");
#endif
#if USE_LEVELDB
	ret.emplace_back("leveldb");
#endif
	ret.emplace_back("files");
	return ret;
}

PlayerDatabase *ServerEnvironment::openPlayerDatabase(const std::string &name,
		const std::string &savedir, const Settings &conf)
{

	if (name == "sqlite3")
		return new PlayerDatabaseSQLite3(savedir);

	if (name == "dummy")
		return new Database_Dummy();

#if USE_POSTGRESQL
	if (name == "postgresql") {
		std::string connect_string;
		conf.getNoEx("pgsql_player_connection", connect_string);
		return new PlayerDatabasePostgreSQL(connect_string);
	}
#endif

#if USE_LEVELDB
	if (name == "leveldb")
		return new PlayerDatabaseLevelDB(savedir);
#endif

	if (name == "files")
		return new PlayerDatabaseFiles(savedir + DIR_DELIM + "players");

	throw BaseException(std::string("Database backend ") + name + " not supported.");
}

bool ServerEnvironment::migratePlayersDatabase(const GameParams &game_params,
		const Settings &cmd_args)
{
	std::string migrate_to = cmd_args.get("migrate-players");
	Settings world_mt;
	std::string world_mt_path = game_params.world_path + DIR_DELIM + "world.mt";
	if (!world_mt.readConfigFile(world_mt_path.c_str())) {
		errorstream << "Cannot read world.mt!" << std::endl;
		return false;
	}

	if (!world_mt.exists("player_backend")) {
		errorstream << "Please specify your current backend in world.mt:"
			<< std::endl
			<< "	player_backend = {files|sqlite3|leveldb|postgresql}"
			<< std::endl;
		return false;
	}

	std::string backend = world_mt.get("player_backend");
	if (backend == migrate_to) {
		errorstream << "Cannot migrate: new backend is same"
			<< " as the old one" << std::endl;
		return false;
	}

	const std::string players_backup_path = game_params.world_path + DIR_DELIM
		+ "players.bak";

	if (backend == "files") {
		// Create backup directory
		fs::CreateDir(players_backup_path);
	}

	try {
		PlayerDatabase *srcdb = ServerEnvironment::openPlayerDatabase(backend,
			game_params.world_path, world_mt);
		PlayerDatabase *dstdb = ServerEnvironment::openPlayerDatabase(migrate_to,
			game_params.world_path, world_mt);

		std::vector<std::string> player_list;
		srcdb->listPlayers(player_list);
		for (auto it = player_list.begin();
			it != player_list.end(); ++it) {
			actionstream << "Migrating player " << it->c_str() << std::endl;
			RemotePlayer player(it->c_str(), NULL);
			PlayerSAO playerSAO(NULL, &player, 15000, false);

			srcdb->loadPlayer(&player, &playerSAO);

			playerSAO.finalize(&player, std::set<std::string>());
			player.setPlayerSAO(&playerSAO);

			dstdb->savePlayer(&player);

			// For files source, move player files to backup dir
			if (backend == "files") {
				fs::Rename(
					game_params.world_path + DIR_DELIM + "players" + DIR_DELIM + (*it),
					players_backup_path + DIR_DELIM + (*it));
			}
		}

		actionstream << "Successfully migrated " << player_list.size() << " players"
			<< std::endl;
		world_mt.set("player_backend", migrate_to);
		if (!world_mt.updateConfigFile(world_mt_path.c_str()))
			errorstream << "Failed to update world.mt!" << std::endl;
		else
			actionstream << "world.mt updated" << std::endl;

		// When migration is finished from file backend, remove players directory if empty
		if (backend == "files") {
			fs::DeleteSingleFileOrEmptyDirectory(game_params.world_path + DIR_DELIM
				+ "players");
		}

		delete srcdb;
		delete dstdb;

	} catch (BaseException &e) {
		errorstream << "An error occurred during migration: " << e.what() << std::endl;
		return false;
	}
	return true;
}

std::vector<std::string> ServerEnvironment::getAuthDatabaseBackends()
{
	std::vector<std::string> ret;
	ret.emplace_back("sqlite3");
	ret.emplace_back("dummy");
#if USE_POSTGRESQL
	ret.emplace_back("postgresql");
#endif
	ret.emplace_back("files");
#if USE_LEVELDB
	ret.emplace_back("leveldb");
#endif
	return ret;
}

AuthDatabase *ServerEnvironment::openAuthDatabase(
		const std::string &name, const std::string &savedir, const Settings &conf)
{
	if (name == "sqlite3")
		return new AuthDatabaseSQLite3(savedir);

#if USE_POSTGRESQL
	if (name == "postgresql") {
		std::string connect_string;
		conf.getNoEx("pgsql_auth_connection", connect_string);
		return new AuthDatabasePostgreSQL(connect_string);
	}
#endif

	if (name == "files")
		return new AuthDatabaseFiles(savedir);

#if USE_LEVELDB
	if (name == "leveldb")
		return new AuthDatabaseLevelDB(savedir);
#endif

	throw BaseException(std::string("Database backend ") + name + " not supported.");
}

bool ServerEnvironment::migrateAuthDatabase(
		const GameParams &game_params, const Settings &cmd_args)
{
	std::string migrate_to = cmd_args.get("migrate-auth");
	Settings world_mt;
	std::string world_mt_path = game_params.world_path + DIR_DELIM + "world.mt";
	if (!world_mt.readConfigFile(world_mt_path.c_str())) {
		errorstream << "Cannot read world.mt!" << std::endl;
		return false;
	}

	std::string backend = "files";
	if (world_mt.exists("auth_backend"))
		backend = world_mt.get("auth_backend");
	else
		warningstream << "No auth_backend found in world.mt, "
				"assuming \"files\"." << std::endl;

	if (backend == migrate_to) {
		errorstream << "Cannot migrate: new backend is same"
				<< " as the old one" << std::endl;
		return false;
	}

	try {
		const std::unique_ptr<AuthDatabase> srcdb(ServerEnvironment::openAuthDatabase(
				backend, game_params.world_path, world_mt));
		const std::unique_ptr<AuthDatabase> dstdb(ServerEnvironment::openAuthDatabase(
				migrate_to, game_params.world_path, world_mt));

		std::vector<std::string> names_list;
		srcdb->listNames(names_list);
		for (const std::string &name : names_list) {
			actionstream << "Migrating auth entry for " << name << std::endl;
			bool success;
			AuthEntry authEntry;
			success = srcdb->getAuth(name, authEntry);
			success = success && dstdb->createAuth(authEntry);
			if (!success)
				errorstream << "Failed to migrate " << name << std::endl;
		}

		actionstream << "Successfully migrated " << names_list.size()
				<< " auth entries" << std::endl;
		world_mt.set("auth_backend", migrate_to);
		if (!world_mt.updateConfigFile(world_mt_path.c_str()))
			errorstream << "Failed to update world.mt!" << std::endl;
		else
			actionstream << "world.mt updated" << std::endl;

		if (backend == "files") {
			// special-case files migration:
			// move auth.txt to auth.txt.bak if possible
			std::string auth_txt_path =
					game_params.world_path + DIR_DELIM + "auth.txt";
			std::string auth_bak_path = auth_txt_path + ".bak";
			if (!fs::PathExists(auth_bak_path))
				if (fs::Rename(auth_txt_path, auth_bak_path))
					actionstream << "Renamed auth.txt to auth.txt.bak"
							<< std::endl;
				else
					errorstream << "Could not rename auth.txt to "
							"auth.txt.bak" << std::endl;
			else
				warningstream << "auth.txt.bak already exists, auth.txt "
						"not renamed" << std::endl;
		}

	} catch (BaseException &e) {
		errorstream << "An error occurred during migration: " << e.what()
			    << std::endl;
		return false;
	}
	return true;
}
