// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "server.h"
#include <iostream>
#include <queue>
#include <algorithm>
#include "irr_v2d.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "server/ban.h"
#include "environment.h"
#include "servermap.h"
#include "threading/mutex_auto_lock.h"
#include "constants.h"
#include "voxel.h"
#include "config.h"
#include "version.h"
#include "filesys.h"
#include "mapblock.h"
#include "server/serveractiveobject.h"
#include "serialization.h" // SER_FMT_VER_INVALID
#include "settings.h"
#include "profiler.h"
#include "log.h"
#include "scripting_server.h"
#include "nodedef.h"
#include "itemdef.h"
#include "craftdef.h"
#include "emerge.h"
#include "mapgen/mapgen.h"
#include "mapgen/mg_biome.h"
#include "content_mapnode.h"
#include "content_nodemeta.h"
#include "content/mods.h"
#include "modchannels.h"
#include "server/serverlist.h"
#include "util/string.h"
#include "server/rollback.h"
#include "util/serialize.h"
#include "util/thread.h"
#include "defaultsettings.h"
#include "server/mods.h"
#include "util/base64.h"
#include "util/hashing.h"
#include "util/hex.h"
#include "database/database.h"
#include "chatmessage.h"
#include "chat_interface.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include "server/serverinventorymgr.h"
#include "translation.h"
#include "database/database-sqlite3.h"
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif
#include "database/database-files.h"
#include "database/database-dummy.h"
#include "gameparams.h"
#include "particles.h"
#include "gettext.h"
#include "util/tracy_wrapper.h"

#include <csignal>

class ClientNotFoundException : public BaseException
{
public:
	ClientNotFoundException(const char *s):
		BaseException(s)
	{}
};

ModIPCStore::~ModIPCStore()
{
	// we don't have to do this, it's pure debugging aid
	if (!std::unique_lock(mutex, std::try_to_lock).owns_lock()) {
		errorstream << FUNCTION_NAME << ": lock is still in use!" << std::endl;
		assert(0);
	}
}

class ServerThread : public Thread
{
public:

	ServerThread(Server *server):
		Thread("Server"),
		m_server(server)
	{}

	void *run();

private:
	Server *m_server;
};

void *ServerThread::run()
{
	ZoneScoped;

	BEGIN_DEBUG_EXCEPTION_HANDLER

	/*
	 * The real business of the server happens on the ServerThread.
	 * How this works:
	 * AsyncRunStep() (which runs the actual server step) is called at the
	 * server-step frequency. Receive() is used for waiting between the steps.
	 */

	auto framemarker = FrameMarker("ServerThread::run()-frame").started();
	try {
		m_server->AsyncRunStep(0.0f, true);
	} catch (con::ConnectionBindFailed &e) {
		m_server->setAsyncFatalError(e.what());
	} catch (LuaError &e) {
		m_server->setAsyncFatalError(e);
	} catch (ModError &e) {
		m_server->setAsyncFatalError(e.what());
	}
	framemarker.end();

	float dtime = 0.0f;

	while (!stopRequested()) {
		framemarker.start();
		ScopeProfiler spm(g_profiler, "Server::RunStep() (max)", SPT_MAX);

		u64 t0 = porting::getTimeUs();

		const auto step_settings = m_server->getStepSettings();

		try {
			// see explanation inside
			// (+1 ms, because we don't sleep more fine-grained)
			if (dtime > step_settings.steplen + 0.001f)
				m_server->yieldToOtherThreads(dtime);

			m_server->AsyncRunStep(step_settings.pause ? 0.0f : dtime);

			const float remaining_time = step_settings.steplen
					- 1e-6f * (porting::getTimeUs() - t0);
			m_server->Receive(remaining_time);

		} catch (con::PeerNotFoundException &e) {
			infostream<<"Server: PeerNotFoundException"<<std::endl;
		} catch (ClientNotFoundException &e) {
			infostream<<"Server: ClientNotFoundException"<<std::endl;
		} catch (con::ConnectionBindFailed &e) {
			m_server->setAsyncFatalError(e.what());
		} catch (LuaError &e) {
			m_server->setAsyncFatalError(e);
		} catch (ModError &e) {
			m_server->setAsyncFatalError(e.what());
		}

		dtime = 1e-6f * (porting::getTimeUs() - t0);
		framemarker.end();
	}

	END_DEBUG_EXCEPTION_HANDLER

	return nullptr;
}

v3f ServerPlayingSound::getPos(ServerEnvironment *env, bool *pos_exists) const
{
	if (pos_exists)
		*pos_exists = false;

	switch (type ){
	case SoundLocation::Local:
		return v3f(0,0,0);
	case SoundLocation::Position:
		if (pos_exists)
			*pos_exists = true;
		return pos;
	case SoundLocation::Object:
		{
			if (object == 0)
				return v3f(0,0,0);
			ServerActiveObject *sao = env->getActiveObject(object);
			if (!sao)
				return v3f(0,0,0);
			if (pos_exists)
				*pos_exists = true;
			return sao->getBasePosition();
		}
	}

	return v3f(0,0,0);
}

void Server::ShutdownState::reset()
{
	m_timer = 0.0f;
	message.clear();
	should_reconnect = false;
	is_requested = false;
}

void Server::ShutdownState::trigger(float delay, const std::string &msg, bool reconnect)
{
	m_timer = delay;
	message = msg;
	should_reconnect = reconnect;
}

void Server::ShutdownState::tick(float dtime, Server *server)
{
	if (m_timer <= 0.0f)
		return;

	// Timed shutdown
	static const float shutdown_msg_times[] =
	{
		1, 2, 3, 4, 5, 10, 20, 40, 60, 120, 180, 300, 600, 1200, 1800, 3600
	};

	// Automated messages
	if (m_timer < shutdown_msg_times[ARRLEN(shutdown_msg_times) - 1]) {
		for (float t : shutdown_msg_times) {
			// If shutdown timer matches an automessage, shot it
			if (m_timer > t && m_timer - dtime < t) {
				std::wstring periodicMsg = getShutdownTimerMessage();

				infostream << wide_to_utf8(periodicMsg).c_str() << std::endl;
				server->SendChatMessage(PEER_ID_INEXISTENT, periodicMsg);
				break;
			}
		}
	}

	m_timer -= dtime;
	if (m_timer < 0.0f) {
		m_timer = 0.0f;
		is_requested = true;
	}
}

std::wstring Server::ShutdownState::getShutdownTimerMessage() const
{
	std::wstringstream ws;
	ws << L"*** Server shutting down in "
		<< duration_to_string(myround(m_timer)).c_str() << ".";
	return ws.str();
}

/*
	Server
*/

Server::Server(
		const std::string &path_world,
		const SubgameSpec &gamespec,
		bool simple_singleplayer_mode,
		Address bind_addr,
		bool dedicated,
		ChatInterface *iface,
		std::string *shutdown_errmsg
	):
	m_bind_addr(bind_addr),
	m_path_world(path_world),
	m_gamespec(gamespec),
	m_simple_singleplayer_mode(simple_singleplayer_mode),
	m_dedicated(dedicated),
	m_async_fatal_error(""),
	m_con(con::createMTP(CONNECTION_TIMEOUT, m_bind_addr.isIPv6(), this)),
	m_itemdef(createItemDefManager()),
	m_nodedef(createNodeDefManager()),
	m_craftdef(createCraftDefManager()),
	m_thread(new ServerThread(this)),
	m_clients(m_con),
	m_admin_chat(iface),
	m_shutdown_errmsg(shutdown_errmsg),
	m_modchannel_mgr(new ModChannelMgr())
{
	if (m_path_world.empty())
		throw ServerError("Supplied empty world path");

	if (!gamespec.isValid())
		throw ServerError("Supplied invalid gamespec");

#if USE_PROMETHEUS
	if (!simple_singleplayer_mode) {
		// Note: may return null
		m_metrics_backend.reset(createPrometheusMetricsBackend());
	}
#endif
	if (!m_metrics_backend)
		m_metrics_backend = std::make_unique<MetricsBackend>();

	m_uptime_counter = m_metrics_backend->addCounter("minetest_core_server_uptime", "Server uptime (in seconds)");
	m_player_gauge = m_metrics_backend->addGauge("minetest_core_player_number", "Number of connected players");

	m_timeofday_gauge = m_metrics_backend->addGauge(
			"minetest_core_timeofday",
			"Time of day value");

	m_lag_gauge = m_metrics_backend->addGauge(
			"minetest_core_latency",
			"Latency value (in seconds)");


	const std::string aom_types[] = {"reliable", "unreliable"};
	for (u32 i = 0; i < ARRLEN(aom_types); i++) {
		std::string help_str("Number of active object messages generated (");
		help_str.append(aom_types[i]).append(")");
		m_aom_buffer_counter[i] = m_metrics_backend->addCounter(
				"minetest_core_aom_generated_count", help_str,
				{{"type", aom_types[i]}});
	}

	m_packet_recv_counter = m_metrics_backend->addCounter(
			"minetest_core_server_packet_recv",
			"Processable packets received");

	m_packet_recv_processed_counter = m_metrics_backend->addCounter(
			"minetest_core_server_packet_recv_processed",
			"Valid received packets processed");

	m_map_edit_event_counter = m_metrics_backend->addCounter(
			"minetest_core_map_edit_events",
			"Number of map edit events");

	m_lag_gauge->set(g_settings->getFloat("dedicated_server_step"));

	m_path_mod_data = porting::path_user + DIR_DELIM "mod_data";
	if (!fs::CreateDir(m_path_mod_data))
		throw ServerError("Failed to create mod data dir");
}

Server::~Server()
{
	// Send shutdown message
	SendChatMessage(PEER_ID_INEXISTENT, ChatMessage(CHATMESSAGE_TYPE_ANNOUNCE,
			L"*** Server shutting down"));

	actionstream << "Server: Shutting down" << std::endl;

	// Stop server step from happening
	if (m_thread) {
		stop();
		delete m_thread;
	}

	// Stop all emerge activity and finish off mapgen callbacks. Do this before
	// shutdown callbacks since there may be state that is finalized in a
	// callback.
	// Note: The emerge manager is not deleted yet because further code can
	//       still interact with map loading.
	if (m_emerge)
		m_emerge->stopThreads();

	if (m_env) {
		EnvAutoLock envlock(this);

		infostream << "Server: Executing shutdown hooks" << std::endl;
		try {
			m_script->on_shutdown();
		} catch (ModError &e) {
			addShutdownError(e);
		}

		infostream << "Server: Saving players" << std::endl;
		m_env->saveLoadedPlayers();

		infostream << "Server: Kicking players" << std::endl;
		std::string kick_msg;
		bool reconnect = false;
		if (isShutdownRequested()) {
			reconnect = m_shutdown_state.should_reconnect;
			kick_msg = m_shutdown_state.message;
		}
		if (kick_msg.empty()) {
			kick_msg = g_settings->get("kick_msg_shutdown");
		}
		m_env->saveLoadedPlayers(true);
		kickAllPlayers(SERVER_ACCESSDENIED_SHUTDOWN,
			kick_msg, reconnect);

		try {
			// Empty out the environment, this can also invoke callbacks.
			m_env->deactivateBlocksAndObjects();
		} catch (ModError &e) {
			addShutdownError(e);
		}

		infostream << "Server: Saving environment metadata" << std::endl;
		m_env->saveMeta();

		// Delete classes that depend on the environment
		m_inventory_mgr.reset();
		m_script.reset();

		// Note that this also deletes and saves the map.
		delete m_env;
		m_env = nullptr;
	}

	// Write any changes before deletion.
	if (m_mod_storage_database)
		m_mod_storage_database->endSave();

	// Clean up files
	for (auto &it : m_media) {
		if (it.second.delete_at_shutdown) {
			fs::DeleteSingleFileOrEmptyDirectory(it.second.path);
		}
	}

	// emerge may depend on definition managers, so destroy first
	m_emerge.reset();

	// Delete the rest in the reverse order of creation
	delete m_game_settings;
	delete m_banmanager;
	delete m_mod_storage_database;
	delete m_rollback;
	delete m_itemdef;
	delete m_nodedef;
	delete m_craftdef;

	while (!m_unsent_map_edit_queue.empty()) {
		delete m_unsent_map_edit_queue.front();
		m_unsent_map_edit_queue.pop();
	}
}

void Server::init()
{
	infostream << "Server created for gameid \"" << m_gamespec.id << "\"";
	if (m_simple_singleplayer_mode)
		infostream << " in simple singleplayer mode" << std::endl;
	else
		infostream << std::endl;
	infostream << "- world:  " << m_path_world << std::endl;
	infostream << "- game:   " << m_gamespec.path << std::endl;

	m_game_settings = Settings::createLayer(SL_GAME);

	// Create world if it doesn't exist
	try {
		loadGameConfAndInitWorld(m_path_world,
				fs::GetFilenameFromPath(m_path_world.c_str()),
				m_gamespec, false, "");
	} catch (const BaseException &e) {
		throw ServerError(std::string("Failed to initialize world: ") + e.what());
	}

	// Create emerge manager
	m_emerge = std::make_unique<EmergeManager>(this, m_metrics_backend.get());

	// Create ban manager
	std::string ban_path = m_path_world + DIR_DELIM "ipban.txt";
	m_banmanager = new BanManager(ban_path);

	// Create mod storage database and begin a save for later
	m_mod_storage_database = openModStorageDatabase(m_path_world);
	m_mod_storage_database->beginSave();

	m_modmgr = std::make_unique<ServerModManager>(m_path_world, m_gamespec);

	// complain about mods with unsatisfied dependencies
	if (!m_modmgr->isConsistent()) {
		std::string error = m_modmgr->getUnsatisfiedModsError();
		throw ServerError(error);
	}

	//lock environment
	EnvAutoLock envlock(this);

	// Create the Map (loads map_meta.txt, overriding configured mapgen params)
	std::unique_ptr<ServerMap> startup_server_map;
	try {
		startup_server_map = std::make_unique<ServerMap>(m_path_world, this,
				m_emerge.get(), m_metrics_backend.get());
	} catch (DatabaseException &e) {
		throw ServerError(std::string(
			"Failed to initialize the map database. The world may be "
			"corrupted or in an unsupported format.\n") + e.what());
	}

	// Initialize scripting
	infostream << "Server: Initializing Lua" << std::endl;

	m_script = std::make_unique<ServerScripting>(this);

	// Must be created before mod loading because we have some inventory creation
	m_inventory_mgr = std::make_unique<ServerInventoryManager>();

	m_script->loadBuiltin();

	m_gamespec.checkAndLog();
	m_modmgr->loadMods(*m_script);

	m_script->saveGlobals();

	// Read Textures and calculate sha1 sums
	fillMediaCache();

	// Apply item aliases in the node definition manager
	m_nodedef->updateAliases(m_itemdef);

	// Apply texture overrides from texturepack/override.txt
	std::vector<std::string> paths;
	fs::GetRecursiveDirs(paths, g_settings->get("texture_path"));
	fs::GetRecursiveDirs(paths, m_gamespec.path + DIR_DELIM + "textures");
	for (const std::string &path : paths) {
		TextureOverrideSource override_source(path + DIR_DELIM + "override.txt");
		m_nodedef->applyTextureOverrides(override_source.getNodeTileOverrides());
		m_itemdef->applyTextureOverrides(override_source.getItemTextureOverrides());
	}

	m_nodedef->setNodeRegistrationStatus(true);

	// Perform pending node name resolutions
	m_nodedef->runNodeResolveCallbacks();

	// unmap node names in cross-references
	m_nodedef->resolveCrossrefs();

	// init the recipe hashes to speed up crafting
	m_craftdef->initHashes(this);

	// Initialize Environment
	m_env = new ServerEnvironment(std::move(startup_server_map),
		this, m_metrics_backend.get());
	m_env->init();

	m_inventory_mgr->setEnv(m_env);
	m_clients.setEnv(m_env);

	// Initialize mapgens
	auto &servermap = m_env->getServerMap();
	if (!servermap.settings_mgr.makeMapgenParams())
		FATAL_ERROR("Couldn't create any mapgen type");
	m_emerge->initMapgens(servermap.getMapgenParams());

	if (g_settings->getBool("enable_rollback_recording")) {
		// Create rollback manager
		m_rollback = new RollbackManager(m_path_world, this);
	}

	// Give environment reference to scripting api
	m_script->initializeEnvironment(m_env);

	// Do this after regular script init is done
	m_script->initAsync();

	// Register us to receive map edit events
	servermap.addEventReceiver(this);

	m_env->loadMeta();

	// Those settings can be overwritten in world.mt, they are
	// intended to be cached after environment loading.
	m_liquid_transform_every = g_settings->getFloat("liquid_update");
	m_max_chatmessage_length = g_settings->getU16("chat_message_max_size");
	m_csm_restriction_flags = g_settings->getU64("csm_restriction_flags");
	m_csm_restriction_noderange = g_settings->getU32("csm_restriction_noderange");
}

