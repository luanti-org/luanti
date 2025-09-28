// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "client/client.h"

#include "exceptions.h"
#include "irr_v2d.h"
#include "util/base64.h"
#include "client/camera.h"
#include "client/mesh_generator_thread.h"
#include "chatmessage.h"
#include "client/clientmedia.h"
#include "log.h"
#include "servermap.h"
#include "mapsector.h"
#include "client/minimap.h"
#include "modchannels.h"
#include "nodedef.h"
#include "serialization.h"
#include "util/strfnd.h"
#include "client/clientevent.h"
#include "client/sound.h"
#include "client/localplayer.h"
#include "network/clientopcodes.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "script/scripting_client.h"
#include "util/serialize.h"
#include "util/srp.h"
#include "util/hashing.h"
#include "tileanimation.h"
#include "gettext.h"
#include "skyparams.h"
#include "particles.h"
#include <memory>
#include <sstream>

const char *accessDeniedStrings[SERVER_ACCESSDENIED_MAX] = {
	N_("Invalid password"),
	N_("Your client sent something the server didn't expect.  Try reconnecting or updating your client."),
	N_("The server is running in singleplayer mode.  You cannot connect."),
	N_("Your client's version is not supported.\nPlease contact the server administrator."),
	N_("Player name contains disallowed characters"),
	N_("Player name not allowed"),
	N_("Too many users"),
	N_("Empty passwords are disallowed.  Set a password and try again."),
	N_("Another client is connected with this name.  If your client closed unexpectedly, try again in a minute."),
	N_("Internal server error"),
	"",
	N_("Server shutting down"),
	N_("The server has experienced an internal error.  You will now be disconnected.")
};

void Client::handleCommand_Deprecated(NetworkPacket* pkt)
{
	infostream << "Got deprecated command "
			<< toClientCommandTable[pkt->getCommand()].name << " from peer "
			<< pkt->getPeerId() << "!" << std::endl;
}

void Client::handleCommand_Hello(NetworkPacket* pkt)
{
	if (pkt->getSize() < 1)
		return;

	u8 serialization_ver; // negotiated value
	u16 proto_ver;
	u16 unused_compression_mode;
	u32 auth_mechs;
	std::string unused;
	*pkt >> serialization_ver >> unused_compression_mode >> proto_ver
		>> auth_mechs >> unused;

	// Chose an auth method we support
	AuthMechanism chosen_auth_mechanism = choseAuthMech(auth_mechs);

	infostream << "Client: TOCLIENT_HELLO received with "
			<< "serialization_ver=" << (u32)serialization_ver
			<< ", auth_mechs=" << auth_mechs
			<< ", proto_ver=" << proto_ver
			<< ". Doing auth with mech " << chosen_auth_mechanism << std::endl;

	if (!ser_ver_supported_read(serialization_ver)) {
		infostream << "Client: TOCLIENT_HELLO: Server sent "
				<< "unsupported ser_fmt_ver=" << (int)serialization_ver << std::endl;
		return;
	}

	m_server_ser_ver = serialization_ver;
	m_proto_ver = proto_ver;

	if (m_chosen_auth_mech != AUTH_MECHANISM_NONE) {
		// we received a TOCLIENT_HELLO while auth was already going on
		errorstream << "Client: TOCLIENT_HELLO while auth was already going on"
			<< "(chosen_mech=" << m_chosen_auth_mech << ")." << std::endl;
		if (m_chosen_auth_mech == AUTH_MECHANISM_SRP ||
				m_chosen_auth_mech == AUTH_MECHANISM_LEGACY_PASSWORD) {
			srp_user_delete((SRPUser *) m_auth_data);
			m_auth_data = 0;
		}
	}

	// Authenticate using that method, or abort if there wasn't any method found
	if (chosen_auth_mechanism != AUTH_MECHANISM_NONE) {
		bool is_register = chosen_auth_mechanism == AUTH_MECHANISM_FIRST_SRP;
		ELoginRegister mode = is_register ? ELoginRegister::Register : ELoginRegister::Login;
		if (m_allow_login_or_register != ELoginRegister::Any &&
				m_allow_login_or_register != mode) {
			m_chosen_auth_mech = AUTH_MECHANISM_NONE;
			m_access_denied = true;
			if (m_allow_login_or_register == ELoginRegister::Login) {
				m_access_denied_reason =
						gettext("Name is not registered. To create an account on this server, click 'Register'");
			} else {
				m_access_denied_reason =
						gettext("Name is taken. Please choose another name");
			}
			m_con->Disconnect();
		} else {
			startAuth(chosen_auth_mechanism);
		}
	} else {
		m_chosen_auth_mech = AUTH_MECHANISM_NONE;
		m_access_denied = true;
		m_access_denied_reason = "Unknown";
		m_con->Disconnect();
	}

}

void Client::handleCommand_AuthAccept(NetworkPacket* pkt)
{
	deleteAuthData();

	v3f unused;
	*pkt >> unused >> m_map_seed >> m_recommended_send_interval
		>> m_sudo_auth_methods;

	infostream << "Client: received map seed: " << m_map_seed << std::endl;
	infostream << "Client: received recommended send interval "
					<< m_recommended_send_interval<<std::endl;

	// Reply to server
	/*~ DO NOT TRANSLATE THIS LITERALLY!
	This is a special string which needs to contain the translation's
	language code (e.g. "de" for German). */
	std::string lang = gettext("LANG_CODE");
	if (lang == "LANG_CODE")
		lang.clear();

	NetworkPacket resp_pkt(TOSERVER_INIT2, sizeof(u16) + lang.size());
	resp_pkt << lang;
	Send(&resp_pkt);

	m_state = LC_Init;

	// Log meaningful info
	if (!m_internal_server) {
		Address remote = m_con->GetPeerAddress(PEER_ID_SERVER);
		actionstream << "Connected to " << m_address_name << " (";
		remote.print(actionstream);
		actionstream << ")" << std::endl;
	}
}

void Client::handleCommand_AcceptSudoMode(NetworkPacket* pkt)
{
	deleteAuthData();

	m_password = m_new_password;

	verbosestream << "Client: Received TOCLIENT_ACCEPT_SUDO_MODE." << std::endl;

	// send packet to actually set the password
	startAuth(AUTH_MECHANISM_FIRST_SRP);

	// reset again
	m_chosen_auth_mech = AUTH_MECHANISM_NONE;
}

void Client::handleCommand_DenySudoMode(NetworkPacket* pkt)
{
	ChatMessage *chatMessage = new ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
			L"Password change denied. Password NOT changed.");
	pushToChatQueue(chatMessage);
	// reset everything and be sad
	deleteAuthData();
}

