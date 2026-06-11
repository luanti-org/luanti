// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "chatmessage.h"
#include "server.h"
#include "serverenvironment.h"
#include "log.h"
#include "emerge.h"
#include "itemdef.h"
#include "mapblock.h"
#include "modchannels.h"
#include "nodedef.h"
#include "porting.h" // strcasecmp
#include "remoteplayer.h"
#include "rollback_interface.h"
#include "scripting_server.h"
#include "serialization.h"
#include "settings.h"
#include "tool.h"
#include "network/connection.h"
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "network/serverpacketdispatcher.h"
#include "server/player_sao.h"
#include "server/serverinventorymgr.h"
#include "util/auth.h"
#include "util/base64.h"
#include "util/pointedthing.h"
#include "util/srp.h"
#include "clientdynamicinfo.h"

#include <algorithm>

void Server::handleCommand_Init(const server::PacketContext &ctx,
	const server::InitPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemoteClient *client = getClient(peer_id, CS_Created);

	Address addr;
	std::string addr_s;
	try {
		addr = m_con->GetPeerAddress(peer_id);
		addr_s = addr.serializeString();
	} catch (con::PeerNotFoundException &e) {
		/*
		 * no peer for this packet found
		 * most common reason is peer timeout, e.g. peer didn't
		 * respond for some time, your server was overloaded or
		 * things like that.
		 */
		infostream << "Server: peer " << peer_id << " not found during INIT?!"
			<< std::endl;
		return;
	}

	if (client->getState() > CS_Created) {
		verbosestream << "Server: Ignoring multiple TOSERVER_INITs from " <<
			addr_s << " (peer_id=" << peer_id << ")" << std::endl;
		return;
	}

	client->setCachedAddress(addr);

	verbosestream << "Server: Got TOSERVER_INIT from " << addr_s <<
		" (peer_id=" << peer_id << ")" << std::endl;

	if (denyIfBanned(peer_id))
		return;

	// Use the highest version supported by both
	const u8 serialization_ver = std::min(p.max_ser_ver, SER_FMT_VER_HIGHEST_WRITE);

	if (!ser_ver_supported_write(serialization_ver)) {
		actionstream << "Server: A mismatched client tried to connect from " <<
			addr_s << " ser_fmt_max=" << (int)serialization_ver << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	client->setPendingSerializationVersion(serialization_ver);

	/*
		Read and check network protocol version
	*/

	u16 net_proto_version = 0;

	// Figure out a working version if it is possible at all
	if (p.max_net_proto_version >= SERVER_PROTOCOL_VERSION_MIN ||
			p.min_net_proto_version <= LATEST_PROTOCOL_VERSION) {
		// If maximum is larger than our maximum, go with our maximum
		if (p.max_net_proto_version > LATEST_PROTOCOL_VERSION)
			net_proto_version = LATEST_PROTOCOL_VERSION;
		// Else go with client's maximum
		else
			net_proto_version = p.max_net_proto_version;
	}

	verbosestream << "Server: " << addr_s << ": Protocol version: min: "
			<< p.min_net_proto_version << ", max: " << p.max_net_proto_version
			<< ", chosen: " << net_proto_version << std::endl;

	client->net_proto_version = net_proto_version;

	if (net_proto_version < Server::getProtocolVersionMin() ||
			net_proto_version > Server::getProtocolVersionMax()) {
		actionstream << "Server: A mismatched client tried to connect from " <<
			addr_s << " proto_max=" << (int)p.max_net_proto_version << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_VERSION);
		return;
	}

	/*
		Validate player name
	*/
	const std::string &playerName = p.player_name;
	const char* playername = playerName.c_str();

	size_t pns = playerName.size();
	if (pns == 0 || pns > PLAYERNAME_SIZE) {
		actionstream << "Server: Player with " <<
			((pns > PLAYERNAME_SIZE) ? "a too long" : "an empty") <<
			" name tried to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	if (!string_allowed(playerName, PLAYERNAME_ALLOWED_CHARS)) {
		actionstream << "Server: Player with an invalid name tried to connect "
			"from " << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_CHARS_IN_NAME);
		return;
	}

	// Do not allow multiple players in simple singleplayer mode
	if (isSingleplayer() && !m_clients.getClientIDs(CS_HelloSent).empty()) {
		infostream << "Server: Not allowing another client (" << addr_s <<
			") to connect in simple singleplayer mode" << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_SINGLEPLAYER);
		return;
	}
	// Or the "singleplayer" name to be used on regular servers
	if (!isSingleplayer() && strcasecmp(playername, "singleplayer") == 0) {
		actionstream << "Server: Player with the name \"singleplayer\" tried "
			"to connect from " << addr_s << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_NAME);
		return;
	}

	{
		RemotePlayer *player = m_env->getPlayer(playername, true);
		// If player is already connected, cancel
		if (player && player->getPeerId() != PEER_ID_INEXISTENT) {
			actionstream << "Server: Player with name \"" << playername <<
				"\" tried to connect, but player with same name is already connected" << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
			return;
		}
	}

	client->setName(playerName);

	{
		std::string reason;
		if (m_script->on_prejoinplayer(playername, addr_s, &reason)) {
			actionstream << "Server: Player with the name \"" << playerName <<
				"\" tried to connect from " << addr_s <<
				" but was disallowed for the following reason: " << reason <<
				std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_CUSTOM_STRING, reason);
			return;
		}
	}

	infostream << "Server: New connection: \"" << playerName << "\" from " <<
		addr_s << " (peer_id=" << peer_id << ")" << std::endl;

	// Early check for user limit, so the client doesn't need to run
	// through the join process only to be denied.
	if (checkUserLimit(playerName, addr_s)) {
		actionstream << "Server: " << playername << " tried to join from " <<
			addr_s << ", but the user limit was reached." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_TOO_MANY_USERS);
		return;
	}

	/*
		Compose auth methods for answer
	*/
	std::string encpwd; // encrypted Password field for the user
	bool has_auth = m_script->getAuth(playername, &encpwd, nullptr);
	u32 auth_mechs = 0;

	client->chosen_mech = AUTH_MECHANISM_NONE;

	if (has_auth) {
		std::vector<std::string> pwd_components = str_split(encpwd, '#');
		if (pwd_components.size() == 4) {
			if (pwd_components[1] == "1") { // 1 means srp
				auth_mechs |= AUTH_MECHANISM_SRP;
				client->enc_pwd = encpwd;
			} else {
				actionstream << "User " << playername << " tried to log in, "
					"but password field was invalid (unknown mechcode)." <<
					std::endl;
				DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
				return;
			}
		} else if (base64_is_valid(encpwd)) {
			auth_mechs |= AUTH_MECHANISM_LEGACY_PASSWORD;
			client->enc_pwd = encpwd;
		} else {
			actionstream << "User " << playername << " tried to log in, but "
				"password field was invalid (invalid base64)." << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
	} else {
		std::string default_password = g_settings->get("default_password");
		if (isSingleplayer() || default_password.length() == 0) {
			auth_mechs |= AUTH_MECHANISM_FIRST_SRP;
		} else {
			// Take care of default passwords.
			client->enc_pwd = get_encoded_srp_verifier(playerName, default_password);
			auth_mechs |= AUTH_MECHANISM_SRP;
			// Allocate player in db, but only on successful login.
			client->create_player_on_auth_success = true;
		}
	}

	/*
		Answer with a TOCLIENT_HELLO
	*/

	verbosestream << "Sending TOCLIENT_HELLO with auth method field: "
		<< auth_mechs << std::endl;

	m_netserver->sendHello(peer_id, serialization_ver,
		net_proto_version, auth_mechs);

	client->allowed_auth_mechs = auth_mechs;

	m_clients.event(peer_id, CSE_Hello);
}

