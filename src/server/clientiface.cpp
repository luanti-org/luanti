// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <sstream>
#include "clientiface.h"
#include "debug.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "network/serveropcodes.h"
#include "remoteplayer.h"
#include "serialization.h" // SER_FMT_VER_INVALID
#include "settings.h"
#include "mapblock.h"
#include "serverenvironment.h"
#include "map.h"
#include "emerge.h"
#include "server/luaentity_sao.h"
#include "server/player_sao.h"
#include "log.h"
#include "util/srp.h"
#include "util/string.h"
#include "face_position_cache.h"

static std::string string_sanitize_ascii(const std::string &s, u32 max_length)
{
	std::string out;
	for (char c : s) {
		if (out.size() >= max_length)
			break;
		if (c > 32 && c < 127)
			out.push_back(c);
	}
	return out;
}

const char *ClientInterface::statenames[] = {
	"Invalid",
	"Disconnecting",
	"Denied",
	"Created",
	"HelloSent",
	"AwaitingInit2",
	"InitDone",
	"DefinitionsSent",
	"Active",
	"SudoMode",
};

const char *ClientInterface::state2Name(ClientState state)
{
	return statenames[state];
}

RemoteClient::RemoteClient() :
	serialization_version(SER_FMT_VER_INVALID),
	m_pending_serialization_version(SER_FMT_VER_INVALID),
	m_max_simul_sends(g_settings->getU16("max_simultaneous_block_sends_per_client")),
	m_min_time_from_building(
		g_settings->getFloat("full_block_send_enable_min_time_from_building")),
	m_max_send_distance(g_settings->getS16("max_block_send_distance")),
	m_block_optimize_distance(g_settings->getS16("block_send_optimize_distance")),
	m_block_cull_optimize_distance(g_settings->getS16("block_cull_optimize_distance")),
	m_max_gen_distance(g_settings->getS16("max_block_generate_distance")),
	m_occ_cull(g_settings->getBool("server_side_occlusion_culling"))
{
}

void RemoteClient::ResendBlockIfOnWire(v3s16 p)
{
	// if this block is on wire, mark it for sending again as soon as possible
	if (m_blocks_sending.find(p) != m_blocks_sending.end()) {
		SetBlockNotSent(p);
	}
}

static LuaEntitySAO *getAttachedObject(PlayerSAO *sao, ServerEnvironment *env)
{
	ServerActiveObject *ao = sao;
	while (ao->getParent())
		ao = ao->getParent();

	return ao == sao ? nullptr : dynamic_cast<LuaEntitySAO*>(ao);
}

