// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irr_v3d.h"                   // for irrlicht datatypes

#include "network/address.h"
#include "network/networkprotocol.h" // session_t
#include "porting.h"
#include "threading/mutex_auto_lock.h"
#include "clientdynamicinfo.h"

#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class EmergeManager;
class MapBlock;
class NetworkPacket;
class ServerEnvironment;

/*
 * State Transitions

      Start
  (peer connect)
        |
        v
      /-----------------\
      |                 |
      |    Created      |
      |                 |
      \-----------------/
               |                  depending of the incoming packet
               ----------------------------------------
                                                      v
                                       +-----------------------------+
                                       |IN:                          |
                                       | TOSERVER_INIT               |
                                       +-----------------------------+
                                                      | invalid playername
                                                      | or denied by mod
                                                      v
                                       +-----------------------------+
                                       |OUT:                         |
                                       | TOCLIENT_HELLO              |
                                       +-----------------------------+
                                                      |
                                                      |
                                                      v
      /-----------------\                    /-----------------\
      |                 |                    |                 |
      |  AwaitingInit2  |<---------          |    HelloSent    |
      |                 |         |          |                 |
      \-----------------/         |          \-----------------/
               |                  |                   |
+-----------------------------+   |    *-----------------------------*     Auth fails
|IN:                          |   |    |Authentication, depending on |------------------
| TOSERVER_INIT2              |   |    | packet sent by client       |                 |
+-----------------------------+   |    *-----------------------------*                 |
               |                  |                   |                                |
               |                  |                   | Authentication                 |
               v                  |                   |  successful                    |
      /-----------------\         |                   v                                |
      |                 |         |    +-----------------------------+                 |
      |    InitDone     |         |    |OUT:                         |                 |
      |                 |         |    | TOCLIENT_AUTH_ACCEPT        |                 |
      \-----------------/         |    +-----------------------------+                 |
               |                  |                   |                                |
+-----------------------------+   ---------------------                                |
|OUT:                         |                                                        |
| TOCLIENT_MOVEMENT           |                                                        |
| TOCLIENT_ITEMDEF            |                                                        |
| TOCLIENT_NODEDEF            |                                                        |
| TOCLIENT_ANNOUNCE_MEDIA     |                                                        |
| TOCLIENT_DETACHED_INVENTORY |                                                        |
| TOCLIENT_TIME_OF_DAY        |                                                        |
+-----------------------------+                                                        |
               |                                                                       |
               |                                                                       |
               |      -----------------------------                                    |
               v      |                           |                                    |
      /-----------------\                         v                                    |
      |                 |             +-----------------------------+                  |
      | DefinitionsSent |             |IN:                          |                  |
      |                 |             | TOSERVER_REQUEST_MEDIA      |                  |
      \-----------------/             |                             |                  |
               |                      +-----------------------------+                  |
               |      ^                           |                                    |
               |      -----------------------------                                    |
               v                                                                       v
+-----------------------------+                        --------------------------------+
|IN:                          |                        |                               ^
| TOSERVER_CLIENT_READY       |                        v                               |
+-----------------------------+            +------------------------+                  |
               |                           |OUT:                    |                  |
               v                           | TOCLIENT_ACCESS_DENIED |                  |
+-----------------------------+            +------------------------+                  |
|OUT:                         |                        |                               |
| TOCLIENT_MOVE_PLAYER        |                        v                               |
| TOCLIENT_PRIVILEGES         |                /-----------------\                     |
| TOCLIENT_INVENTORY_FORMSPEC |                |                 |                     |
| UpdateCrafting              |                |     Denied      |                     |
| TOCLIENT_INVENTORY          |                |                 |                     |
| TOCLIENT_HP (opt)           |                \-----------------/                     |
| TOCLIENT_BREATH             |                                                        |
| TOCLIENT_DEATHSCREEN_LEGACY |                                                        |
+-----------------------------+                                                        |
              |                                                                        |
              v                                                                        |
      /-----------------\      async mod action (ban, kick)                            |
      |                 |---------------------------------------------------------------
 ---->|     Active      |
 |    |                 |----------------------------------------------
 |    \-----------------/      timeout                                v
 |       |           |                                  +-----------------------------+
 |       |           |                                  |OUT:                         |
 |       |           |                                  | TOCLIENT_DISCONNECT         |
 |       |           |                                  +-----------------------------+
 |       |           |                                                |
 |       |           v                                                v
 |       |  +-----------------------------+                    /-----------------\
 |       |  |IN:                          |                    |                 |
 |       |  | TOSERVER_DISCONNECT         |------------------->|  Disconnecting  |
 |       |  +-----------------------------+                    |                 |
 |       |                                                     \-----------------/
 |       | any auth packet which was
 |       | allowed in TOCLIENT_AUTH_ACCEPT
 |       v
 |    *-----------------------------* Auth      +-------------------------------+
 |    |Authentication, depending on | succeeds  |OUT:                           |
 |    | packet sent by client       |---------->| TOCLIENT_ACCEPT_SUDO_MODE     |
 |    *-----------------------------*           +-------------------------------+
 |                  |                                            |
 |                  | Auth fails                        /-----------------\
 |                  v                                   |                 |
 |    +-------------------------------+                 |    SudoMode     |
 |    |OUT:                           |                 |                 |
 |    | TOCLIENT_DENY_SUDO_MODE       |                 \-----------------/
 |    +-------------------------------+                          |
 |                  |                                            v
 |                  |                               +-----------------------------+
 |                  |    sets password accordingly  |IN:                          |
 -------------------+-------------------------------| TOSERVER_FIRST_SRP          |
                                                    +-----------------------------+

*/
namespace con {
	class IConnection;
}