void Server::start()
{
	init();

	infostream << "Starting server thread..." << std::endl;

	// Stop thread if already running
	m_thread->stop();

	// Initialize connection
	m_con->Serve(m_bind_addr);

	// Start thread
	m_thread->start();

	// ASCII art for the win!
	const char *art[] = {
		R"( _                   _   _ )",
		R"(| |_   _  __ _ _ __ | |_(_))",
		R"(| | | | |/ _` | '_ \| __| |)",
		R"(| | |_| | (_| | | | | |_| |)",
		R"(|_|\__,_|\__,_|_| |_|\__|_|)",
	};
	if (!m_admin_chat) {
		// we're not printing to rawstream to avoid it showing up in the logs.
		// however it would then mess up the ncurses terminal (m_admin_chat),
		// so we skip it in that case.
		for (size_t i = 0; i < ARRLEN(art); ++i)
			std::cerr << art[i] << (i == ARRLEN(art) - 1 ? "" : "\n");
		// add a "tail" with the engine version
		std::cerr << "  ___ " << g_version_hash << std::endl;
	}

	actionstream << "World at [" << m_path_world << "]" << std::endl;
	actionstream << "Server for gameid=\"" << m_gamespec.id
			<< "\" listening on ";
	m_bind_addr.print(actionstream);
	actionstream << "." << std::endl;
}

void Server::stop()
{
	infostream<<"Server: Stopping and waiting for threads"<<std::endl;

	// Stop threads (set run=false first so both start stopping)
	m_thread->stop();
	m_thread->wait();

	infostream<<"Server: Threads stopped"<<std::endl;
}

void Server::step()
{
	// Throw if fatal error occurred in thread
	std::string async_err = m_async_fatal_error.get();
	if (!async_err.empty()) {
		if (!m_simple_singleplayer_mode) {
			kickAllPlayers(SERVER_ACCESSDENIED_CRASH,
				g_settings->get("kick_msg_crash"),
				g_settings->getBool("ask_reconnect_on_crash"));
		}
		throw ServerError("AsyncErr: " + async_err);
	}
}

void Server::AsyncRunStep(float dtime, bool initial_step)
{
	ZoneScoped;
	auto framemarker = FrameMarker("Server::AsyncRunStep()-frame").started();

	if (!m_async_fatal_error.get().empty()) {
		infostream << "Refusing server step in error state" << std::endl;
		return;
	}

	{
		// Send blocks to clients
		SendBlocks(dtime);
	}

	// If paused, this function is called with a 0.0f literal
	if ((dtime == 0.0f) && !initial_step)
		return;

	ScopeProfiler sp(g_profiler, "Server::AsyncRunStep()", SPT_AVG);

	/*
		Update uptime
	*/
	m_uptime_counter->increment(dtime);

	/*
		Update time of day and overall game time
	*/
	m_env->setTimeOfDaySpeed(g_settings->getFloat("time_speed"));

	/*
		Send to clients at constant intervals
	*/

	static const float time_send_interval = 5.0f;
	m_time_of_day_send_timer -= dtime;
	if (m_time_of_day_send_timer < 0) {
		m_time_of_day_send_timer = time_send_interval;
		u16 time = m_env->getTimeOfDay();
		float time_speed = g_settings->getFloat("time_speed");
		SendTimeOfDay(PEER_ID_INEXISTENT, time, time_speed);

		m_timeofday_gauge->set(time);
	}

	{
		EnvAutoLock lock(this);
		float max_lag = m_env->getMaxLagEstimate();
		constexpr float lag_warn_threshold = 1.0f;

		// Decrease value gradually, halve it every minute.
		if (m_max_lag_decrease.step(dtime, 0.5f)) {
			// To reproduce this number enter "solve (x)**(60/0.5) = 0.5"
			// into Wolfram Alpha.
			max_lag *= 0.99425f;
		}

		// Report a 20% change to the log but only if we're >10% off target
		// also report if we crossed into the warning boundary
		if (dtime >= max_lag * 1.2f ||
			(max_lag < lag_warn_threshold && dtime >= lag_warn_threshold)) {
			const float steplen = getStepSettings().steplen;
			if (dtime > steplen * 1.1f) {
				auto &to = dtime >= lag_warn_threshold ? warningstream : infostream;
				to << "Server: Maximum lag peaked at " << dtime
					<< " (steplen=" << steplen << ")" << std::endl;
			}
		}
		max_lag = std::max(max_lag, dtime),

		m_env->reportMaxLagEstimate(max_lag);

		// Step environment
		m_env->step(dtime);
	}

	static const float map_timer_and_unload_dtime = 2.92;
	if(m_map_timer_and_unload_interval.step(dtime, map_timer_and_unload_dtime))
	{
		EnvAutoLock lock(this);
		// Run Map's timers and unload unused data
		ScopeProfiler sp(g_profiler, "Server: map timer and unload");
		m_env->getMap().timerUpdate(map_timer_and_unload_dtime,
			std::max(g_settings->getFloat("server_unload_unused_data_timeout"), 0.0f),
			-1);
	}

	/*
		Note: Orphan MapBlock ptrs become dangling after this call.
	*/
	m_env->getServerMap().step();

	/*
		Listen to the admin chat, if available
	*/
	if (m_admin_chat) {
		if (!m_admin_chat->command_queue.empty()) {
			EnvAutoLock lock(this);
			while (!m_admin_chat->command_queue.empty()) {
				ChatEvent *evt = m_admin_chat->command_queue.pop_frontNoEx();
				handleChatInterfaceEvent(evt);
				delete evt;
			}
		}
		m_admin_chat->outgoing_queue.push_back(
			new ChatEventTimeInfo(m_env->getGameTime(), m_env->getTimeOfDay()));
	}

	/*
		Do background stuff
	*/

	/* Transform liquids */
	m_liquid_transform_timer += dtime;
	if(m_liquid_transform_timer >= m_liquid_transform_every)
	{
		m_liquid_transform_timer -= m_liquid_transform_every;

		EnvAutoLock lock(this);

		ScopeProfiler sp(g_profiler, "Server: liquid transform");

		std::map<v3s16, MapBlock*> modified_blocks;
		m_env->getServerMap().transformLiquids(modified_blocks, m_env);

		if (!modified_blocks.empty()) {
			MapEditEvent event;
			event.type = MEET_OTHER;
			event.low_priority = true;
			event.setModifiedBlocks(modified_blocks);
			m_env->getMap().dispatchEvent(event);
		}
	}
	m_clients.step(dtime);

	// increase/decrease lag gauge gradually
	if (m_lag_gauge->get() > dtime) {
		m_lag_gauge->decrement(dtime/100);
	} else {
		m_lag_gauge->increment(dtime/100);
	}

	{
		float &counter = m_step_pending_dyn_media_timer;
		counter += dtime;
		if (counter >= 5.0f) {
			stepPendingDynMediaCallbacks(counter);
			counter = 0;
		}
	}


#if USE_CURL
	// send masterserver announce
	{
		float &counter = m_masterserver_timer;
		if (!isSingleplayer() && (!counter || counter >= 300.0) &&
				g_settings->getBool("server_announce")) {
			ServerList::sendAnnounce(counter ? ServerList::AA_UPDATE :
						ServerList::AA_START,
					m_bind_addr.getPort(),
					m_clients.getPlayerNames(),
					m_uptime_counter->get(),
					m_env->getGameTime(),
					m_lag_gauge->get(),
					m_gamespec.id,
					Mapgen::getMapgenName(m_emerge->mgparams->mgtype),
					m_modmgr->getMods(),
					m_dedicated);
			counter = 0.01;
		}
		counter += dtime;
	}
#endif

	/*
		Check added and deleted active objects
	*/
	{
		//infostream<<"Server: Checking added and deleted active objects"<<std::endl;
		EnvAutoLock envlock(this);

		// This guarantees that each object recomputes its cache only once per server step,
		// unless get_effective_observers is called.
		// If we were to update observer sets eagerly in set_observers instead,
		// the total costs of calls to set_observers could theoretically be higher.
		m_env->invalidateActiveObjectObserverCaches();

		{
			ClientInterface::AutoLock clientlock(m_clients);
			const RemoteClientMap &clients = m_clients.getClientList();
			ScopeProfiler sp(g_profiler, "Server: update objects within range");

			m_player_gauge->set(clients.size());
			for (const auto &client_it : clients) {
				RemoteClient *client = client_it.second;

				if (client->getState() < CS_DefinitionsSent)
					continue;

				// This can happen if the client times out somehow
				if (!m_env->getPlayer(client->peer_id))
					continue;

				PlayerSAO *playersao = getPlayerSAO(client->peer_id);
				if (!playersao)
					continue;

				SendActiveObjectRemoveAdd(client, playersao);
			}
		}

		// Write changes to the mod storage
		m_mod_storage_save_timer -= dtime;
		if (m_mod_storage_save_timer <= 0.0f) {
			m_mod_storage_save_timer = g_settings->getFloat("server_map_save_interval");
			m_mod_storage_database->endSave();
			m_mod_storage_database->beginSave();
		}
	}

	/*
		Send object messages
	*/
	{
		EnvAutoLock envlock(this);
		ScopeProfiler sp(g_profiler, "Server: send SAO messages");

		// Key = object id
		// Value = data sent by object
		std::unordered_map<u16, std::vector<ActiveObjectMessage>*> buffered_messages;

		// Get active object messages from environment
		ActiveObjectMessage aom(0);
		u32 count_reliable = 0, count_unreliable = 0;
		for(;;) {
			if (!m_env->getActiveObjectMessage(&aom))
				break;
			if (aom.reliable)
				count_reliable++;
			else
				count_unreliable++;

			std::vector<ActiveObjectMessage>* message_list = nullptr;
			auto n = buffered_messages.find(aom.id);
			if (n == buffered_messages.end()) {
				message_list = new std::vector<ActiveObjectMessage>;
				buffered_messages[aom.id] = message_list;
			} else {
				message_list = n->second;
			}
			message_list->push_back(std::move(aom));
		}

		m_aom_buffer_counter[0]->increment(count_reliable);
		m_aom_buffer_counter[1]->increment(count_unreliable);

		{
			ClientInterface::AutoLock clientlock(m_clients);
			const RemoteClientMap &clients = m_clients.getClientList();
			// Route data to every client
			std::string reliable_data, unreliable_data;
			for (const auto &client_it : clients) {
				reliable_data.clear();
				unreliable_data.clear();
				RemoteClient *client = client_it.second;
				PlayerSAO *player = getPlayerSAO(client->peer_id);
				// Go through all objects in message buffer
				for (const auto &buffered_message : buffered_messages) {
					// If object does not exist or is not known by client, skip it
					u16 id = buffered_message.first;
					ServerActiveObject *sao = m_env->getActiveObject(id);
					if (!sao || client->m_known_objects.find(id) == client->m_known_objects.end())
						continue;

					// Get message list of object
					std::vector<ActiveObjectMessage>* list = buffered_message.second;
					// Go through every message
					for (const ActiveObjectMessage &aom : *list) {
						// Send position updates to players who do not see the attachment
						if (aom.datastring[0] == AO_CMD_UPDATE_POSITION) {
							if (sao->getId() == player->getId())
								continue;

							// Do not send position updates for attached players
							// as long the parent is known to the client
							ServerActiveObject *parent = sao->getParent();
							if (parent && client->m_known_objects.find(parent->getId()) !=
									client->m_known_objects.end())
								continue;
						}

						// Add full new data to appropriate buffer
						std::string &buffer = aom.reliable ? reliable_data : unreliable_data;
						char idbuf[2];
						writeU16((u8*) idbuf, aom.id);
						// u16 id
						// std::string data
						buffer.append(idbuf, sizeof(idbuf));
						buffer.append(serializeString16(aom.datastring));
					}
				}
				/*
					reliable_data and unreliable_data are now ready.
					Send them.
				*/
				if (!reliable_data.empty()) {
					SendActiveObjectMessages(client->peer_id, reliable_data);
				}

				if (!unreliable_data.empty()) {
					SendActiveObjectMessages(client->peer_id, unreliable_data, false);
				}
			}
		}

		// Clear buffered_messages
		for (auto &buffered_message : buffered_messages) {
			delete buffered_message.second;
		}
	}

	/*
		Send queued-for-sending map edit events.
	*/
	{
		// We will be accessing the environment
		EnvAutoLock lock(this);

		// Single change sending is disabled if queue size is big
		bool disable_single_change_sending = false;
		if(m_unsent_map_edit_queue.size() >= 4)
			disable_single_change_sending = true;

		const auto event_count = m_unsent_map_edit_queue.size();
		m_map_edit_event_counter->increment(event_count);

		// We'll log the amount of each
		Profiler prof;

		size_t block_count = 0;
		std::unordered_set<v3s16> node_meta_updates;

		while (!m_unsent_map_edit_queue.empty()) {
			MapEditEvent* event = m_unsent_map_edit_queue.front();
			m_unsent_map_edit_queue.pop();

			// Players far away from the change are stored here.
			// Instead of sending the changes, MapBlocks are set not sent
			// for them.
			std::unordered_set<u16> far_players;

			switch (event->type) {
			case MEET_ADDNODE:
			case MEET_SWAPNODE:
				prof.add("MEET_ADDNODE", 1);
				sendAddNode(event->p, event->n, &far_players,
						disable_single_change_sending ? 5 : 30,
						event->type == MEET_ADDNODE);
				break;
			case MEET_REMOVENODE:
				prof.add("MEET_REMOVENODE", 1);
				sendRemoveNode(event->p, &far_players,
						disable_single_change_sending ? 5 : 30);
				break;
			case MEET_BLOCK_NODE_METADATA_CHANGED: {
				prof.add("MEET_BLOCK_NODE_METADATA_CHANGED", 1);
				if (!event->is_private_change) {
					node_meta_updates.emplace(event->p);
				}

				if (MapBlock *block = m_env->getMap().getBlockNoCreateNoEx(
						getNodeBlockPos(event->p))) {
					block->raiseModified(MOD_STATE_WRITE_NEEDED,
						MOD_REASON_REPORT_META_CHANGE);
				}
				break;
			}
			case MEET_OTHER:
				prof.add("MEET_OTHER", 1);
				m_clients.markBlocksNotSent(event->modified_blocks, event->low_priority);
				break;
			default:
				prof.add("unknown", 1);
				warningstream << "Server: Unknown MapEditEvent "
						<< ((u32)event->type) << std::endl;
				break;
			}

			block_count += event->modified_blocks.size();

			/*
				Set blocks not sent to far players
			*/
			for (const u16 far_player : far_players) {
				if (RemoteClient *client = getClient(far_player))
					client->SetBlocksNotSent(event->modified_blocks, event->low_priority);
			}

			delete event;
		}

		if (event_count != 0) {
			verbosestream << "Server: MapEditEvents modified total "
				<< block_count << " blocks:" << std::endl;
			prof.print(verbosestream);
		}

		// Send all metadata updates
		if (!node_meta_updates.empty())
			sendMetadataChanged(node_meta_updates);
	}

	/*
		Trigger emerge thread
		Doing this every 2s is left over from old code, unclear if this is still needed.
	*/
	{
		float &counter = m_emergethread_trigger_timer;
		counter -= dtime;
		if (counter <= 0.0f) {
			counter = 2.0f;

			m_emerge->startThreads();
		}
	}

	// Save map, players and auth stuff
	{
		float &counter = m_savemap_timer;
		counter += dtime;
		static thread_local const float save_interval =
			g_settings->getFloat("server_map_save_interval");
		if (counter >= save_interval) {
			counter = 0.0;
			EnvAutoLock lock(this);

			ScopeProfiler sp(g_profiler, "Server: map saving (sum)");

			// Save ban file
			if (m_banmanager->isModified()) {
				m_banmanager->save();
			}

			// Save changed parts of map
			m_env->getMap().save(MOD_STATE_WRITE_NEEDED);

			// Save players
			m_env->saveLoadedPlayers();

			// Save environment metadata
			m_env->saveMeta();
		}
	}

	m_shutdown_state.tick(dtime, this);
}