void RemoteClient::GetNextBlocks (
		ServerEnvironment *env,
		EmergeManager * emerge,
		float dtime,
		std::vector<PrioritySortedBlockTransfer> &dest)
{
	// Increment timers
	m_nothing_to_send_pause_timer -= dtime;
	m_map_send_completion_timer += dtime;

	if (m_map_send_completion_timer > g_settings->getFloat("server_unload_unused_data_timeout") * 0.8f) {
		infostream << "Server: Player " << m_name << ", peer_id=" << peer_id
				<< ": full map send is taking too long ("
				<< m_map_send_completion_timer
				<< "s), restarting to avoid visible blocks being unloaded."
				<< std::endl;
		m_map_send_completion_timer = 0.0f;
		m_nearest_unsent_d = 0;
	}

	if (m_nothing_to_send_pause_timer >= 0)
		return;

	RemotePlayer *player = env->getPlayer(peer_id);
	// This can happen sometimes; clients and players are not in perfect sync.
	if (!player)
		return;

	PlayerSAO *sao = player->getPlayerSAO();
	if (!sao)
		return;

	// Won't send anything if already sending
	if (m_blocks_sending.size() >= m_max_simul_sends) {
		//infostream<<"Not sending any blocks, Queue full."<<std::endl;
		return;
	}

	v3f playerpos = sao->getBasePosition();
	// if the player is attached, get the velocity from the attached object
	LuaEntitySAO *lsao = getAttachedObject(sao, env);
	const v3f &playerspeed = lsao? lsao->getVelocity() : player->getSpeed();
	v3f playerspeeddir(0,0,0);
	if (playerspeed.getLength() > 1.0f * BS)
		playerspeeddir = playerspeed / playerspeed.getLength();
	// Predict to next block
	v3f playerpos_predicted = playerpos + playerspeeddir * (MAP_BLOCKSIZE * BS);

	v3s16 center_nodepos = floatToInt(playerpos_predicted, BS);

	v3s16 center = getNodeBlockPos(center_nodepos);

	// Camera position and direction
	v3f camera_pos = sao->getEyePosition();
	v3f camera_dir = v3f(0,0,1);
	camera_dir.rotateYZBy(sao->getLookPitch());
	camera_dir.rotateXZBy(sao->getRotation().Y);

	if (sao->getCameraInverted())
		camera_dir = -camera_dir;

	u16 max_simul_sends_usually = m_max_simul_sends;

	/*
		Check the time from last addNode/removeNode.

		Decrease send rate if player is building stuff.
	*/
	m_time_from_building += dtime;
	if (m_time_from_building < m_min_time_from_building) {
		max_simul_sends_usually
			= LIMITED_MAX_SIMULTANEOUS_BLOCK_SENDS;
	}

	/*
		Number of blocks sending + number of blocks selected for sending
	*/
	u32 num_blocks_selected = m_blocks_sending.size();

	/*
		next time d will be continued from the d from which the nearest
		unsent block was found this time.

		This is because not necessarily any of the blocks found this
		time are actually sent.
	*/
	s32 new_nearest_unsent_d = -1;

	// Get view range and camera fov (radians) from the client
	s16 fog_distance = sao->getPlayer()->getSkyParams().fog_distance;
	s16 wanted_range = sao->getWantedRange() + 1;
	if (fog_distance >= 0) {
		// enforce if limited by mod
		wanted_range = std::min<unsigned>(wanted_range, std::ceil((float)fog_distance / MAP_BLOCKSIZE));
	}
	float camera_fov = sao->getFov();

	/*
		Get the starting value of the block finder radius.
	*/
	if (m_last_center != center) {
		m_nearest_unsent_d = 0;
		m_last_center = center;
		m_map_send_completion_timer = 0.0f;
	}
	// reset the unsent distance if the view angle has changed more that 10% of the fov
	// (this matches isBlockInSight which allows for an extra 10%)
	if (camera_dir.dotProduct(m_last_camera_dir) < std::cos(camera_fov * 0.1f)) {
		m_nearest_unsent_d = 0;
		m_last_camera_dir = camera_dir;
		m_map_send_completion_timer = 0.0f;
	}

	s16 d_start = m_nearest_unsent_d;

	// Distrust client-sent FOV and get server-set player object property
	// zoom FOV (degrees) as a check to avoid hacked clients using FOV to load
	// distant world.
	// (zoom is disabled by value 0)
	float prop_zoom_fov = sao->getZoomFOV() < 0.001f ?
		0.0f :
		std::max(camera_fov, sao->getZoomFOV() * core::DEGTORAD);

	const s16 full_d_max = std::min(adjustDist(m_max_send_distance, prop_zoom_fov),
		wanted_range);
	const s16 d_opt = std::min(adjustDist(m_block_optimize_distance, prop_zoom_fov),
		wanted_range);
	const s16 d_cull_opt = std::min(adjustDist(m_block_cull_optimize_distance, prop_zoom_fov),
		wanted_range);
	// f32 to prevent overflow, it is also what isBlockInSight(...) expects
	const f32 d_blocks_in_sight = full_d_max * BS * MAP_BLOCKSIZE;

	s16 d_max_gen = std::min(adjustDist(m_max_gen_distance, prop_zoom_fov),
		wanted_range);

	s16 d_max = full_d_max;

	// Don't loop very much at a time
	s16 max_d_increment_at_time = 2;
	if (d_max > d_start + max_d_increment_at_time)
		d_max = d_start + max_d_increment_at_time;

	// cos(angle between velocity and camera) * |velocity|
	// Limit to 0.0f in case player moves backwards.
	f32 dot = rangelim(camera_dir.dotProduct(playerspeed), 0.0f, 300.0f);

	// Reduce the field of view when a player moves and looks forward.
	// limit max fov effect to 50%, 60% at 20n/s fly speed
	camera_fov = camera_fov / (1 + dot / 300.0f);

	s32 nearest_emerged_d = -1;
	s32 nearest_sent_d = -1;
	//bool queue_is_full = false;

	const v3s16 cam_pos_nodes = floatToInt(camera_pos, BS);

	s16 d;
	for (d = d_start; d <= d_max; d++) {
		/*
			Get the border/face dot coordinates of a "d-radiused"
			box
		*/
		const auto &list = FacePositionCache::getFacePositions(d);

		for (auto li = list.begin(); li != list.end(); ++li) {
			v3s16 p = *li + center;

			/*
				Send throttling
				- Don't allow too many simultaneous transfers
				- EXCEPT when the blocks are very close

				Also, don't send blocks that are already flying.
			*/

			// Start with the usual maximum
			u16 max_simul_dynamic = max_simul_sends_usually;

			// If block is very close, allow full maximum
			if (d <= BLOCK_SEND_DISABLE_LIMITS_MAX_D)
				max_simul_dynamic = m_max_simul_sends;

			/*
				Do not go over max mapgen limit
			*/
			if (blockpos_over_max_limit(p))
				continue;

			// If this is true, inexistent block will be made from scratch
			bool generate = d <= d_max_gen;

			/*
				Don't generate or send if not in sight
				FIXME This only works if the client uses a small enough
				FOV setting. The default of 72 degrees is fine.
				Also retrieve a smaller view cone in the direction of the player's
				movement.
				(0.1 is about 5 degrees)
			*/
			f32 dist;
			if (!(isBlockInSight(p, camera_pos, camera_dir, camera_fov,
						d_blocks_in_sight, &dist) ||
					(playerspeed.getLength() > 1.0f * BS &&
					isBlockInSight(p, camera_pos, playerspeeddir, 0.1f,
						d_blocks_in_sight)))) {
				continue;
			}

			/*
				Check if map has this block
			*/
			MapBlock *block = env->getMap().getBlockNoCreateNoEx(p);
			if (block) {
				// First: Reset usage timer, this block will be of use in the future.
				block->resetUsageTimer();
			}

			// Don't select too many blocks for sending
			if (num_blocks_selected >= max_simul_dynamic) {
				//queue_is_full = true;
				goto queue_full_break;
			}

			// Don't send blocks that are currently being transferred
			if (m_blocks_sending.find(p) != m_blocks_sending.end())
				continue;

			/*
				Don't send already sent blocks
			*/
			if (m_blocks_sent.find(p) != m_blocks_sent.end())
				continue;

			if (block) {
				/*
					If block is not generated and generating new ones is
					not wanted, skip block.
				*/
				if (!block->isGenerated() && !generate)
					continue;

				/*
					If block is not close, don't send it if it
					consists of air only.
				*/
				if (d >= d_opt && block->isAir())
						continue;
			}

			const bool want_emerge = !block || !block->isGenerated();

			// if the block is already in the emerge queue we don't have to check again
			if (!want_emerge || !emerge->isBlockInQueue(p)) {
				/*
					Check occlusion cache first.
				 */
				if (m_blocks_occ.find(p) != m_blocks_occ.end())
					continue;

				/*
					Note that we do this even before the block is loaded as this does not depend on its contents.
				 */
				if (m_occ_cull &&
						env->getMap().isBlockOccluded(p * MAP_BLOCKSIZE, cam_pos_nodes, d >= d_cull_opt)) {
					m_blocks_occ.insert(p);
					continue;
				}
			}

			/*
				Add inexistent block to emerge queue.
			*/
			if (want_emerge) {
				if (nearest_emerged_d == -1)
					nearest_emerged_d = d;
				if (emerge->enqueueBlockEmerge(peer_id, p, generate))
					continue;
				else
					goto queue_full_break;
			}

			if (nearest_sent_d == -1)
				nearest_sent_d = d;

			/*
				Add block to send queue
			*/
			dest.emplace_back((float)dist, p, peer_id);

			num_blocks_selected += 1;
		}
	}
queue_full_break:

	// If nothing was found for sending and nothing was queued for
	// emerging, continue next time browsing from here
	if (nearest_emerged_d != -1) {
		new_nearest_unsent_d = nearest_emerged_d;
	} else {
		if (d > full_d_max) {
			new_nearest_unsent_d = 0;
			m_nothing_to_send_pause_timer = 2.0f;
			infostream << "Server: Player " << m_name << ", peer_id=" << peer_id
				<< ": full map send completed after " << m_map_send_completion_timer
				<< "s, restarting" << std::endl;
			m_map_send_completion_timer = 0.0f;
		} else {
			if (nearest_sent_d != -1)
				new_nearest_unsent_d = nearest_sent_d;
			else
				new_nearest_unsent_d = d;
		}
	}

	if (new_nearest_unsent_d != -1 && m_nearest_unsent_d != new_nearest_unsent_d) {
		m_nearest_unsent_d = new_nearest_unsent_d;
		// if the distance has changed, clear the occlusion cache
		m_blocks_occ.clear();
	}
}