void Server::handleCommand_Init2(const server::PacketContext &ctx,
	const server::Init2Payload &p)
{
	const session_t peer_id = ctx.peer_id;
	verbosestream << "Server: Got TOSERVER_INIT2 from " << peer_id << std::endl;

	RemoteClient *client = getClientNoEx(peer_id, CS_Invalid);
	if (!client || client->getState() != CS_AwaitingInit2) {
		actionstream << "Server: Ignoring TOSERVER_INIT2 in wrong state from "
			<< peer_id << std::endl;
		return;
	}

	m_clients.event(peer_id, CSE_GotInit2);
	u16 protocol_version = m_clients.getProtocolVersion(peer_id);

	const std::string &lang = p.lang;

	/*
		Send some initialization data
	*/

	infostream << "Server: Sending content to " << getPlayerName(peer_id) <<
		std::endl;

	// Send item definitions
	SendItemDef(peer_id, m_itemdef, protocol_version);

	// Send node definitions
	SendNodeDef(peer_id, m_nodedef, protocol_version);

	m_clients.event(peer_id, CSE_SetDefinitionsSent);

	// Send media announcement
	sendMediaAnnouncement(peer_id, lang);

	// Keep client language for server translations
	client->setLangCode(lang);

	// Send active objects
	{
		PlayerSAO *sao = getPlayerSAO(peer_id);
		if (sao)
			SendActiveObjectRemoveAdd(client, sao);
	}

	// Send detached inventories
	sendDetachedInventories(peer_id, false);

	// Send player movement settings
	SendMovement(peer_id);

	// Send time of day
	u16 time = m_env->getTimeOfDay();
	float time_speed = g_settings->getFloat("time_speed");
	m_netserver->sendTimeOfDay(peer_id, time, time_speed);

	m_netserver->sendCSMRestrictionFlags(peer_id,
		m_csm_restriction_flags, m_csm_restriction_noderange);
}

void Server::handleCommand_RequestMedia(const server::PacketContext &ctx,
	const server::RequestMediaPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	verbosestream << "Client " << getPlayerName(peer_id)
		<< " requested media file(s):\n";
	for (const std::string &name : p.files)
		verbosestream << "  " << name << "\n";
	verbosestream << std::flush;

	sendRequestedMedia(peer_id, p.files);
}

void Server::handleCommand_ClientReady(const server::PacketContext &ctx,
	const server::ClientReadyPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemoteClient *client = getClient(peer_id, CS_Created);
	assert(client);

	client->setVersionInfo(p.major, p.minor, p.patch, p.full_ver);

	// Since only active clients count for the user limit, two could race the
	// join process so we have to do a final check for the user limit here.
	std::string addr_s = client->getAddress().serializeString();
	if (checkUserLimit(client->getName(), addr_s)) {
		actionstream << "Server: " << client->getName() << " tried to join from " <<
			addr_s << ", but the user limit was reached (late)." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_TOO_MANY_USERS);
		return;
	}

	// Emerge player
	PlayerSAO* playersao = StageTwoClientInit(peer_id);
	if (!playersao) {
		errorstream << "Server: stage 2 client init failed "
			"peer_id=" << peer_id << std::endl;
		DisconnectPeer(peer_id);
		return;
	}

	playersao->getPlayer()->formspec_version = p.formspec_ver;
	m_clients.event(peer_id, CSE_SetClientReady);

	// Send player list to this client
	{
		const std::vector<std::string> &players = m_clients.getPlayerNames();
		m_netserver->sendPlayerListUpdateTo(peer_id, PLAYER_LIST_INIT, players);
	}

	s64 last_login;
	m_script->getAuth(playersao->getPlayer()->getName(), nullptr, nullptr, &last_login);
	m_script->on_joinplayer(playersao, last_login);

	// Send shutdown timer if shutdown has been scheduled
	if (m_shutdown_state.isTimerRunning())
		SendChatMessage(peer_id, m_shutdown_state.getShutdownTimerMessage());
}

void Server::handleCommand_GotBlocks(const server::PacketContext &ctx,
	const server::GotBlocksPayload &p)
{
	ClientInterface::AutoLock lock(m_clients);
	RemoteClient *client = m_clients.lockedGetClientNoEx(ctx.peer_id);
	if (!client)
		return;

	for (const v3s16 &pos : p.positions)
		client->GotBlock(pos);
}