void Server::Receive(float min_time)
{
	ZoneScoped;
	auto framemarker = FrameMarker("Server::Receive()-frame").started();

	const u64 t0 = porting::getTimeUs();
	const float min_time_us = min_time * 1e6f;
	auto remaining_time_us = [&]() -> float {
		return std::max(0.0f, min_time_us - (porting::getTimeUs() - t0));
	};

	NetworkPacket pkt;
	session_t peer_id;
	for (;;) {
		pkt.clear();
		peer_id = 0;
		try {
			// Round up since the target step length is the minimum step length,
			// we only have millisecond precision and we don't want to busy-wait
			// by calling ReceiveTimeoutMs(.., 0) repeatedly.
			const u32 cur_timeout_ms = std::ceil(remaining_time_us() / 1000.0f);

			if (!m_con->ReceiveTimeoutMs(&pkt, cur_timeout_ms)) {
				// No incoming data.
				if (remaining_time_us() > 0.0f)
					continue;
				else
					break;
			}

			peer_id = pkt.getPeerId();
			m_packet_recv_counter->increment();
			ProcessData(&pkt);
			m_packet_recv_processed_counter->increment();
		} catch (const con::InvalidIncomingDataException &e) {
			infostream << "Server::Receive(): InvalidIncomingDataException: what()="
					<< e.what() << std::endl;
		} catch (const SerializationError &e) {
			infostream << "Server::Receive(): SerializationError: what()="
					<< e.what() << std::endl;
		} catch (const ClientStateError &e) {
			errorstream << "ClientStateError: peer=" << peer_id << " what()="
					 << e.what() << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		} catch (con::PeerNotFoundException &e) {
			infostream << "Server: PeerNotFoundException" << std::endl;
		} catch (ClientNotFoundException &e) {
			infostream << "Server: ClientNotFoundException" << std::endl;
		}
	}
}

void Server::yieldToOtherThreads(float dtime)
{
	/*
	 * Problem: the server thread and emerge thread compete for the envlock.
	 * While the emerge thread needs it just once or twice for every processed item
	 * the server thread uses it much more generously.
	 * This is usually not a problem as the server sleeps between steps, which leaves
	 * enough chance. But if the server is overloaded it's busy all the time and
	 * - even with a fair envlock - the emerge thread can't get up to speed.
	 * This generally has a much worse impact on gameplay than server lag itself
	 * ever would.
	 *
	 * Workaround: If we detect that the server is overloaded, introduce some careful
	 * artificial sleeps to leave the emerge threads enough chance to do their job.
	 *
	 * In the future the emerge code should be reworked to exclusively use a result
	 * queue, thereby avoiding this problem (and terrible workaround).
	 */

	// don't activate workaround too quickly
	constexpr size_t MIN_EMERGE_QUEUE_SIZE = 32;
	const size_t qs_initial = m_emerge->getQueueSize();
	if (qs_initial < MIN_EMERGE_QUEUE_SIZE)
		return;

	// give the thread a chance to run for every 28ms (on average)
	// this was experimentally determined
	const float QUANTUM = 28.0f / 1000;
	// put an upper limit to not cause too much lag, also so this doesn't become self-sustaining
	const int SLEEP_MAX = 10;

	int sleep_count = std::clamp<int>(dtime / QUANTUM, 1, SLEEP_MAX);

	ScopeProfiler sp(g_profiler, "Server::yieldTo...() sleep", SPT_AVG);
	size_t qs = qs_initial;
	while (sleep_count-- > 0) {
		sleep_ms(1);
		// abort if we don't make progress
		size_t qs2 = m_emerge->getQueueSize();
		if (qs2 >= qs || qs2 == 0)
			break;
		qs = qs2;
	}
	g_profiler->avg("Server::yieldTo...() progress [#]", qs_initial - qs);
}

PlayerSAO *Server::StageTwoClientInit(session_t peer_id)
{
	std::string playername;
	std::unique_ptr<PlayerSAO> sao;
	{
		ClientInterface::AutoLock clientlock(m_clients);
		RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_InitDone);
		if (client) {
			playername = client->getName();
			sao = emergePlayer(playername.c_str(), peer_id, client->net_proto_version);
		}
	}

	RemotePlayer *player = sao ? sao->getPlayer() : nullptr;

	// If failed, cancel
	if (!player) {
		RemotePlayer *joined = m_env->getPlayer(playername.c_str());
		if (joined && joined->getPeerId() != PEER_ID_INEXISTENT) {
			actionstream << "Server: Failed to emerge player \"" << playername
					<< "\" (player allocated to another client)" << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
		} else {
			errorstream << "Server: " << playername << ": Failed to emerge player"
					<< std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
		}
		return nullptr;
	}

	// Add player to environment
	m_env->addPlayer(player);

	/* Clean up old HUD elements from previous sessions */
	player->clearHud();

	/* Add object to environment */
	PlayerSAO *playersao = sao.get();
	m_env->addActiveObject(std::move(sao));

	if (playersao->isNewPlayer())
		m_script->on_newplayer(playersao);

	/*
		Send complete position information
	*/
	SendMovePlayer(playersao);

	// Send privileges
	SendPlayerPrivileges(peer_id);

	// Send inventory formspec
	SendPlayerInventoryFormspec(peer_id);

	// Send inventory
	SendInventory(player, false);

	// Send HP
	SendPlayerHP(playersao, false);

	// Send Breath
	SendPlayerBreath(playersao);

	/*
		Update player list and print action
	*/
	{
		NetworkPacket notice_pkt(TOCLIENT_UPDATE_PLAYER_LIST, 0, PEER_ID_INEXISTENT);
		notice_pkt << (u8) PLAYER_LIST_ADD << (u16) 1 << player->getName();
		m_clients.sendToAll(&notice_pkt);
	}
	{
		std::string ip_str = getPeerAddress(player->getPeerId()).serializeString();
		const auto &names = m_clients.getPlayerNames();

		actionstream << player->getName() << " [" << ip_str << "] joins game. List of players: ";
		for (const std::string &name : names)
			actionstream << name << " ";
		actionstream << player->getName() << std::endl;
	}
	return playersao;
}

inline void Server::handleCommand(NetworkPacket *pkt)
{
	const ToServerCommandHandler &opHandle = toServerCommandTable[pkt->getCommand()];
	(this->*opHandle.handler)(pkt);
}

void Server::ProcessData(NetworkPacket *pkt)
{
	// Environment is locked first.
	EnvAutoLock envlock(this);

	ScopeProfiler sp(g_profiler, "Server: Process network packet (sum)");
	u32 peer_id = pkt->getPeerId();

	try {
		ToServerCommand command = (ToServerCommand) pkt->getCommand();

		// Command must be handled into ToServerCommandHandler
		if (command >= TOSERVER_NUM_MSG_TYPES) {
			infostream << "Server: Ignoring unknown command "
					 << static_cast<unsigned>(command) << std::endl;
			return;
		}

		if (toServerCommandTable[command].state == TOSERVER_STATE_NOT_CONNECTED) {
			handleCommand(pkt);
			return;
		}

		u8 peer_ser_ver = getClient(peer_id, CS_InitDone)->serialization_version;

		if(peer_ser_ver == SER_FMT_VER_INVALID) {
			errorstream << "Server: Peer serialization format invalid. "
					"Skipping incoming command "
					<< static_cast<unsigned>(command) << std::endl;
			return;
		}

		/* Handle commands related to client startup */
		if (toServerCommandTable[command].state == TOSERVER_STATE_STARTUP) {
			handleCommand(pkt);
			return;
		}

		if (m_clients.getClientState(peer_id) < CS_Active) {
			warningstream << "Server: Got packet command "
					<< static_cast<unsigned>(command)
					<< " for peer id " << peer_id
					<< " but client isn't active yet. Dropping packet." << std::endl;
			return;
		}

		handleCommand(pkt);
	} catch (SendFailedException &e) {
		errorstream << "Server::ProcessData(): SendFailedException: "
				<< "what=" << e.what()
				<< std::endl;
	} catch (PacketError &e) {
		actionstream << "Server::ProcessData(): PacketError: "
				<< "what=" << e.what()
				<< std::endl;
	}
}

void Server::setTimeOfDay(u32 time)
{
	m_env->setTimeOfDay(time);
	m_time_of_day_send_timer = 0;
}

void Server::onMapEditEvent(const MapEditEvent &event)
{
	if (m_ignore_map_edit_events_area.contains(event.getArea()))
		return;

	m_unsent_map_edit_queue.push(new MapEditEvent(event));
}

void Server::peerAdded(con::IPeer *peer)
{
	verbosestream << "Server::peerAdded(): id=" << peer->id << std::endl;

	m_clients.CreateClient(peer->id);
}

void Server::deletingPeer(con::IPeer *peer, bool timeout)
{
	verbosestream << "Server::deletingPeer(): id=" << peer->id
		<< ", timeout=" << timeout << std::endl;

	m_clients.event(peer->id, CSE_Disconnect);
	DeleteClient(peer->id, timeout ? CDR_TIMEOUT : CDR_LEAVE);
}

bool Server::getClientConInfo(session_t peer_id, con::rtt_stat_type type, float* retval)
{
	*retval = m_con->getPeerStat(peer_id,type);
	return *retval != -1;
}

bool Server::getClientInfo(session_t peer_id, ClientInfo &ret)
{
	ClientInterface::AutoLock clientlock(m_clients);
	RemoteClient* client = m_clients.lockedGetClientNoEx(peer_id, CS_Invalid);

	if (!client)
		return false;

	ret.state = client->getState();
	ret.addr = client->getAddress();
	ret.uptime = client->uptime();
	ret.ser_vers = client->serialization_version;
	ret.prot_vers = client->net_proto_version;

	ret.major = client->getMajor();
	ret.minor = client->getMinor();
	ret.patch = client->getPatch();
	ret.vers_string = client->getFullVer();

	ret.lang_code = client->getLangCode();

	return true;
}

const ClientDynamicInfo *Server::getClientDynamicInfo(session_t peer_id)
{
	ClientInterface::AutoLock clientlock(m_clients);
	RemoteClient *client = m_clients.lockedGetClientNoEx(peer_id, CS_Invalid);

	if (!client)
		return nullptr;

	return &client->getDynamicInfo();
}

void Server::printToConsoleOnly(const std::string &text)
{
	if (m_admin_chat) {
		m_admin_chat->outgoing_queue.push_back(
			new ChatEventChat("", utf8_to_wide(text)));
	} else {
		std::cout << text << std::endl;
	}
}

void Server::Send(NetworkPacket *pkt)
{
	FATAL_ERROR_IF(pkt->getPeerId() == 0, "Server::Send() missing peer ID");
	Send(pkt->getPeerId(), pkt);
}

void Server::Send(session_t peer_id, NetworkPacket *pkt)
{
	m_clients.send(peer_id, pkt);
}

void Server::SendMovement(session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_MOVEMENT, 12 * sizeof(float), peer_id);

	pkt << g_settings->getFloat("movement_acceleration_default");
	pkt << g_settings->getFloat("movement_acceleration_air");
	pkt << g_settings->getFloat("movement_acceleration_fast");
	pkt << g_settings->getFloat("movement_speed_walk");
	pkt << g_settings->getFloat("movement_speed_crouch");
	pkt << g_settings->getFloat("movement_speed_fast");
	pkt << g_settings->getFloat("movement_speed_climb");
	pkt << g_settings->getFloat("movement_speed_jump");
	pkt << g_settings->getFloat("movement_liquid_fluidity");
	pkt << g_settings->getFloat("movement_liquid_fluidity_smooth");
	pkt << g_settings->getFloat("movement_liquid_sink");
	pkt << g_settings->getFloat("movement_gravity");

	Send(&pkt);
}

void Server::HandlePlayerHPChange(PlayerSAO *playersao, const PlayerHPChangeReason &reason)
{
	m_script->player_event(playersao, "health_changed");
	SendPlayerHP(playersao, reason.type != PlayerHPChangeReason::SET_HP_MAX);

	// Send to other clients
	playersao->sendPunchCommand();

	if (playersao->isDead())
		HandlePlayerDeath(playersao, reason);
}

void Server::SendPlayerHP(PlayerSAO *playersao, bool effect)
{
	SendHP(playersao->getPeerID(), playersao->getHP(), effect);
}

void Server::SendHP(session_t peer_id, u16 hp, bool effect)
{
	NetworkPacket pkt(TOCLIENT_HP, 3, peer_id);
	pkt << hp << effect;
	Send(&pkt);
}

void Server::SendBreath(session_t peer_id, u16 breath)
{
	NetworkPacket pkt(TOCLIENT_BREATH, 2, peer_id);
	pkt << (u16) breath;
	Send(&pkt);
}

void Server::SendAccessDenied(session_t peer_id, AccessDeniedCode reason,
		std::string_view custom_reason, bool reconnect)
{
	assert(reason < SERVER_ACCESSDENIED_MAX);

	NetworkPacket pkt(TOCLIENT_ACCESS_DENIED, 1, peer_id);
	pkt << (u8)reason << custom_reason << (u8)reconnect;
	Send(&pkt);
}

void Server::SendItemDef(session_t peer_id,
		IItemDefManager *itemdef, u16 protocol_version)
{
	auto *client = m_clients.getClientNoEx(peer_id, CS_Created);
	assert(client);

	NetworkPacket pkt(TOCLIENT_ITEMDEF, 0, peer_id);

	std::ostringstream tmp_os2(std::ios::binary);
	{
		std::ostringstream tmp_os(std::ios::binary);
		itemdef->serialize(tmp_os, protocol_version);
		if (client->net_proto_version >= 48)
			compressZstd(tmp_os.str(), tmp_os2);
		else
			compressZlib(tmp_os.str(), tmp_os2);
	}
	pkt.putLongString(tmp_os2.str());

	// Make data buffer
	verbosestream << "Server: Sending item definitions to id(" << peer_id
			<< "): size=" << pkt.getSize() << std::endl;

	Send(&pkt);
}

void Server::SendNodeDef(session_t peer_id,
	const NodeDefManager *nodedef, u16 protocol_version)
{
	auto *client = m_clients.getClientNoEx(peer_id, CS_Created);
	assert(client);

	NetworkPacket pkt(TOCLIENT_NODEDEF, 0, peer_id);

	std::ostringstream tmp_os2(std::ios::binary);
	{
		std::ostringstream tmp_os(std::ios::binary);
		nodedef->serialize(tmp_os, protocol_version);
		if (client->net_proto_version >= 48)
			compressZstd(tmp_os.str(), tmp_os2);
		else
			compressZlib(tmp_os.str(), tmp_os2);
	}
	pkt.putLongString(tmp_os2.str());

	// Make data buffer
	verbosestream << "Server: Sending node definitions to id(" << peer_id
			<< "): size=" << pkt.getSize() << std::endl;

	Send(&pkt);
}

/*
	Non-static send methods
*/

void Server::SendInventory(RemotePlayer *player, bool incremental)
{
	// Do not send new format to old clients
	incremental &= player->protocol_version >= 38;

	UpdateCrafting(player);

	/*
		Serialize it
	*/

	NetworkPacket pkt(TOCLIENT_INVENTORY, 0, player->getPeerId());

	std::ostringstream os(std::ios::binary);
	player->inventory.serialize(os, incremental);
	player->inventory.setModified(false);
	player->setModified(true);

	pkt.putRawString(os.str());
	Send(&pkt);
}

void Server::SendChatMessage(session_t peer_id, const ChatMessage &message)
{
	NetworkPacket pkt(TOCLIENT_CHAT_MESSAGE, 0, peer_id);
	u8 version = 1;
	u8 type = message.type;
	pkt << version << type << message.sender << message.message
		<< static_cast<u64>(message.timestamp);

	if (peer_id != PEER_ID_INEXISTENT) {
		Send(&pkt);
	} else {
		// If a client has completed auth but is still joining, still send chat
		m_clients.sendToAll(&pkt, CS_InitDone);
	}
}

void Server::SendShowFormspecMessage(session_t peer_id, const std::string &formspec,
	const std::string &formname)
{
	NetworkPacket pkt(TOCLIENT_SHOW_FORMSPEC, 0, peer_id);
	if (formspec.empty()){
		// The client should close the formspec
		// But make sure there wasn't another one open in meantime
		// If the formname is empty, any open formspec will be closed so the
		// form name should always be erased from the state.
		const auto it = m_formspec_state_data.find(peer_id);
		if (it != m_formspec_state_data.end() &&
				(it->second == formname || formname.empty())) {
			m_formspec_state_data.erase(peer_id);
		}
		pkt.putLongString("");
	} else {
		m_formspec_state_data[peer_id] = formname;
		pkt.putLongString(formspec);
	}
	pkt << formname;

	Send(&pkt);
}

// Spawns a particle on peer with peer_id
void Server::SendSpawnParticle(session_t peer_id, u16 protocol_version,
	const ParticleParameters &p)
{
	static thread_local const float radius =
			g_settings->getS16("max_block_send_distance") * MAP_BLOCKSIZE * BS;

	if (peer_id == PEER_ID_INEXISTENT) {
		std::vector<session_t> clients = m_clients.getClientIDs();
		const v3f pos = p.pos * BS;
		const float radius_sq = radius * radius;

		for (const session_t client_id : clients) {
			RemotePlayer *player = m_env->getPlayer(client_id);
			if (!player)
				continue;

			PlayerSAO *sao = player->getPlayerSAO();
			if (!sao)
				continue;

			// Do not send to distant clients
			if (sao->getBasePosition().getDistanceFromSQ(pos) > radius_sq)
				continue;

			SendSpawnParticle(client_id, player->protocol_version, p);
		}
		return;
	}
	assert(protocol_version != 0);

	NetworkPacket pkt(TOCLIENT_SPAWN_PARTICLE, 0, peer_id);

	{
		// NetworkPacket and iostreams are incompatible...
		std::ostringstream oss(std::ios_base::binary);
		p.serialize(oss, protocol_version);
		pkt.putRawString(oss.str());
	}

	Send(&pkt);
}