// Also make sure to update the ClientInterface::statenames
// array when modifying these enums

enum ClientState
{
	CS_Invalid,
	CS_Disconnecting,
	CS_Denied,
	CS_Created,
	CS_HelloSent,
	CS_AwaitingInit2,
	CS_InitDone,
	CS_DefinitionsSent,
	CS_Active,
	CS_SudoMode
};

enum ClientStateEvent
{
	CSE_Hello,
	CSE_AuthAccept,
	CSE_GotInit2,
	CSE_SetDenied,
	CSE_SetDefinitionsSent,
	CSE_SetClientReady,
	CSE_SudoSuccess,
	CSE_SudoLeave,
	CSE_Disconnect
};

/*
	Used for queueing and sorting block transfers in containers

	Lower priority number means higher priority.
*/
struct PrioritySortedBlockTransfer
{
	PrioritySortedBlockTransfer(float a_priority, const v3s16 &a_pos, session_t a_peer_id)
	{
		priority = a_priority;
		pos = a_pos;
		peer_id = a_peer_id;
	}
	bool operator < (const PrioritySortedBlockTransfer &other) const
	{
		return priority < other.priority;
	}
	float priority;
	v3s16 pos;
	session_t peer_id;
};

class RemoteClient
{
public:
	// peer_id=0 means this client has no associated peer
	// NOTE: If client is made allowed to exist while peer doesn't,
	//       this has to be set to 0 when there is no peer.
	//       Also, the client must be moved to some other container.
	session_t peer_id = PEER_ID_INEXISTENT;
	// The serialization version to use with the client
	u8 serialization_version;
	//
	u16 net_proto_version = 0;

	/* Authentication information */
	std::string enc_pwd = "";
	bool create_player_on_auth_success = false;
	AuthMechanism chosen_mech  = AUTH_MECHANISM_NONE;
	void *auth_data = nullptr;
	u32 allowed_auth_mechs = 0;

	void resetChosenMech();

	bool isMechAllowed(AuthMechanism mech)
	{ return allowed_auth_mechs & mech; }

	void setEncryptedPassword(const std::string& pwd);

	RemoteClient();
	~RemoteClient() = default;

	/*
		Finds block that should be sent next to the client.
		Environment should be locked when this is called.
		dtime is used for resetting send radius at slow interval
	*/
	void GetNextBlocks(ServerEnvironment *env, EmergeManager* emerge,
			float dtime, std::vector<PrioritySortedBlockTransfer> &dest);

	void GotBlock(v3s16 p);

	void SentBlock(v3s16 p);