void Server::process_PlayerPos(RemotePlayer *player, PlayerSAO *playersao,
	const server::PlayerPosPayload &p)
{
	player->control.unpackKeysPressed(p.keyPressed);

	if (!playersao->isAttached()) {
		// Only update player positions when moving freely
		// to not interfere with attachment handling
		playersao->setBasePosition(v3f((f32)p.pos.X / 100.0f,
			(f32)p.pos.Y / 100.0f, (f32)p.pos.Z / 100.0f));
		player->setSpeed(v3f((f32)p.speed.X / 100.0f,
			(f32)p.speed.Y / 100.0f, (f32)p.speed.Z / 100.0f));
	}

	const f32 pitch = modulo360f(p.pitch);
	const f32 yaw = wrapDegrees_0_360(p.yaw);
	playersao->setLookPitch(pitch);
	playersao->setPlayerYaw(yaw);
	playersao->setFov(p.fov);
	playersao->setWantedRange(p.wanted_range);
	playersao->setCameraInverted(p.bits & 0x01);

	// If the client did not send movement data, fall back to keys.
	if (p.movement_speed == 0.0f && p.movement_direction == 0) {
		player->control.movement_speed = 0.0f;
		player->control.movement_direction = 0.0f;
		player->control.setMovementFromKeys();
	} else {
		player->control.movement_speed = p.movement_speed;
		player->control.movement_direction = p.movement_direction;
	}

	if (playersao->checkMovementCheat()) {
		// Call callbacks
		m_script->on_cheat(playersao, "moved_too_fast");
		SendMovePlayer(playersao);
	}
}

void Server::handleCommand_PlayerPos(const server::PacketContext &ctx,
	const server::PlayerPosPayload &p)
{
	RemotePlayer *player = m_env->getPlayer(ctx.peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	// If player is dead we don't care of this packet
	if (playersao->isDead()) {
		verbosestream << "TOSERVER_PLAYERPOS: " << player->getName()
				<< " is dead. Ignoring packet";
		return;
	}

	process_PlayerPos(player, playersao, p);
}

void Server::handleCommand_DeletedBlocks(const server::PacketContext &ctx,
	const server::DeletedBlocksPayload &p)
{
	ClientInterface::AutoLock lock(m_clients);
	RemoteClient *client = m_clients.lockedGetClientNoEx(ctx.peer_id);
	if (!client)
		return;

	for (const v3s16 &pos : p.positions)
		client->SetBlockNotSent(pos);
}

void Server::handleCommand_InventoryAction(const server::PacketContext &ctx,
	const server::InventoryActionPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	// Strip command and create a stream
	std::istringstream is(p.serialized, std::ios_base::binary);
	// Create an action
	std::unique_ptr<InventoryAction> a(InventoryAction::deSerialize(is));
	if (!a) {
		infostream << "TOSERVER_INVENTORY_ACTION: "
				<< "InventoryAction::deSerialize() returned NULL"
				<< std::endl;
		return;
	}

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			"player:" + player->getName());

	/*
		Note: Always set inventory not sent, to repair cases
		where the client made a bad prediction.
	*/

	auto mark_player_inv_list_dirty = [this](const InventoryLocation &loc,
			const std::string &list_name) {

		// Undo the client prediction of the affected list. See `clientApply`.
		if (loc.type != InventoryLocation::PLAYER)
			return;

		Inventory *inv = m_inventory_mgr->getInventory(loc);
		if (!inv)
			return;

		InventoryList *list = inv->getList(list_name);
		if (!list)
			return;

		list->setModified(true);
	};

	const bool player_has_interact = checkPriv(player->getName(), "interact");

	auto check_inv_access = [player, player_has_interact, this] (
			const InventoryLocation &loc) -> bool {

		// Players without interact may modify their own inventory
		if (!player_has_interact && loc.type != InventoryLocation::PLAYER) {
			infostream << "Cannot modify foreign inventory: "
					<< "No interact privilege" << std::endl;
			return false;
		}

		switch (loc.type) {
		case InventoryLocation::CURRENT_PLAYER:
			// Only used internally on the client, never sent
			return false;
		case InventoryLocation::PLAYER:
			// Allow access to own inventory in all cases
			return loc.name == player->getName();
		case InventoryLocation::NODEMETA:
			{
				// Check for out-of-range interaction
				v3f node_pos   = intToFloat(loc.p, BS);
				v3f player_pos = player->getPlayerSAO()->getEyePosition();
				f32 d = player_pos.getDistanceFrom(node_pos);
				return checkInteractDistance(player, d, "inventory");
			}
		case InventoryLocation::DETACHED:
			return getInventoryMgr()->checkDetachedInventoryAccess(loc, player->getName());
		default:
			return false;
		}
	};

	/*
		Handle restrictions and special cases of the move action
	*/
	if (a->getType() == IAction::Move) {
		IMoveAction *ma = (IMoveAction*)a.get();

		ma->from_inv.applyCurrentPlayer(player->getName());
		ma->to_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(ma->from_inv);
		mark_player_inv_list_dirty(ma->from_inv, ma->from_list);
		bool inv_different = ma->from_inv != ma->to_inv;
		if (inv_different)
			m_inventory_mgr->setInventoryModified(ma->to_inv);
		if (inv_different || ma->from_list != ma->to_list)
			mark_player_inv_list_dirty(ma->to_inv, ma->to_list);

		if (!check_inv_access(ma->from_inv) ||
				!check_inv_access(ma->to_inv))
			return;

		/*
			Disable moving items out of craftpreview
		*/
		if (ma->from_list == "craftpreview") {
			infostream << "Ignoring IMoveAction from "
					<< (ma->from_inv.dump()) << ":" << ma->from_list
					<< " to " << (ma->to_inv.dump()) << ":" << ma->to_list
					<< " because src is " << ma->from_list << std::endl;
			return;
		}

		/*
			Disable moving items into craftresult and craftpreview
		*/
		if (ma->to_list == "craftpreview" || ma->to_list == "craftresult") {
			infostream << "Ignoring IMoveAction from "
					<< (ma->from_inv.dump()) << ":" << ma->from_list
					<< " to " << (ma->to_inv.dump()) << ":" << ma->to_list
					<< " because dst is " << ma->to_list << std::endl;
			return;
		}
	}
	/*
		Handle restrictions and special cases of the drop action
	*/
	else if (a->getType() == IAction::Drop) {
		IDropAction *da = (IDropAction*)a.get();

		da->from_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(da->from_inv);
		mark_player_inv_list_dirty(da->from_inv, da->from_list);

		/*
			Disable dropping items out of craftpreview
		*/
		if (da->from_list == "craftpreview") {
			infostream << "Ignoring IDropAction from "
					<< (da->from_inv.dump()) << ":" << da->from_list
					<< " because src is " << da->from_list << std::endl;
			return;
		}

		// Disallow dropping items if not allowed to interact
		if (!player_has_interact || !check_inv_access(da->from_inv))
			return;

		// Disallow dropping items if dead
		if (playersao->isDead()) {
			infostream << "Ignoring IDropAction from "
					<< (da->from_inv.dump()) << ":" << da->from_list
					<< " because player is dead." << std::endl;
			return;
		}
	}
	/*
		Handle restrictions and special cases of the craft action
	*/
	else if (a->getType() == IAction::Craft) {
		ICraftAction *ca = (ICraftAction*)a.get();

		ca->craft_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(ca->craft_inv);
		// Note: `ICraftAction::clientApply` is empty, thus nothing to revert.

		// Disallow crafting if not allowed to interact
		if (!player_has_interact) {
			infostream << "Cannot craft: "
					<< "No interact privilege" << std::endl;
			return;
		}

		if (!check_inv_access(ca->craft_inv))
			return;
	} else {
		// Unknown action. Ignored.
		return;
	}

	// Do the action
	a->apply(m_inventory_mgr.get(), playersao, this);
}