// Adds a ParticleSpawner on peer with peer_id
void Server::SendAddParticleSpawner(session_t peer_id, u16 protocol_version,
	const ParticleSpawnerParameters &p, u16 attached_id, u32 id)
{
	static thread_local const float radius =
			g_settings->getS16("max_block_send_distance") * MAP_BLOCKSIZE * BS;

	if (peer_id == PEER_ID_INEXISTENT) {
		std::vector<session_t> clients = m_clients.getClientIDs();
		const v3f pos = (
			p.pos.start.min.val +
			p.pos.start.max.val +
			p.pos.end.min.val +
			p.pos.end.max.val
		) / 4.0f * BS;
		const float radius_sq = radius * radius;
		/* Don't send short-lived spawners to distant players.
		 * This could be replaced with proper tracking at some point.
		 * A lifetime of 0 means that the spawner exists forever.*/
		const bool distance_check = !attached_id && p.time <= 1.0f && p.time != 0.0f;

		for (const session_t client_id : clients) {
			RemotePlayer *player = m_env->getPlayer(client_id);
			if (!player)
				continue;

			if (distance_check) {
				PlayerSAO *sao = player->getPlayerSAO();
				if (!sao)
					continue;
				if (sao->getBasePosition().getDistanceFromSQ(pos) > radius_sq)
					continue;
			}

			SendAddParticleSpawner(client_id, player->protocol_version,
				p, attached_id, id);
		}
		return;
	}
	assert(protocol_version != 0);

	NetworkPacket pkt(TOCLIENT_ADD_PARTICLESPAWNER, 100, peer_id);

	pkt << p.amount << p.time;

	std::ostringstream os(std::ios_base::binary);
	if (protocol_version >= 42) {
		// Serialize entire thing
		p.pos.serialize(os);
		p.vel.serialize(os);
		p.acc.serialize(os);
		p.exptime.serialize(os);
		p.size.serialize(os);
	} else {
		// serialize legacy fields only (compatibility)
		p.pos.start.legacySerialize(os);
		p.vel.start.legacySerialize(os);
		p.acc.start.legacySerialize(os);
		p.exptime.start.legacySerialize(os);
		p.size.start.legacySerialize(os);
	}
	pkt.putRawString(os.str());
	pkt << p.collisiondetection;

	pkt.putLongString(p.texture.string);

	pkt << id << p.vertical << p.collision_removal << attached_id;
	{
		os.str("");
		p.animation.serialize(os, protocol_version);
		pkt.putRawString(os.str());
	}
	pkt << p.glow << p.object_collision;
	pkt << p.node.param0 << p.node.param2 << p.node_tile;

	{ // serialize new fields
		os.str("");
		if (protocol_version < 42) {
			// initial bias for older properties
			pkt << p.pos.start.bias
				<< p.vel.start.bias
				<< p.acc.start.bias
				<< p.exptime.start.bias
				<< p.size.start.bias;

			// final tween frames of older properties
			p.pos.end.serialize(os);
			p.vel.end.serialize(os);
			p.acc.end.serialize(os);
			p.exptime.end.serialize(os);
			p.size.end.serialize(os);
		}
		// else: fields are already written by serialize() very early

		// properties for legacy texture field
		p.texture.serialize(os, protocol_version, true);

		// new properties
		p.drag.serialize(os);
		p.jitter.serialize(os);
		p.bounce.serialize(os);
		ParticleParamTypes::serializeParameterValue(os, p.attractor_kind);
		if (p.attractor_kind != ParticleParamTypes::AttractorKind::none) {
			p.attract.serialize(os);
			p.attractor_origin.serialize(os);
			writeU16(os, p.attractor_attachment); /* object ID */
			writeU8(os, p.attractor_kill);
			if (p.attractor_kind != ParticleParamTypes::AttractorKind::point) {
				p.attractor_direction.serialize(os);
				writeU16(os, p.attractor_direction_attachment);
			}
		}
		p.radius.serialize(os);

		ParticleParamTypes::serializeParameterValue(os, (u16)p.texpool.size());
		for (const auto& tex : p.texpool) {
			tex.serialize(os, protocol_version);
		}

		pkt.putRawString(os.str());
	}

	Send(&pkt);
}

void Server::SendDeleteParticleSpawner(session_t peer_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_DELETE_PARTICLESPAWNER, 4, peer_id);

	pkt << id;

	if (peer_id != PEER_ID_INEXISTENT)
		Send(&pkt);
	else
		m_clients.sendToAll(&pkt);

}

void Server::SendHUDAdd(session_t peer_id, u32 id, HudElement *form)
{
	NetworkPacket pkt(TOCLIENT_HUDADD, 0 , peer_id);

	pkt << id << (u8) form->type << form->pos << form->name << form->scale
			<< form->text << form->number << form->item << form->dir
			<< form->align << form->offset << form->world_pos << form->size
			<< form->z_index << form->text2 << form->style;

	Send(&pkt);
}

void Server::SendHUDRemove(session_t peer_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_HUDRM, 4, peer_id);
	pkt << id;
	Send(&pkt);
}

void Server::SendHUDChange(session_t peer_id, u32 id, HudElementStat stat, void *value)
{
	NetworkPacket pkt(TOCLIENT_HUDCHANGE, 0, peer_id);
	pkt << id << (u8) stat;

	switch (stat) {
		case HUD_STAT_POS:
		case HUD_STAT_SCALE:
		case HUD_STAT_ALIGN:
		case HUD_STAT_OFFSET:
			pkt << *(v2f *) value;
			break;
		case HUD_STAT_NAME:
		case HUD_STAT_TEXT:
		case HUD_STAT_TEXT2:
			pkt << *(std::string *) value;
			break;
		case HUD_STAT_WORLD_POS:
			pkt << *(v3f *) value;
			break;
		case HUD_STAT_SIZE:
			pkt << *(v2s32 *) value;
			break;
		default: // all other types
			pkt << *(u32 *) value;
			break;
	}

	Send(&pkt);
}

void Server::SendHUDSetFlags(session_t peer_id, u32 flags, u32 mask)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_FLAGS, 4 + 4, peer_id);

	pkt << flags << mask;

	Send(&pkt);
}

void Server::SendHUDSetParam(session_t peer_id, u16 param, std::string_view value)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_PARAM, 0, peer_id);
	pkt << param << value;
	Send(&pkt);
}

void Server::SendSetSky(session_t peer_id, const SkyboxParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_SKY, 0, peer_id);

	// Handle prior clients here
	if (m_clients.getProtocolVersion(peer_id) < 39) {
		pkt << params.bgcolor << params.type << (u16) params.textures.size();

		for (const std::string& texture : params.textures)
			pkt << texture;

		pkt << params.clouds;
	} else { // Handle current clients and future clients
		pkt << params.bgcolor << params.type
			<< params.clouds << params.fog_sun_tint
			<< params.fog_moon_tint << params.fog_tint_type;

		if (params.type == "skybox") {
			pkt << (u16) params.textures.size();
			for (const std::string &texture : params.textures)
				pkt << texture;
		} else if (params.type == "regular") {
			auto &c = params.sky_color;
			pkt << c.day_sky << c.day_horizon << c.dawn_sky << c.dawn_horizon
				<< c.night_sky << c.night_horizon << c.indoors;
		}

		pkt << params.body_orbit_tilt << params.fog_distance << params.fog_start
			<< params.fog_color;
	}

	Send(&pkt);
}

void Server::SendSetSun(session_t peer_id, const SunParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_SUN, 0, peer_id);
	pkt << params.visible << params.texture
		<< params.tonemap << params.sunrise
		<< params.sunrise_visible << params.scale;

	Send(&pkt);
}
void Server::SendSetMoon(session_t peer_id, const MoonParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_MOON, 0, peer_id);

	pkt << params.visible << params.texture
		<< params.tonemap << params.scale;

	Send(&pkt);
}
void Server::SendSetStars(session_t peer_id, const StarParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_STARS, 0, peer_id);

	pkt << params.visible << params.count
		<< params.starcolor << params.scale
		<< params.day_opacity;

	Send(&pkt);
}

void Server::SendCloudParams(session_t peer_id, const CloudParams &params)
{
	NetworkPacket pkt(TOCLIENT_CLOUD_PARAMS, 0, peer_id);
	pkt << params.density << params.color_bright << params.color_ambient
		<< params.height << params.thickness << params.speed << params.color_shadow;
	Send(&pkt);
}

void Server::SendOverrideDayNightRatio(session_t peer_id, bool do_override,
		float ratio)
{
	NetworkPacket pkt(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO,
			1 + 2, peer_id);

	pkt << do_override << (u16) (ratio * 65535);

	Send(&pkt);
}

void Server::SendSetLighting(session_t peer_id, const Lighting &lighting)
{
	NetworkPacket pkt(TOCLIENT_SET_LIGHTING,
			4, peer_id);

	pkt << lighting.shadow_intensity;
	pkt << lighting.saturation;

	pkt << lighting.exposure.luminance_min
			<< lighting.exposure.luminance_max
			<< lighting.exposure.exposure_correction
			<< lighting.exposure.speed_dark_bright
			<< lighting.exposure.speed_bright_dark
			<< lighting.exposure.center_weight_power;

	pkt << lighting.volumetric_light_strength << lighting.shadow_tint;
	pkt << lighting.bloom_intensity << lighting.bloom_strength_factor <<
			lighting.bloom_radius;

	Send(&pkt);
}

void Server::SendCamera(session_t peer_id, Player *player)
{
	NetworkPacket pkt(TOCLIENT_CAMERA, 1, peer_id);

	pkt << static_cast<u8>(player->allowed_camera_mode);

	Send(&pkt);
}

void Server::SendTimeOfDay(session_t peer_id, u16 time, f32 time_speed)
{
	NetworkPacket pkt(TOCLIENT_TIME_OF_DAY, 0, peer_id);
	pkt << time << time_speed;

	if (peer_id == PEER_ID_INEXISTENT) {
		m_clients.sendToAll(&pkt);
	}
	else {
		Send(&pkt);
	}
}

void Server::SendPlayerBreath(PlayerSAO *sao)
{
	assert(sao);

	m_script->player_event(sao, "breath_changed");
	SendBreath(sao->getPeerID(), sao->getBreath());
}

void Server::SendMovePlayer(PlayerSAO *sao)
{
	// Send attachment updates instantly to the client prior updating position
	sao->sendOutdatedData();

	NetworkPacket pkt(TOCLIENT_MOVE_PLAYER, sizeof(v3f) + sizeof(f32) * 2, sao->getPeerID());
	pkt << sao->getBasePosition() << sao->getLookPitch() << sao->getRotation().Y;

	{
		verbosestream << "Server: Sending TOCLIENT_MOVE_PLAYER"
				<< " pos=" << sao->getBasePosition()
				<< " pitch=" << sao->getLookPitch()
				<< " yaw=" << sao->getRotation().Y
				<< std::endl;
	}

	Send(&pkt);
}

void Server::SendMovePlayerRel(session_t peer_id, const v3f &added_pos)
{
	NetworkPacket pkt(TOCLIENT_MOVE_PLAYER_REL, 0, peer_id);
	pkt << added_pos;
	Send(&pkt);
}

void Server::SendPlayerFov(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_FOV, 4 + 1 + 4, peer_id);

	PlayerFovSpec fov_spec = player->getFov();
	pkt << fov_spec.fov << fov_spec.is_multiplier << fov_spec.transition_time;

	Send(&pkt);
}

void Server::SendLocalPlayerAnimations(session_t peer_id, v2f animation_frames[4],
		f32 animation_speed)
{
	NetworkPacket pkt(TOCLIENT_LOCAL_PLAYER_ANIMATIONS, 0,
		peer_id);

	for (int i = 0; i < 4; ++i) {
		if (m_clients.getProtocolVersion(peer_id) >= 46) {
			pkt << animation_frames[i];
		} else {
			pkt << v2s32::from(animation_frames[i]);
		}
	}

	pkt  << animation_speed;

	Send(&pkt);
}

void Server::SendEyeOffset(session_t peer_id, v3f first, v3f third, v3f third_front)
{
	NetworkPacket pkt(TOCLIENT_EYE_OFFSET, 0, peer_id);
	pkt << first << third << third_front;
	Send(&pkt);
}

void Server::SendPlayerPrivileges(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	std::set<std::string> privs;
	m_script->getAuth(player->getName(), NULL, &privs);

	NetworkPacket pkt(TOCLIENT_PRIVILEGES, 0, peer_id);
	pkt << (u16) privs.size();

	for (const std::string &priv : privs) {
		pkt << priv;
	}

	Send(&pkt);
}

void Server::SendPlayerInventoryFormspec(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_INVENTORY_FORMSPEC, 0, peer_id);
	pkt.putLongString(player->inventory_formspec);

	Send(&pkt);
}

void Server::SendPlayerFormspecPrepend(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_FORMSPEC_PREPEND, 0, peer_id);
	pkt << player->formspec_prepend;
	Send(&pkt);
}

void Server::SendActiveObjectRemoveAdd(RemoteClient *client, PlayerSAO *playersao)
{
	// Radius inside which objects are active
	static thread_local const s16 radius =
		g_settings->getS16("active_object_send_range_blocks") * MAP_BLOCKSIZE;

	// Radius inside which players are active
	static thread_local const bool is_transfer_limited =
		g_settings->exists("unlimited_player_transfer_distance") &&
		!g_settings->getBool("unlimited_player_transfer_distance");

	static thread_local const s16 player_transfer_dist =
		g_settings->getS16("player_transfer_distance") * MAP_BLOCKSIZE;

	s16 player_radius = player_transfer_dist == 0 && is_transfer_limited ?
		radius : player_transfer_dist;

	s16 my_radius = MYMIN(radius, playersao->getWantedRange() * MAP_BLOCKSIZE);
	if (my_radius <= 0)
		my_radius = radius;

	std::vector<std::pair<bool, u16>> removed_objects;
	std::vector<u16> added_objects;
	m_env->getRemovedActiveObjects(playersao, my_radius, player_radius,
		client->m_known_objects, removed_objects);
	m_env->getAddedActiveObjects(playersao, my_radius, player_radius,
		client->m_known_objects, added_objects);

	if (removed_objects.empty() && added_objects.empty())
		return;

	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD,
		2 * removed_objects.size() + 32 * added_objects.size(), client->peer_id);

	// Removed objects
	pkt << static_cast<u16>(removed_objects.size());

	for (auto &it : removed_objects) {
		const auto [gone, id] = it;
		ServerActiveObject *obj = m_env->getActiveObject(id);

		pkt << id;

		// Remove from known objects
		client->m_known_objects.erase(id);
		if (obj && obj->m_known_by_count > 0)
			obj->m_known_by_count--;
	}

	// Note: Do yet NOT stop or remove object-attached sounds where the object goes out
	// of range (client side). Such sounds would need to be re-sent when coming into range.
	// Currently, the client will initiate m_playing_sounds clean ups indirectly by
	// "Server::handleCommand_RemovedSounds".

	// Added objects
	pkt << static_cast<u16>(added_objects.size());

	for (u16 id : added_objects) {
		ServerActiveObject *obj = m_env->getActiveObject(id);
		if (!obj) {
			warningstream << FUNCTION_NAME << ": found NULL object id="
				<< (int)id << std::endl;
			continue;
		}

		u8 type = obj->getSendType();

		pkt << id << type;
		pkt.putLongString(obj->getClientInitializationData(client->net_proto_version));

		// Add to known objects
		client->m_known_objects.insert(id);
		obj->m_known_by_count++;
	}

	Send(&pkt);
}

void Server::SendActiveObjectMessages(session_t peer_id, const std::string &datas,
		bool reliable)
{
	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_MESSAGES,
			datas.size(), peer_id);

	pkt.putRawString(datas);

	auto &ccf = clientCommandFactoryTable[pkt.getCommand()];
	m_clients.sendCustom(pkt.getPeerId(), reliable ? ccf.channel : 1, &pkt, reliable);
}

void Server::SendCSMRestrictionFlags(session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_CSM_RESTRICTION_FLAGS,
		sizeof(m_csm_restriction_flags) + sizeof(m_csm_restriction_noderange), peer_id);
	pkt << m_csm_restriction_flags << m_csm_restriction_noderange;
	Send(&pkt);
}