	void SetBlockNotSent(v3s16 p, bool low_priority = false);
	void SetBlocksNotSent(const std::vector<v3s16> &blocks, bool low_priority = false);

	/**
	 * tell client about this block being modified right now.
	 * this information is required to requeue the block in case it's "on wire"
	 * while modification is processed by server
	 * @param p position of modified block
	 */
	void ResendBlockIfOnWire(v3s16 p);

	u32 getSendingCount() const { return m_blocks_sending.size(); }

	bool isBlockSent(v3s16 p) const
	{
		return m_blocks_sent.find(p) != m_blocks_sent.end();
	}

	bool markMediaSent(const std::string &name) {
		auto insert_result = m_media_sent.emplace(name);
		return insert_result.second; // true = was inserted
	}

	void PrintInfo(std::ostream &o)
	{
		o << "RemoteClient " << peer_id << ": "
			<<"blocks_sent=" << m_blocks_sent.size()
			<<", blocks_sending=" << m_blocks_sending.size()
			<<", nearest_unsent_d=" << m_nearest_unsent_d
			<<", map_send_completion_timer=" << (int)(m_map_send_completion_timer + 0.5f)
			<<", excess_gotblocks=" << m_excess_gotblocks;
		m_excess_gotblocks = 0;
	}

	// Time from last placing or removing blocks
	float m_time_from_building = 9999;

	/*
		List of active objects that the client knows of.
	*/
	std::set<u16> m_known_objects;

	ClientState getState() const { return m_state; }

	const std::string &getName() const { return m_name; }

	void setName(const std::string &name) { m_name = name; }

	/* update internal client state */
	void notifyEvent(ClientStateEvent event);

	/* set expected serialization version */
	void setPendingSerializationVersion(u8 version)
		{ m_pending_serialization_version = version; }

	void confirmSerializationVersion()
		{ serialization_version = m_pending_serialization_version; }

	/* get uptime */
	u64 uptime() const { return porting::getTimeS() - m_connection_time; }

	/* set version information */
	void setVersionInfo(u8 major, u8 minor, u8 patch, const std::string &full);

	/* read version information */
	u8 getMajor() const { return m_version_major; }
	u8 getMinor() const { return m_version_minor; }
	u8 getPatch() const { return m_version_patch; }
	const std::string &getFullVer() const { return m_full_version; }

	void setLangCode(const std::string &code);
	const std::string &getLangCode() const { return m_lang_code; }

	void setCachedAddress(const Address &addr) { m_addr = addr; }
	const Address &getAddress() const { return m_addr; }

	void setDynamicInfo(const ClientDynamicInfo &info) { m_dynamic_info = info; }
	const ClientDynamicInfo &getDynamicInfo() const { return m_dynamic_info; }

private:
	// Version is stored in here after INIT before INIT2
	u8 m_pending_serialization_version;

	/* current state of client */
	ClientState m_state = CS_Created;

	// Cached here so retrieval doesn't have to go to connection API
	Address m_addr;

	// Client-sent language code
	std::string m_lang_code;

	// Client-sent dynamic info
	ClientDynamicInfo m_dynamic_info{};

	/*
		Blocks that have been sent to client.
		- These don't have to be sent again.
		- A block is cleared from here when client says it has
		  deleted it from it's memory

		List of block positions.
		No MapBlock* is stored here because the blocks can get deleted.
	*/
	std::unordered_set<v3s16> m_blocks_sent;

	/*
		Cache of blocks that have been occlusion culled at the current distance.
		As GetNextBlocks traverses the same distance multiple times, this saves
		significant CPU time.
	 */
	std::unordered_set<v3s16> m_blocks_occ;

	s16 m_nearest_unsent_d = 0;
	v3s16 m_last_center;
	v3f m_last_camera_dir;

	const u16 m_max_simul_sends;
	const float m_min_time_from_building;
	const s16 m_max_send_distance;
	const s16 m_block_optimize_distance;
	const s16 m_block_cull_optimize_distance;
	const s16 m_max_gen_distance;
	const bool m_occ_cull;

	/*
		Set of media files the client has already requested
		We won't send the same file twice to avoid bandwidth consumption attacks.
	*/
	std::unordered_set<std::string> m_media_sent;