void Server::handleCommand_ChatMessage(const server::PacketContext &ctx,
	const server::ChatMessagePayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	const auto &name = player->getName();

	std::wstring answer_to_sender = handleChat(name, p.message, true, player);
	if (!answer_to_sender.empty()) {
		// Send the answer to sender
		SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
			answer_to_sender));
	}
}

void Server::handleCommand_Damage(const server::PacketContext &ctx,
	const server::DamagePayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	if (!playersao->isImmortal()) {
		if (playersao->isDead()) {
			verbosestream << "Server: "
				"Ignoring damage as player " << player->getName()
				<< " is already dead" << std::endl;
			return;
		}

		actionstream << player->getName() << " damaged by "
				<< (int)p.damage << " hp at " << (playersao->getBasePosition() / BS)
				<< std::endl;

		PlayerHPChangeReason reason(PlayerHPChangeReason::FALL);
		playersao->setHP((s32)playersao->getHP() - (s32)p.damage, reason, true);
	}
}

void Server::handleCommand_PlayerItem(const server::PacketContext &ctx,
	const server::PlayerItemPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	if (player->getMaxHotbarItemcount() == 0) {
		return; // ignore silently
	} else if (p.item >= player->getMaxHotbarItemcount()) {
		actionstream << "Player " << player->getName()
			<< " tried to access item=" << p.item
			<< " out of hotbar_itemcount="
			<< player->getMaxHotbarItemcount()
			<< "; ignoring." << std::endl;
		return;
	}

	playersao->getPlayer()->setWieldIndex(p.item);
}

bool Server::checkInteractDistance(RemotePlayer *player, const f32 d, const std::string &what)
{
	ItemStack selected_item, hand_item;
	const ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);
	f32 max_d = BS * getToolRange(tool_item, hand_item, m_itemdef);

	// Cube diagonal * 1.5 for maximal supported node extents:
	// sqrt(3) * 1.5 ≅ 2.6
	if (d > max_d + 2.6f * BS) {
		actionstream << "Player " << player->getName()
				<< " tried to access " << what
				<< " from too far: "
				<< "d=" << d << ", max_d=" << max_d
				<< "; ignoring." << std::endl;
		// Call callbacks
		m_script->on_cheat(player->getPlayerSAO(), "interacted_too_far");
		return false;
	}
	return true;
}

// Tiny helper to retrieve the selected item into an std::optional
static inline void getWieldedItem(const PlayerSAO *playersao, std::optional<ItemStack> &ret)
{
	ret = ItemStack();
	playersao->getWieldedItem(&(*ret));
}