void Client::handleCommand_AccessDenied(NetworkPacket* pkt)
{
	// The server didn't like our password. Note, this needs
	// to be processed even if the serialization format has
	// not been agreed yet, the same as TOCLIENT_AUTH_ACCEPT.
	m_access_denied = true;

	if (pkt->getCommand() != TOCLIENT_ACCESS_DENIED) {
		// Servers older than 5.6 still send TOCLIENT_ACCESS_DENIED_LEGACY sometimes.
		// see commit a65f6f07f3a5601207b790edcc8cc945133112f7
		if (pkt->getSize() >= 2) {
			std::wstring wide_reason;
			*pkt >> wide_reason;
			m_access_denied_reason = wide_to_utf8(wide_reason);
		}
		return;
	}

	if (pkt->getSize() < 1)
		return;

	u8 denyCode;
	*pkt >> denyCode;

	if (pkt->getRemainingBytes() > 0)
		*pkt >> m_access_denied_reason;

	if (m_access_denied_reason.empty()) {
		if (denyCode >= SERVER_ACCESSDENIED_MAX) {
			m_access_denied_reason = gettext("Unknown disconnect reason.");
		} else if (denyCode != SERVER_ACCESSDENIED_CUSTOM_STRING) {
			m_access_denied_reason = gettext(accessDeniedStrings[denyCode]);
		}
	}

	if (denyCode == SERVER_ACCESSDENIED_TOO_MANY_USERS) {
		m_access_denied_reconnect = true;
	} else if (pkt->getRemainingBytes() > 0) {
		u8 reconnect;
		*pkt >> reconnect;
		m_access_denied_reconnect = reconnect & 1;
	}
}

void Client::handleCommand_RemoveNode(NetworkPacket* pkt)
{
	v3s16 p;
	*pkt >> p;
	removeNode(p);
}

void Client::handleCommand_AddNode(NetworkPacket* pkt)
{
	v3s16 p;
	*pkt >> p;

	auto *ptr = reinterpret_cast<const u8*>(pkt->getRemainingString());
	pkt->skip(MapNode::serializedLength(m_server_ser_ver)); // performs length check

	MapNode n;
	n.deSerialize(ptr, m_server_ser_ver);

	bool keep_metadata;
	*pkt >> keep_metadata;

	addNode(p, n, !keep_metadata);
}

void Client::handleCommand_NodemetaChanged(NetworkPacket *pkt)
{
	if (pkt->getSize() < 1)
		return;

	std::istringstream is(pkt->readLongString(), std::ios::binary);
	std::stringstream sstr(std::ios::binary | std::ios::in | std::ios::out);
	decompressZlib(is, sstr);

	NodeMetadataList meta_updates_list(false);
	meta_updates_list.deSerialize(sstr, m_itemdef, true);

	Map &map = m_env.getMap();
	for (auto i = meta_updates_list.begin();
			i != meta_updates_list.end(); ++i) {
		v3s16 pos = i->first;

		if (map.isValidPosition(pos) &&
				map.setNodeMetadata(pos, i->second))
			continue; // Prevent from deleting metadata

		// Meta couldn't be set, unused metadata
		delete i->second;
	}
}

void Client::handleCommand_BlockData(NetworkPacket* pkt)
{
	// Ignore too small packet
	if (pkt->getSize() < 6)
		return;

	v3s16 p;
	*pkt >> p;

	std::string datastring(pkt->getRemainingString(), pkt->getRemainingBytes());
	std::istringstream istr(datastring, std::ios_base::binary);

	MapSector *sector;
	MapBlock *block;

	v2s16 p2d(p.X, p.Z);
	sector = m_env.getMap().emergeSector(p2d);

	assert(sector->getPos() == p2d);

	block = sector->getBlockNoCreateNoEx(p.Y);
	if (block) {
		/*
			Update an existing block
		*/
		block->deSerialize(istr, m_server_ser_ver, false);
		block->deSerializeNetworkSpecific(istr);
	}
	else {
		/*
			Create a new block
		*/
		block = sector->createBlankBlock(p.Y);
		block->deSerialize(istr, m_server_ser_ver, false);
		block->deSerializeNetworkSpecific(istr);
	}

	if (m_localdb) {
		ServerMap::saveBlock(block, m_localdb.get());
	}

	/*
		Add it to mesh update queue and set it to be acknowledged after update.
	*/
	addUpdateMeshTaskWithEdge(p, true);
}

void Client::handleCommand_Inventory(NetworkPacket* pkt)
{
	if (pkt->getSize() < 1)
		return;

	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	player->inventory.deSerialize(is);

	m_update_wielded_item = true;

	m_inventory_from_server = std::make_unique<Inventory>(player->inventory);
	m_inventory_from_server_age = 0.0f;
}

void Client::handleCommand_TimeOfDay(NetworkPacket* pkt)
{
	if (pkt->getSize() < 2)
		return;

	u16 time_of_day;
	*pkt >> time_of_day;
	time_of_day      = time_of_day % 24000;

	float time_speed;
	*pkt >> time_speed;

	// Update environment
	m_env.setTimeOfDay(time_of_day);
	m_env.setTimeOfDaySpeed(time_speed);
}

void Client::handleCommand_ChatMessage(NetworkPacket *pkt)
{
	/*
		u8 version
		u8 message_type
		u16 sendername length
		wstring sendername
		u16 length
		wstring message
	 */

	ChatMessage *chatMessage = new ChatMessage();
	u8 version, message_type;
	*pkt >> version >> message_type;

	if (version != 1 || message_type >= CHATMESSAGE_TYPE_MAX) {
		delete chatMessage;
		return;
	}

	u64 timestamp;
	*pkt >> chatMessage->sender >> chatMessage->message >> timestamp;
	chatMessage->timestamp = static_cast<std::time_t>(timestamp);

	chatMessage->type = (ChatMessageType) message_type;

	// log the chat message
	actionstream << "CHAT: " << wide_to_utf8(unescape_enriched(chatMessage->message)) << std::endl;

	// @TODO send this to CSM using ChatMessage object
	if (modsLoaded() && m_script->on_receiving_message(
			wide_to_utf8(chatMessage->message))) {
		// Message was consumed by CSM and should not be handled by client
		delete chatMessage;
	} else {
		pushToChatQueue(chatMessage);
	}
}

void Client::handleCommand_ActiveObjectRemoveAdd(NetworkPacket* pkt)
{
	/*
		u16 count of removed objects
		for all removed objects {
			u16 id
		}
		u16 count of added objects
		for all added objects {
			u16 id
			u8 type
			u32 initialization data length
			string initialization data
		}
	*/

	try {
		u8 type;
		u16 removed_count, added_count, id;

		// Read removed objects
		*pkt >> removed_count;

		for (u16 i = 0; i < removed_count; i++) {
			*pkt >> id;
			m_env.removeActiveObject(id);
			// Object-attached sounds MUST NOT be removed here because they might
			// have started to play immediately before the entity was removed.
		}

		// Read added objects
		*pkt >> added_count;

		for (u16 i = 0; i < added_count; i++) {
			*pkt >> id >> type;
			m_env.addActiveObject(id, type, pkt->readLongString());
		}
	} catch (PacketError &e) {
		infostream << "handleCommand_ActiveObjectRemoveAdd: " << e.what()
				<< ". The packet is unreliable, ignoring" << std::endl;
	}

	// m_activeobjects_received is false before the first
	// TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD packet is received
	m_activeobjects_received = true;
}

