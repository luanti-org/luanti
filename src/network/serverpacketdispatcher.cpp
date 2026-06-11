// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti Authors

#include "network/serverpacketdispatcher.h"

#include "constants.h"
#include "exceptions.h"
#include "log.h"
#include "network/connection.h" // con::SendFailedException, con::IConnection
#include "network/networkexceptions.h"
#include "network/networkpacket.h"
#include "network/serveropcodes.h"
#include "porting.h" // porting::getTimeUs
#include "profiler.h" // g_profiler, ScopeProfiler
#include "serialization.h" // SER_FMT_VER_INVALID
#include "server.h" // EnvAutoLock
#include "util/pointedthing.h"
#include "util/srp.h"
#include "util/tracy_wrapper.h" // ZoneScoped, FrameMarker
#include <algorithm>
#include <cmath>
#include <sstream>

namespace server
{

// ---------------------------------------------------------------------------
// Per-opcode decoders
// ---------------------------------------------------------------------------
//
// Each decoder is a free function that takes a NetworkPacket and returns a
// freshly-constructed payload. Truncation is left to NetworkPacket::operator>>
// which throws PacketError; the dispatcher catches it and drops the packet,
// logging the same way the original Server::ProcessData did.
//
// Decoders write into a thread_local buffer through placement-new, so no
// per-packet allocation is needed for the dispatch.

static InitPayload decodeInit(NetworkPacket &pkt)
{
	InitPayload p;
	pkt >> p.max_ser_ver;
	pkt >> p.unused;
	pkt >> p.min_net_proto_version;
	pkt >> p.max_net_proto_version;
	pkt >> p.player_name;
	return p;
}

static Init2Payload decodeInit2(NetworkPacket &pkt)
{
	Init2Payload p;
	if (pkt.getRemainingBytes() > 0)
		pkt >> p.lang;
	return p;
}

static RequestMediaPayload decodeRequestMedia(NetworkPacket &pkt)
{
	RequestMediaPayload p;
	u16 numfiles;
	pkt >> numfiles;
	for (u16 i = 0; i < numfiles; i++) {
		std::string name;
		pkt >> name;
		p.files.insert(std::move(name));
	}
	return p;
}

static ClientReadyPayload decodeClientReady(NetworkPacket &pkt)
{
	ClientReadyPayload p;
	pkt >> p.major;
	pkt >> p.minor;
	pkt >> p.patch;
	pkt >> p.reserved;
	pkt >> p.full_ver;
	if (pkt.getRemainingBytes() >= 2)
		pkt >> p.formspec_ver;
	return p;
}

static GotBlocksPayload decodeGotBlocks(NetworkPacket &pkt)
{
	GotBlocksPayload p;
	u8 count;
	pkt >> count;
	p.positions.reserve(count);
	for (u16 i = 0; i < count; i++) {
		v3s16 pos;
		pkt >> pos;
		p.positions.push_back(pos);
	}
	return p;
}

static DeletedBlocksPayload decodeDeletedBlocks(NetworkPacket &pkt)
{
	DeletedBlocksPayload p;
	u8 count;
	pkt >> count;
	p.positions.reserve(count);
	for (u16 i = 0; i < count; i++) {
		v3s16 pos;
		pkt >> pos;
		p.positions.push_back(pos);
	}
	return p;
}

static PlayerPosPayload decodePlayerPos(NetworkPacket &pkt)
{
	PlayerPosPayload p;
	pkt >> p.pos;
	pkt >> p.speed;
	s32 f32pitch, f32yaw;
	pkt >> f32pitch;
	pkt >> f32yaw;
	p.pitch = (f32)f32pitch / 100.0f;
	p.yaw = (f32)f32yaw / 100.0f;
	pkt >> p.keyPressed;
	u8 f32fov;
	pkt >> f32fov;
	p.fov = (f32)f32fov / 80.0f;
	pkt >> p.wanted_range;

	p.bits = 0;
	p.movement_speed = 0.0f;
	p.movement_direction = 0;

	if (pkt.hasRemainingBytes()) {
		pkt >> p.bits;
		if (pkt.hasRemainingBytes()) {
			f32 ms;
			pkt >> ms;
			if (ms != ms) // NaN
				ms = 0.0f;
			p.movement_speed = std::clamp(ms, 0.0f, 1.0f);
			pkt >> p.movement_direction;
		}
	}
	return p;
}

static InventoryActionPayload decodeInventoryAction(NetworkPacket &pkt)
{
	InventoryActionPayload p;
	auto remaining = pkt.getRemainingNoCopy();
	p.serialized.assign(remaining.data(), remaining.size());
	return p;
}

static ChatMessagePayload decodeChatMessage(NetworkPacket &pkt)
{
	ChatMessagePayload p;
	pkt >> p.message;
	return p;
}

static DamagePayload decodeDamage(NetworkPacket &pkt)
{
	DamagePayload p;
	pkt >> p.damage;
	return p;
}

static PlayerItemPayload decodePlayerItem(NetworkPacket &pkt)
{
	PlayerItemPayload p;
	pkt >> p.item;
	return p;
}

static InteractPayload decodeInteract(NetworkPacket &pkt)
{
	InteractPayload p;
	u8 action;
	pkt >> action;
	p.action = action;
	pkt >> p.item;
	std::istringstream is(pkt.readLongString(), std::ios::binary);
	p.pointed.deSerialize(is);
	p.player_pos = decodePlayerPos(pkt);
	return p;
}

static RemovedSoundsPayload decodeRemovedSounds(NetworkPacket &pkt)
{
	RemovedSoundsPayload p;
	u16 num;
	pkt >> num;
	p.ids.reserve(num);
	for (u16 k = 0; k < num; k++) {
		s32 id;
		pkt >> id;
		p.ids.push_back(id);
	}
	return p;
}

// Reused by NodeMetaFields and InventoryFields. Throws PacketError on
// truncation; the 640K cap throws as well so the dispatcher logs the same
// "ignoring malformed" line for any wire-level problem.
static StringMap decodeFormspecFields(NetworkPacket &pkt)
{
	u16 field_count;
	pkt >> field_count;
	StringMap fields;
	size_t length = 0;
	for (u16 k = 0; k < field_count; k++) {
		std::string fieldname, fieldvalue;
		pkt >> fieldname;
		fieldvalue = pkt.readLongString();
		fieldname = sanitize_untrusted(fieldname, false);
		fieldvalue = sanitize_untrusted(fieldvalue);
		length += fieldname.size() + fieldvalue.size();
		fields[std::move(fieldname)] = std::move(fieldvalue);
	}
	if (length >= 640u * 1024u)
		throw PacketError("Formspec fields exceed 640K cap");
	return fields;
}

static NodeMetaFieldsPayload decodeNodeMetaFields(NetworkPacket &pkt)
{
	NodeMetaFieldsPayload p;
	pkt >> p.pos;
	pkt >> p.formname;
	p.fields = decodeFormspecFields(pkt);
	return p;
}

static InventoryFieldsPayload decodeInventoryFields(NetworkPacket &pkt)
{
	InventoryFieldsPayload p;
	pkt >> p.formspec_name;
	p.fields = decodeFormspecFields(pkt);
	return p;
}

static FirstSrpPayload decodeFirstSrp(NetworkPacket &pkt)
{
	FirstSrpPayload p;
	pkt >> p.salt;
	pkt >> p.verification_key;
	pkt >> p.is_empty;
	return p;
}

static SrpBytesAPayload decodeSrpBytesA(NetworkPacket &pkt)
{
	SrpBytesAPayload p;
	pkt >> p.bytes_A;
	pkt >> p.based_on;
	return p;
}

static SrpBytesMPayload decodeSrpBytesM(NetworkPacket &pkt)
{
	SrpBytesMPayload p;
	pkt >> p.bytes_M;
	return p;
}

static ModChannelJoinPayload decodeModChannelJoin(NetworkPacket &pkt)
{
	ModChannelJoinPayload p;
	pkt >> p.channel_name;
	return p;
}

static ModChannelLeavePayload decodeModChannelLeave(NetworkPacket &pkt)
{
	ModChannelLeavePayload p;
	pkt >> p.channel_name;
	return p;
}

static ModChannelMsgPayload decodeModChannelMsg(NetworkPacket &pkt)
{
	ModChannelMsgPayload p;
	pkt >> p.channel_name;
	pkt >> p.message;
	return p;
}

static HaveMediaPayload decodeHaveMedia(NetworkPacket &pkt)
{
	HaveMediaPayload p;
	u8 numtokens;
	pkt >> numtokens;
	p.tokens.reserve(numtokens);
	for (u16 i = 0; i < numtokens; i++) {
		u32 n;
		pkt >> n;
		p.tokens.push_back(n);
	}
	return p;
}

static UpdateClientInfoPayload decodeUpdateClientInfo(NetworkPacket &pkt)
{
	UpdateClientInfoPayload p;
	pkt >> p.render_target_size.X;
	pkt >> p.render_target_size.Y;
	pkt >> p.real_gui_scaling;
	pkt >> p.real_hud_scaling;
	pkt >> p.max_fs_size.X;
	pkt >> p.max_fs_size.Y;
	p.touch_controls = false;
	if (pkt.hasRemainingBytes())
		pkt >> p.touch_controls;
	return p;
}

// ---------------------------------------------------------------------------
// ServerPacketDispatcher
// ---------------------------------------------------------------------------

static void enrich_exception(BaseException &e, const NetworkPacket &pkt, bool include_pos)
{
	const u16 cmd = pkt.getCommand();
	std::ostringstream oss;
	if (cmd < TOSERVER_NUM_MSG_TYPES)
		oss << " name=" << toServerCommandTable[cmd].name;

	if (include_pos) {
		// (not necessary for PacketError: already in e.what())

		oss << " cmd=" << cmd
			<< " offset=" << pkt.getOffset()
			<< " size=" << pkt.getSize();
	}

	e.append(" @").append(oss.str());
}

ServerPacketDispatcher::ServerPacketDispatcher(MetricsBackend *metrics)
	: m_table{},
		m_packet_recv_counter(metrics ? metrics->addCounter(
			"minetest_core_server_packet_recv",
			"Processable packets received") : nullptr),
		m_packet_recv_processed_counter(metrics ? metrics->addCounter(
			"minetest_core_server_packet_recv_processed",
			"Valid received packets processed") : nullptr)
{
	// Helper to populate one entry. The macro saves us from writing the
	// dispatch-entry boilerplate 24 times and the entire table stays
	// static / zero-allocating.
	#define REGISTER(opcode, decoder, T, handler)                              \
		m_table[opcode] = {                                                    \
			/* decode  */ &typedDecodeEntry<T, &decoder>,                      \
			/* handle  */ &typedHandlerEntry<T, &Server::handler>,             \
			/* destroy */ &typedDestroyEntry<T>                                \
		}

	REGISTER(TOSERVER_INIT,             decodeInit,             InitPayload,             handleCommand_Init);
	REGISTER(TOSERVER_INIT2,            decodeInit2,            Init2Payload,            handleCommand_Init2);
	REGISTER(TOSERVER_MODCHANNEL_JOIN,  decodeModChannelJoin,   ModChannelJoinPayload,   handleCommand_ModChannelJoin);
	REGISTER(TOSERVER_MODCHANNEL_LEAVE, decodeModChannelLeave,  ModChannelLeavePayload,  handleCommand_ModChannelLeave);
	REGISTER(TOSERVER_MODCHANNEL_MSG,   decodeModChannelMsg,    ModChannelMsgPayload,    handleCommand_ModChannelMsg);
	REGISTER(TOSERVER_PLAYERPOS,        decodePlayerPos,        PlayerPosPayload,        handleCommand_PlayerPos);
	REGISTER(TOSERVER_GOTBLOCKS,        decodeGotBlocks,        GotBlocksPayload,        handleCommand_GotBlocks);
	REGISTER(TOSERVER_DELETEDBLOCKS,    decodeDeletedBlocks,    DeletedBlocksPayload,    handleCommand_DeletedBlocks);
	REGISTER(TOSERVER_INVENTORY_ACTION, decodeInventoryAction,  InventoryActionPayload,  handleCommand_InventoryAction);
	REGISTER(TOSERVER_CHAT_MESSAGE,     decodeChatMessage,      ChatMessagePayload,      handleCommand_ChatMessage);
	REGISTER(TOSERVER_DAMAGE,           decodeDamage,           DamagePayload,           handleCommand_Damage);
	REGISTER(TOSERVER_PLAYERITEM,       decodePlayerItem,       PlayerItemPayload,       handleCommand_PlayerItem);
	REGISTER(TOSERVER_INTERACT,         decodeInteract,         InteractPayload,         handleCommand_Interact);
	REGISTER(TOSERVER_REMOVED_SOUNDS,   decodeRemovedSounds,    RemovedSoundsPayload,    handleCommand_RemovedSounds);
	REGISTER(TOSERVER_NODEMETA_FIELDS,  decodeNodeMetaFields,   NodeMetaFieldsPayload,   handleCommand_NodeMetaFields);
	REGISTER(TOSERVER_INVENTORY_FIELDS, decodeInventoryFields,  InventoryFieldsPayload,  handleCommand_InventoryFields);
	REGISTER(TOSERVER_REQUEST_MEDIA,    decodeRequestMedia,     RequestMediaPayload,     handleCommand_RequestMedia);
	REGISTER(TOSERVER_HAVE_MEDIA,       decodeHaveMedia,        HaveMediaPayload,        handleCommand_HaveMedia);
	REGISTER(TOSERVER_CLIENT_READY,     decodeClientReady,      ClientReadyPayload,      handleCommand_ClientReady);
	REGISTER(TOSERVER_FIRST_SRP,        decodeFirstSrp,         FirstSrpPayload,         handleCommand_FirstSrp);
	REGISTER(TOSERVER_SRP_BYTES_A,      decodeSrpBytesA,        SrpBytesAPayload,        handleCommand_SrpBytesA);
	REGISTER(TOSERVER_SRP_BYTES_M,      decodeSrpBytesM,        SrpBytesMPayload,        handleCommand_SrpBytesM);
	REGISTER(TOSERVER_UPDATE_CLIENT_INFO, decodeUpdateClientInfo, UpdateClientInfoPayload, handleCommand_UpdateClientInfo);

	#undef REGISTER
}

void ServerPacketDispatcher::processIncomingPackets(con::IConnection *con, Server *owner, float min_time)
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

