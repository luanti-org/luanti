// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti Authors

#pragma once

#include "irr_v3d.h"
#include "network/networkprotocol.h"
#include "network/networkpacket.h"
#include "util/pointedthing.h"
#include "util/string.h"
#include <array>
#include <cstddef>
#include <memory>
#include <unordered_set>
#include <vector>

class Server;

namespace server
{

// Per-packet context injected alongside the decoded payload.
struct PacketContext {
	session_t peer_id = 0;
	NetworkPacket *raw = nullptr;
};

struct InitPayload {
	u8  max_ser_ver = 0;
	u16 unused = 0;
	u16 min_net_proto_version = 0;
	u16 max_net_proto_version = 0;
	std::string player_name;
};
struct Init2Payload        { std::string lang; };
struct RequestMediaPayload { std::unordered_set<std::string> files; };
struct ClientReadyPayload  {
	u8 major = 0, minor = 0, patch = 0, reserved = 0;
	std::string full_ver;
	u16 formspec_ver = 1;
};
struct GotBlocksPayload    { std::vector<v3s16> positions; };
struct DeletedBlocksPayload{ std::vector<v3s16> positions; };
struct PlayerPosPayload    {
	v3s32 pos, speed;
	f32 pitch = 0.0f, yaw = 0.0f, fov = 0.0f, movement_speed = 0.0f;
	s32 movement_direction = 0;
	u32 keyPressed = 0;
	u8  wanted_range = 0, bits = 0;
};
struct InventoryActionPayload {
	std::string serialized; // raw bytes of the InventoryAction blob
};
struct ChatMessagePayload  { std::wstring message; };
struct DamagePayload       { u16 damage = 0; };
struct PlayerItemPayload   { u16 item = 0; };
struct InteractPayload     {
	u8 action = 0;
	u16 item = 0;
	PointedThing pointed;
	PlayerPosPayload player_pos;
};
struct RemovedSoundsPayload{ std::vector<s32> ids; };
struct NodeMetaFieldsPayload {
	v3s16 pos;
	std::string formname;
	StringMap fields;
};
struct InventoryFieldsPayload {
	std::string formspec_name;
	StringMap fields;
};
struct FirstSrpPayload {
	std::string salt, verification_key;
	u8 is_empty = 0;
};
struct SrpBytesAPayload    { std::string bytes_A; u8 based_on = 0; };
struct SrpBytesMPayload    { std::string bytes_M; };
struct ModChannelJoinPayload  { std::string channel_name; };
struct ModChannelLeavePayload { std::string channel_name; };
struct ModChannelMsgPayload   { std::string channel_name, message; };
struct HaveMediaPayload       { std::vector<u32> tokens; };
struct UpdateClientInfoPayload {
	v2u32 render_target_size{0, 0};
	f32 real_gui_scaling = 0.0f, real_hud_scaling = 0.0f;
	v2u32 max_fs_size{0, 0};
	bool touch_controls = false;
};

// Largest decoded payload; sized to fit the biggest struct above (Interact
// carries a PointedThing + PlayerPosPayload). Used as the in-place slot for
// the decoder output. ProcessData runs on a single thread, so one
// thread_local buffer is enough and avoids per-packet allocation.
constexpr std::size_t MAX_PAYLOAD_SIZE = 512;
static_assert(sizeof(InteractPayload) <= MAX_PAYLOAD_SIZE,
	"raise MAX_PAYLOAD_SIZE if you add a bigger payload");

// One entry per opcode. The decoder does placement-new of T into the
// caller-supplied buffer; the handler is a stateless function pointer
// generated at compile time that reinterprets the void* payload and calls
// the typed Server::handleCommand_* method. The destructor releases any
// heap memory the decoder allocated (vectors, strings, StringMap) so the
// shared in-place buffer is ready for the next packet.
struct DispatchEntry {
	void *(*decode)(NetworkPacket &, void *out);
	void (*handle)(Server *, const PacketContext &, const void *payload);
	void (*destroy)(void *);
};

class IPacketDispatcher {
public:
	virtual ~IPacketDispatcher() = default;

	// Validate range, apply state filter, decode payload, call the typed
	// handler. Never throws (catches SendFailedException internally).
	virtual void dispatch(NetworkPacket *pkt, Server *owner) = 0;

	// For debug / Profiler output.
	virtual const char *commandName(u16 cmd) const = 0;

	// For unit tests: run the decoder registered for `cmd` against `pkt`
	// into the buffer at `out`. Returns the resulting typed payload (cast
	// as `void *`), or nullptr if `cmd` has no registered decoder or the
	// packet is malformed. The caller must call `destroyPayload(cmd, ptr)`
	// to release heap memory the decoder allocated.
	virtual void *decode(u16 cmd, NetworkPacket &pkt, void *out) const = 0;
	virtual void destroyPayload(u16 cmd, void *ptr) const = 0;
};

class ServerPacketDispatcher final : public IPacketDispatcher {
public:
	ServerPacketDispatcher();

	void dispatch(NetworkPacket *pkt, Server *owner) override;
	const char *commandName(u16 cmd) const override;
	void *decode(u16 cmd, NetworkPacket &pkt, void *out) const override;
	void destroyPayload(u16 cmd, void *ptr) const override;

private:
	std::array<DispatchEntry, TOSERVER_NUM_MSG_TYPES> m_table;
};

std::unique_ptr<IPacketDispatcher> newPacketDispatcher();

// Internal: bind a typed Server::* handler to a stateless C-function pointer
// that takes a `const void *` payload. The thunk reinterprets the pointer
// and forwards. The decoder thunk does placement-new of T into the buffer.
template <typename T, T (*D)(NetworkPacket &)>
inline void *typedDecodeEntry(NetworkPacket &pkt, void *out)
{
	return new (out) T(D(pkt));
}

template <typename T>
inline void typedDestroyEntry(void *p)
{
	static_cast<T *>(p)->~T();
}

template <typename T, void (Server::*M)(const PacketContext &, const T &)>
inline void typedHandlerEntry(Server *s, const PacketContext &ctx, const void *payload)
{
	(s->*M)(ctx, *static_cast<const T *>(payload));
}

} // namespace server