void Client::handleCommand_ActiveObjectMessages(NetworkPacket* pkt)
{
	/*
		for all objects
		{
			u16 id
			u16 message length
			string message
		}
	*/
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	try {
		while (is.good()) {
			u16 id = readU16(is);
			if (!is.good())
				break;

			std::string message = deSerializeString16(is);

			// Pass on to the environment
			m_env.processActiveObjectMessage(id, message);
		}
	} catch (SerializationError &e) {
		errorstream << "Client::handleCommand_ActiveObjectMessages: "
			<< "caught SerializationError: " << e.what() << std::endl;
	}
}

void Client::handleCommand_Movement(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	float mad, maa, maf, msw, mscr, msf, mscl, msj, lf, lfs, ls, g;

	*pkt >> mad >> maa >> maf >> msw >> mscr >> msf >> mscl >> msj
		>> lf >> lfs >> ls >> g;

	player->movement_acceleration_default   = mad * BS;
	player->movement_acceleration_air       = maa * BS;
	player->movement_acceleration_fast      = maf * BS;
	player->movement_speed_walk             = msw * BS;
	player->movement_speed_crouch           = mscr * BS;
	player->movement_speed_fast             = msf * BS;
	player->movement_speed_climb            = mscl * BS;
	player->movement_speed_jump             = msj * BS;
	player->movement_liquid_fluidity        = lf * BS;
	player->movement_liquid_fluidity_smooth = lfs * BS;
	player->movement_liquid_sink            = ls * BS;
	player->movement_gravity                = g * BS;
}

void Client::handleCommand_Fov(NetworkPacket *pkt)
{
	f32 fov;
	bool is_multiplier = false;
	f32 transition_time = 0.0f;

	*pkt >> fov >> is_multiplier;

	// Wrap transition_time extraction within a
	// try-catch to preserve backwards compat
	try {
		*pkt >> transition_time;
	} catch (PacketError &e) {};

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player);
	player->setFov({ fov, is_multiplier, transition_time });
	m_camera->notifyFovChange();
}

void Client::handleCommand_HP(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	u16 oldhp = player->hp;

	u16 hp;
	*pkt >> hp;
	bool damage_effect = true;
	try {
		*pkt >> damage_effect;
	} catch (PacketError &e) {};

	player->hp = hp;

	if (modsLoaded())
		m_script->on_hp_modification(hp);

	if (hp < oldhp) {
		// Add to ClientEvent queue
		ClientEvent *event = new ClientEvent();
		event->type = CE_PLAYER_DAMAGE;
		event->player_damage.amount = oldhp - hp;
		event->player_damage.effect = damage_effect;
		m_client_event_queue.push(event);
	}
}

void Client::handleCommand_Breath(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	u16 breath;

	*pkt >> breath;

	player->setBreath(breath);
}

void Client::handleCommand_MovePlayer(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	v3f pos;
	f32 pitch, yaw;

	*pkt >> pos >> pitch >> yaw;

	player->setPosition(pos);

	infostream << "Client got TOCLIENT_MOVE_PLAYER"
			<< " pos=" << pos
			<< " pitch=" << pitch
			<< " yaw=" << yaw
			<< std::endl;

	/*
		Add to ClientEvent queue.
		This has to be sent to the main program because otherwise
		it would just force the pitch and yaw values to whatever
		the camera points to.
	*/
	ClientEvent *event = new ClientEvent();
	event->type = CE_PLAYER_FORCE_MOVE;
	event->player_force_move.pitch = pitch;
	event->player_force_move.yaw = yaw;
	m_client_event_queue.push(event);
}

void Client::handleCommand_MovePlayerRel(NetworkPacket *pkt)
{
	v3f added_pos;

	*pkt >> added_pos;

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player);
	player->addPosition(added_pos);
}

void Client::handleCommand_DeathScreenLegacy(NetworkPacket* pkt)
{
	ClientEvent *event = new ClientEvent();
	event->type = CE_DEATHSCREEN_LEGACY;
	m_client_event_queue.push(event);
}

void Client::handleCommand_AnnounceMedia(NetworkPacket* pkt)
{
	infostream << "Client: Received media announcement: packet size: "
			<< pkt->getSize() << std::endl;

	if (m_media_downloader == NULL ||
			m_media_downloader->isStarted()) {
		const char *problem = m_media_downloader ?
			"we already saw another announcement" :
			"all media has been received already";
		errorstream << "Client: Received media announcement but "
			<< problem << "!" << std::endl;
		return;
	}

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_manager->isRunning());

	if (m_proto_ver >= 48) {
		// compressed table of media names
		std::vector<std::string> names;
		{
			std::istringstream iss(pkt->readLongString(), std::ios::binary);
			std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
			decompressZstd(iss, ss);
			names = deserializeString16Array(ss);
		}

		// raw hash for each media file
		for (auto &name : names) {
			auto sha1_raw = pkt->readRawString(20);
			m_media_downloader->addFile(name, sha1_raw);
		}
	} else {
		u16 num_files;
		*pkt >> num_files;

		std::string name, sha1_base64;
		for (u16 i = 0; i < num_files; i++) {
			*pkt >> name >> sha1_base64;

			std::string sha1_raw = base64_decode(sha1_base64);
			m_media_downloader->addFile(name, sha1_raw);
		}
	}

	{
		// Remote media servers
		std::string str;
		*pkt >> str;

		Strfnd sf(str);
		while (!sf.at_end()) {
			std::string baseurl = trim(sf.next(","));
			if (!baseurl.empty()) {
				m_remote_media_servers.emplace_back(baseurl);
				m_media_downloader->addRemoteServer(baseurl);
			}
		}
	}

	m_media_downloader->step(this);
}

void Client::handleCommand_Media(NetworkPacket* pkt)
{
	u16 num_bunches;
	u16 bunch_i;
	u32 num_files;

	*pkt >> num_bunches >> bunch_i >> num_files;

	infostream << "Client: Received files: bunch " << bunch_i << "/"
			<< num_bunches << " files=" << num_files
			<< " size=" << pkt->getSize() << std::endl;

	if (num_files == 0)
		return;

	bool init_phase = m_media_downloader && m_media_downloader->isStarted();

	if (init_phase) {
		// Mesh update thread must be stopped while
		// updating content definitions
		sanity_check(!m_mesh_update_manager->isRunning());
	}

	for (u32 i = 0; i < num_files; i++) {
		std::string name, data;

		*pkt >> name;
		data = pkt->readLongString();
		if (m_proto_ver >= 48) {
			std::istringstream iss(data, std::ios::binary);
			std::ostringstream oss(std::ios::binary);
			decompressZstd(iss, oss);
			data = oss.str();
		}

		bool ok = false;
		if (init_phase) {
			ok = m_media_downloader->conventionalTransferDone(name, data, this);
		} else {
			// Check pending dynamic transfers, one of them must be it
			for (const auto &it : m_pending_media_downloads) {
				if (it.second->conventionalTransferDone(name, data, this)) {
					ok = true;
					break;
				}
			}
		}
		if (!ok) {
			errorstream << "Client: Received media \"" << name
				<< "\" but no downloads pending. " << num_bunches << " bunches, "
				<< num_files << " in this one. (init_phase=" << init_phase
				<< ")" << std::endl;
		}
	}
}

