// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irr_v3d.h"
#include "map.h"
#include "hud.h"
#include "gamedef.h"
#include "content/mods.h"
#include "inventorymanager.h"
#include "content/subgames.h"
#include "network/peerhandler.h"
#include "network/connection.h"
#include "util/numeric.h"
#include "util/thread.h"
#include "util/basic_macros.h"
#include "util/metricsbackend.h"
#include "serverenvironment.h"
#include "server/clientiface.h"
#include "threading/ordered_mutex.h"
#include "chatmessage.h"
#include "sound.h"
#include "translation.h"
#include "script/common/c_types.h" // LuaError
#include <atomic>
#include <csignal>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <unordered_set>
#include <optional>
#include <string_view>
#include <shared_mutex>
#include <condition_variable>

class ChatEvent;
struct ChatEventChat;
struct ChatInterface;
class IWritableItemDefManager;
class NodeDefManager;
class IWritableCraftDefManager;
class BanManager;
class Inventory;
class ModChannelMgr;
class RemotePlayer;
class Player;
class PlayerSAO;
struct PlayerHPChangeReason;
class IRollbackManager;
struct RollbackAction;
class EmergeManager;
class ServerScripting;
class ServerEnvironment;
struct SoundSpec;
struct CloudParams;
struct SkyboxParams;
struct SunParams;
struct MoonParams;
struct StarParams;
struct Lighting;
class ServerThread;
class ServerModManager;
class ServerInventoryManager;
struct PackedValue;
struct ParticleParameters;
struct ParticleSpawnerParameters;

// Anticheat flags
enum {
	AC_DIGGING     = 0x01,
	AC_INTERACTION = 0x02,
	AC_MOVEMENT    = 0x04
};

constexpr const static FlagDesc flagdesc_anticheat[] = {
	{"digging",     AC_DIGGING},
	{"interaction", AC_INTERACTION},
	{"movement",    AC_MOVEMENT},
	{NULL,          0}
};

enum ClientDeletionReason {
	CDR_LEAVE,
	CDR_TIMEOUT,
	CDR_DENY
};

struct MediaInfo
{
	std::string path;
	std::string sha1_digest;
	// true = not announced in TOCLIENT_ANNOUNCE_MEDIA (at player join)
	bool no_announce;
	// if true, this is an ephemeral entry. used by dynamic media.
	bool ephemeral;
	// does what it says. used by some cases of dynamic media.
	bool delete_at_shutdown;

	MediaInfo(std::string_view path_ = "",
	          std::string_view sha1_digest_ = ""):
		path(path_),
		sha1_digest(sha1_digest_),
		no_announce(false),
		ephemeral(false),
		delete_at_shutdown(false)
	{
	}
};

// Combines the pure sound (SoundSpec) with positional information
struct ServerPlayingSound
{
	SoundLocation type = SoundLocation::Local;

	float gain = 1.0f; // for amplification of the base sound
	float max_hear_distance = 32 * BS;
	v3f pos;
	u16 object = 0;
	std::string to_player;
	std::string exclude_player;

	v3f getPos(ServerEnvironment *env, bool *pos_exists) const;

	SoundSpec spec;

	std::unordered_set<session_t> clients; // peer ids
};

struct MinimapMode {
	MinimapType type = MINIMAP_TYPE_OFF;
	std::string label;
	u16 size = 0;
	std::string texture;
	u16 scale = 1;
};

// structure for everything getClientInfo returns, for convenience
struct ClientInfo {
	ClientState state;
	Address addr;
	u32 uptime;
	u8 ser_vers;
	u16 prot_vers;
	u8 major, minor, patch;
	std::string vers_string, lang_code;
};

struct ModIPCStore {
	ModIPCStore() = default;
	~ModIPCStore();

	/// RW lock for this entire structure
	std::shared_mutex mutex;
	/// Signalled on any changes to the map contents
	std::condition_variable_any condvar;
	/**
	 * Map storing the data
	 *
	 * @note Do not store `nil` data in this map, instead remove the whole key.
	 */
	std::unordered_map<std::string, std::unique_ptr<PackedValue>> map;

	/// @note Should be called without holding the lock.
	inline void signal() { condvar.notify_all(); }
};