void Server::handleCommand_Interact(const server::PacketContext &ctx,
	const server::InteractPayload &p)
{
	verbosestream << "TOSERVER_INTERACT: action=" << (int)p.action
		<< ", item=" << p.item
		<< ", pointed=" << p.pointed.dump() << std::endl;

	const session_t peer_id = ctx.peer_id;
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	if (playersao->isDead()) {
		actionstream << "Server: " << player->getName()
				<< " tried to interact while dead; ignoring." << std::endl;
		if (p.pointed.type == POINTEDTHING_NODE) {
			// Re-send block to revert change on client-side
			RemoteClient *client = getClient(peer_id);
			v3s16 blockpos = getNodeBlockPos(p.pointed.node_undersurface);
			client->SetBlockNotSent(blockpos);
		}
		// Call callbacks
		m_script->on_cheat(playersao, "interacted_while_dead");
		return;
	}

	process_PlayerPos(player, playersao, p.player_pos);

	v3f player_pos = playersao->getLastGoodPosition();

	// Update wielded item
	if (player->getMaxHotbarItemcount() == 0) {
		return; // ignore silently
	} else if (p.item >= player->getMaxHotbarItemcount()) {
		actionstream << "Player " << player->getName()
			<< " tried to access item=" << p.item
			<< " out of hotbar_itemcount="
			<< player->getMaxHotbarItemcount()
			<< "; ignoring." << std::endl;
		return;
	}

	player->setWieldIndex(p.item);

	// Get pointed to object (NULL if not POINTEDTYPE_OBJECT)
	ServerActiveObject *pointed_object = NULL;
	if (p.pointed.type == POINTEDTHING_OBJECT) {
		pointed_object = m_env->getActiveObject(p.pointed.object_id);
		if (pointed_object == NULL) {
			verbosestream << "TOSERVER_INTERACT: "
				"pointed object is NULL" << std::endl;
			return;
		}

	}

	const u8 action = p.action;

	/*
		Make sure the player is allowed to do it
	*/
	if (!checkPriv(player->getName(), "interact")) {
		actionstream << player->getName() << " attempted to interact with " <<
				p.pointed.dump() << " without 'interact' privilege" << std::endl;

		if (p.pointed.type != POINTEDTHING_NODE)
			return;

		// Re-send block to revert change on client-side
		RemoteClient *client = getClient(peer_id);
		// Digging completed -> under
		if (action == INTERACT_DIGGING_COMPLETED) {
			v3s16 blockpos = getNodeBlockPos(p.pointed.node_undersurface);
			client->SetBlockNotSent(blockpos);
		}
		// Placement -> above
		else if (action == INTERACT_PLACE) {
			v3s16 blockpos = getNodeBlockPos(p.pointed.node_abovesurface);
			client->SetBlockNotSent(blockpos);
		}
		return;
	}

	/*
		Check that target is reasonably close
	*/
	static thread_local const u32 anticheat_flags =
		g_settings->getFlagStr("anticheat_flags", flagdesc_anticheat, nullptr);

	if ((action == INTERACT_START_DIGGING || action == INTERACT_DIGGING_COMPLETED ||
			action == INTERACT_PLACE || action == INTERACT_USE) &&
			(anticheat_flags & AC_INTERACTION) && !isSingleplayer()) {
		v3f target_pos = player_pos;
		if (p.pointed.type == POINTEDTHING_NODE) {
			target_pos = intToFloat(p.pointed.node_undersurface, BS);
		} else if (p.pointed.type == POINTEDTHING_OBJECT) {
			if (playersao->getId() == pointed_object->getId()) {
				actionstream << "Server: " << player->getName()
					<< " attempted to interact with themselves" << std::endl;
				m_script->on_cheat(playersao, "interacted_with_self");
				return;
			}
			target_pos = pointed_object->getBasePosition();
		}
		float d = playersao->getEyePosition().getDistanceFrom(target_pos);

		if (!checkInteractDistance(player, d, p.pointed.dump())) {
			if (p.pointed.type == POINTEDTHING_NODE) {
				// Re-send block to revert change on client-side
				RemoteClient *client = getClient(peer_id);
				v3s16 blockpos = getNodeBlockPos(p.pointed.node_undersurface);
				client->SetBlockNotSent(blockpos);
			}
			return;
		}
	}

	/*
		If something goes wrong, this player is to blame
	*/
	RollbackScopeActor rollback_scope(m_rollback,
			"player:" + player->getName());

	switch (action) {
	// Start digging or punch object
	case INTERACT_START_DIGGING: {
		if (p.pointed.type == POINTEDTHING_NODE) {
			MapNode n(CONTENT_IGNORE);
			bool pos_ok;

			v3s16 p_under = p.pointed.node_undersurface;
			n = m_env->getMap().getNode(p_under, &pos_ok);
			if (!pos_ok) {
				infostream << "Server: Not punching: Node not found. "
					"Adding block to emerge queue." << std::endl;
				m_emerge->enqueueBlockEmerge(peer_id,
					getNodeBlockPos(p.pointed.node_abovesurface), false);
			}

			if (n.getContent() != CONTENT_IGNORE)
				m_script->node_on_punch(p_under, n, playersao, p.pointed);

			// Cheat prevention
			playersao->noCheatDigStart(p_under);

			return;
		}

		// Skip if the object can't be interacted with anymore
		if (p.pointed.type != POINTEDTHING_OBJECT || pointed_object->isGone())
			return;

		ItemStack selected_item, hand_item;
		ItemStack tool_item = playersao->getWieldedItem(&selected_item, &hand_item);
		const ToolCapabilities &toolcap =
				tool_item.getToolCapabilities(m_itemdef, &hand_item);
		v3f dir = (pointed_object->getBasePosition() -
				(playersao->getBasePosition() + playersao->getEyeOffset())
					).normalize();
		float time_from_last_punch =
			playersao->resetTimeFromLastPunch();

		u32 wear = pointed_object->punch(dir, toolcap, playersao,
				time_from_last_punch, tool_item.wear);

		// Callback may have changed item, so get it again
		playersao->getWieldedItem(&selected_item);
		bool changed = selected_item.addWear(wear, m_itemdef);
		if (changed)
			playersao->setWieldedItem(selected_item);

		return;
	} // action == INTERACT_START_DIGGING

	case INTERACT_STOP_DIGGING:
		// Nothing to do
		return;

	case INTERACT_DIGGING_COMPLETED: {
		// Only digging of nodes
		if (p.pointed.type != POINTEDTHING_NODE)
			return;
		bool pos_ok;
		v3s16 p_under = p.pointed.node_undersurface;
		MapNode n = m_env->getMap().getNode(p_under, &pos_ok);
		if (!pos_ok) {
			infostream << "Server: Not finishing digging: Node not found. "
				"Adding block to emerge queue." << std::endl;
			m_emerge->enqueueBlockEmerge(peer_id,
				getNodeBlockPos(p.pointed.node_abovesurface), false);
		}

		/* Cheat prevention */
		bool is_valid_dig = true;
		if ((anticheat_flags & AC_DIGGING) && !isSingleplayer()) {
			v3s16 nocheat_p = playersao->getNoCheatDigPos();
			float nocheat_t = playersao->getNoCheatDigTime();
			playersao->noCheatDigEnd();
			// If player didn't start digging this, ignore dig
			if (nocheat_p != p_under) {
				infostream << "Server: " << player->getName()
						<< " started digging "
						<< nocheat_p << " and completed digging "
						<< p_under << "; not digging." << std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "finished_unknown_dig");
			}

			// Get player's wielded item
			// See also: Game::handleDigging
			ItemStack selected_item, hand_item;
			ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

			// Get diggability and expected digging time
			DigParams params = getDigParams(m_nodedef->get(n).groups,
					&tool_item.getToolCapabilities(m_itemdef, &hand_item),
					tool_item.wear);
			// If can't dig, try hand
			if (!params.diggable) {
				params = getDigParams(m_nodedef->get(n).groups,
					&hand_item.getToolCapabilities(m_itemdef));
			}
			// If can't dig, ignore dig
			if (!params.diggable) {
				infostream << "Server: " << player->getName()
						<< " completed digging " << p_under
						<< ", which is not diggable with tool; not digging."
						<< std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "dug_unbreakable");
			}
			// Check digging time
			// If already invalidated, we don't have to
			if (!is_valid_dig) {
				// Well not our problem then
			}
			// Clean and long dig
			else if (params.time > 2.0 && nocheat_t * 1.2 > params.time) {
				// All is good, but grab time from pool; don't care if
				// it's actually available
				playersao->getDigPool().grab(params.time);
			}
			// Short or laggy dig
			// Try getting the time from pool
			else if (playersao->getDigPool().grab(params.time)) {
				// All is good
			}
			// Dig not possible
			else {
				infostream << "Server: " << player->getName()
						<< " completed digging " << p_under
						<< "too fast; not digging." << std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "dug_too_fast");
			}
		}

		/* Actually dig node */

		if (is_valid_dig && n.getContent() != CONTENT_IGNORE)
			m_script->node_on_dig(p_under, n, playersao);

		v3s16 blockpos = getNodeBlockPos(p_under);
		RemoteClient *client = getClient(peer_id);
		// Send unusual result (that is, node not being removed)
		if (m_env->getMap().getNode(p_under).getContent() != CONTENT_AIR)
			// Re-send block to revert change on client-side
			client->SetBlockNotSent(blockpos);
		else
			client->ResendBlockIfOnWire(blockpos);

		return;
	} // action == INTERACT_DIGGING_COMPLETED

	// Place block or right-click object
	case INTERACT_PLACE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		const bool had_prediction = !selected_item->getDefinition(m_itemdef).
			node_placement_prediction.empty();

		if (p.pointed.type == POINTEDTHING_OBJECT) {
			// Right click object

			// Skip if object can't be interacted with anymore
			if (pointed_object->isGone())
				return;

			actionstream << player->getName() << " right-clicks object "
					<< p.pointed.object_id << ": "
					<< pointed_object->getDescription() << std::endl;

			// Do stuff
			if (m_script->item_OnSecondaryUse(selected_item, playersao, p.pointed)) {
				if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
					SendInventory(player, true);
			}

			// on_secondary_use might have removed the object
			if (pointed_object->isGone())
				return;

			pointed_object->rightClick(playersao);
		} else if (m_script->item_OnPlace(selected_item, playersao, p.pointed)) {
			// Placement was handled in lua

			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		if (p.pointed.type != POINTEDTHING_NODE)
			return;

		getClient(peer_id)->m_time_from_building = 0;

		// If item has node placement prediction, always send the
		// blocks to make sure the client knows what exactly happened
		RemoteClient *client = getClient(peer_id);
		v3s16 blockpos = getNodeBlockPos(p.pointed.node_abovesurface);
		v3s16 blockpos2 = getNodeBlockPos(p.pointed.node_undersurface);
		if (had_prediction) {
			client->SetBlockNotSent(blockpos);
			if (blockpos2 != blockpos)
				client->SetBlockNotSent(blockpos2);
		} else {
			client->ResendBlockIfOnWire(blockpos);
			if (blockpos2 != blockpos)
				client->ResendBlockIfOnWire(blockpos2);
		}

		return;
	} // action == INTERACT_PLACE

	case INTERACT_USE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		actionstream << player->getName() << " uses " << selected_item->name
				<< ", pointing at " << p.pointed.dump() << std::endl;

		if (m_script->item_OnUse(selected_item, playersao, p.pointed)) {
			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		return;
	}

	// Rightclick air
	case INTERACT_ACTIVATE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		actionstream << player->getName() << " activates "
				<< selected_item->name << std::endl;

		// The legacy code reassigned p.pointed.type to NOTHING here, but
		// since the decoder is the only producer of pointed and the wire
		// only ever sends NOTHING for INTERACT_ACTIVATE, the field is
		// already correct. We pass it through to the script as-is.
		if (m_script->item_OnSecondaryUse(selected_item, playersao, p.pointed)) {
			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		return;
	}

	default:
		warningstream << "Server: Invalid action " << action << std::endl;

	}
}