void Client::handleCommand_NodeDef(NetworkPacket* pkt)
{
	infostream << "Client: Received node definitions: packet size: "
			<< pkt->getSize() << std::endl;

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_manager->isRunning());

	// Decompress node definitions
	std::istringstream tmp_is(pkt->readLongString(), std::ios::binary);
	std::stringstream tmp_os(std::ios::binary | std::ios::in | std::ios::out);
	if (m_proto_ver >= 48)
		decompressZstd(tmp_is, tmp_os);
	else
		decompressZlib(tmp_is, tmp_os);

	// Deserialize node definitions
	m_nodedef->deSerialize(tmp_os, m_proto_ver);
	m_nodedef_received = true;
}

void Client::handleCommand_ItemDef(NetworkPacket* pkt)
{
	infostream << "Client: Received item definitions: packet size: "
			<< pkt->getSize() << std::endl;

	// Mesh update thread must be stopped while
	// updating content definitions
	sanity_check(!m_mesh_update_manager->isRunning());

	// Decompress item definitions
	std::istringstream tmp_is(pkt->readLongString(), std::ios::binary);
	std::stringstream tmp_os(std::ios::binary | std::ios::in | std::ios::out);
	if (m_proto_ver >= 48)
		decompressZstd(tmp_is, tmp_os);
	else
		decompressZlib(tmp_is, tmp_os);

	// Deserialize node definitions
	m_itemdef->deSerialize(tmp_os, m_proto_ver);
	m_itemdef_received = true;
}

void Client::handleCommand_PlaySound(NetworkPacket* pkt)
{
	/*
		[0] s32 server_id
		[4] u16 name length
		[6] char name[len]
		[ 6 + len] f32 gain
		[10 + len] u8 type (SoundLocation)
		[11 + len] v3f pos (in BS-space)
		[23 + len] u16 object_id
		[25 + len] bool loop
		[26 + len] f32 fade
		[30 + len] f32 pitch
		[34 + len] bool ephemeral
		[35 + len] f32 start_time (in seconds)
	*/

	s32 server_id;

	SoundSpec spec;
	SoundLocation type;
	v3f pos;
	u16 object_id;
	bool ephemeral = false;

	*pkt >> server_id >> spec.name >> spec.gain >> (u8 &)type >> pos >> object_id >> spec.loop;
	pos *= 1.0f/BS;

	try {
		*pkt >> spec.fade;
		*pkt >> spec.pitch;
		*pkt >> ephemeral;
		*pkt >> spec.start_time;
	} catch (PacketError &e) {};

	// Generate a new id
	sound_handle_t client_id = (ephemeral && object_id == 0) ? 0 : m_sound->allocateId(2);

	// Start playing
	switch(type) {
	case SoundLocation::Local:
		m_sound->playSound(client_id, spec);
		break;
	case SoundLocation::Position:
		m_sound->playSoundAt(client_id, spec, pos, v3f(0.0f));
		break;
	case SoundLocation::Object: {
		ClientActiveObject *cao = m_env.getActiveObject(object_id);
		v3f vel(0.0f);
		if (cao) {
			pos = cao->getPosition() * (1.0f/BS);
			vel = cao->getVelocity() * (1.0f/BS);
		}
		// Note that the server sends 'pos' correctly even for attached sounds,
		// so this fallback path is not a mistake.
		m_sound->playSoundAt(client_id, spec, pos, vel);
		break;
	}
	default:
		// Unknown SoundLocation, instantly remove sound
		if (client_id != 0)
			m_sound->freeId(client_id, 2);
		if (!ephemeral)
			sendRemovedSounds({server_id});
		return;
	}

	if (client_id != 0) {
		// Note: m_sounds_client_to_server takes 1 ownership
		// For ephemeral sounds, server_id is not meaningful
		if (ephemeral) {
			m_sounds_client_to_server[client_id] = -1;
		} else {
			m_sounds_server_to_client[server_id] = client_id;
			m_sounds_client_to_server[client_id] = server_id;
		}
		if (object_id != 0)
			m_sounds_to_objects[client_id] = object_id;
	}
}

void Client::handleCommand_StopSound(NetworkPacket* pkt)
{
	s32 server_id;

	*pkt >> server_id;

	auto i = m_sounds_server_to_client.find(server_id);
	if (i != m_sounds_server_to_client.end()) {
		int client_id = i->second;
		m_sound->stopSound(client_id);
	}
}

void Client::handleCommand_FadeSound(NetworkPacket *pkt)
{
	s32 sound_id;
	float step;
	float gain;

	*pkt >> sound_id >> step >> gain;

	auto i = m_sounds_server_to_client.find(sound_id);
	if (i != m_sounds_server_to_client.end())
		m_sound->fadeSound(i->second, step, gain);
}

void Client::handleCommand_Privileges(NetworkPacket* pkt)
{
	m_privileges.clear();
	infostream << "Client: Privileges updated: ";
	u16 num_privileges;

	*pkt >> num_privileges;

	for (u16 i = 0; i < num_privileges; i++) {
		std::string priv;

		*pkt >> priv;

		m_privileges.insert(priv);
		infostream << priv << " ";
	}
	infostream << std::endl;
}

void Client::handleCommand_InventoryFormSpec(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	// Store formspec in LocalPlayer
	player->inventory_formspec = pkt->readLongString();
	player->inventory_formspec_override.clear();
}

void Client::handleCommand_DetachedInventory(NetworkPacket* pkt)
{
	std::string name;
	bool keep_inv = true;
	*pkt >> name >> keep_inv;

	infostream << "Client: Detached inventory update: \"" << name
		<< "\", mode=" << (keep_inv ? "update" : "remove") << std::endl;

	const auto &inv_it = m_detached_inventories.find(name);
	if (!keep_inv) {
		if (inv_it != m_detached_inventories.end()) {
			delete inv_it->second;
			m_detached_inventories.erase(inv_it);
		}
		return;
	}
	Inventory *inv = nullptr;
	if (inv_it == m_detached_inventories.end()) {
		inv = new Inventory(m_itemdef);
		m_detached_inventories[name] = inv;
	} else {
		inv = inv_it->second;
	}

	// this used to be the length of the following string, ignore it
	pkt->skip(2);

	std::string contents(pkt->getRemainingString(), pkt->getRemainingBytes());
	std::istringstream is(contents, std::ios::binary);
	inv->deSerialize(is);
}