	/*
		Blocks that are currently on the line.
		This is used for throttling the sending of blocks.
		- The size of this list is limited to some value
		Block is added when it is sent with BLOCKDATA.
		Block is removed when GOTBLOCKS is received.
	*/
	std::unordered_set<v3s16> m_blocks_sending;

	/*
		Count of excess GotBlocks().
		There is an excess amount because the client sometimes
		gets a block so late that the server sends it again,
		and the client then sends two GOTBLOCKs.
		This is reset by PrintInfo()
	*/
	u32 m_excess_gotblocks = 0;

	// CPU usage optimization
	float m_nothing_to_send_pause_timer = 0.0f;

	// measure how long it takes the server to send the complete map
	float m_map_send_completion_timer = 0.0f;

	/*
		name of player using this client
	*/
	std::string m_name = "";

	/*
		client information
	*/
	u8 m_version_major = 0;
	u8 m_version_minor = 0;
	u8 m_version_patch = 0;

	std::string m_full_version = "unknown";

	/*
		time this client was created
	 */
	const u64 m_connection_time = porting::getTimeS();
};

typedef std::unordered_map<u16, RemoteClient*> RemoteClientMap;

class ClientInterface {
public:

	friend class Server;

	ClientInterface(const std::shared_ptr<con::IConnection> &con);
	~ClientInterface();

	/* run sync step */
	void step(float dtime);

	/* get list of active client id's */
	std::vector<session_t> getClientIDs(ClientState min_state=CS_Active);

	/* mark blocks as not sent on all active clients */
	void markBlocksNotSent(const std::vector<v3s16> &positions, bool low_priority = false);

	/* verify if server user limit was reached */
	bool isUserLimitReached();

	/* get list of client player names */
	const std::vector<std::string> &getPlayerNames() const { return m_clients_names; }

	/* send to one client */
	void send(session_t peer_id, NetworkPacket *pkt);

	/* send to one client, deviating from the standard params */
	void sendCustom(session_t peer_id, u8 channel, NetworkPacket *pkt, bool reliable);

	/* send to all clients */
	void sendToAll(NetworkPacket *pkt, ClientState state_min = CS_Active);

	/* delete a client */
	void DeleteClient(session_t peer_id);

	/* create client */
	void CreateClient(session_t peer_id);

	/* get a client by peer_id */
	RemoteClient *getClientNoEx(session_t peer_id,  ClientState state_min = CS_Active);

	/* get client by peer_id (make sure you have list lock before!*/
	RemoteClient *lockedGetClientNoEx(session_t peer_id,  ClientState state_min = CS_Active);

	/* get state of client by id*/
	ClientState getClientState(session_t peer_id);

	/* get protocol version of client */
	u16 getProtocolVersion(session_t peer_id);

	/* event to update client state */
	void event(session_t peer_id, ClientStateEvent event);

	/* Set environment. Do not call this function if environment is already set */
	void setEnv(ServerEnvironment *env)
	{
		assert(m_env == NULL); // pre-condition
		m_env = env;
	}

	static const char *state2Name(ClientState state);

protected:
	class AutoLock {
	public:
		AutoLock(ClientInterface &iface): m_lock(iface.m_clients_mutex) {}

	private:
		RecursiveMutexAutoLock m_lock;
	};

	RemoteClientMap& getClientList() { return m_clients; }

private:
	/* update internal player list */
	void UpdatePlayerList();

	// Connection
	std::shared_ptr<con::IConnection> m_con;

	// FIXME?: as far as I can tell this lock is pointless because only the server
	// thread ever touches the clients. Consider how getClientNoEx() returns
	// a raw pointer too.
	std::recursive_mutex m_clients_mutex;
	// Connected clients (behind the mutex)
	RemoteClientMap m_clients;
	std::vector<std::string> m_clients_names; // for announcing to server list

	// Environment
	ServerEnvironment *m_env = nullptr;

	float m_print_info_timer = 0;
	float m_check_linger_timer = 0;

	static const char *statenames[];

	// Note that this puts a fixed timeout on the init & auth phase for a client.
	// (lingering is enforced until CS_InitDone)
	static constexpr int LINGER_TIMEOUT = 12;
};