void Server::SendPlayerSpeed(session_t peer_id, const v3f &added_vel)
{
	NetworkPacket pkt(TOCLIENT_PLAYER_SPEED, 0, peer_id);
	pkt << added_vel;
	Send(&pkt);
}

inline s32 Server::nextSoundId()
{
	s32 free_id = m_playing_sounds_id_last_used;
	do {
		if (free_id == INT32_MAX)
			free_id = 0; // signed overflow is undefined
		else
			free_id++;

		if (free_id == m_playing_sounds_id_last_used)
			return 0;
	} while (free_id == 0 || m_playing_sounds.find(free_id) != m_playing_sounds.end());

	m_playing_sounds_id_last_used = free_id;
	return free_id;
}

s32 Server::playSound(ServerPlayingSound &params, bool ephemeral)
{
	// Find out initial position of sound
	bool pos_exists = false;
	const v3f pos = params.getPos(m_env, &pos_exists);
	// If position is not found while it should be, cancel sound
	if(pos_exists != (params.type != SoundLocation::Local))
		return -1;

	// Filter destination clients
	std::vector<session_t> dst_clients;
	if (!params.to_player.empty()) {
		RemotePlayer *player = m_env->getPlayer(params.to_player.c_str());
		if(!player){
			infostream<<"Server::playSound: Player \""<<params.to_player
					<<"\" not found"<<std::endl;
			return -1;
		}
		dst_clients.push_back(player->getPeerId());
	} else {
		std::vector<session_t> clients = m_clients.getClientIDs();

		for (const session_t client_id : clients) {
			RemotePlayer *player = m_env->getPlayer(client_id);
			if (!player)
				continue;
			if (!params.exclude_player.empty() &&
					params.exclude_player == player->getName())
				continue;

			PlayerSAO *sao = player->getPlayerSAO();
			if (!sao)
				continue;

			if (pos_exists) {
				if(sao->getBasePosition().getDistanceFrom(pos) >
						params.max_hear_distance)
					continue;
			}
			dst_clients.push_back(client_id);
		}
	}

	if(dst_clients.empty())
		return -1;

	// old clients will still use this, so pick a reserved ID (-1)
	const s32 id = ephemeral ? -1 : nextSoundId();
	if (id == 0)
		return 0;

	float gain = params.gain * params.spec.gain;
	NetworkPacket pkt(TOCLIENT_PLAY_SOUND, 0);
	pkt << id << params.spec.name << gain
			<< (u8) params.type << pos << params.object
			<< params.spec.loop << params.spec.fade << params.spec.pitch
			<< ephemeral << params.spec.start_time;

	const bool as_reliable = !ephemeral;

	for (const session_t peer_id : dst_clients) {
		if (!ephemeral)
			params.clients.insert(peer_id);
		m_clients.sendCustom(peer_id, 0, &pkt, as_reliable);
	}

	if (!ephemeral)
		m_playing_sounds[id] = std::move(params);
	return id;
}
void Server::stopSound(s32 handle)
{
	auto it = m_playing_sounds.find(handle);
	if (it == m_playing_sounds.end())
		return;

	ServerPlayingSound &psound = it->second;

	NetworkPacket pkt(TOCLIENT_STOP_SOUND, 4);
	pkt << handle;

	for (session_t peer_id : psound.clients) {
		Send(peer_id, &pkt);
	}

	// Remove sound reference
	m_playing_sounds.erase(it);
}

void Server::fadeSound(s32 handle, float step, float gain)
{
	auto it = m_playing_sounds.find(handle);
	if (it == m_playing_sounds.end())
		return;

	ServerPlayingSound &psound = it->second;
	psound.gain = gain; // destination gain

	NetworkPacket pkt(TOCLIENT_FADE_SOUND, 4);
	pkt << handle << step << gain;

	for (session_t peer_id : psound.clients) {
		Send(peer_id, &pkt);
	}

	// Remove sound reference
	if (gain <= 0 || psound.clients.empty())
		m_playing_sounds.erase(it);
}

void Server::sendRemoveNode(v3s16 p, std::unordered_set<u16> *far_players,
		float far_d_nodes)
{
	v3f p_f = intToFloat(p, BS);
	v3s16 block_pos = getNodeBlockPos(p);

	NetworkPacket pkt(TOCLIENT_REMOVENODE, 6);
	pkt << p;

	sendNodeChangePkt(pkt, block_pos, p_f, far_d_nodes, far_players);
}

void Server::sendAddNode(v3s16 p, MapNode n, std::unordered_set<u16> *far_players,
		float far_d_nodes, bool remove_metadata)
{
	v3f p_f = intToFloat(p, BS);
	v3s16 block_pos = getNodeBlockPos(p);

	NetworkPacket pkt(TOCLIENT_ADDNODE, 6 + 2 + 1 + 1 + 1);
	pkt << p << n.param0 << n.param1 << n.param2
			<< (u8) (remove_metadata ? 0 : 1);
	sendNodeChangePkt(pkt, block_pos, p_f, far_d_nodes, far_players);
}

void Server::sendNodeChangePkt(NetworkPacket &pkt, v3s16 block_pos,
		v3f p, float far_d_nodes, std::unordered_set<u16> *far_players)
{
	float maxd = far_d_nodes * BS;
	std::vector<session_t> clients = m_clients.getClientIDs();
	ClientInterface::AutoLock clientlock(m_clients);

	for (session_t client_id : clients) {
		RemoteClient *client = m_clients.lockedGetClientNoEx(client_id);
		if (!client)
			continue;

		RemotePlayer *player = m_env->getPlayer(client_id);
		PlayerSAO *sao = player ? player->getPlayerSAO() : nullptr;

		// If player is far away, only set modified blocks not sent
		if (!client->isBlockSent(block_pos) || (sao &&
				sao->getBasePosition().getDistanceFrom(p) > maxd)) {
			if (far_players)
				far_players->emplace(client_id);
			else
				client->SetBlockNotSent(block_pos);
			continue;
		}

		Send(client_id, &pkt);
	}
}

void Server::sendMetadataChanged(const std::unordered_set<v3s16> &positions, float far_d_nodes)
{
	NodeMetadataList meta_updates_list(false);
	std::ostringstream os(std::ios::binary);

	std::vector<session_t> clients = m_clients.getClientIDs();
	ClientInterface::AutoLock clientlock(m_clients);

	for (session_t i : clients) {
		RemoteClient *client = m_clients.lockedGetClientNoEx(i);
		if (!client)
			continue;

		ServerActiveObject *player = getPlayerSAO(i);
		v3s16 player_pos;
		if (player)
			player_pos = floatToInt(player->getBasePosition(), BS);

		for (const v3s16 pos : positions) {
			NodeMetadata *meta = m_env->getMap().getNodeMetadata(pos);

			if (!meta)
				continue;

			v3s16 block_pos = getNodeBlockPos(pos);
			if (!client->isBlockSent(block_pos) ||
					player_pos.getDistanceFrom(pos) > far_d_nodes) {
				client->SetBlockNotSent(block_pos);
				continue;
			}

			// Add the change to send list
			meta_updates_list.set(pos, meta);
		}
		if (meta_updates_list.size() == 0)
			continue;

		// Send the meta changes
		os.str("");
		meta_updates_list.serialize(os, client->serialization_version, false, true, true);
		std::string raw = os.str();
		os.str("");
		compressZlib(raw, os);

		NetworkPacket pkt(TOCLIENT_NODEMETA_CHANGED, 0, i);
		pkt.putLongString(os.str());
		Send(&pkt);

		meta_updates_list.clear();
	}
}

void Server::SendBlockNoLock(session_t peer_id, MapBlock *block, u8 ver,
		u16 net_proto_version, SerializedBlockCache *cache)
{
	thread_local const int net_compression_level = rangelim(g_settings->getS16("map_compression_level_net"), -1, 9);
	std::string s, *sptr = nullptr;

	if (cache) {
		auto it = cache->find({block->getPos(), ver});
		if (it != cache->end())
			sptr = &it->second;
	}

	// Serialize the block in the right format
	if (!sptr) {
		std::ostringstream os(std::ios_base::binary);
		block->serialize(os, ver, false, net_compression_level);
		block->serializeNetworkSpecific(os);
		s = os.str();
		sptr = &s;
	}

	NetworkPacket pkt(TOCLIENT_BLOCKDATA, 2 + 2 + 2 + sptr->size(), peer_id);
	pkt << block->getPos();
	pkt.putRawString(*sptr);
	Send(&pkt);

	// Store away in cache
	if (cache && sptr == &s)
		(*cache)[{block->getPos(), ver}] = std::move(s);
}

void Server::SendBlocks(float dtime)
{
	EnvAutoLock envlock(this);

	std::vector<PrioritySortedBlockTransfer> queue;

	u32 total_sending = 0, unique_clients = 0;

	{
		ScopeProfiler sp2(g_profiler, "Server::SendBlocks(): Collect list");

		std::vector<session_t> clients = m_clients.getClientIDs();

		ClientInterface::AutoLock clientlock(m_clients);
		for (const session_t client_id : clients) {
			RemoteClient *client = m_clients.lockedGetClientNoEx(client_id, CS_Active);

			if (!client)
				continue;

			total_sending += client->getSendingCount();
			const auto old_count = queue.size();
			client->GetNextBlocks(m_env, m_emerge.get(), dtime, queue);
			unique_clients += queue.size() > old_count ? 1 : 0;
		}
	}

	// Sort.
	// Lowest priority number comes first.
	// Lowest is most important.
	std::sort(queue.begin(), queue.end());

	ClientInterface::AutoLock clientlock(m_clients);

	// Maximal total count calculation
	// The per-client block sends is halved with the maximal online users
	u32 max_blocks_to_send = (m_env->getPlayerCount() + g_settings->getU32("max_users")) *
		g_settings->getU32("max_simultaneous_block_sends_per_client") / 4 + 1;

	ScopeProfiler sp(g_profiler, "Server::SendBlocks(): Send to clients");
	Map &map = m_env->getMap();

	SerializedBlockCache cache, *cache_ptr = nullptr;
	if (unique_clients > 1) {
		// caching is pointless with a single client
		cache_ptr = &cache;
	}

	for (const PrioritySortedBlockTransfer &block_to_send : queue) {
		if (total_sending >= max_blocks_to_send)
			break;

		MapBlock *block = map.getBlockNoCreateNoEx(block_to_send.pos);
		if (!block)
			continue;

		RemoteClient *client = m_clients.lockedGetClientNoEx(block_to_send.peer_id,
				CS_Active);
		if (!client)
			continue;

		SendBlockNoLock(block_to_send.peer_id, block, client->serialization_version,
				client->net_proto_version, cache_ptr);

		client->SentBlock(block_to_send.pos);
		total_sending++;
	}
}

bool Server::SendBlock(session_t peer_id, const v3s16 &blockpos)
{
	MapBlock *block = m_env->getMap().getBlockNoCreateNoEx(blockpos);
	if (!block)
		return false;

	ClientInterface::AutoLock clientlock(m_clients);
	RemoteClient *client = m_clients.lockedGetClientNoEx(peer_id, CS_Active);
	if (!client || client->isBlockSent(blockpos))
		return false;
	SendBlockNoLock(peer_id, block, client->serialization_version,
			client->net_proto_version);

	return true;
}

bool Server::addMediaFile(const std::string &filename,
	const std::string &filepath, std::string *filedata_to,
	std::string *digest_to)
{
	// If name contains illegal characters, ignore the file
	if (!string_allowed(filename, TEXTURENAME_ALLOWED_CHARS)) {
		warningstream << "Server: ignoring file as it has disallowed characters: \""
				<< filename << "\"" << std::endl;
		return false;
	}
	// If name is not in a supported format, ignore it
	const char *supported_ext[] = {
		".png", ".jpg", ".tga",
		".ogg",
		".x", ".b3d", ".obj", ".gltf", ".glb",
		// Translation file formats
		".tr", ".po", ".mo",
		// Fonts
		".ttf", ".woff",
		NULL
	};
	if (removeStringEnd(filename, supported_ext).empty()) {
		infostream << "Server: ignoring unsupported file extension: \""
				<< filename << "\"" << std::endl;
		return false;
	}
	// Ok, attempt to load the file and add to cache

	// Read data
	std::string filedata;
	if (!fs::ReadFile(filepath, filedata, true)) {
		return false;
	}

	if (filedata.empty()) {
		errorstream << "Server::addMediaFile(): Empty file \""
				<< filepath << "\"" << std::endl;
		return false;
	}

	std::string sha1 = hashing::sha1(filedata);
	std::string sha1_hex = hex_encode(sha1);
	if (digest_to)
		*digest_to = sha1;

	// Put in list
	m_media[filename] = MediaInfo(filepath, sha1);
	verbosestream << "Server: " << sha1_hex << " is " << filename
			<< std::endl;

	if (filedata_to)
		*filedata_to = std::move(filedata);
	return true;
}

void Server::fillMediaCache()
{
	infostream << "Server: Calculating media file checksums" << std::endl;

	// Collect all media file paths
	std::vector<std::string> paths;

	// ordered in descending priority
	paths.push_back(getBuiltinLuaPath() + DIR_DELIM + "locale");
	fs::GetRecursiveDirs(paths, porting::path_user + DIR_DELIM + "textures" + DIR_DELIM + "server");
	fs::GetRecursiveDirs(paths, m_gamespec.path + DIR_DELIM + "textures");
	m_modmgr->getModsMediaPaths(paths);

	// Collect media file information from paths into cache
	for (const std::string &mediapath : paths) {
		std::vector<fs::DirListNode> dirlist = fs::GetDirListing(mediapath);
		for (const fs::DirListNode &dln : dirlist) {
			if (dln.dir) // Ignore dirs (already in paths)
				continue;

			const std::string &filename = dln.name;
			if (m_media.find(filename) != m_media.end()) // Do not override
				continue;

			std::string filepath = mediapath;
			filepath.append(DIR_DELIM).append(filename);
			addMediaFile(filename, filepath);
		}
	}

	infostream << "Server: " << m_media.size() << " media files collected" << std::endl;
}

void Server::sendMediaAnnouncement(session_t peer_id, const std::string &lang_code)
{
	std::string translation_formats[3] = { ".tr", ".po", ".mo" };
	std::string lang_suffixes[3];
	for (size_t i = 0; i < 3; i++) {
		lang_suffixes[i].append(".").append(lang_code).append(translation_formats[i]);
	}

	auto include = [&] (const std::string &name, const MediaInfo &info) -> bool {
		if (info.no_announce)
			return false;
		for (size_t j = 0; j < 3; j++) {
			if (str_ends_with(name, translation_formats[j]) && !str_ends_with(name, lang_suffixes[j])) {
				return false;
			}
		}
		return true;
	};

	// Make packet
	auto *client = m_clients.getClientNoEx(peer_id, CS_Created);
	assert(client);
	NetworkPacket pkt(TOCLIENT_ANNOUNCE_MEDIA, 0, peer_id);

	size_t media_sent = 0;
	if (client->net_proto_version < 48) {
		for (const auto &i : m_media) {
			if (include(i.first, i.second))
				media_sent++;
		}
		assert(media_sent < U16_MAX);
		pkt << static_cast<u16>(media_sent);
		for (const auto &i : m_media) {
			if (include(i.first, i.second))
				pkt << i.first << base64_encode(i.second.sha1_digest);
		}
	} else {
		std::vector<std::string> names;
		for (const auto &i : m_media) {
			if (include(i.first, i.second))
				names.emplace_back(i.first);
		}
		media_sent = names.size();

		// compressed table of media names
		{
			std::ostringstream oss(std::ios::binary);
			auto tmp = serializeString16Array(names);
			compressZstd(tmp, oss);
			pkt.putLongString(oss.str());
		}

		// then the raw hash for each file
		for (const auto &i : m_media) {
			if (include(i.first, i.second)) {
				assert(i.second.sha1_digest.size() == 20);
				pkt.putRawString(i.second.sha1_digest);
			}
		}
	}

	// and the remote media server(s)
	pkt << g_settings->get("remote_media");
	Send(&pkt);

	verbosestream << "Server: Announcing files to id(" << peer_id
		<< "): count=" << media_sent << " size=" << pkt.getSize() << std::endl;
}

namespace {

struct SendableMedia
{
	const std::string &name;
	const std::string &path;
	std::string data;

	SendableMedia(const std::string &name, const std::string &path,
			std::string &&data):
		name(name), path(path), data(std::move(data))
	{}
};

}