void Client::handleCommand_ShowFormSpec(NetworkPacket* pkt)
{
	std::string formspec = pkt->readLongString();
	std::string formname;

	*pkt >> formname;

	ClientEvent *event = new ClientEvent();
	event->type = CE_SHOW_FORMSPEC;
	// pointer is required as event is a struct only!
	// adding a std:string to a struct isn't possible
	event->show_formspec.formspec = new std::string(formspec);
	event->show_formspec.formname = new std::string(formname);
	m_client_event_queue.push(event);
}

void Client::handleCommand_SpawnParticle(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	ParticleParameters p;
	p.deSerialize(is, m_proto_ver);

	ClientEvent *event = new ClientEvent();
	event->type           = CE_SPAWN_PARTICLE;
	event->spawn_particle = new ParticleParameters(p);

	m_client_event_queue.push(event);
}

void Client::handleCommand_SpawnParticleBatch(NetworkPacket *pkt)
{
	std::stringstream particle_batch_data(std::ios::binary | std::ios::in | std::ios::out);
	{
		std::istringstream compressed(pkt->readLongString(), std::ios::binary);
		decompressZstd(compressed, particle_batch_data);
	}

	while (particle_batch_data.peek() != EOF) {
		auto p = std::make_unique<ParticleParameters>();
		{
			std::istringstream particle_data(deSerializeString32(particle_batch_data), std::ios::binary);
			p->deSerialize(particle_data, m_proto_ver);
		}

		ClientEvent *event = new ClientEvent();
		event->type = CE_SPAWN_PARTICLE;
		event->spawn_particle = p.release();

		m_client_event_queue.push(event);
	}
}

void Client::handleCommand_AddParticleSpawner(NetworkPacket* pkt)
{
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);

	ParticleSpawnerParameters p;
	u32 server_id;
	u16 attached_id = 0;

	p.amount             = readU16(is);
	p.time               = readF32(is);
	if (p.time < 0)
		throw PacketError("particle spawner time < 0");

	bool missing_end_values = false;
	if (m_proto_ver >= 42) {
		// All tweenable parameters
		p.pos.deSerialize(is);
		p.vel.deSerialize(is);
		p.acc.deSerialize(is);
		p.exptime.deSerialize(is);
		p.size.deSerialize(is);
	} else {
		p.pos.start.legacyDeSerialize(is);
		p.vel.start.legacyDeSerialize(is);
		p.acc.start.legacyDeSerialize(is);
		p.exptime.start.legacyDeSerialize(is);
		p.size.start.legacyDeSerialize(is);
		missing_end_values = true;
	}

	p.collisiondetection = readU8(is);
	p.texture.string     = deSerializeString32(is);

	server_id = readU32(is);

	p.vertical = readU8(is);
	p.collision_removal = readU8(is);

	attached_id = readU16(is);

	p.animation.deSerialize(is, m_proto_ver);
	p.glow = readU8(is);
	p.object_collision = readU8(is);

	// This is kinda awful
	do {
		u16 tmp_param0 = readU16(is);
		if (is.eof())
			break;
		p.node.param0 = tmp_param0;
		p.node.param2 = readU8(is);
		p.node_tile   = readU8(is);

		if (m_proto_ver < 42) {
			// v >= 5.6.0
			f32 tmp_sbias = readF32(is);
			if (is.eof())
				break;

			// initial bias must be stored separately in the stream to preserve
			// backwards compatibility with older clients, which do not support
			// a bias field in their range "format"
			p.pos.start.bias = tmp_sbias;
			p.vel.start.bias = readF32(is);
			p.acc.start.bias = readF32(is);
			p.exptime.start.bias = readF32(is);
			p.size.start.bias = readF32(is);

			p.pos.end.deSerialize(is);
			p.vel.end.deSerialize(is);
			p.acc.end.deSerialize(is);
			p.exptime.end.deSerialize(is);
			p.size.end.deSerialize(is);

			missing_end_values = false;
		}
		// else: fields are already read by deSerialize() very early

		// properties for legacy texture field
		p.texture.deSerialize(is, m_proto_ver, true);

		p.drag.deSerialize(is);
		p.jitter.deSerialize(is);
		p.bounce.deSerialize(is);
		ParticleParamTypes::deSerializeParameterValue(is, p.attractor_kind);
		using ParticleParamTypes::AttractorKind;
		if (p.attractor_kind != AttractorKind::none) {
			p.attract.deSerialize(is);
			p.attractor_origin.deSerialize(is);
			p.attractor_attachment = readU16(is);
			/* we only check the first bit, in order to allow this value
			 * to be turned into a bit flag field later if needed */
			p.attractor_kill = !!(readU8(is) & 1);
			if (p.attractor_kind != AttractorKind::point) {
				p.attractor_direction.deSerialize(is);
				p.attractor_direction_attachment = readU16(is);
			}
		}
		p.radius.deSerialize(is);

		u16 texpoolsz = readU16(is);
		p.texpool.reserve(texpoolsz);
		for (u16 i = 0; i < texpoolsz; ++i) {
			ServerParticleTexture newtex;
			newtex.deSerialize(is, m_proto_ver);
			p.texpool.push_back(newtex);
		}
	} while(0);

	if (missing_end_values) {
		// there's no tweening data to be had, so we need to set the
		// legacy params to constant values, otherwise everything old
		// will tween to zero
		p.pos.end = p.pos.start;
		p.vel.end = p.vel.start;
		p.acc.end = p.acc.start;
		p.exptime.end = p.exptime.start;
		p.size.end = p.size.start;
	}

	auto event = new ClientEvent();
	event->type                            = CE_ADD_PARTICLESPAWNER;
	event->add_particlespawner.p           = new ParticleSpawnerParameters(p);
	event->add_particlespawner.attached_id = attached_id;
	event->add_particlespawner.id          = server_id;

	m_client_event_queue.push(event);
}


void Client::handleCommand_DeleteParticleSpawner(NetworkPacket* pkt)
{
	u32 server_id;
	*pkt >> server_id;

	ClientEvent *event = new ClientEvent();
	event->type = CE_DELETE_PARTICLESPAWNER;
	event->delete_particlespawner.id = server_id;

	m_client_event_queue.push(event);
}