void RemoteClient::GotBlock(v3s16 p)
{
	if (m_blocks_sending.erase(p) > 0) {
		// only add to sent blocks if it actually was sending
		// (it might have been modified since)
		m_blocks_sent.insert(p);
	} else {
		m_excess_gotblocks++;
	}
}

void RemoteClient::SentBlock(v3s16 p)
{
	if (!m_blocks_sending.insert(p).second)
		infostream<<"RemoteClient::SentBlock(): Sent block"
				" already in m_blocks_sending"<<std::endl;
}

void RemoteClient::SetBlockNotSent(v3s16 p, bool low_priority)
{
	m_nothing_to_send_pause_timer = 0;

	// remove the block from sending and sent sets,
	// and reset the scan loop if found
	if (m_blocks_sending.erase(p) + m_blocks_sent.erase(p) > 0) {
		// If this is a low priority event, do not reset m_nearest_unsent_d.
		// Instead, the send loop will get to the block in the next full loop iteration.
		if (!low_priority) {
			// Note that we do NOT use the euclidean distance here.
			// getNextBlocks builds successive cube-surfaces in the send loop.
			// This resets the distance to the maximum cube size that
			// still guarantees that this block will be scanned again right away.
			//
			// Using m_last_center is OK, as a change in center
			// will reset m_nearest_unsent_d to 0 anyway (see getNextBlocks).
			p -= m_last_center;
			s16 this_d = std::max({std::abs(p.X), std::abs(p.Y), std::abs(p.Z)});
			m_nearest_unsent_d = std::min(m_nearest_unsent_d, this_d);
		}
	}
}