			if (!con->ReceiveTimeoutMs(&pkt, cur_timeout_ms)) {
				// No incoming data.
				if (remaining_time_us() > 0.0f)
					continue;
				else
					break;
			}

			peer_id = pkt.getPeerId();
			if (m_packet_recv_counter)
				m_packet_recv_counter->increment();
			{
				Server::EnvAutoLock envlock(owner);
				ScopeProfiler sp(g_profiler, "Server: Process network packet (sum)");
				dispatchSinglePacket(&pkt, owner);
			}
			if (m_packet_recv_processed_counter)
				m_packet_recv_processed_counter->increment();
		} catch (const con::InvalidIncomingDataException &e) {
			infostream << "Server::Receive(): InvalidIncomingDataException: what()="
					<< e.what() << std::endl;
		} catch (SerializationError &e) {
			enrich_exception(e, pkt, true);
			infostream << "Server::Receive(): SerializationError: what()="
					<< e.what() << std::endl;
		} catch (PacketError &e) {
			enrich_exception(e, pkt, false);
			actionstream << "Server::Receive(): PacketError: what()="
					<< e.what() << std::endl;
		} catch (const ClientStateError &e) {
			errorstream << "ClientStateError: peer=" << peer_id << " what()="
					 << e.what() << std::endl;
			owner->DenyAccess(peer_id, SERVER_ACCESSDENIED_UNEXPECTED_DATA);
		} catch (con::PeerNotFoundException &e) {
			infostream << "Server: PeerNotFoundException" << std::endl;
		} catch (ClientNotFoundException &e) {
			infostream << "Server: ClientNotFoundException" << std::endl;
		}
	}
}