void Client::handleCommand_HudAdd(NetworkPacket* pkt)
{
	u32 server_id;
	u8 type;
	v2f pos;
	std::string name;
	v2f scale;
	std::string text;
	u32 number;
	u32 item;
	u32 dir;
	v2f align;
	v2f offset;
	v3f world_pos;
	v2s32 size;
	s16 z_index = 0;
	std::string text2;
	u32 style = 0;

	*pkt >> server_id >> type >> pos >> name >> scale >> text >> number >> item
		>> dir >> align >> offset;
	try {
		*pkt >> world_pos;
		*pkt >> size;
		*pkt >> z_index;
		*pkt >> text2;
		*pkt >> style;
	} catch(PacketError &e) {};

	ClientEvent *event = new ClientEvent();
	event->type              = CE_HUDADD;
	event->hudadd            = new ClientEventHudAdd();
	event->hudadd->server_id = server_id;
	event->hudadd->type      = type;
	event->hudadd->pos       = pos;
	event->hudadd->name      = name;
	event->hudadd->scale     = scale;
	event->hudadd->text      = text;
	event->hudadd->number    = number;
	event->hudadd->item      = item;
	event->hudadd->dir       = dir;
	event->hudadd->align     = align;
	event->hudadd->offset    = offset;
	event->hudadd->world_pos = world_pos;
	event->hudadd->size      = size;
	event->hudadd->z_index   = z_index;
	event->hudadd->text2     = text2;
	event->hudadd->style     = style;
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudRemove(NetworkPacket* pkt)
{
	u32 server_id;

	*pkt >> server_id;

	ClientEvent *event = new ClientEvent();
	event->type     = CE_HUDRM;
	event->hudrm.id = server_id;
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudChange(NetworkPacket* pkt)
{
	std::string sdata;
	v2f v2fdata;
	v3f v3fdata;
	u32 intdata = 0;
	v2s32 v2s32data;
	u32 server_id;
	u8 stat;

	*pkt >> server_id >> stat;

	// Do nothing if stat is not known
	if (stat >= HudElementStat_END) {
		return;
	}

	// Keep in sync with:server.cpp -> SendHUDChange
	switch (static_cast<HudElementStat>(stat)) {
		case HUD_STAT_POS:
		case HUD_STAT_SCALE:
		case HUD_STAT_ALIGN:
		case HUD_STAT_OFFSET:
			*pkt >> v2fdata;
			break;
		case HUD_STAT_NAME:
		case HUD_STAT_TEXT:
		case HUD_STAT_TEXT2:
			*pkt >> sdata;
			break;
		case HUD_STAT_WORLD_POS:
			*pkt >> v3fdata;
			break;
		case HUD_STAT_SIZE:
			*pkt >> v2s32data;
			break;
		default:
			*pkt >> intdata;
			break;
	}

	ClientEvent *event = new ClientEvent();
	event->type                 = CE_HUDCHANGE;
	event->hudchange            = new ClientEventHudChange();
	event->hudchange->id        = server_id;
	event->hudchange->stat      = static_cast<HudElementStat>(stat);
	event->hudchange->v2fdata   = v2fdata;
	event->hudchange->v3fdata   = v3fdata;
	event->hudchange->sdata     = sdata;
	event->hudchange->data      = intdata;
	event->hudchange->v2s32data = v2s32data;
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetFlags(NetworkPacket* pkt)
{
	u32 flags, mask;

	*pkt >> flags >> mask;

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	bool was_minimap_radar_visible = player->hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE;

	player->hud_flags &= ~mask;
	player->hud_flags |= flags;

	bool m_minimap_radar_disabled_by_server = !(player->hud_flags & HUD_FLAG_MINIMAP_RADAR_VISIBLE);

	// Not so satisying code to keep compatibility with old fixed mode system
	// -->
	// If radar has been disabled, try to find a non radar mode or fall back to 0
	if (m_minimap && m_minimap_radar_disabled_by_server
			&& was_minimap_radar_visible) {
		while (m_minimap->getModeIndex() > 0 &&
				m_minimap->getModeDef().type == MINIMAP_TYPE_RADAR)
			m_minimap->nextMode();
	}
	// <--
	// End of 'not so satifying code'
}

void Client::handleCommand_HudSetParam(NetworkPacket* pkt)
{
	u16 param; std::string value;

	*pkt >> param >> value;

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	if (param == HUD_PARAM_HOTBAR_ITEMCOUNT && value.size() == 4) {
		s32 hotbar_itemcount = readS32((u8*) value.c_str());
		if (hotbar_itemcount > 0 && hotbar_itemcount <= HUD_HOTBAR_ITEMCOUNT_MAX)
			player->hud_hotbar_itemcount = hotbar_itemcount;
	}
	else if (param == HUD_PARAM_HOTBAR_IMAGE) {
		player->hotbar_image = value;
	}
	else if (param == HUD_PARAM_HOTBAR_SELECTED_IMAGE) {
		player->hotbar_selected_image = value;
	}
}

void Client::handleCommand_HudSetSky(NetworkPacket* pkt)
{
	if (m_proto_ver < 39) {
		// Handle Protocol 38 and below servers with old set_sky,
		// ensuring the classic look is kept.
		std::string datastring(pkt->getString(0), pkt->getSize());
		std::istringstream is(datastring, std::ios_base::binary);

		SkyboxParams skybox;
		skybox.bgcolor = video::SColor(readARGB8(is));
		skybox.type = std::string(deSerializeString16(is));
		u16 count = readU16(is);

		for (size_t i = 0; i < count; i++)
			skybox.textures.emplace_back(deSerializeString16(is));

		skybox.clouds = readU8(is) != 0;

		// Use default skybox settings:
		SunParams sun = SkyboxDefaults::getSunDefaults();
		MoonParams moon = SkyboxDefaults::getMoonDefaults();
		StarParams stars = SkyboxDefaults::getStarDefaults();

		// Fix for "regular" skies, as color isn't kept:
		if (skybox.type == "regular") {
			skybox.sky_color = SkyboxDefaults::getSkyColorDefaults();
			skybox.fog_tint_type = "default";
			skybox.fog_moon_tint = video::SColor(255, 255, 255, 255);
			skybox.fog_sun_tint = video::SColor(255, 255, 255, 255);
		} else {
			sun.visible = false;
			sun.sunrise_visible = false;
			moon.visible = false;
			stars.visible = false;
		}

		// Skybox, sun, moon and stars ClientEvents:
		ClientEvent *sky_event = new ClientEvent();
		sky_event->type = CE_SET_SKY;
		sky_event->set_sky = new SkyboxParams(skybox);
		m_client_event_queue.push(sky_event);

		ClientEvent *sun_event = new ClientEvent();
		sun_event->type = CE_SET_SUN;
		sun_event->sun_params = new SunParams(sun);
		m_client_event_queue.push(sun_event);

		ClientEvent *moon_event = new ClientEvent();
		moon_event->type = CE_SET_MOON;
		moon_event->moon_params = new MoonParams(moon);
		m_client_event_queue.push(moon_event);

		ClientEvent *star_event = new ClientEvent();
		star_event->type = CE_SET_STARS;
		star_event->star_params = new StarParams(stars);
		m_client_event_queue.push(star_event);
		return;
	}

	SkyboxParams skybox;

	*pkt >> skybox.bgcolor >> skybox.type >> skybox.clouds >>
		skybox.fog_sun_tint >> skybox.fog_moon_tint >> skybox.fog_tint_type;

	if (skybox.type == "skybox") {
		u16 texture_count;
		std::string texture;
		*pkt >> texture_count;
		for (u16 i = 0; i < texture_count; i++) {
			*pkt >> texture;
			skybox.textures.emplace_back(texture);
		}
	} else if (skybox.type == "regular") {
		auto &c = skybox.sky_color;
		*pkt >> c.day_sky >> c.day_horizon >> c.dawn_sky >> c.dawn_horizon
			>> c.night_sky >> c.night_horizon >> c.indoors;
	}

	if (pkt->getRemainingBytes() >= 4) {
		*pkt >> skybox.body_orbit_tilt;
	}

	if (pkt->getRemainingBytes() >= 6) {
		*pkt >> skybox.fog_distance >> skybox.fog_start;
	}

	if (pkt->getRemainingBytes() >= 4) {
		*pkt >> skybox.fog_color;
	}

	ClientEvent *event = new ClientEvent();
	event->type = CE_SET_SKY;
	event->set_sky = new SkyboxParams(skybox);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetSun(NetworkPacket *pkt)
{
	SunParams sun;

	*pkt >> sun.visible >> sun.texture>> sun.tonemap
		>> sun.sunrise >> sun.sunrise_visible >> sun.scale;

	ClientEvent *event = new ClientEvent();
	event->type        = CE_SET_SUN;
	event->sun_params  = new SunParams(sun);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetMoon(NetworkPacket *pkt)
{
	MoonParams moon;

	*pkt >> moon.visible >> moon.texture
		>> moon.tonemap >> moon.scale;

	ClientEvent *event = new ClientEvent();
	event->type        = CE_SET_MOON;
	event->moon_params = new MoonParams(moon);
	m_client_event_queue.push(event);
}

void Client::handleCommand_HudSetStars(NetworkPacket *pkt)
{
	StarParams stars = SkyboxDefaults::getStarDefaults();

	*pkt >> stars.visible >> stars.count
		>> stars.starcolor >> stars.scale;
	try {
		*pkt >> stars.day_opacity;
	} catch (PacketError &e) {};

	ClientEvent *event = new ClientEvent();
	event->type        = CE_SET_STARS;
	event->star_params = new StarParams(stars);

	m_client_event_queue.push(event);
}

void Client::handleCommand_CloudParams(NetworkPacket* pkt)
{
	f32 density;
	video::SColor color_bright;
	video::SColor color_ambient;
	video::SColor color_shadow = video::SColor(255, 204, 204, 204);
	f32 height;
	f32 thickness;
	v2f speed;

	*pkt >> density >> color_bright >> color_ambient
			>> height >> thickness >> speed;

	if (pkt->getRemainingBytes() >= 4) {
		*pkt >> color_shadow;
	}

	ClientEvent *event = new ClientEvent();
	event->type                       = CE_CLOUD_PARAMS;
	event->cloud_params.density       = density;
	// use the underlying u32 representation, because we can't
	// use struct members with constructors here, and this way
	// we avoid using new() and delete() for no good reason
	event->cloud_params.color_bright  = color_bright.color;
	event->cloud_params.color_ambient = color_ambient.color;
	event->cloud_params.color_shadow = color_shadow.color;
	event->cloud_params.height        = height;
	event->cloud_params.thickness     = thickness;
	// same here: deconstruct to skip constructor
	event->cloud_params.speed_x       = speed.X;
	event->cloud_params.speed_y       = speed.Y;
	m_client_event_queue.push(event);
}

void Client::handleCommand_OverrideDayNightRatio(NetworkPacket* pkt)
{
	bool do_override;
	u16 day_night_ratio_u;

	*pkt >> do_override >> day_night_ratio_u;

	float day_night_ratio_f = (float)day_night_ratio_u / 65536;

	ClientEvent *event = new ClientEvent();
	event->type                                 = CE_OVERRIDE_DAY_NIGHT_RATIO;
	event->override_day_night_ratio.do_override = do_override;
	event->override_day_night_ratio.ratio_f     = day_night_ratio_f;
	m_client_event_queue.push(event);
}

void Client::handleCommand_LocalPlayerAnimations(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	for (int i = 0; i < 4; ++i) {
		if (getProtoVersion() >= 46) {
			*pkt >> player->local_animations[i];
		} else {
			v2s32 local_animation;
			*pkt >> local_animation;
			player->local_animations[i] = v2f::from(local_animation);
		}
	}

	*pkt >> player->local_animation_speed;

	player->last_animation = LocalPlayerAnimation::NO_ANIM;
}

void Client::handleCommand_EyeOffset(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	*pkt >> player->eye_offset_first >> player->eye_offset_third;
	try {
		*pkt >> player->eye_offset_third_front;
	} catch (PacketError &e) {
		player->eye_offset_third_front = player->eye_offset_third;
	}
}

void Client::handleCommand_Camera(NetworkPacket* pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player);

	u8 tmp;
	*pkt >> tmp;
	player->allowed_camera_mode = static_cast<CameraMode>(tmp);

	m_client_event_queue.push(new ClientEvent(CE_UPDATE_CAMERA));
}

void Client::handleCommand_UpdatePlayerList(NetworkPacket* pkt)
{
	u8 type;
	u16 num_players;
	*pkt >> type >> num_players;
	PlayerListModifer notice_type = (PlayerListModifer) type;

	for (u16 i = 0; i < num_players; i++) {
		std::string name;
		*pkt >> name;
		switch (notice_type) {
		case PLAYER_LIST_INIT:
		case PLAYER_LIST_ADD:
			m_env.addPlayerName(name);
			continue;
		case PLAYER_LIST_REMOVE:
			m_env.removePlayerName(name);
			continue;
		}
	}
}

void Client::handleCommand_SrpBytesSandB(NetworkPacket* pkt)
{
	if (m_chosen_auth_mech != AUTH_MECHANISM_SRP &&
			m_chosen_auth_mech != AUTH_MECHANISM_LEGACY_PASSWORD) {
		errorstream << "Client: Received SRP S_B login message,"
			<< " but wasn't supposed to (chosen_mech="
			<< m_chosen_auth_mech << ")." << std::endl;
		return;
	}

	char *bytes_M = 0;
	size_t len_M = 0;
	SRPUser *usr = (SRPUser *) m_auth_data;
	std::string s;
	std::string B;
	*pkt >> s >> B;

	infostream << "Client: Received TOCLIENT_SRP_BYTES_S_B." << std::endl;

	srp_user_process_challenge(usr, (const unsigned char *) s.c_str(), s.size(),
		(const unsigned char *) B.c_str(), B.size(),
		(unsigned char **) &bytes_M, &len_M);

	if ( !bytes_M ) {
		errorstream << "Client: SRP-6a S_B safety check violation!" << std::endl;
		return;
	}

	NetworkPacket resp_pkt(TOSERVER_SRP_BYTES_M, 0);
	resp_pkt << std::string(bytes_M, len_M);
	Send(&resp_pkt);
}

void Client::handleCommand_FormspecPrepend(NetworkPacket *pkt)
{
	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);

	// Store formspec in LocalPlayer
	*pkt >> player->formspec_prepend;
}

void Client::handleCommand_CSMRestrictionFlags(NetworkPacket *pkt)
{
	*pkt >> m_csm_restriction_flags >> m_csm_restriction_noderange;

	// Restrictions were received -> load mods if it's enabled
	// Note: this should be moved after mods receptions from server instead
	loadMods();
}

void Client::handleCommand_PlayerSpeed(NetworkPacket *pkt)
{
	v3f added_vel;

	*pkt >> added_vel;

	LocalPlayer *player = m_env.getLocalPlayer();
	assert(player != NULL);
	player->addVelocity(added_vel);
}

void Client::handleCommand_MediaPush(NetworkPacket *pkt)
{
	std::string raw_hash, filename, filedata;
	u32 token;
	bool cached;

	*pkt >> raw_hash >> filename >> cached;
	if (m_proto_ver >= 40)
		*pkt >> token;
	else
		filedata = pkt->readLongString();

	if (raw_hash.size() != 20 || filename.empty() ||
			(m_proto_ver < 40 && filedata.empty()) ||
			!string_allowed(filename, TEXTURENAME_ALLOWED_CHARS)) {
		throw PacketError("Illegal filename, data or hash");
	}

	verbosestream << "Server pushes media file \"" << filename << "\" ";
	if (filedata.empty())
		verbosestream << "to be fetched ";
	else
		verbosestream << "with " << filedata.size() << " bytes ";
	verbosestream << "(cached=" << cached << ")" << std::endl;

	if (!filedata.empty()) {
		// LEGACY CODEPATH
		// Compute and check checksum of data
		std::string computed_hash = hashing::sha1(filedata);
		if (raw_hash != computed_hash) {
			verbosestream << "Hash of file data mismatches, ignoring." << std::endl;
			return;
		}

		// Actually load media
		loadMedia(filedata, filename, true);

		// Cache file for the next time when this client joins the same server
		if (cached)
			clientMediaUpdateCache(raw_hash, filedata);
		return;
	}

	// create a downloader for this file
	auto downloader(std::make_shared<SingleMediaDownloader>(cached));
	m_pending_media_downloads.emplace_back(token, downloader);
	downloader->addFile(filename, raw_hash);
	for (const auto &baseurl : m_remote_media_servers)
		downloader->addRemoteServer(baseurl);

	downloader->step(this);
}

/*
 * Mod channels
 */

void Client::handleCommand_ModChannelMsg(NetworkPacket *pkt)
{
	std::string channel_name, sender, channel_msg;
	*pkt >> channel_name >> sender >> channel_msg;

	verbosestream << "Mod channel message received from server " << pkt->getPeerId()
		<< " on channel " << channel_name << ". sender: `" << sender << "`, message: "
		<< channel_msg << std::endl;

	if (!m_modchannel_mgr->channelRegistered(channel_name)) {
		verbosestream << "Server sent us messages on unregistered channel "
			<< channel_name << ", ignoring." << std::endl;
		return;
	}

	m_script->on_modchannel_message(channel_name, sender, channel_msg);
}

void Client::handleCommand_ModChannelSignal(NetworkPacket *pkt)
{
	u8 signal_tmp;
	ModChannelSignal signal;
	std::string channel;

	*pkt >> signal_tmp >> channel;

	signal = (ModChannelSignal)signal_tmp;

	bool valid_signal = true;
	// @TODO: send Signal to Lua API
	switch (signal) {
		case MODCHANNEL_SIGNAL_JOIN_OK:
			m_modchannel_mgr->setChannelState(channel, MODCHANNEL_STATE_READ_WRITE);
			infostream << "Server ack our mod channel join on channel `" << channel
				<< "`, joining." << std::endl;
			break;
		case MODCHANNEL_SIGNAL_JOIN_FAILURE:
			// Unable to join, remove channel
			m_modchannel_mgr->leaveChannel(channel, 0);
			infostream << "Server refused our mod channel join on channel `" << channel
				<< "`" << std::endl;
			break;
		case MODCHANNEL_SIGNAL_LEAVE_OK:
#ifndef NDEBUG
			infostream << "Server ack our mod channel leave on channel " << channel
				<< "`, leaving." << std::endl;
#endif
			break;
		case MODCHANNEL_SIGNAL_LEAVE_FAILURE:
			infostream << "Server refused our mod channel leave on channel `" << channel
				<< "`" << std::endl;
			break;
		case MODCHANNEL_SIGNAL_CHANNEL_NOT_REGISTERED:
#ifndef NDEBUG
			// Generally unused, but ensure we don't do an implementation error
			infostream << "Server tells us we sent a message on channel `" << channel
				<< "` but we are not registered. Message was dropped." << std::endl;
#endif
			break;
		case MODCHANNEL_SIGNAL_SET_STATE: {
			u8 state;
			*pkt >> state;

			if (state == MODCHANNEL_STATE_INIT || state >= MODCHANNEL_STATE_MAX) {
				infostream << "Received wrong channel state " << state
						<< ", ignoring." << std::endl;
				return;
			}

			m_modchannel_mgr->setChannelState(channel, (ModChannelState) state);
			infostream << "Server sets mod channel `" << channel
					<< "` in read-only mode." << std::endl;
			break;
		}
		default:
#ifndef NDEBUG
			warningstream << "Received unhandled mod channel signal ID "
				<< signal << ", ignoring." << std::endl;
#endif
			valid_signal = false;
			break;
	}

	// If signal is valid, forward it to client side mods
	if (valid_signal)
		m_script->on_modchannel_signal(channel, signal);
}

void Client::handleCommand_MinimapModes(NetworkPacket *pkt)
{
	u16 count; // modes
	u16 mode;  // wanted current mode index after change

	*pkt >> count >> mode;

	if (m_minimap)
		m_minimap->clearModes();

	for (size_t index = 0; index < count; index++) {
		u16 type;
		std::string label;
		u16 size;
		std::string texture;
		u16 scale;

		*pkt >> type >> label >> size >> texture >> scale;

		if (m_minimap)
			m_minimap->addMode(MinimapType(type), size, label, texture, scale);
	}

	if (m_minimap)
		m_minimap->setModeIndex(mode);
}

void Client::handleCommand_SetLighting(NetworkPacket *pkt)
{
	Lighting& lighting = m_env.getLocalPlayer()->getLighting();

	if (pkt->getRemainingBytes() >= 4)
		*pkt >> lighting.shadow_intensity;
	if (pkt->getRemainingBytes() >= 4)
		*pkt >> lighting.saturation;
	if (pkt->getRemainingBytes() >= 24) {
		*pkt >> lighting.exposure.luminance_min
				>> lighting.exposure.luminance_max
				>> lighting.exposure.exposure_correction
				>> lighting.exposure.speed_dark_bright
				>> lighting.exposure.speed_bright_dark
				>> lighting.exposure.center_weight_power;
	}
	if (pkt->getRemainingBytes() >= 4)
		*pkt >> lighting.volumetric_light_strength;
	if (pkt->getRemainingBytes() >= 4)
		*pkt >> lighting.shadow_tint;
	if (pkt->getRemainingBytes() >= 12) {
		*pkt >> lighting.bloom_intensity
				>> lighting.bloom_strength_factor
				>> lighting.bloom_radius;
	}
}