void Server::handleCommand_RemovedSounds(const server::PacketContext &ctx,
	const server::RemovedSoundsPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	for (s32 id : p.ids) {
		auto i = m_playing_sounds.find(id);
		if (i == m_playing_sounds.end())
			continue;

		ServerPlayingSound &psound = i->second;
		psound.clients.erase(peer_id);
		if (psound.clients.empty())
			m_playing_sounds.erase(i);
	}
}

void Server::handleCommand_NodeMetaFields(const server::PacketContext &ctx,
	const server::NodeMetaFieldsPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	const v3s16 &pos = p.pos;
	const std::string &formname = p.formname;
	const StringMap &fields = p.fields;

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			"player:" + player->getName());

	// Check the target node for rollback data; leave others unnoticed
	RollbackNode rn_old(&m_env->getMap(), pos, this);

	m_script->node_on_receive_fields(pos, formname, fields, playersao);

	// Report rollback data
	RollbackNode rn_new(&m_env->getMap(), pos, this);
	if (rollback() && rn_new != rn_old) {
		RollbackAction action;
		action.setSetNode(pos, rn_old, rn_new);
		rollback()->reportAction(action);
	}
}

void Server::handleCommand_InventoryFields(const server::PacketContext &ctx,
	const server::InventoryFieldsPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemotePlayer *player = m_env->getPlayer(peer_id);

	if (!player)
		return;
	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao)
		return;

	const std::string &client_formspec_name = p.formspec_name;
	const StringMap &fields = p.fields;

	if (client_formspec_name.empty()) { // pass through inventory submits
		m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
		return;
	}

	// verify that we displayed the formspec to the user
	const auto it = m_formspec_state_data.find(peer_id);
	if (it != m_formspec_state_data.end()) {
		const auto &server_formspec_name = it->second;
		if (client_formspec_name == server_formspec_name) {
			// delete state if formspec was closed
			auto it2 = fields.find("quit");
			if (it2 != fields.end() && it2->second == "true")
				m_formspec_state_data.erase(it);

			m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
			return;
		}
		actionstream << player->getName()
			<< " submitted formspec ('" << client_formspec_name
			<< "') but the name of the formspec doesn't match the"
			" expected name ('" << server_formspec_name << "')";

	} else {
		actionstream << player->getName()
			<< " submitted formspec ('" << client_formspec_name
			<< "') but server hasn't sent formspec to client";
	}
	actionstream << ", possible exploitation attempt" << std::endl;
}