void RemoteClient::SetBlocksNotSent(const std::vector<v3s16> &blocks, bool low_priority)
{
	for (v3s16 p : blocks) {
		SetBlockNotSent(p, low_priority);
	}
}

void RemoteClient::notifyEvent(ClientStateEvent event)
{
	std::ostringstream myerror;
	switch (m_state)
	{
	case CS_Invalid:
		//intentionally do nothing
		break;
	case CS_Created:
		switch (event) {
		case CSE_Hello:
			m_state = CS_HelloSent;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		/* GotInit2 SetDefinitionsSent SetMediaSent */
		default:
			myerror << "Created: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_Denied:
		/* don't do anything if in denied state */
		break;
	case CS_HelloSent:
		switch(event)
		{
		case CSE_AuthAccept:
			m_state = CS_AwaitingInit2;
			resetChosenMech();
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			resetChosenMech();
			break;
		default:
			myerror << "HelloSent: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_AwaitingInit2:
		switch(event)
		{
		case CSE_GotInit2:
			confirmSerializationVersion();
			m_state = CS_InitDone;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;

		/* Init SetDefinitionsSent SetMediaSent */
		default:
			myerror << "InitSent: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;

	case CS_InitDone:
		switch(event)
		{
		case CSE_SetDefinitionsSent:
			m_state = CS_DefinitionsSent;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;

		/* Init GotInit2 SetMediaSent */
		default:
			myerror << "InitDone: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_DefinitionsSent:
		switch(event)
		{
		case CSE_SetClientReady:
			m_state = CS_Active;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		/* Init GotInit2 SetDefinitionsSent */
		default:
			myerror << "DefinitionsSent: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
		}
		break;
	case CS_Active:
		switch(event)
		{
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SudoSuccess:
			m_state = CS_SudoMode;
			resetChosenMech();
			break;
		/* Init GotInit2 SetDefinitionsSent SetMediaSent SetDenied */
		default:
			myerror << "Active: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
			break;
		}
		break;
	case CS_SudoMode:
		switch(event)
		{
		case CSE_SetDenied:
			m_state = CS_Denied;
			break;
		case CSE_Disconnect:
			m_state = CS_Disconnecting;
			break;
		case CSE_SudoLeave:
			m_state = CS_Active;
			break;
		default:
			myerror << "Active: Invalid client state transition! " << event;
			throw ClientStateError(myerror.str());
			break;
		}
		break;
	case CS_Disconnecting:
		/* we are already disconnecting */
		break;
	}
}

void RemoteClient::resetChosenMech()
{
	if (auth_data) {
		srp_verifier_delete((SRPVerifier *) auth_data);
		auth_data = nullptr;
	}
	chosen_mech = AUTH_MECHANISM_NONE;
}

void RemoteClient::setEncryptedPassword(const std::string& pwd)
{
	FATAL_ERROR_IF(!str_starts_with(pwd, "#1#"), "must be srp");
	enc_pwd = pwd;
	// We just set SRP encrypted password, we accept only it now
	allowed_auth_mechs = AUTH_MECHANISM_SRP;
}

void RemoteClient::setVersionInfo(u8 major, u8 minor, u8 patch, const std::string &full)
{
	m_version_major = major;
	m_version_minor = minor;
	m_version_patch = patch;
	m_full_version = string_sanitize_ascii(full, 64);
}

void RemoteClient::setLangCode(const std::string &code)
{
	m_lang_code = string_sanitize_ascii(code, 12);
}

ClientInterface::ClientInterface(const std::shared_ptr<con::IConnection> &con)
:
	m_con(con),
	m_env(nullptr)
{

}

ClientInterface::~ClientInterface()
{
	/*
		Delete clients
	*/
	{
		RecursiveMutexAutoLock clientslock(m_clients_mutex);

		for (auto &client_it : m_clients) {
			// Delete client
			delete client_it.second;
		}
	}
}

std::vector<session_t> ClientInterface::getClientIDs(ClientState min_state)
{
	std::vector<session_t> reply;
	RecursiveMutexAutoLock clientslock(m_clients_mutex);
	reply.reserve(m_clients.size());
	for (const auto &m_client : m_clients) {
		if (m_client.second->getState() >= min_state)
			reply.push_back(m_client.second->peer_id);
	}

	return reply;
}

void ClientInterface::markBlocksNotSent(const std::vector<v3s16> &positions, bool low_priority)
{
	RecursiveMutexAutoLock clientslock(m_clients_mutex);
	for (const auto &client : m_clients) {
		if (client.second->getState() >= CS_Active)
			client.second->SetBlocksNotSent(positions, low_priority);
	}
}

bool ClientInterface::isUserLimitReached()
{
	// Note that this only counts clients that have fully joined
	return getClientIDs().size() >= g_settings->getU16("max_users");
}

void ClientInterface::step(float dtime)
{
	m_print_info_timer += dtime;
	if (m_print_info_timer >= 30.0f) {
		m_print_info_timer = 0.0f;
		UpdatePlayerList();
	}

	m_check_linger_timer += dtime;
	if (m_check_linger_timer < 1.0f)
		return;
	m_check_linger_timer = 0;

	RecursiveMutexAutoLock clientslock(m_clients_mutex);
	for (const auto &it : m_clients) {
		auto state = it.second->getState();
		if (state >= CS_InitDone)
			continue;
		if (it.second->uptime() <= LINGER_TIMEOUT)
			continue;
		// Complain louder if this situation is unexpected
		auto &os = state == CS_Disconnecting || state == CS_Denied ?
			infostream : warningstream;
		try {
			Address addr = m_con->GetPeerAddress(it.second->peer_id);
			os << "Disconnecting lingering client from "
				<< addr.serializeString() << " peer_id=" << it.second->peer_id
				<< " (" << state2Name(state) << ")" << std::endl;
			m_con->DisconnectPeer(it.second->peer_id);
		} catch (con::PeerNotFoundException &e) {
		}
	}
}

void ClientInterface::UpdatePlayerList()
{
	if (m_env) {
		std::vector<session_t> clients = getClientIDs();
		m_clients_names.clear();

		if (!clients.empty())
			infostream<<"Players:"<<std::endl;

		for (session_t i : clients) {
			RemotePlayer *player = m_env->getPlayer(i);

			if (player == NULL)
				continue;

			infostream << "* " << player->getName() << "\t";

			{
				RecursiveMutexAutoLock clientslock(m_clients_mutex);
				RemoteClient* client = lockedGetClientNoEx(i);
				if (client)
					client->PrintInfo(infostream);
				infostream << std::endl;
			}

			m_clients_names.emplace_back(player->getName());
		}
	}
}

void ClientInterface::send(session_t peer_id, NetworkPacket *pkt)
{
	auto &ccf = clientCommandFactoryTable[pkt->getCommand()];
	FATAL_ERROR_IF(!ccf.name, "packet type missing in table");

	m_con->Send(peer_id, ccf.channel, pkt, ccf.reliable);
}

void ClientInterface::sendCustom(session_t peer_id, u8 channel, NetworkPacket *pkt, bool reliable)
{
	// check table anyway to prevent mistakes
	FATAL_ERROR_IF(!clientCommandFactoryTable[pkt->getCommand()].name,
		"packet type missing in table");

	m_con->Send(peer_id, channel, pkt, reliable);
}

void ClientInterface::sendToAll(NetworkPacket *pkt, ClientState state_min)
{
	auto &ccf = clientCommandFactoryTable[pkt->getCommand()];
	FATAL_ERROR_IF(!ccf.name, "packet type missing in table");
	RecursiveMutexAutoLock clientslock(m_clients_mutex);
	for (auto &[peer_id, client] : m_clients) {
		if (client->getState() >= state_min)
			m_con->Send(peer_id, ccf.channel, pkt, ccf.reliable);
	}
}

RemoteClient* ClientInterface::getClientNoEx(session_t peer_id, ClientState state_min)
{
	RecursiveMutexAutoLock clientslock(m_clients_mutex);
	RemoteClient *client = lockedGetClientNoEx(peer_id, state_min);
	return client;
}

RemoteClient* ClientInterface::lockedGetClientNoEx(session_t peer_id, ClientState state_min)
{
	RemoteClientMap::const_iterator n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if (n == m_clients.end())
		return nullptr;

	assert(n->second->peer_id == peer_id);
	if (n->second->getState() >= state_min)
		return n->second;

	return nullptr;
}

ClientState ClientInterface::getClientState(session_t peer_id)
{
	RecursiveMutexAutoLock clientslock(m_clients_mutex);
	RemoteClientMap::const_iterator n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if (n == m_clients.end())
		return CS_Invalid;

	return n->second->getState();
}

void ClientInterface::DeleteClient(session_t peer_id)
{
	RecursiveMutexAutoLock conlock(m_clients_mutex);

	// Error check
	RemoteClientMap::iterator n = m_clients.find(peer_id);
	// The client may not exist; clients are immediately removed if their
	// access is denied, and this event occurs later then.
	if (n == m_clients.end())
		return;

	/*
		Mark objects to be not known by the client
	*/
	//TODO this should be done by client destructor!!!
	RemoteClient *client = n->second;
	// Handle objects
	for (u16 id : client->m_known_objects) {
		// Get object
		ServerActiveObject* obj = m_env->getActiveObject(id);

		if(obj && obj->m_known_by_count > 0)
			obj->m_known_by_count--;
	}

	// Delete client
	delete m_clients[peer_id];
	m_clients.erase(peer_id);
}

void ClientInterface::CreateClient(session_t peer_id)
{
	RecursiveMutexAutoLock conlock(m_clients_mutex);

	// Error check
	RemoteClientMap::iterator n = m_clients.find(peer_id);
	// The client shouldn't already exist
	if (n != m_clients.end()) return;

	// Create client
	RemoteClient *client = new RemoteClient();
	client->peer_id = peer_id;
	m_clients[client->peer_id] = client;
}

void ClientInterface::event(session_t peer_id, ClientStateEvent event)
{
	{
		RecursiveMutexAutoLock clientlock(m_clients_mutex);

		// Error check
		RemoteClientMap::iterator n = m_clients.find(peer_id);

		// No client to deliver event
		if (n == m_clients.end())
			return;
		n->second->notifyEvent(event);
	}

	if ((event == CSE_SetClientReady) ||
		(event == CSE_Disconnect)     ||
		(event == CSE_SetDenied))
	{
		UpdatePlayerList();
	}
}

u16 ClientInterface::getProtocolVersion(session_t peer_id)
{
	RecursiveMutexAutoLock conlock(m_clients_mutex);

	// Error check
	RemoteClientMap::iterator n = m_clients.find(peer_id);

	// No client to get version
	if (n == m_clients.end())
		return 0;

	return n->second->net_proto_version;
}