class Server : public con::PeerHandler, public MapEventReceiver,
		public IGameDef
{
public:
	/*
		NOTE: Every public method should be thread-safe
	*/

	Server(
		const std::string &path_world,
		const SubgameSpec &gamespec,
		bool simple_singleplayer_mode,
		Address bind_addr,
		bool dedicated,
		ChatInterface *iface = nullptr,
		std::string *shutdown_errmsg = nullptr
	);
	~Server();
	DISABLE_CLASS_COPY(Server);

	void start();
	void stop();
	// Actual processing is done in another thread.
	// This just checks if there was an error in that thread.
	void step();

	// This is run by ServerThread and does the actual processing
	void AsyncRunStep(float dtime, bool initial_step = false);
	/// Receive and process all incoming packets. Sleep if the time goal isn't met.
	/// @param min_time minimum time to take [s]
	void Receive(float min_time);
	void yieldToOtherThreads(float dtime);

	// Full player initialization after they processed all static media
	// This is a helper function for TOSERVER_CLIENT_READY
	PlayerSAO *StageTwoClientInit(session_t peer_id);

	/*
	 * Command Handlers
	 */

	void handleCommand(NetworkPacket* pkt);

	void handleCommand_Null(NetworkPacket* pkt) {};
	void handleCommand_Deprecated(NetworkPacket* pkt);
	void handleCommand_Init(NetworkPacket* pkt);
	void handleCommand_Init2(NetworkPacket* pkt);
	void handleCommand_ModChannelJoin(NetworkPacket *pkt);
	void handleCommand_ModChannelLeave(NetworkPacket *pkt);
	void handleCommand_ModChannelMsg(NetworkPacket *pkt);
	void handleCommand_RequestMedia(NetworkPacket* pkt);
	void handleCommand_ClientReady(NetworkPacket* pkt);
	void handleCommand_GotBlocks(NetworkPacket* pkt);
	void handleCommand_PlayerPos(NetworkPacket* pkt);
	void handleCommand_DeletedBlocks(NetworkPacket* pkt);
	void handleCommand_InventoryAction(NetworkPacket* pkt);
	void handleCommand_ChatMessage(NetworkPacket* pkt);
	void handleCommand_Damage(NetworkPacket* pkt);
	void handleCommand_PlayerItem(NetworkPacket* pkt);
	void handleCommand_Interact(NetworkPacket* pkt);
	void handleCommand_RemovedSounds(NetworkPacket* pkt);
	void handleCommand_NodeMetaFields(NetworkPacket* pkt);
	void handleCommand_InventoryFields(NetworkPacket* pkt);
	void handleCommand_FirstSrp(NetworkPacket* pkt);
	void handleCommand_SrpBytesA(NetworkPacket* pkt);
	void handleCommand_SrpBytesM(NetworkPacket* pkt);
	void handleCommand_HaveMedia(NetworkPacket *pkt);
	void handleCommand_UpdateClientInfo(NetworkPacket *pkt);

	void ProcessData(NetworkPacket *pkt);

	void Send(NetworkPacket *pkt);
	void Send(session_t peer_id, NetworkPacket *pkt);

	// Helper for handleCommand_PlayerPos and handleCommand_Interact
	void process_PlayerPos(RemotePlayer *player, PlayerSAO *playersao,
		NetworkPacket *pkt);

	// Both setter and getter need no envlock,
	// can be called freely from threads
	void setTimeOfDay(u32 time);

	/*
		Shall be called with the environment locked.
		This is accessed by the map, which is inside the environment,
		so it shouldn't be a problem.
	*/
	void onMapEditEvent(const MapEditEvent &event) override;

	// Connection must be locked when called
	std::string getStatusString();
	inline double getUptime() const { return m_uptime_counter->get(); }

	// read shutdown state
	inline bool isShutdownRequested() const { return m_shutdown_state.is_requested; }

	// request server to shutdown
	void requestShutdown(const std::string &msg, bool reconnect, float delay = 0.0f);

	// Returns -1 if failed, sound handle on success
	// Envlock
	s32 playSound(ServerPlayingSound &params, bool ephemeral=false);
	void stopSound(s32 handle);
	void fadeSound(s32 handle, float step, float gain);

	// Envlock
	std::set<std::string> getPlayerEffectivePrivs(const std::string &name);
	bool checkPriv(const std::string &name, const std::string &priv);
	void reportPrivsModified(const std::string &name=""); // ""=all
	void reportInventoryFormspecModified(const std::string &name);
	void reportFormspecPrependModified(const std::string &name);

	void setIpBanned(const std::string &ip, const std::string &name);
	void unsetIpBanned(const std::string &ip_or_name);
	std::string getBanDescription(const std::string &ip_or_name);
	bool denyIfBanned(session_t peer_id);

	void notifyPlayer(const char *name, const std::wstring &msg);
	void notifyPlayers(const std::wstring &msg);

	void spawnParticle(const std::string &playername,
		const ParticleParameters &p);

	u32 addParticleSpawner(const ParticleSpawnerParameters &p,
		ServerActiveObject *attached, const std::string &to_player,
		const std::string &exclude_player);

	void deleteParticleSpawner(const std::string &playername, u32 id);

	struct DynamicMediaArgs {
		std::string filename;
		std::optional<std::string> filepath;
		std::optional<std::string_view> data;
		u32 token;
		std::string to_player;
		bool ephemeral = false;
		bool client_cache = true;
	};
	bool dynamicAddMedia(const DynamicMediaArgs &args);

	ServerInventoryManager *getInventoryMgr() const { return m_inventory_mgr.get(); }
	void sendDetachedInventory(Inventory *inventory, const std::string &name, session_t peer_id);

	// Envlock and conlock should be locked when using scriptapi
	inline ServerScripting *getScriptIface() { return m_script.get(); }

	// actions: time-reversed list
	// Return value: success/failure
	bool rollbackRevertActions(const std::list<RollbackAction> &actions,
			std::list<std::string> *log);

	// IGameDef interface
	bool isClient() override { return false; }
	// Under envlock
	IItemDefManager* getItemDefManager() override;
	const NodeDefManager* getNodeDefManager() override;
	ICraftDefManager* getCraftDefManager() override;
	u16 allocateUnknownNodeId(const std::string &name) override;
	IRollbackManager *getRollbackManager() override { return m_rollback; }
	EmergeManager *getEmergeManager() { return m_emerge.get(); }
	ModStorageDatabase *getModStorageDatabase() override { return m_mod_storage_database; }

	IWritableItemDefManager* getWritableItemDefManager();
	NodeDefManager* getWritableNodeDefManager();
	IWritableCraftDefManager* getWritableCraftDefManager();

	// Not under envlock
	const std::vector<ModSpec> &getMods() const override;
	const ModSpec* getModSpec(const std::string &modname) const override;
	const SubgameSpec* getGameSpec() const override { return &m_gamespec; }
	static std::string getBuiltinLuaPath();
	std::string getWorldPath() const override { return m_path_world; }
	std::string getModDataPath() const override { return m_path_mod_data; }
	ModIPCStore *getModIPCStore() override { return &m_ipcstore; }

	inline bool isSingleplayer() const
			{ return m_simple_singleplayer_mode; }

	struct StepSettings {
		float steplen;
		bool pause;
	};

	void setStepSettings(StepSettings spdata) { m_step_settings.store(spdata); }
	StepSettings getStepSettings() { return m_step_settings.load(); }

	void setAsyncFatalError(const std::string &error);
	inline void setAsyncFatalError(const LuaError &e)
	{
		setAsyncFatalError(std::string("Lua: ") + e.what());
	}

	// Not thread-safe.
	void addShutdownError(const ModError &e);

	bool showFormspec(const char *name, const std::string &formspec, const std::string &formname);
	Map & getMap() { return m_env->getMap(); }
	ServerEnvironment & getEnv() { return *m_env; }
	v3f findSpawnPos();

	u32 hudAdd(RemotePlayer *player, HudElement *element);
	bool hudRemove(RemotePlayer *player, u32 id);
	bool hudChange(RemotePlayer *player, u32 id, HudElementStat stat, void *value);
	bool hudSetFlags(RemotePlayer *player, u32 flags, u32 mask);
	bool hudSetHotbarItemcount(RemotePlayer *player, s32 hotbar_itemcount);
	void hudSetHotbarImage(RemotePlayer *player, const std::string &name);
	void hudSetHotbarSelectedImage(RemotePlayer *player, const std::string &name);

	/// @note this is only available for client state >= CS_HelloSent
	Address getPeerAddress(session_t peer_id);

	void setLocalPlayerAnimations(RemotePlayer *player, v2f animation_frames[4],
			f32 frame_speed);
	void setPlayerEyeOffset(RemotePlayer *player, v3f first, v3f third, v3f third_front);

	void setSky(RemotePlayer *player, const SkyboxParams &params);
	void setSun(RemotePlayer *player, const SunParams &params);
	void setMoon(RemotePlayer *player, const MoonParams &params);
	void setStars(RemotePlayer *player, const StarParams &params);

	void setClouds(RemotePlayer *player, const CloudParams &params);

	void overrideDayNightRatio(RemotePlayer *player, bool do_override, float brightness);

	void setLighting(RemotePlayer *player, const Lighting &lighting);

	/* con::PeerHandler implementation. */
	void peerAdded(con::IPeer *peer) override;
	void deletingPeer(con::IPeer *peer, bool timeout) override;

	void DenySudoAccess(session_t peer_id);
	void DenyAccess(session_t peer_id, AccessDeniedCode reason,
		std::string_view custom_reason = "", bool reconnect = false);
	void kickAllPlayers(AccessDeniedCode reason,
		const std::string &str_reason, bool reconnect);
	void acceptAuth(session_t peer_id, bool forSudoMode);
	void DisconnectPeer(session_t peer_id);
	bool getClientConInfo(session_t peer_id, con::rtt_stat_type type, float *retval);
	bool getClientInfo(session_t peer_id, ClientInfo &ret);
	const ClientDynamicInfo *getClientDynamicInfo(session_t peer_id);

	void printToConsoleOnly(const std::string &text);

	void HandlePlayerHPChange(PlayerSAO *sao, const PlayerHPChangeReason &reason);
	void SendPlayerHP(PlayerSAO *sao, bool effect);
	void SendPlayerBreath(PlayerSAO *sao);
	void SendInventory(RemotePlayer *player, bool incremental);
	void SendMovePlayer(PlayerSAO *sao);
	void SendMovePlayerRel(session_t peer_id, const v3f &added_pos);
	void SendPlayerSpeed(session_t peer_id, const v3f &added_vel);
	void SendPlayerFov(session_t peer_id);
	void SendCamera(session_t peer_id, Player *player);

	void SendMinimapModes(session_t peer_id,
			std::vector<MinimapMode> &modes,
			size_t wanted_mode);

	void sendDetachedInventories(session_t peer_id, bool incremental);

	bool joinModChannel(const std::string &channel) override;
	bool leaveModChannel(const std::string &channel) override;
	bool sendModChannelMessage(const std::string &channel, const std::string &message) override;
	ModChannel *getModChannel(const std::string &channel) override;

	// Send block to specific player only
	bool SendBlock(session_t peer_id, const v3s16 &blockpos);

	// Get or load translations for a language
	Translations *getTranslationLanguage(const std::string &lang_code);

	// Returns all media files the server knows about
	// map key = binary sha1, map value = file path
	std::unordered_map<std::string, std::string> getMediaList();

	static std::vector<std::string> getModStorageDatabaseBackends();

	static ModStorageDatabase *openModStorageDatabase(const std::string &world_path);

	static ModStorageDatabase *openModStorageDatabase(const std::string &backend,
			const std::string &world_path, const Settings &world_mt);

	static bool migrateModStorageDatabase(const GameParams &game_params,
			const Settings &cmd_args);

	static u16 getProtocolVersionMin();
	static u16 getProtocolVersionMax();

	// Lua files registered for init of async env, pair of modname + path
	std::vector<std::pair<std::string, std::string>> m_async_init_files;
	// Identical but for mapgen env
	std::vector<std::pair<std::string, std::string>> m_mapgen_init_files;

	// Data transferred into other Lua envs at init time
	std::unique_ptr<PackedValue> m_lua_globals_data;

	// Bind address
	Address m_bind_addr;

	// Public helper for taking the envlock in a scope
	class EnvAutoLock {
	public:
		EnvAutoLock(Server *server): m_lock(server->m_env_mutex) {}

	private:
		std::lock_guard<ordered_mutex> m_lock;
	};

protected:
	/* Do not add more members here, this is only required to make unit tests work. */

	// Scripting
	// Envlock and conlock should be locked when using Lua
	std::unique_ptr<ServerScripting> m_script;

	// Mods
	std::unique_ptr<ServerModManager> m_modmgr;

private:
	friend class EmergeThread;
	friend class RemoteClient;

	// unittest classes
	friend class TestServerShutdownState;
	friend class TestMoveAction;

	struct ShutdownState {
		friend class TestServerShutdownState;
		public:
			bool is_requested = false;
			bool should_reconnect = false;
			std::string message;

			void reset();
			void trigger(float delay, const std::string &msg, bool reconnect);
			void tick(float dtime, Server *server);
			std::wstring getShutdownTimerMessage() const;
			bool isTimerRunning() const { return m_timer > 0.0f; }
		private:
			float m_timer = 0.0f;
	};

	struct PendingDynamicMediaCallback {
		std::string filename; // only set if media entry and file is to be deleted
		float expiry_timer;
		std::unordered_set<session_t> waiting_players;
	};

	// The standard library does not implement std::hash for pairs so we have this:
	struct SBCHash {
		size_t operator() (const std::pair<v3s16, u16> &p) const {
			return std::hash<v3s16>()(p.first) ^ p.second;
		}
	};

	typedef std::unordered_map<std::pair<v3s16, u16>, std::string, SBCHash> SerializedBlockCache;

	void init();

	void SendMovement(session_t peer_id);
	void SendHP(session_t peer_id, u16 hp, bool effect);
	void SendBreath(session_t peer_id, u16 breath);
	void SendAccessDenied(session_t peer_id, AccessDeniedCode reason,
		std::string_view custom_reason, bool reconnect = false);
	void SendItemDef(session_t peer_id, IItemDefManager *itemdef, u16 protocol_version);
	void SendNodeDef(session_t peer_id, const NodeDefManager *nodedef,
		u16 protocol_version);


	virtual void SendChatMessage(session_t peer_id, const ChatMessage &message);
	void SendTimeOfDay(session_t peer_id, u16 time, f32 time_speed);

	void SendLocalPlayerAnimations(session_t peer_id, v2f animation_frames[4],
		f32 animation_speed);
	void SendEyeOffset(session_t peer_id, v3f first, v3f third, v3f third_front);
	void SendPlayerPrivileges(session_t peer_id);
	void SendPlayerInventoryFormspec(session_t peer_id);
	void SendPlayerFormspecPrepend(session_t peer_id);
	void SendShowFormspecMessage(session_t peer_id, const std::string &formspec,
		const std::string &formname);
	void SendHUDAdd(session_t peer_id, u32 id, HudElement *form);
	void SendHUDRemove(session_t peer_id, u32 id);
	void SendHUDChange(session_t peer_id, u32 id, HudElementStat stat, void *value);
	void SendHUDSetFlags(session_t peer_id, u32 flags, u32 mask);
	void SendHUDSetParam(session_t peer_id, u16 param, std::string_view value);
	void SendSetSky(session_t peer_id, const SkyboxParams &params);
	void SendSetSun(session_t peer_id, const SunParams &params);
	void SendSetMoon(session_t peer_id, const MoonParams &params);
	void SendSetStars(session_t peer_id, const StarParams &params);
	void SendCloudParams(session_t peer_id, const CloudParams &params);
	void SendOverrideDayNightRatio(session_t peer_id, bool do_override, float ratio);
	void SendSetLighting(session_t peer_id, const Lighting &lighting);

	void broadcastModChannelMessage(const std::string &channel,
			const std::string &message, session_t from_peer);

	/*
		Send a node removal/addition event to all clients except ignore_id.
		Additionally, if far_players!=NULL, players further away than
		far_d_nodes are ignored and their peer_ids are added to far_players
	*/
	// Envlock and conlock should be locked when calling these
	void sendRemoveNode(v3s16 p, std::unordered_set<u16> *far_players = nullptr,
			float far_d_nodes = 100);
	void sendAddNode(v3s16 p, MapNode n,
			std::unordered_set<u16> *far_players = nullptr,
			float far_d_nodes = 100, bool remove_metadata = true);
	void sendNodeChangePkt(NetworkPacket &pkt, v3s16 block_pos,
			v3f p, float far_d_nodes, std::unordered_set<u16> *far_players);

	void sendMetadataChanged(const std::unordered_set<v3s16> &positions,
			float far_d_nodes = 100);

	// Environment and Connection must be locked when called
	// `cache` may only be very short lived! (invalidation not handeled)
	void SendBlockNoLock(session_t peer_id, MapBlock *block, u8 ver,
		u16 net_proto_version, SerializedBlockCache *cache = nullptr);

	// Sends blocks to clients (locks env and con on its own)
	void SendBlocks(float dtime);

	bool addMediaFile(const std::string &filename, const std::string &filepath,
			std::string *filedata = nullptr, std::string *digest = nullptr);
	void fillMediaCache();
	void sendMediaAnnouncement(session_t peer_id, const std::string &lang_code);
	void sendRequestedMedia(session_t peer_id,
			const std::unordered_set<std::string> &tosend);
	void stepPendingDynMediaCallbacks(float dtime);

	/// @brief send particle spawner to a selection of clients
	void SendAddParticleSpawner(const std::string &to_player,
		const std::string &exclude_player,
		const ParticleSpawnerParameters &p, u16 attached_id, u32 id);
	/// @brief send particle spawner to one client (internal)
	void SendAddParticleSpawner(session_t peer_id, u16 protocol_version,
		const ParticleSpawnerParameters &p, u16 attached_id, u32 id);

	void SendDeleteParticleSpawner(session_t peer_id, u32 id);

	// Spawn particles for a specific client, batching them if clients support it.
	void SendSpawnParticles(RemotePlayer *player,
			const std::vector<ParticleParameters> &particles);
	// Spawn all particles for this step, batching them if clients support it.
	void SendSpawnParticles();

	void SendActiveObjectRemoveAdd(RemoteClient *client, PlayerSAO *playersao);
	void SendActiveObjectMessages(session_t peer_id, const std::string &datas,
		bool reliable = true);
	void SendCSMRestrictionFlags(session_t peer_id);

	/*
		Something random
	*/

	void HandlePlayerDeath(PlayerSAO* sao, const PlayerHPChangeReason &reason);
	void DeleteClient(session_t peer_id, ClientDeletionReason reason);
	void UpdateCrafting(RemotePlayer *player);
	bool checkInteractDistance(RemotePlayer *player, const f32 d, const std::string &what);

	void handleChatInterfaceEvent(ChatEvent *evt);

	/// @brief Checks if user limit allows a potential client to join
	/// @return true if the client can NOT join
	bool checkUserLimit(const std::string &player_name, const std::string &addr_s);

	// This returns the answer to the sender of wmessage, or "" if there is none
	std::wstring handleChat(const std::string &name, std::wstring wmessage_input,
		bool check_shout_priv = false, RemotePlayer *player = nullptr);
	void handleAdminChat(const ChatEventChat *evt);

	// When called, connection mutex should be locked
	RemoteClient* getClient(session_t peer_id, ClientState state_min = CS_Active);
	RemoteClient* getClientNoEx(session_t peer_id, ClientState state_min = CS_Active);

	// When called, environment mutex should be locked
	std::string getPlayerName(session_t peer_id);
	PlayerSAO *getPlayerSAO(session_t peer_id);

	/*
		Get a player from memory or creates one.
		If player is already connected, return NULL
		Does not verify/modify auth info and password.

		Call with env and con locked.
	*/
	std::unique_ptr<PlayerSAO> emergePlayer(const char *name, session_t peer_id,
		u16 proto_version);

	/*
		Variables
	*/

	// Environment mutex (envlock)
	ordered_mutex m_env_mutex;

	// World directory
	std::string m_path_world;
	std::string m_path_mod_data;
	// Subgame specification
	SubgameSpec m_gamespec;
	// If true, do not allow multiple players and hide some multiplayer
	// functionality
	bool m_simple_singleplayer_mode;
	u16 m_max_chatmessage_length;
	// For "dedicated" server list flag
	bool m_dedicated;

	// Game settings layer
	Settings *m_game_settings = nullptr;

	// Thread can set; step() will throw as ServerError
	MutexedVariable<std::string> m_async_fatal_error;

	// Some timers
	float m_time_of_day_send_timer = 0.0f;
	float m_liquid_transform_timer = 0.0f;
	float m_liquid_transform_every = 1.0f;
	float m_masterserver_timer = 0.0f;
	float m_emergethread_trigger_timer = 0.0f;
	float m_savemap_timer = 0.0f;
	IntervalLimiter m_map_timer_and_unload_interval;
	IntervalLimiter m_max_lag_decrease;

	// Environment
	ServerEnvironment *m_env = nullptr;

	// server connection
	std::shared_ptr<con::IConnection> m_con;

	// Ban checking
	BanManager *m_banmanager = nullptr;

	// Rollback manager (behind m_env_mutex)
	IRollbackManager *m_rollback = nullptr;

	// Emerge manager
	std::unique_ptr<EmergeManager> m_emerge;

	// Item definition manager
	IWritableItemDefManager *m_itemdef;

	// Node definition manager
	NodeDefManager *m_nodedef;

	// Craft definition manager
	IWritableCraftDefManager *m_craftdef;

	std::unordered_map<std::string, Translations> server_translations;

	ModIPCStore m_ipcstore;

	/*
		Threads
	*/
	// Set by Game
	std::atomic<StepSettings> m_step_settings{{0.1f, false}};

	// The server mainly operates in this thread
	ServerThread *m_thread = nullptr;

	/*
	 	Client interface
	*/
	ClientInterface m_clients;

	std::unordered_map<session_t, std::string> m_formspec_state_data;

	/*
		Random stuff
	*/

	ShutdownState m_shutdown_state;

	ChatInterface *m_admin_chat;
	std::string m_admin_nick;

	// If a mod error occurs while shutting down, the error message will be
	// written into this.
	std::string *const m_shutdown_errmsg;

	/*
		Map edit event queue. Automatically receives all map edits.
		The constructor of this class registers us to receive them through
		onMapEditEvent

		NOTE: Should these be moved to actually be members of
		ServerEnvironment?
	*/

	/*
		Queue of map edits from the environment for sending to the clients
		This is behind m_env_mutex
	*/
	std::queue<MapEditEvent*> m_unsent_map_edit_queue;
	/*
		If a non-empty area, map edit events contained within are left
		unsent. Done at map generation time to speed up editing of the
		generated area, as it will be sent anyway.
		This is behind m_env_mutex
	*/
	VoxelArea m_ignore_map_edit_events_area;

	// media files known to server
	std::unordered_map<std::string, MediaInfo> m_media;

	// pending dynamic media callbacks, clients inform the server when they have a file fetched
	std::unordered_map<u32, PendingDynamicMediaCallback> m_pending_dyn_media;
	float m_step_pending_dyn_media_timer = 0.0f;

	/*
		Sounds
	*/
	std::unordered_map<s32, ServerPlayingSound> m_playing_sounds;
	s32 m_playing_sounds_id_last_used = 0; // positive values only
	s32 nextSoundId();

	ModStorageDatabase *m_mod_storage_database = nullptr;
	float m_mod_storage_save_timer = 10.0f;

	// CSM restrictions byteflag
	u64 m_csm_restriction_flags = CSMRestrictionFlags::CSM_RF_NONE;
	u32 m_csm_restriction_noderange = 8;

	// ModChannel manager
	std::unique_ptr<ModChannelMgr> m_modchannel_mgr;

	// Inventory manager
	std::unique_ptr<ServerInventoryManager> m_inventory_mgr;

	// Global server metrics backend
	std::unique_ptr<MetricsBackend> m_metrics_backend;

	// Server metrics
	MetricCounterPtr m_uptime_counter;
	MetricGaugePtr m_player_gauge;
	MetricGaugePtr m_timeofday_gauge;
	MetricGaugePtr m_lag_gauge;
	MetricCounterPtr m_aom_buffer_counter[2]; // [0] = rel, [1] = unrel
	MetricCounterPtr m_packet_recv_counter;
	MetricCounterPtr m_packet_recv_processed_counter;
	MetricCounterPtr m_map_edit_event_counter;

	// Particles to send this server step
	// [playername] = list of params, empty playername for broadcast
	std::unordered_map<std::string, std::vector<ParticleParameters>> m_particles_to_send;
};

/*
	Runs a simple dedicated server loop.

	Shuts down when kill is set to true.
*/
void dedicated_server_loop(Server &server, volatile std::sig_atomic_t &kill);