void Server::handleCommand_FirstSrp(const server::PacketContext &ctx,
	const server::FirstSrpPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();
	const std::string playername = client->getName();

	const std::string &salt = p.salt;
	const std::string &verification_key = p.verification_key;
	const u8 is_empty = p.is_empty;

	std::string addr_s = client->getAddress().serializeString();

	verbosestream << "Server: Got TOSERVER_FIRST_SRP from " << addr_s
		<< ", with is_empty=" << (is_empty == 1) << std::endl;

	const bool empty_disallowed = !isSingleplayer() && is_empty == 1 &&
		g_settings->getBool("disallow_empty_password");

	// Either this packet is sent because the user is new or to change the password
	if (cstate == CS_HelloSent) {
		if (!client->isMechAllowed(AUTH_MECHANISM_FIRST_SRP)) {
			actionstream << "Server: Client from " << addr_s
					<< " tried to set password without being "
					<< "authenticated, or the username being new." << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}

		if (empty_disallowed) {
			actionstream << "Server: " << playername
					<< " supplied empty password from " << addr_s << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_EMPTY_PASSWORD);
			return;
		}

		std::string encpwd = encode_srp_verifier(verification_key, salt);

		// It is possible for multiple connections to get this far with the same
		// player name. In the end only one player with a given name will be emerged
		// (see Server::StateTwoClientInit) but we still have to be careful here.
		if (m_script->getAuth(playername, nullptr, nullptr)) {
			// Another client beat us to it
			actionstream << "Server: Client from " << addr_s
				<< " tried to register " << playername << " a second time."
				<< std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_ALREADY_CONNECTED);
			return;
		}

		m_script->createAuth(playername, encpwd);
		client->setEncryptedPassword(encpwd);

		m_script->on_authplayer(playername, addr_s, true);
		acceptAuth(peer_id, false);
	} else {
		if (cstate < CS_SudoMode) {
			infostream << "Server: Ignoring TOSERVER_FIRST_SRP from "
					<< addr_s << ": " << "Client has wrong state " << cstate << "."
					<< std::endl;
			return;
		}
		m_clients.event(peer_id, CSE_SudoLeave);

		if (empty_disallowed) {
			actionstream << "Server: " << playername
					<< " supplied empty password" << std::endl;
			SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
				L"Changing to an empty password is not allowed."));
			return;
		}

		std::string encpwd = encode_srp_verifier(verification_key, salt);
		bool success = m_script->setPassword(playername, encpwd);
		if (success) {
			actionstream << playername << " changes password" << std::endl;
			SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
				L"Password change successful."));
			client->setEncryptedPassword(encpwd);
		} else {
			actionstream << playername <<
				" tries to change password but it fails" << std::endl;
			SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
				L"Password change failed or unavailable."));
		}
	}
}