void Server::sendRequestedMedia(session_t peer_id,
		const std::unordered_set<std::string> &tosend)
{
	auto *client = getClient(peer_id, CS_DefinitionsSent);
	assert(client);

	const bool compress = client->net_proto_version >= 48;

	infostream << "Server::sendRequestedMedia(): Sending "
		<< tosend.size() << " files to " << client->getName()
		<< (compress ? " (compressed)" : "") << std::endl;

	/* Read files and prepare bunches */

	// Put 5KB in one bunch (this is not accurate)
	// This is a tradeoff between burdening the network with too many packets
	// and burdening it with too large split packets.
	const u32 bytes_per_bunch = 5000;

	std::vector<std::vector<SendableMedia>> file_bunches;
	file_bunches.emplace_back();

	// Note that applying a "real" bin packing algorithm here is not necessarily
	// an improvement (might even perform worse) since games usually have lots
	// of files larger than 5KB and the current algorithm already minimizes
	// the amount of bunches quite well (at the expense of overshooting).

	u32 file_size_bunch_total = 0;
	size_t bytes_compressed = 0, bytes_uncompressed = 0;
	for (const std::string &name : tosend) {
		auto it = m_media.find(name);

		if (it == m_media.end()) {
			errorstream<<"Server::sendRequestedMedia(): Client asked for "
					<<"unknown file \""<<(name)<<"\""<<std::endl;
			continue;
		}
		const auto &m = it->second;

		// no_announce <=> usually ephemeral dynamic media, which may
		// have duplicate filenames. So we can't check it.
		if (!m.no_announce) {
			if (!client->markMediaSent(name)) {
				infostream << "Server::sendRequestedMedia(): Client asked has "
					"requested \"" << name << "\" before, not sending it again."
					<< std::endl;
				continue;
			}
		}

		// Read data
		std::string data;
		if (!fs::ReadFile(m.path, data, true)) {
			continue;
		}
		bytes_uncompressed += data.size();
		if (compress) {
			// Zstd is very fast and can handle non-compressible data efficiently
			// so we can just throw it at every file. Still we don't want to
			// spend too much here, so we use the lowest compression level.
			std::ostringstream oss(std::ios::binary);
			compressZstd(data, oss, 1);
			data = oss.str();
		}
		bytes_compressed += data.size();

		// Put in list
		file_size_bunch_total += data.size();
		file_bunches.back().emplace_back(name, m.path, std::move(data));

		// Start next bunch if got enough data
		if(file_size_bunch_total >= bytes_per_bunch) {
			file_bunches.emplace_back();
			file_size_bunch_total = 0;
		}
	}

	/* Create and send packets */

	const u16 num_bunches = file_bunches.size();
	for (u16 i = 0; i < num_bunches; i++) {
		auto &bunch = file_bunches[i];
		NetworkPacket pkt(TOCLIENT_MEDIA, 4 + 0, peer_id);

		const u32 bunch_size = bunch.size();
		pkt << num_bunches << i << bunch_size;

		for (auto &j : bunch) {
			pkt << j.name;
			pkt.putLongString(j.data);
		}
		bunch.clear(); // free memory early

		verbosestream << "Server::sendRequestedMedia(): bunch "
				<< i << "/" << num_bunches
				<< " files=" << bunch_size
				<< " size="  << pkt.getSize() << std::endl;
		Send(&pkt);
	}

	if (compress && bytes_uncompressed != 0) {
		int percent = bytes_compressed / (float)bytes_uncompressed * 100;
		int diff = (int)bytes_compressed - (int)bytes_uncompressed;
		infostream << "Server::sendRequestedMedia(): size after compression "
			<< percent << "% (" << (diff > 0 ? '+' : '-') << std::abs(diff)
			<< " byte)" << std::endl;
	}
}

void Server::stepPendingDynMediaCallbacks(float dtime)
{
	EnvAutoLock lock(this);

	for (auto it = m_pending_dyn_media.begin(); it != m_pending_dyn_media.end();) {
		it->second.expiry_timer -= dtime;
		bool del = it->second.waiting_players.empty() || it->second.expiry_timer < 0;

		if (!del) {
			it++;
			continue;
		}

		const auto &name = it->second.filename;
		if (!name.empty()) {
			assert(m_media.count(name));
			// if no_announce isn't set we're definitely deleting the wrong file!
			sanity_check(m_media[name].no_announce);

			fs::DeleteSingleFileOrEmptyDirectory(m_media[name].path);
			m_media.erase(name);
		}
		getScriptIface()->freeDynamicMediaCallback(it->first);
		it = m_pending_dyn_media.erase(it);
	}
}

void Server::SendMinimapModes(session_t peer_id,
		std::vector<MinimapMode> &modes, size_t wanted_mode)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return;

	NetworkPacket pkt(TOCLIENT_MINIMAP_MODES, 0, peer_id);
	pkt << (u16)modes.size() << (u16)wanted_mode;

	for (auto &mode : modes)
		pkt << (u16)mode.type << mode.label << mode.size << mode.texture << mode.scale;

	Send(&pkt);
}

void Server::sendDetachedInventory(Inventory *inventory, const std::string &name, session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_DETACHED_INVENTORY, 0, peer_id);
	pkt << name;

	if (!inventory) {
		pkt << false; // Remove inventory
	} else {
		pkt << true; // Update inventory

		// Serialization & NetworkPacket isn't a love story
		std::ostringstream os(std::ios_base::binary);
		inventory->serialize(os);
		inventory->setModified(false);

		const std::string &os_str = os.str();
		pkt << static_cast<u16>(os_str.size()); // HACK: to keep compatibility with 5.0.0 clients
		pkt.putRawString(os_str);
	}

	if (peer_id == PEER_ID_INEXISTENT)
		m_clients.sendToAll(&pkt);
	else
		Send(&pkt);
}

void Server::sendDetachedInventories(session_t peer_id, bool incremental)
{
	// Lookup player name, to filter detached inventories just after
	std::string peer_name;
	if (peer_id != PEER_ID_INEXISTENT) {
		peer_name = getClient(peer_id, CS_Created)->getName();
	}

	auto send_cb = [this, peer_id](const std::string &name, Inventory *inv) {
		sendDetachedInventory(inv, name, peer_id);
	};

	m_inventory_mgr->sendDetachedInventories(peer_name, incremental, send_cb);
}

/*
	Something random
*/

void Server::HandlePlayerDeath(PlayerSAO *playersao, const PlayerHPChangeReason &reason)
{
	infostream << "Server::DiePlayer(): Player "
			<< playersao->getPlayer()->getName()
			<< " dies" << std::endl;

	playersao->clearParentAttachment();

	// Trigger scripted stuff
	m_script->on_dieplayer(playersao, reason);
}

void Server::DenySudoAccess(session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_DENY_SUDO_MODE, 0, peer_id);
	Send(&pkt);
}


void Server::DenyAccess(session_t peer_id, AccessDeniedCode reason,
		std::string_view custom_reason, bool reconnect)
{
	SendAccessDenied(peer_id, reason, custom_reason, reconnect);
	m_clients.event(peer_id, CSE_SetDenied);
	DisconnectPeer(peer_id);
}

void Server::kickAllPlayers(AccessDeniedCode reason,
		const std::string &str_reason, bool reconnect)
{
	std::vector<session_t> clients = m_clients.getClientIDs();
	for (const session_t client_id : clients) {
		DenyAccess(client_id, reason, str_reason, reconnect);
	}
}

void Server::DisconnectPeer(session_t peer_id)
{
	m_modchannel_mgr->leaveAllChannels(peer_id);
	m_con->DisconnectPeer(peer_id);
}

void Server::acceptAuth(session_t peer_id, bool forSudoMode)
{
	if (!forSudoMode) {
		RemoteClient* client = getClient(peer_id, CS_Invalid);

		NetworkPacket resp_pkt(TOCLIENT_AUTH_ACCEPT, 1 + 6 + 8 + 4, peer_id);

		resp_pkt << v3f() << (u64) m_env->getServerMap().getSeed()
				<< g_settings->getFloat("dedicated_server_step")
				<< client->allowed_auth_mechs;

		Send(&resp_pkt);
		m_clients.event(peer_id, CSE_AuthAccept);
	} else {
		NetworkPacket resp_pkt(TOCLIENT_ACCEPT_SUDO_MODE, 1 + 6 + 8 + 4, peer_id);

		// We only support SRP right now
		u32 sudo_auth_mechs = AUTH_MECHANISM_FIRST_SRP;

		resp_pkt << sudo_auth_mechs;
		Send(&resp_pkt);
		m_clients.event(peer_id, CSE_SudoSuccess);
	}
}

void Server::DeleteClient(session_t peer_id, ClientDeletionReason reason)
{
	std::wstring message;
	{
		/*
			Clear references to playing sounds
		*/
		for (auto i = m_playing_sounds.begin(); i != m_playing_sounds.end();) {
			ServerPlayingSound &psound = i->second;
			psound.clients.erase(peer_id);
			if (psound.clients.empty())
				i = m_playing_sounds.erase(i);
			else
				++i;
		}

		// clear formspec info so the next client can't abuse the current state
		m_formspec_state_data.erase(peer_id);

		RemotePlayer *player = m_env->getPlayer(peer_id);

		/* Run scripts and remove from environment */
		if (player) {
			PlayerSAO *playersao = player->getPlayerSAO();
			assert(playersao);

			// inform connected clients
			const std::string &player_name = player->getName();
			NetworkPacket notice(TOCLIENT_UPDATE_PLAYER_LIST, 0, PEER_ID_INEXISTENT);
			// (u16) 1 + std::string represents a vector serialization representation
			notice << (u8) PLAYER_LIST_REMOVE  << (u16) 1 << player_name;
			m_clients.sendToAll(&notice);
			// run scripts
			m_script->on_leaveplayer(playersao, reason == CDR_TIMEOUT);

			playersao->disconnected();
		}

		/*
			Print out action
		*/
		{
			if (player && reason != CDR_DENY) {
				std::ostringstream os(std::ios_base::binary);
				std::vector<session_t> clients = m_clients.getClientIDs();

				for (const session_t client_id : clients) {
					// Get player
					RemotePlayer *player = m_env->getPlayer(client_id);
					if (!player)
						continue;

					// Get name of player
					os << player->getName() << " ";
				}

				std::string name = player->getName();
				actionstream << name << " "
						<< (reason == CDR_TIMEOUT ? "times out." : "leaves game.")
						<< " List of players: " << os.str() << std::endl;
				if (m_admin_chat)
					m_admin_chat->outgoing_queue.push_back(
						new ChatEventNick(CET_NICK_REMOVE, name));
			}
		}
		{
			EnvAutoLock envlock(this);
			m_clients.DeleteClient(peer_id);
		}
	}

	// Send leave chat message to all remaining clients
	if (!message.empty()) {
		SendChatMessage(PEER_ID_INEXISTENT,
				ChatMessage(CHATMESSAGE_TYPE_ANNOUNCE, message));
	}
}

void Server::UpdateCrafting(RemotePlayer *player)
{
	InventoryList *clist = player->inventory.getList("craft");
	if (!clist || clist->getSize() == 0)
		return;

	if (!clist->checkModified())
		return;

	// Get a preview for crafting
	ItemStack preview;
	InventoryLocation loc;
	loc.setPlayer(player->getName());
	std::vector<ItemStack> output_replacements;
	getCraftingResult(&player->inventory, preview, output_replacements, false, this);
	m_env->getScriptIface()->item_CraftPredict(preview, player->getPlayerSAO(),
			clist, loc);

	InventoryList *plist = player->inventory.getList("craftpreview");
	if (plist && plist->getSize() >= 1) {
		// Put the new preview in
		plist->changeItem(0, preview);
	}
}

void Server::handleChatInterfaceEvent(ChatEvent *evt)
{
	if (evt->type == CET_NICK_ADD) {
		// The terminal informed us of its nick choice
		m_admin_nick = ((ChatEventNick *)evt)->nick;
		if (!m_script->getAuth(m_admin_nick, NULL, NULL)) {
			errorstream << "You haven't set up an account." << std::endl
				<< "Please log in using the client as '"
				<< m_admin_nick << "' with a secure password." << std::endl
				<< "Until then, you can't execute admin tasks via the console," << std::endl
				<< "and everybody can claim the user account instead of you," << std::endl
				<< "giving them full control over this server." << std::endl;
		}
	} else {
		assert(evt->type == CET_CHAT);
		handleAdminChat((ChatEventChat *)evt);
	}
}

std::wstring Server::handleChat(const std::string &name,
	std::wstring wmessage, bool check_shout_priv, RemotePlayer *player)
{
	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			std::string("player:") + name);

	if (g_settings->getBool("strip_color_codes"))
		wmessage = unescape_enriched(wmessage);

	wmessage = trim(wmessage);

	if (player) {
		switch (player->canSendChatMessage()) {
		case RPLAYER_CHATRESULT_FLOODING: {
			std::wstringstream ws;
			ws << L"You cannot send more messages. You are limited to "
					<< g_settings->getFloat("chat_message_limit_per_10sec")
					<< L" messages per 10 seconds.";
			return ws.str();
		}
		case RPLAYER_CHATRESULT_KICK:
			DenyAccess(player->getPeerId(), SERVER_ACCESSDENIED_CUSTOM_STRING,
				"You have been kicked due to message flooding.");
			return L"";
		case RPLAYER_CHATRESULT_OK:
			break;
		default:
			FATAL_ERROR("Unhandled chat filtering result found.");
		}
	}

	if (m_max_chatmessage_length > 0
			&& wmessage.length() > m_max_chatmessage_length) {
		return L"Your message exceed the maximum chat message limit set on the server. "
				L"It was refused. Send a shorter message";
	}

	auto message = wide_to_utf8(wmessage);
	if (message.empty())
		return L"";

	if (message.find_first_of("\r\n") != std::string::npos)
		return L"Newlines are not permitted in chat messages";

	// Run script hook, exit if script ate the chat message
	if (m_script->on_chat_message(name, message))
		return L"";

	// Line to send
	std::wstring line;
	// Whether to send line to the player that sent the message, or to all players
	bool broadcast_line = true;

	if (check_shout_priv && !checkPriv(name, "shout")) {
		line += L"-!- You don't have permission to shout.";
		broadcast_line = false;
	} else {
		/*
			Workaround for fixing chat on Android. Lua doesn't handle
			the Cyrillic alphabet and some characters on older Android devices
		*/
#ifdef __ANDROID__
		line += L"<" + utf8_to_wide(name) + L"> " + wmessage;
#else
		line += utf8_to_wide(m_script->formatChatMessage(name, message));
#endif
	}

	/*
		Tell calling method to send the message to sender
	*/
	if (!broadcast_line)
		return line;

	/*
		Send the message to others
	*/
	actionstream << "CHAT: " << wide_to_utf8(unescape_enriched(line)) << std::endl;

	ChatMessage chatmsg(line);

	SendChatMessage(PEER_ID_INEXISTENT, chatmsg);

	return L"";
}

void Server::handleAdminChat(const ChatEventChat *evt)
{
	std::string name = evt->nick;
	std::wstring wmessage = evt->evt_msg;

	std::wstring answer = handleChat(name, wmessage);

	// If asked to send answer to sender
	if (!answer.empty()) {
		m_admin_chat->outgoing_queue.push_back(new ChatEventChat("", answer));
	}
}

RemoteClient *Server::getClient(session_t peer_id, ClientState state_min)
{
	RemoteClient *client = getClientNoEx(peer_id,state_min);
	if(!client)
		throw ClientNotFoundException("Client not found");

	return client;
}
RemoteClient *Server::getClientNoEx(session_t peer_id, ClientState state_min)
{
	return m_clients.getClientNoEx(peer_id, state_min);
}

std::string Server::getPlayerName(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return "[id="+itos(peer_id)+"]";
	return player->getName();
}

PlayerSAO *Server::getPlayerSAO(session_t peer_id)
{
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player)
		return NULL;
	return player->getPlayerSAO();
}

std::string Server::getStatusString()
{
	std::ostringstream os(std::ios_base::binary);
	os << "# Server: ";
	// Version
	os << "version: " << g_version_string;
	// Game
	os << " | game: " << (m_gamespec.title.empty() ? m_gamespec.id : m_gamespec.title);
	// Uptime
	os << " | uptime: " << duration_to_string((int) m_uptime_counter->get());
	// Max lag estimate
	os << " | max lag: " << std::setprecision(3);
	os << (m_env ? m_env->getMaxLagEstimate() : 0) << "s";

	// Information about clients
	bool first = true;
	os << " | clients: ";
	if (m_env) {
		std::vector<std::string> player_names = m_clients.getPlayerNames();

		std::sort(player_names.begin(), player_names.end());

		for (const std::string& name : player_names) {
			if (!first)
				os << ", ";
			else
				first = false;
			os << name;
		}
	}

	if (m_env && !((ServerMap*)(&m_env->getMap()))->isSavingEnabled())
		os << std::endl << "# Server: " << " WARNING: Map saving is disabled.";

	if (!g_settings->get("motd").empty())
		os << std::endl << "# Server: " << g_settings->get("motd");

	return os.str();
}