void ServerPacketDispatcher::dispatchSinglePacket(NetworkPacket *pkt, Server *owner)
{
	const u16 cmd = pkt->getCommand();
	if (cmd >= TOSERVER_NUM_MSG_TYPES) {
		infostream << "Server: Ignoring unknown command " << cmd << std::endl;
		return;
	}
	const auto &h = toServerCommandTable[cmd];
	if (h.name == nullptr) {
		infostream << "Server: ignoring unsupported command " << cmd
			<< " from peer " << pkt->getPeerId() << std::endl;
		return;
	}

	const session_t peer_id = pkt->getPeerId();

	// Apply the connection-state filter, lifted from the old ProcessData().
	switch (h.state) {
	case TOSERVER_STATE_NOT_CONNECTED:
		break;
	case TOSERVER_STATE_STARTUP: {
		try {
			(void)owner->getClient(peer_id, CS_Created);
		} catch (...) {
			warningstream << "Server: Got packet command " << (int)cmd
				<< " for peer id " << peer_id
				<< " but client hasn't reached CS_Created. Dropping packet." << std::endl;
			return;
		}
		break;
	}
	case TOSERVER_STATE_INGAME: {
		try {
			u8 peer_ser_ver = owner->getClient(peer_id, CS_InitDone)->serialization_version;
			if (peer_ser_ver == SER_FMT_VER_INVALID) {
				errorstream << "Server: Peer serialization format invalid. "
					"Skipping incoming command " << (int)cmd << std::endl;
				return;
			}
		} catch (...) {
			warningstream << "Server: Got packet command " << (int)cmd
				<< " for peer id " << peer_id
				<< " but client isn't active yet. Dropping packet." << std::endl;
			return;
		}
		break;
	}
	case TOSERVER_STATE_ALL:
		break;
	}

	const DispatchEntry &e = m_table[cmd];
	if (!e.decode || !e.handle) {
		infostream << "Server: ignoring unsupported " << h.name
			<< " from peer " << peer_id << std::endl;
		return;
	}

	// Single in-place buffer for the decoded payload. ProcessData() runs on
	// a single thread (Server's Receive thread), so a thread_local slot is
	// enough — no per-packet allocation needed.
	thread_local std::aligned_storage<MAX_PAYLOAD_SIZE, alignof(max_align_t)>::type
		payload_buf;

	const void *decoded = nullptr;
	try {
		decoded = e.decode(*pkt, &payload_buf);
	} catch (PacketError &err) {
		infostream << "Server: ignoring malformed " << h.name
			<< " from peer " << peer_id << ": " << err.what() << std::endl;
		return;
	} catch (std::exception &err) {
		infostream << "Server: ignoring malformed " << h.name
			<< " from peer " << peer_id << ": " << err.what() << std::endl;
		return;
	}
	if (!decoded) {
		infostream << "Server: ignoring malformed " << h.name
			<< " from peer " << peer_id << std::endl;
		return;
	}

	try {
		e.handle(owner, peer_id, decoded);
	} catch (SendFailedException &err) {
		errorstream << "Server::ProcessData(): SendFailedException: "
			<< err.what() << std::endl;
	}
	// Destruct the payload in-place to free any heap memory the decode
	// allocated (vectors, strings, StringMap). The next packet will
	// placement-new a new T over the same buffer.
	e.destroy(const_cast<void *>(decoded));
}

const char *ServerPacketDispatcher::commandName(u16 cmd) const
{
	if (cmd >= TOSERVER_NUM_MSG_TYPES)
		return nullptr;
	return toServerCommandTable[cmd].name;
}

void *ServerPacketDispatcher::decode(u16 cmd, NetworkPacket &pkt, void *out) const
{
	if (cmd >= TOSERVER_NUM_MSG_TYPES)
		return nullptr;
	const auto &e = m_table[cmd];
	if (!e.decode)
		return nullptr;
	return e.decode(pkt, out);
}

void ServerPacketDispatcher::destroyPayload(u16 cmd, void *ptr) const
{
	if (cmd >= TOSERVER_NUM_MSG_TYPES || !ptr)
		return;
	const auto &e = m_table[cmd];
	if (e.destroy)
		e.destroy(ptr);
}

std::unique_ptr<IPacketDispatcher> newPacketDispatcher(MetricsBackend *metrics)
{
	return std::make_unique<ServerPacketDispatcher>(metrics);
}

} // namespace server