void Server::handleCommand_SrpBytesA(const server::PacketContext &ctx,
	const server::SrpBytesAPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();

	std::string addr_s = client->getAddress().serializeString();

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		actionstream << "Server: got SRP _A packet in wrong state " << cstate <<
			" from " << addr_s <<
			". Ignoring." << std::endl;
		return;
	}

	const bool wantSudo = (cstate == CS_Active);

	if (client->chosen_mech != AUTH_MECHANISM_NONE) {
		actionstream << "Server: got SRP _A packet, while auth is already "
			"going on with mech " << client->chosen_mech << " from " <<
			addr_s <<
			" (wantSudo=" << wantSudo << "). Ignoring." << std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	const std::string &bytes_A = p.bytes_A;
	const u8 based_on = p.based_on;

	infostream << "Server: TOSERVER_SRP_BYTES_A received with "
		<< "based_on=" << int(based_on) << " and len_A="
		<< bytes_A.length() << std::endl;

	AuthMechanism chosen = (based_on == 0) ?
		AUTH_MECHANISM_LEGACY_PASSWORD : AUTH_MECHANISM_SRP;

	if (wantSudo) {
		// Right now, the auth mechs don't change between login and sudo mode.
		if (!client->isMechAllowed(chosen)) {
			actionstream << "Server: Player \"" << client->getName() <<
				"\" from " << addr_s <<
				" tried to change password using unallowed mech " << chosen <<
				std::endl;
			DenySudoAccess(peer_id);
			return;
		}
	} else {
		if (!client->isMechAllowed(chosen)) {
			actionstream << "Server: Client tried to authenticate from " <<
				addr_s <<
				" using unallowed mech " << chosen << std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
			return;
		}
	}

	client->chosen_mech = chosen;

	std::string salt, verifier;

	if (based_on == 0) {

		generate_srp_verifier_and_salt(client->getName(), client->enc_pwd,
			&verifier, &salt);
	} else if (!decode_srp_verifier_and_salt(client->enc_pwd, &verifier, &salt)) {
		// Non-base64 errors should have been catched in the init handler
		actionstream << "Server: User " << client->getName() <<
			" tried to log in, but srp verifier field was invalid (most likely "
			"invalid base64)." << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
		return;
	}

	char *bytes_B = 0;
	size_t len_B = 0;

	client->auth_data = srp_verifier_new(SRP_SHA256, SRP_NG_2048,
		client->getName().c_str(),
		(const unsigned char *) salt.c_str(), salt.size(),
		(const unsigned char *) verifier.c_str(), verifier.size(),
		(const unsigned char *) bytes_A.c_str(), bytes_A.size(),
		NULL, 0,
		(unsigned char **) &bytes_B, &len_B, NULL, NULL);

	if (!bytes_B) {
		actionstream << "Server: User " << client->getName()
			<< " tried to log in, SRP-6a safety check violated in _A handler."
			<< std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			client->resetChosenMech();
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	m_netserver->sendSrpBytesSandB(peer_id, salt, bytes_B, len_B);
}

void Server::handleCommand_SrpBytesM(const server::PacketContext &ctx,
	const server::SrpBytesMPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	ClientState cstate = client->getState();
	const std::string addr_s = client->getAddress().serializeString();
	const std::string playername = client->getName();

	const bool wantSudo = (cstate == CS_Active);

	verbosestream << "Server: Received TOSERVER_SRP_BYTES_M." << std::endl;

	if (!((cstate == CS_HelloSent) || (cstate == CS_Active))) {
		warningstream << "Server: got SRP_M packet in wrong state "
			<< cstate << " from " << addr_s << ". Ignoring." << std::endl;
		return;
	}

	if (client->chosen_mech != AUTH_MECHANISM_SRP &&
			client->chosen_mech != AUTH_MECHANISM_LEGACY_PASSWORD) {
		warningstream << "Server: got SRP_M packet, while auth "
			"is going on with mech " << client->chosen_mech << " from "
			<< addr_s << " (wantSudo=" << wantSudo << "). Denying." << std::endl;
		if (wantSudo) {
			DenySudoAccess(peer_id);
			return;
		}

		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	const std::string &bytes_M = p.bytes_M;

	if (srp_verifier_get_session_key_length((SRPVerifier *) client->auth_data)
			!= bytes_M.size()) {
		actionstream << "Server: User " << playername << " at " << addr_s
			<< " sent bytes_M with invalid length " << bytes_M.size() << std::endl;
		DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		return;
	}

	unsigned char *bytes_HAMK = 0;

	srp_verifier_verify_session((SRPVerifier *) client->auth_data,
		(unsigned char *)bytes_M.c_str(), &bytes_HAMK);

	// skip authentication check for singleplayer world.
	const bool is_true_singleplayer = isSingleplayer() && (strcasecmp(playername.c_str(), "singleplayer") == 0);
	if (!bytes_HAMK && !is_true_singleplayer) {
		if (wantSudo) {
			actionstream << "Server: User " << playername << " at " << addr_s
				<< " tried to change their password, but supplied wrong"
				<< " (SRP) password for authentication." << std::endl;
			DenySudoAccess(peer_id);
			client->resetChosenMech();
			return;
		}

		actionstream << "Server: User " << playername << " at " << addr_s
			<< " supplied wrong password (auth mechanism: SRP)." << std::endl;
		m_script->on_authplayer(playername, addr_s, false);
		DenyAccess(peer_id, SERVER_ACCESSDENIED_WRONG_PASSWORD);
		return;
	}

	if (client->create_player_on_auth_success) {
		m_script->createAuth(playername, client->enc_pwd);

		if (!m_script->getAuth(playername, nullptr, nullptr)) {
			errorstream << "Server: " << playername <<
				" cannot be authenticated (auth handler does not work?)" <<
				std::endl;
			DenyAccess(peer_id, SERVER_ACCESSDENIED_SERVER_FAIL);
			return;
		}
		client->create_player_on_auth_success = false;
	}

	m_script->on_authplayer(playername, addr_s, true);
	acceptAuth(peer_id, wantSudo);
}

/*
 * Mod channels
 */

void Server::handleCommand_ModChannelJoin(const server::PacketContext &ctx,
	const server::ModChannelJoinPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	const std::string &channel_name = p.channel_name;

	// Send signal to client to notify join succeed or not
	ModChannelSignal sig;
	if (g_settings->getBool("enable_mod_channels") &&
			m_modchannel_mgr->joinChannel(channel_name, peer_id)) {
		sig = MODCHANNEL_SIGNAL_JOIN_OK;
		infostream << "Peer " << peer_id << " joined channel " <<
			channel_name << std::endl;
	}
	else {
		sig = MODCHANNEL_SIGNAL_JOIN_FAILURE;
		infostream << "Peer " << peer_id << " tried to join channel " <<
			channel_name << ", but was already registered." << std::endl;
	}
	m_netserver->sendModChannelSignal(peer_id, sig, channel_name);
}

void Server::handleCommand_ModChannelLeave(const server::PacketContext &ctx,
	const server::ModChannelLeavePayload &p)
{
	const session_t peer_id = ctx.peer_id;
	const std::string &channel_name = p.channel_name;

	ModChannelSignal sig;
	if (g_settings->getBool("enable_mod_channels") &&
			m_modchannel_mgr->leaveChannel(channel_name, peer_id)) {
		sig = MODCHANNEL_SIGNAL_LEAVE_OK;
		infostream << "Peer " << peer_id << " left channel " <<
			channel_name << std::endl;
	} else {
		sig = MODCHANNEL_SIGNAL_LEAVE_FAILURE;
		infostream << "Peer " << peer_id << " left channel " << channel_name <<
			", but was not registered." << std::endl;
	}
	m_netserver->sendModChannelSignal(peer_id, sig, channel_name);
}

void Server::handleCommand_ModChannelMsg(const server::PacketContext &ctx,
	const server::ModChannelMsgPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	const std::string &channel_name = p.channel_name;
	const std::string &channel_msg = p.message;

	verbosestream << "Mod channel message received from peer " << peer_id <<
		" on channel " << channel_name << " message: " << channel_msg <<
		std::endl;

	// If mod channels are not enabled, discard message
	if (!g_settings->getBool("enable_mod_channels")) {
		return;
	}

	// If channel not registered, signal it and ignore message
	if (!m_modchannel_mgr->channelRegistered(channel_name)) {
		m_netserver->sendModChannelSignal(peer_id,
			MODCHANNEL_SIGNAL_CHANNEL_NOT_REGISTERED, channel_name);
		return;
	}

	// @TODO: filter, rate limit

	broadcastModChannelMessage(channel_name, channel_msg, peer_id);
}

void Server::handleCommand_HaveMedia(const server::PacketContext &ctx,
	const server::HaveMediaPayload &p)
{
	const session_t peer_id = ctx.peer_id;
	auto player = m_env->getPlayer(peer_id);

	for (const u32 token : p.tokens) {
		auto it = m_pending_dyn_media.find(token);
		if (it == m_pending_dyn_media.end())
			continue;
		if (it->second.waiting_players.count(peer_id)) {
			it->second.waiting_players.erase(peer_id);
			if (player)
				getScriptIface()->on_dynamic_media_added(token, player->getName());
		}
	}
}

void Server::handleCommand_UpdateClientInfo(const server::PacketContext &ctx,
	const server::UpdateClientInfoPayload &p)
{
	ClientDynamicInfo info;
	info.render_target_size = p.render_target_size;
	info.real_gui_scaling = p.real_gui_scaling;
	info.real_hud_scaling = p.real_hud_scaling;
	// v2u32 -> v2f32 conversion for the FS size, matching the old behaviour.
	info.max_fs_size = v2f32((f32)p.max_fs_size.X, (f32)p.max_fs_size.Y);
	info.touch_controls = p.touch_controls;

	const session_t peer_id = ctx.peer_id;
	RemoteClient *client = getClient(peer_id, CS_Invalid);
	client->setDynamicInfo(info);
}