std::set<std::string> Server::getPlayerEffectivePrivs(const std::string &name)
{
	std::set<std::string> privs;
	m_script->getAuth(name, NULL, &privs);
	return privs;
}

bool Server::checkPriv(const std::string &name, const std::string &priv)
{
	std::set<std::string> privs = getPlayerEffectivePrivs(name);
	return (privs.count(priv) != 0);
}

void Server::reportPrivsModified(const std::string &name)
{
	if (name.empty()) {
		std::vector<session_t> clients = m_clients.getClientIDs();
		for (const session_t client_id : clients) {
			RemotePlayer *player = m_env->getPlayer(client_id);
			reportPrivsModified(player->getName());
		}
	} else {
		RemotePlayer *player = m_env->getPlayer(name.c_str());
		if (!player)
			return;
		SendPlayerPrivileges(player->getPeerId());
		PlayerSAO *sao = player->getPlayerSAO();
		if(!sao)
			return;
		sao->updatePrivileges(getPlayerEffectivePrivs(name));
	}
}

void Server::reportInventoryFormspecModified(const std::string &name)
{
	RemotePlayer *player = m_env->getPlayer(name.c_str());
	if (!player)
		return;
	SendPlayerInventoryFormspec(player->getPeerId());
}

void Server::reportFormspecPrependModified(const std::string &name)
{
	RemotePlayer *player = m_env->getPlayer(name.c_str());
	if (!player)
		return;
	SendPlayerFormspecPrepend(player->getPeerId());
}

void Server::setIpBanned(const std::string &ip, const std::string &name)
{
	m_banmanager->add(ip, name);

	auto clients = m_clients.getClientIDs(CS_Created);
	for (const auto peer_id : clients) {
		denyIfBanned(peer_id);
	}
}

void Server::unsetIpBanned(const std::string &ip_or_name)
{
	m_banmanager->remove(ip_or_name);
}

std::string Server::getBanDescription(const std::string &ip_or_name)
{
	return m_banmanager->getBanDescription(ip_or_name);
}

bool Server::denyIfBanned(session_t peer_id)
{
	Address address = getPeerAddress(peer_id);
	std::string addr_s = address.serializeString();

	if (m_banmanager->isIpBanned(addr_s)) {
		std::string ban_name = m_banmanager->getBanName(addr_s);
		actionstream << "Server: A banned client tried to connect from "
			<< addr_s << "; banned name was " << ban_name << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING,
			"Your IP is banned. Banned name was " + ban_name);
		return true;
	}
	return false;
}

bool Server::checkUserLimit(const std::string &player_name, const std::string &addr_s)
{
	if (!m_clients.isUserLimitReached())
		return false;
	if (player_name == g_settings->get("name")) // admin can always join
		return false;
	return !m_script->can_bypass_userlimit(player_name, addr_s);
}

void Server::notifyPlayer(const char *name, const std::wstring &msg)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return;

	if (m_admin_nick == name && !m_admin_nick.empty()) {
		m_admin_chat->outgoing_queue.push_back(new ChatEventChat("", msg));
	}

	RemotePlayer *player = m_env->getPlayer(name);
	if (!player)
		return;

	SendChatMessage(player->getPeerId(), ChatMessage(msg));
}

bool Server::showFormspec(const char *playername, const std::string &formspec,
	const std::string &formname)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return false;

	RemotePlayer *player = m_env->getPlayer(playername);
	if (!player)
		return false;

	SendShowFormspecMessage(player->getPeerId(), formspec, formname);
	return true;
}

u32 Server::hudAdd(RemotePlayer *player, HudElement *form)
{
	if (!player)
		return -1;

	u32 id = player->addHud(form);

	SendHUDAdd(player->getPeerId(), id, form);

	return id;
}

bool Server::hudRemove(RemotePlayer *player, u32 id) {
	if (!player)
		return false;

	HudElement* todel = player->removeHud(id);

	if (!todel)
		return false;

	delete todel;

	SendHUDRemove(player->getPeerId(), id);
	return true;
}

bool Server::hudChange(RemotePlayer *player, u32 id, HudElementStat stat, void *data)
{
	if (!player)
		return false;

	SendHUDChange(player->getPeerId(), id, stat, data);
	return true;
}

bool Server::hudSetFlags(RemotePlayer *player, u32 flags, u32 mask)
{
	if (!player)
		return false;

	u32 new_hud_flags = (player->hud_flags & ~mask) | flags;
	if (new_hud_flags == player->hud_flags) // no change
		return true;

	SendHUDSetFlags(player->getPeerId(), flags, mask);
	player->hud_flags = new_hud_flags;

	PlayerSAO* playersao = player->getPlayerSAO();

	if (!playersao)
		return false;

	m_script->player_event(playersao, "hud_changed");
	return true;
}

bool Server::hudSetHotbarItemcount(RemotePlayer *player, s32 hotbar_itemcount)
{
	if (!player)
		return false;

	if (hotbar_itemcount <= 0 || hotbar_itemcount > HUD_HOTBAR_ITEMCOUNT_MAX)
		return false;

	if (player->getHotbarItemcount() == hotbar_itemcount)
		return true;

	player->setHotbarItemcount(hotbar_itemcount);
	std::ostringstream os(std::ios::binary);
	writeS32(os, hotbar_itemcount);
	SendHUDSetParam(player->getPeerId(), HUD_PARAM_HOTBAR_ITEMCOUNT, os.str());
	return true;
}

void Server::hudSetHotbarImage(RemotePlayer *player, const std::string &name)
{
	if (!player)
		return;

	if (player->getHotbarImage() == name)
		return;

	player->setHotbarImage(name);
	SendHUDSetParam(player->getPeerId(), HUD_PARAM_HOTBAR_IMAGE, name);
}

void Server::hudSetHotbarSelectedImage(RemotePlayer *player, const std::string &name)
{
	if (!player)
		return;

	if (player->getHotbarSelectedImage() == name)
		return;

	player->setHotbarSelectedImage(name);
	SendHUDSetParam(player->getPeerId(), HUD_PARAM_HOTBAR_SELECTED_IMAGE, name);
}

Address Server::getPeerAddress(session_t peer_id)
{
	return getClient(peer_id, CS_Invalid)->getAddress();
}

void Server::setLocalPlayerAnimations(RemotePlayer *player,
		v2f animation_frames[4], f32 frame_speed)
{
	sanity_check(player);
	player->setLocalAnimations(animation_frames, frame_speed);
	SendLocalPlayerAnimations(player->getPeerId(), animation_frames, frame_speed);
}

void Server::setPlayerEyeOffset(RemotePlayer *player, v3f first, v3f third, v3f third_front)
{
	sanity_check(player);
	if (std::tie(player->eye_offset_first, player->eye_offset_third,
			player->eye_offset_third_front) == std::tie(first, third, third_front))
		return; // no change

	player->eye_offset_first = first;
	player->eye_offset_third = third;
	player->eye_offset_third_front = third_front;
	SendEyeOffset(player->getPeerId(), first, third, third_front);
}

void Server::setSky(RemotePlayer *player, const SkyboxParams &params)
{
	sanity_check(player);
	player->setSky(params);
	SendSetSky(player->getPeerId(), params);
}

void Server::setSun(RemotePlayer *player, const SunParams &params)
{
	sanity_check(player);
	player->setSun(params);
	SendSetSun(player->getPeerId(), params);
}

void Server::setMoon(RemotePlayer *player, const MoonParams &params)
{
	sanity_check(player);
	player->setMoon(params);
	SendSetMoon(player->getPeerId(), params);
}

void Server::setStars(RemotePlayer *player, const StarParams &params)
{
	sanity_check(player);
	player->setStars(params);
	SendSetStars(player->getPeerId(), params);
}

void Server::setClouds(RemotePlayer *player, const CloudParams &params)
{
	sanity_check(player);
	player->setCloudParams(params);
	SendCloudParams(player->getPeerId(), params);
}

void Server::overrideDayNightRatio(RemotePlayer *player, bool do_override,
	float ratio)
{
	sanity_check(player);
	player->overrideDayNightRatio(do_override, ratio);
	SendOverrideDayNightRatio(player->getPeerId(), do_override, ratio);
}

void Server::setLighting(RemotePlayer *player, const Lighting &lighting)
{
	sanity_check(player);
	player->setLighting(lighting);
	SendSetLighting(player->getPeerId(), lighting);
}

void Server::notifyPlayers(const std::wstring &msg)
{
	SendChatMessage(PEER_ID_INEXISTENT, ChatMessage(msg));
}

void Server::spawnParticle(const std::string &playername,
	const ParticleParameters &p)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return;

	session_t peer_id = PEER_ID_INEXISTENT;
	u16 proto_ver = 0;
	if (!playername.empty()) {
		RemotePlayer *player = m_env->getPlayer(playername.c_str());
		if (!player)
			return;
		peer_id = player->getPeerId();
		proto_ver = player->protocol_version;
	}

	SendSpawnParticle(peer_id, proto_ver, p);
}

u32 Server::addParticleSpawner(const ParticleSpawnerParameters &p,
	ServerActiveObject *attached, const std::string &playername)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		return -1;

	session_t peer_id = PEER_ID_INEXISTENT;
	u16 proto_ver = 0;
	if (!playername.empty()) {
		RemotePlayer *player = m_env->getPlayer(playername.c_str());
		if (!player)
			return -1;
		peer_id = player->getPeerId();
		proto_ver = player->protocol_version;
	}

	u16 attached_id = attached ? attached->getId() : 0;

	u32 id;
	if (attached_id == 0)
		id = m_env->addParticleSpawner(p.time);
	else
		id = m_env->addParticleSpawner(p.time, attached_id);

	SendAddParticleSpawner(peer_id, proto_ver, p, attached_id, id);
	return id;
}

void Server::deleteParticleSpawner(const std::string &playername, u32 id)
{
	// m_env will be NULL if the server is initializing
	if (!m_env)
		throw ServerError("Can't delete particle spawners during initialisation!");

	session_t peer_id = PEER_ID_INEXISTENT;
	if (!playername.empty()) {
		RemotePlayer *player = m_env->getPlayer(playername.c_str());
		if (!player)
			return;
		peer_id = player->getPeerId();
	}

	m_env->deleteParticleSpawner(id);
	SendDeleteParticleSpawner(peer_id, id);
}

namespace {
	std::string writeToTempFile(std::string_view content)
	{
		auto filepath = fs::CreateTempFile();
		if (filepath.empty())
			return "";
		auto os = open_ofstream(filepath.c_str(), true);
		if (!os.good())
			return "";
		os << content;
		os.close();
		if (os.fail()) {
			fs::DeleteSingleFileOrEmptyDirectory(filepath);
			return "";
		}
		return filepath;
	}
}

bool Server::dynamicAddMedia(const DynamicMediaArgs &a)
{
	std::string filename = a.filename;
	std::string filepath;

	// Deal with file -or- data, as provided
	// (Note: caller must ensure validity, so sanity_check is okay)
	if (a.filepath) {
		sanity_check(!a.data);
		filepath = *a.filepath;
		if (filename.empty())
			filename = fs::GetFilenameFromPath(filepath.c_str());
	} else {
		sanity_check(a.data);
		sanity_check(!filename.empty());

		// Write the file to disk. addMediaFile() will read it right back but this
		// is the best way without turning the media loading code into spaghetti.
		filepath = writeToTempFile(*a.data);
		if (filepath.empty()) {
			errorstream << "Server: failed writing media file \""
				<< filename << "\" to disk" << std::endl;
			return false;
		}
		verbosestream << "Server: \"" << filename << "\" temporarily written to "
			<< filepath << std::endl;
	}

	// Do some checks
	auto it = m_media.find(filename);
	if (it != m_media.end()) {
		// Allow the same path to be "added" again in certain conditions
		if (a.ephemeral || it->second.path != filepath) {
			errorstream << "Server::dynamicAddMedia(): file \"" << filename
				<< "\" already exists in media cache" << std::endl;
			return false;
		}
	}

	if (!m_env && (!a.to_player.empty() || a.ephemeral)) {
		errorstream << "Server::dynamicAddMedia(): "
			"adding ephemeral or player-specific media at startup is nonsense"
			<< std::endl;
		return false;
	}

	// Load the file and add it to our media cache
	std::string filedata, raw_hash;
	bool ok = addMediaFile(filename, filepath, &filedata, &raw_hash);
	if (!ok)
		return false;
	assert(!filedata.empty());

	const auto &media_it = m_media.find(filename);
	assert(media_it != m_media.end());

	if (a.ephemeral) {
		if (!a.data) {
			// Create a copy of the file and swap out the path, this removes the
			// requirement that mods keep the file accessible at the original path.
			filepath = writeToTempFile(filedata);
			if (filepath.empty()) {
				errorstream << "Server: failed creating a copy of media file \""
					<< filename << "\"" << std::endl;
				m_media.erase(filename);
				return false;
			}
			verbosestream << "Server: \"" << filename << "\" temporarily copied to "
				<< filepath << std::endl;
			media_it->second.path = filepath;
		}

		media_it->second.no_announce = true;
		// stepPendingDynMediaCallbacks will clean the file up later
	} else if (a.data) {
		// data is in a temporary file but not ephemeral, so the cleanup point
		// is different.
		media_it->second.delete_at_shutdown = true;
	}
	if (!a.to_player.empty()) {
		// only sent to one player (who must be online), so shouldn't announce.
		media_it->second.no_announce = true;
	}

	std::unordered_set<session_t> delivered, waiting;

	// Push file to existing clients
	if (m_env) {
		NetworkPacket pkt(TOCLIENT_MEDIA_PUSH, 0);
		pkt << raw_hash << filename;
		// NOTE: the meaning of a.ephemeral was accidentally inverted between proto 39 and 40,
		// when dynamic_add_media v2 was added. As of 5.12.0 the server sends it correctly again.
		// Compatibility code on the client-side was not added.
		pkt << static_cast<bool>(!a.ephemeral);

		NetworkPacket legacy_pkt = pkt;

		// Newer clients get asked to fetch the file (asynchronous)
		pkt << a.token;
		// Older clients have an awful hack that just throws the data at them
		legacy_pkt.putLongString(filedata);

		ClientInterface::AutoLock clientlock(m_clients);
		for (auto &pair : m_clients.getClientList()) {
			if (pair.second->getState() == CS_DefinitionsSent && !a.ephemeral) {
				/*
					If a client is in the DefinitionsSent state it is too late to
					transfer the file via sendMediaAnnouncement() but at the same
					time the client cannot accept a media push yet.
					Short of artificially delaying the joining process there is no
					way for the server to resolve this so we (currently) opt not to.
				*/
				warningstream << "The media \"" << filename << "\" (dynamic) could "
					"not be delivered to " << pair.second->getName()
					<< " due to a race condition." << std::endl;
				continue;
			}
			if (pair.second->getState() < CS_Active)
				continue;

			const auto proto_ver = pair.second->net_proto_version;
			if (proto_ver < 39)
				continue;

			const session_t peer_id = pair.second->peer_id;
			if (!a.to_player.empty() && getPlayerName(peer_id) != a.to_player)
				continue;

			if (proto_ver < 40) {
				delivered.emplace(peer_id);
				/*
					The network layer only guarantees ordered delivery inside a channel.
					Since the very next packet could be one that uses the media, we have
					to push the media over ALL channels to ensure it is processed before
					it is used. In practice this means channels 1 and 0.
				*/
				m_clients.sendCustom(peer_id, 1, &legacy_pkt, true);
				m_clients.sendCustom(peer_id, 0, &legacy_pkt, true);
			} else {
				waiting.emplace(peer_id);
				Send(peer_id, &pkt);
			}
		}
	}

	// Run callback for players that already had the file delivered (legacy-only)
	for (session_t peer_id : delivered) {
		if (auto player = m_env->getPlayer(peer_id))
			getScriptIface()->on_dynamic_media_added(a.token, player->getName());
	}

	// Save all others in our pending state
	auto &state = m_pending_dyn_media[a.token];
	state.waiting_players = std::move(waiting);
	// regardless of success throw away the callback after a while
	state.expiry_timer = 60.0f;
	if (a.ephemeral)
		state.filename = filename;

	return true;
}

// actions: time-reversed list
// Return value: success/failure
bool Server::rollbackRevertActions(const std::list<RollbackAction> &actions,
		std::list<std::string> *log)
{
	infostream<<"Server::rollbackRevertActions(len="<<actions.size()<<")"<<std::endl;
	auto *map = &m_env->getServerMap();

	// Fail if no actions to handle
	if (actions.empty()) {
		assert(log);
		log->push_back("Nothing to do.");
		return false;
	}

	int num_tried = 0;
	int num_failed = 0;

	for (const RollbackAction &action : actions) {
		num_tried++;
		bool success = action.applyRevert(map, m_inventory_mgr.get(), this);
		if(!success){
			num_failed++;
			std::ostringstream os;
			os<<"Revert of step ("<<num_tried<<") "<<action.toString()<<" failed";
			infostream<<"Map::rollbackRevertActions(): "<<os.str()<<std::endl;
			if (log)
				log->push_back(os.str());
		}else{
			std::ostringstream os;
			os<<"Successfully reverted step ("<<num_tried<<") "<<action.toString();
			infostream<<"Map::rollbackRevertActions(): "<<os.str()<<std::endl;
			if (log)
				log->push_back(os.str());
		}
	}

	infostream<<"Map::rollbackRevertActions(): "<<num_failed<<"/"<<num_tried
			<<" failed"<<std::endl;

	// Call it done if less than half failed
	return num_failed <= num_tried/2;
}

// IGameDef interface
// Under envlock
IItemDefManager *Server::getItemDefManager()
{
	return m_itemdef;
}

const NodeDefManager *Server::getNodeDefManager()
{
	return m_nodedef;
}

ICraftDefManager *Server::getCraftDefManager()
{
	return m_craftdef;
}

u16 Server::allocateUnknownNodeId(const std::string &name)
{
	return m_nodedef->allocateDummy(name);
}

IWritableItemDefManager *Server::getWritableItemDefManager()
{
	return m_itemdef;
}

NodeDefManager *Server::getWritableNodeDefManager()
{
	return m_nodedef;
}

IWritableCraftDefManager *Server::getWritableCraftDefManager()
{
	return m_craftdef;
}

const std::vector<ModSpec> & Server::getMods() const
{
	return m_modmgr->getMods();
}

const ModSpec *Server::getModSpec(const std::string &modname) const
{
	return m_modmgr->getModSpec(modname);
}

std::string Server::getBuiltinLuaPath()
{
	return porting::path_share + DIR_DELIM + "builtin";
}

void Server::setAsyncFatalError(const std::string &error)
{
	// print error right here in the thread that set it, for clearer logging
	infostream << "setAsyncFatalError: " << error << std::endl;

	m_async_fatal_error.set(error);

	// make sure server steps stop happening immediately
	if (m_thread)
		m_thread->stop();
}

// Not thread-safe.
void Server::addShutdownError(const ModError &e)
{
	// DO NOT TRANSLATE the `ModError`, it's used by `ui.lua`
	std::string msg = fmtgettext("%s while shutting down: ", "ModError") +
			e.what() + strgettext("\nCheck debug.txt for details.");
	errorstream << msg << std::endl;

	if (m_shutdown_errmsg) {
		if (m_shutdown_errmsg->empty()) {
			*m_shutdown_errmsg = msg;
		} else {
			*m_shutdown_errmsg += "\n\n" + msg;
		}
	}
}

v3f Server::findSpawnPos()
{
	ServerMap &map = m_env->getServerMap();

	std::optional<v3f> staticSpawnPoint;
	if (g_settings->getV3FNoEx("static_spawnpoint", staticSpawnPoint) && staticSpawnPoint.has_value())
	{
		return *staticSpawnPoint * BS;
	}

	v3f nodeposf;

	bool is_good = false;
	// Limit spawn range to mapgen edges (determined by 'mapgen_limit')
	s32 range_max = map.getMapgenParams()->getSpawnRangeMax();

	// Try to find a good place a few times
	for (s32 i = 0; i < 4000 && !is_good; i++) {
		s32 range = MYMIN(1 + i, range_max);
		// We're going to try to throw the player to this position
		v2s16 nodepos2d = v2s16(
			-range + myrand_range(0, range*2),
			-range + myrand_range(0, range*2));
		// Get spawn level at point
		s16 spawn_level = m_emerge->getSpawnLevelAtPoint(nodepos2d);
		// Continue if MAX_MAP_GENERATION_LIMIT was returned by the mapgen to
		// signify an unsuitable spawn position, or if outside limits.
		if (spawn_level >= MAX_MAP_GENERATION_LIMIT ||
				spawn_level <= -MAX_MAP_GENERATION_LIMIT)
			continue;

		v3s16 nodepos(nodepos2d.X, spawn_level, nodepos2d.Y);
		// Consecutive empty nodes
		s32 air_count = 0;

		// Search upwards from 'spawn level' for 2 consecutive empty nodes, to
		// avoid obstructions in already-generated mapblocks.
		// In ungenerated mapblocks consisting of 'ignore' nodes, there will be
		// no obstructions, but mapgen decorations are generated after spawn so
		// the player may end up inside one.
		for (s32 i = 0; i < 8; i++) {
			v3s16 blockpos = getNodeBlockPos(nodepos);
			map.emergeBlock(blockpos, true);
			content_t c = map.getNode(nodepos).getContent();

			// In generated mapblocks allow spawn in all 'airlike' drawtype nodes.
			// In ungenerated mapblocks allow spawn in 'ignore' nodes.
			if (m_nodedef->get(c).drawtype == NDT_AIRLIKE || c == CONTENT_IGNORE) {
				air_count++;
				if (air_count >= 2) {
					// Spawn in lower empty node
					nodepos.Y--;
					nodeposf = intToFloat(nodepos, BS);
					// Don't spawn the player outside map boundaries
					if (objectpos_over_limit(nodeposf))
						// Exit this loop, positions above are probably over limit
						break;

					// Good position found, cause an exit from main loop
					is_good = true;
					break;
				}
			} else {
				air_count = 0;
			}
			nodepos.Y++;
		}
	}

	if (is_good)
		return nodeposf;

	// No suitable spawn point found, return fallback 0,0,0
	return v3f(0.0f, 0.0f, 0.0f);
}

void Server::requestShutdown(const std::string &msg, bool reconnect, float delay)
{
	if (delay == 0.0f) {
	// No delay, shutdown immediately
		m_shutdown_state.is_requested = true;
		// only print to the infostream, a chat message saying
		// "Server Shutting Down" is sent when the server destructs.
		infostream << "*** Immediate Server shutdown requested." << std::endl;
	} else if (delay < 0.0f && m_shutdown_state.isTimerRunning()) {
		// Negative delay, cancel shutdown if requested
		m_shutdown_state.reset();

		const char *s = "*** Server shutdown canceled.";

		infostream << s << std::endl;
		SendChatMessage(PEER_ID_INEXISTENT, utf8_to_wide(s));
		// m_shutdown_state already handled, skip.
		return;
	} else if (delay > 0.0f) {
	// Positive delay, tell the clients when the server will shut down
		std::ostringstream oss;

		oss << "*** Server shutting down in "
			<< duration_to_string(myround(delay)) << ".";

		infostream << oss.str() << std::endl;
		SendChatMessage(PEER_ID_INEXISTENT, utf8_to_wide(oss.str()));
	}

	m_shutdown_state.trigger(delay, msg, reconnect);
}

std::unique_ptr<PlayerSAO> Server::emergePlayer(const char *name, session_t peer_id,
	u16 proto_version)
{
	/*
		Try to get an existing player
	*/
	RemotePlayer *player = m_env->getPlayer(name);

	// If player is already connected, cancel
	if (player) {
		infostream<<"emergePlayer(): Player already connected"<<std::endl;
		return NULL;
	}

	/*
		If player with the wanted peer_id already exists, cancel.
	*/
	if (m_env->getPlayer(peer_id)) {
		infostream<<"emergePlayer(): Player with wrong name but same"
				" peer_id already exists"<<std::endl;
		return NULL;
	}

	/*
		Object construction sequence/hierarchy
		--------------------------------------
		1. RemoteClient (tightly connection-bound)
		2. RemotePlayer (controls, in-game appearance)
		3. PlayerSAO (movable object in-game)
			PlayerSAO controls the peer_id assignment of RemotePlayer,
			indicating whether the player is ready

		Destruction sequence
		--------------------
		1. PlayerSAO pending removal flag
		2. PlayerSAO save data before free
		3. RemotePlayer, then PlayerSAO freed
		4. RemoteClient freed
	*/

	if (!player) {
		player = new RemotePlayer(name, idef());
	}

	// Load player
	auto playersao = m_env->loadPlayer(player, peer_id);

	// Complete init with server parts
	playersao->finalize(player, getPlayerEffectivePrivs(player->getName()));
	player->protocol_version = proto_version;

	return playersao;
}

void dedicated_server_loop(Server &server, volatile std::sig_atomic_t &kill)
{
	verbosestream<<"dedicated_server_loop()"<<std::endl;

	IntervalLimiter m_profiler_interval;

	constexpr float steplen = 0.05f; // always 50 ms
	const float profiler_print_interval = g_settings->getFloat("profiler_print_interval");

	server.setStepSettings(Server::StepSettings{
			g_settings->getFloat("dedicated_server_step"),
			false
		});

	/*
	 * The dedicated server loop only provides a way to main.cpp to kill the
	 * server externally (bool &kill).
	 */

	for(;;) {
		// This is kind of a hack but can be done like this
		// because server.step() is very light
		sleep_ms((int)(steplen*1000.0f));
		server.step();

		if (server.isShutdownRequested() || kill)
			break;

		/*
			Profiler
		*/
		if (profiler_print_interval > 0) {
			if (m_profiler_interval.step(steplen, profiler_print_interval)) {
				infostream << "Profiler:" << std::endl;
				g_profiler->print(infostream);
				g_profiler->clear();
			}
		}
	}

	infostream << "Dedicated server quitting" << std::endl;
#if USE_CURL
	if (g_settings->getBool("server_announce"))
		ServerList::sendAnnounce(ServerList::AA_DELETE,
			server.m_bind_addr.getPort());
#endif

	if (profiler_print_interval > 0) {
		infostream << "Profiler:" << std::endl;
		g_profiler->print(infostream);
		g_profiler->clear();
	}
}

/*
 * Mod channels
 */


bool Server::joinModChannel(const std::string &channel)
{
	return m_modchannel_mgr->joinChannel(channel, PEER_ID_SERVER) &&
			m_modchannel_mgr->setChannelState(channel, MODCHANNEL_STATE_READ_WRITE);
}

bool Server::leaveModChannel(const std::string &channel)
{
	return m_modchannel_mgr->leaveChannel(channel, PEER_ID_SERVER);
}

bool Server::sendModChannelMessage(const std::string &channel, const std::string &message)
{
	if (!m_modchannel_mgr->canWriteOnChannel(channel))
		return false;

	broadcastModChannelMessage(channel, message, PEER_ID_SERVER);
	return true;
}

ModChannel* Server::getModChannel(const std::string &channel)
{
	return m_modchannel_mgr->getModChannel(channel);
}

void Server::broadcastModChannelMessage(const std::string &channel,
		const std::string &message, session_t from_peer)
{
	const std::vector<u16> &peers = m_modchannel_mgr->getChannelPeers(channel);
	if (peers.empty())
		return;

	if (message.size() > STRING_MAX_LEN) {
		warningstream << "ModChannel message too long, dropping before sending "
				<< " (" << message.size() << " > " << STRING_MAX_LEN << ", channel: "
				<< channel << ")" << std::endl;
		return;
	}

	std::string sender;
	if (from_peer != PEER_ID_SERVER) {
		sender = getPlayerName(from_peer);
	}

	NetworkPacket resp_pkt(TOCLIENT_MODCHANNEL_MSG,
			2 + channel.size() + 2 + sender.size() + 2 + message.size());
	resp_pkt << channel << sender << message;
	for (session_t peer_id : peers) {
		// Ignore sender
		if (peer_id == from_peer)
			continue;

		Send(peer_id, &resp_pkt);
	}

	if (from_peer != PEER_ID_SERVER) {
		m_script->on_modchannel_message(channel, sender, message);
	}
}

Translations *Server::getTranslationLanguage(const std::string &lang_code)
{
	if (lang_code.empty())
		return nullptr;

	auto it = server_translations.find(lang_code);
	if (it != server_translations.end())
		return &it->second; // Already loaded

	// [] will create an entry
	auto *translations = &server_translations[lang_code];

	for (const auto &i : m_media) {
		if (Translations::getFileLanguage(i.first) == lang_code) {
			std::string data;
			if (fs::ReadFile(i.second.path, data, true)) {
				translations->loadTranslation(i.first, data);
			}
		}
	}

	return translations;
}

std::unordered_map<std::string, std::string> Server::getMediaList()
{
	EnvAutoLock envlock(this);

	std::unordered_map<std::string, std::string> ret;
	for (auto &it : m_media) {
		if (it.second.no_announce)
			continue;
		ret.emplace(it.second.sha1_digest, it.second.path);
	}
	return ret;
}

ModStorageDatabase *Server::openModStorageDatabase(const std::string &world_path)
{
	std::string world_mt_path = world_path + DIR_DELIM + "world.mt";
	Settings world_mt;
	if (!world_mt.readConfigFile(world_mt_path.c_str()))
		throw BaseException("Cannot read world.mt!");

	std::string backend = world_mt.exists("mod_storage_backend") ?
		world_mt.get("mod_storage_backend") : "files";
	if (backend == "files")
		warningstream << "/!\\ You are using the old mod storage files backend. "
			<< "This backend is deprecated and may be removed in a future release /!\\"
			<< std::endl << "Switching to SQLite3 is advised, "
			<< "please read https://wiki.luanti.org/Database_backends." << std::endl;

	return openModStorageDatabase(backend, world_path, world_mt);
}

ModStorageDatabase *Server::openModStorageDatabase(const std::string &backend,
		const std::string &world_path, const Settings &world_mt)
{
	if (backend == "sqlite3")
		return new ModStorageDatabaseSQLite3(world_path);

#if USE_POSTGRESQL
	if (backend == "postgresql") {
		std::string connect_string;
		world_mt.getNoEx("pgsql_mod_storage_connection", connect_string);
		return new ModStorageDatabasePostgreSQL(connect_string);
	}
#endif // USE_POSTGRESQL

	if (backend == "files")
		return new ModStorageDatabaseFiles(world_path);

	if (backend == "dummy")
		return new Database_Dummy();

	throw BaseException("Mod storage database backend " + backend + " not supported");
}

bool Server::migrateModStorageDatabase(const GameParams &game_params, const Settings &cmd_args)
{
	std::string migrate_to = cmd_args.get("migrate-mod-storage");
	Settings world_mt;
	std::string world_mt_path = game_params.world_path + DIR_DELIM + "world.mt";
	if (!world_mt.readConfigFile(world_mt_path.c_str())) {
		errorstream << "Cannot read world.mt!" << std::endl;
		return false;
	}

	std::string backend = world_mt.exists("mod_storage_backend") ?
		world_mt.get("mod_storage_backend") : "files";
	if (backend == migrate_to) {
		errorstream << "Cannot migrate: new backend is same"
			<< " as the old one" << std::endl;
		return false;
	}

	ModStorageDatabase *srcdb = nullptr;
	ModStorageDatabase *dstdb = nullptr;

	bool succeeded = false;

	try {
		srcdb = Server::openModStorageDatabase(backend, game_params.world_path, world_mt);
		dstdb = Server::openModStorageDatabase(migrate_to, game_params.world_path, world_mt);

		dstdb->beginSave();

		std::vector<std::string> mod_list;
		srcdb->listMods(&mod_list);
		for (const std::string &modname : mod_list) {
			StringMap meta;
			srcdb->getModEntries(modname, &meta);
			for (const auto &pair : meta) {
				dstdb->setModEntry(modname, pair.first, pair.second);
			}
		}

		dstdb->endSave();

		succeeded = true;

		actionstream << "Successfully migrated the metadata of "
			<< mod_list.size() << " mods" << std::endl;
		world_mt.set("mod_storage_backend", migrate_to);
		if (!world_mt.updateConfigFile(world_mt_path.c_str()))
			errorstream << "Failed to update world.mt!" << std::endl;
		else
			actionstream << "world.mt updated" << std::endl;

	} catch (BaseException &e) {
		errorstream << "An error occurred during migration: " << e.what() << std::endl;
	}

	delete srcdb;
	delete dstdb;

	if (succeeded && backend == "files") {
		// Back up files
		const std::string storage_path = game_params.world_path + DIR_DELIM + "mod_storage";
		const std::string backup_path = game_params.world_path + DIR_DELIM + "mod_storage.bak";
		if (!fs::Rename(storage_path, backup_path))
			warningstream << "After migration, " << storage_path
				<< " could not be renamed to " << backup_path << std::endl;
	}

	return succeeded;
}

u16 Server::getProtocolVersionMin()
{
	u16 min_proto = g_settings->getU16("protocol_version_min");
	if (g_settings->getBool("strict_protocol_version_checking"))
		min_proto = LATEST_PROTOCOL_VERSION;
	return rangelim(min_proto,
		SERVER_PROTOCOL_VERSION_MIN,
		LATEST_PROTOCOL_VERSION);
}

u16 Server::getProtocolVersionMax()
{
	return LATEST_PROTOCOL_VERSION;
}
