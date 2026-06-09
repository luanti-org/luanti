// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "inetserver.h"

class NetworkPacket;
class ClientInterface;

namespace con {
	class IConnection;
}

namespace server {

/**
 * Production implementation of `INetServer` (and therefore of
 * `INetSender`).
 *
 * Owns a reference to the underlying `con::IConnection` and to
 * the server's `ClientInterface` (used for the protected
 * `sendToAll` broadcast path, for the `ClientCommandFactory`
 * table lookup, and for `getProtocolVersion`).
 *
 * All packet serialization lives in `netserver.cpp`.
 *
 * The hot path (`send`/`sendHP`/etc.) compiles to a single
 * `m_con->Send(...)` call wrapped in a command-table lookup. It
 * does not allocate (beyond the per-packet buffer) and does
 * not take any extra mutex.
 */
class ConnectionNetServer final : public INetServer
{
public:
	ConnectionNetServer(con::IConnection &con, ClientInterface &clients);

	// ---- low-level building blocks ----
	void sendCustom(session_t peer_id, u8 channel,
			NetworkPacket &pkt, bool reliable) override;
	void disconnectPeer(session_t peer_id) override;
	u16   getProtocolVersion(session_t peer_id) override;

	// ---- TOCLIENT_UPDATE_PLAYER_LIST ----
	void sendPlayerListUpdate(PlayerListModifer action,
			const std::vector<std::string> &names) override;
	void sendPlayerListUpdateTo(session_t peer_id,
			PlayerListModifer action,
			const std::vector<std::string> &names) override;

	// ---- TOCLIENT_MOVEMENT ----
	void sendMovement(session_t peer_id,
			float accel_default, float accel_air, float accel_fast,
			float speed_walk, float speed_crouch, float speed_fast,
			float speed_climb, float speed_jump,
			float liquid_fluidity, float liquid_fluidity_smooth,
			float liquid_sink, float gravity) override;

	// ---- small typed packets ----
	void sendHP(session_t peer_id, u16 hp, bool effect) override;
	void sendBreath(session_t peer_id, u16 breath) override;
	void sendAccessDenied(session_t peer_id,
			AccessDeniedCode reason, std::string_view custom_reason,
			bool reconnect) override;
	void sendChatMessage(session_t peer_id,
			u8 type, const std::wstring &sender,
			const std::wstring &message, u64 timestamp) override;
	void sendShowFormspec(session_t peer_id,
			const std::string &formspec, const std::string &formname) override;
	void sendTimeOfDay(session_t peer_id, u16 time, f32 speed) override;
	void sendOverrideDayNightRatio(session_t peer_id,
			bool do_override, f32 ratio) override;
	void sendCSMRestrictionFlags(session_t peer_id,
			u64 flags, u32 noderange) override;
	void sendMovePlayer(session_t peer_id,
			const v3f &pos, f32 pitch, f32 yaw) override;
	void sendMovePlayerRel(session_t peer_id, const v3f &added_pos) override;
	void sendPlayerSpeed(session_t peer_id, const v3f &vel) override;
	void sendPlayerFov(session_t peer_id,
			f32 fov, bool is_multiplier, f32 transition_time) override;
	void sendEyeOffset(session_t peer_id,
			const v3f &first, const v3f &third,
			const v3f &third_front) override;
	void sendLocalPlayerAnimations(session_t peer_id,
			const v2f frames[4], f32 speed) override;
	void sendCamera(session_t peer_id, u8 allowed_camera_mode) override;

	// ---- inventory (full) ----
	void sendInventory(session_t peer_id,
			std::string &&serialized_inventory,
			bool incremental, bool skip_wield_anim) override;

	// ---- HUD ----
	void sendHUDAdd(session_t peer_id, u32 id,
			const HudElement &element) override;
	void sendHUDRemove(session_t peer_id, u32 id) override;
	void sendHUDChange(session_t peer_id, u32 id,
			HudElementStat stat, const void *value) override;
	void sendHUDSetFlags(session_t peer_id, u32 flags, u32 mask) override;
	void sendHUDSetParam(session_t peer_id, u16 param,
			std::string_view value) override;

	// ---- sky / world visuals ----
	void sendSetSky(session_t peer_id, const SkyboxParams &params) override;
	void sendSetSun(session_t peer_id, const SunParams &params) override;
	void sendSetMoon(session_t peer_id, const MoonParams &params) override;
	void sendSetStars(session_t peer_id, const StarParams &params) override;
	void sendCloudParams(session_t peer_id, const CloudParams &params) override;
	void sendSetLighting(session_t peer_id, const Lighting &lighting) override;

	// ---- privilege / formspec / inventory-formspec ----
	void sendPlayerPrivileges(session_t peer_id,
			const std::vector<std::string> &privs) override;
	void sendPlayerInventoryFormspec(session_t peer_id,
			const std::string &formspec) override;
	void sendPlayerFormspecPrepend(session_t peer_id,
			const std::string &prepend) override;

	// ---- particles ----
	void sendSpawnParticle(session_t peer_id,
			const ParticleParameters &p) override;
	void sendSpawnParticleBatch(session_t peer_id,
			std::string &&serialized) override;
	void sendAddParticleSpawner(session_t peer_id, u16 proto_version,
			const ParticleSpawnerParameters &p, u16 attached_id, u32 id) override;
	void sendDeleteParticleSpawner(session_t peer_id, u32 id) override;

	// ---- active objects ----
	void sendActiveObjectMessages(session_t peer_id,
			const std::string &datas, bool reliable) override;
	void sendActiveObjectRemoveAdd(session_t peer_id,
			const std::vector<u16> &removed,
			const std::vector<ActiveObjectRef> &added) override;

	// ---- map edits ----
	void sendRemoveNode(const std::vector<session_t> &peers,
			const v3s16 &pos) override;
	void sendAddNode(const std::vector<session_t> &peers,
			const v3s16 &pos, const MapNode &n,
			u16 type, bool keep_metadata,
			const std::string &metadata_serialized) override;
	void sendNodeMetaChanged(session_t peer_id, u16 type,
			const std::string &metadata_serialized) override;

	// ---- inventory (detached) ----
	void sendDetachedInventory(session_t peer_id,
			const std::string &name,
			const std::string &inventory_serialized,
			bool update_inventory) override;

	// ---- definitions ----
	void sendItemDef(session_t peer_id,
			IItemDefManager &itemdef, u16 protocol_version) override;
	void sendNodeDef(session_t peer_id,
			const NodeDefManager &nodedef, u16 protocol_version) override;

	// ---- map blocks ----
	void sendBlockData(session_t peer_id,
			const v3s16 &pos, const std::string &serialized_block) override;

	// ---- sounds ----
	void sendPlaySound(session_t peer_id, s32 handle,
			const std::string &spec_name, f32 gain, u8 type,
			const v3f &pos, u16 object, bool loop, f32 fade,
			f32 pitch, bool ephemeral, f32 start_time) override;
	void sendStopSound(session_t peer_id, s32 handle) override;
	void sendFadeSound(session_t peer_id, s32 handle,
			f32 step, f32 gain) override;

	// ---- media ----
	void sendMediaAnnouncement(session_t peer_id,
			const std::vector<std::string> &filenames,
			const std::vector<std::string> &sha1_hashes,
			const std::string &remote_media,
			u16 protocol_version) override;
	void sendMedia(session_t peer_id,
			const std::string &filename, std::string_view data) override;

	void sendMediaBunch(session_t peer_id,
			u16 num_bunches, u16 bunch_index,
			const std::vector<std::pair<std::string_view, std::string_view>> &files) override;
	void sendMediaPush(const std::vector<session_t> &peers,
			const std::string &raw_hash,
			const std::string &filename,
			bool cached, u32 token) override;

	// ---- minimap ----
	void sendMinimapModes(session_t peer_id,
			const std::vector<MinimapMode> &modes, size_t wanted_mode) override;

	// ---- protocol / connection control ----
	void sendDenySudoMode(session_t peer_id) override;

	// ---- auth handshake ----
	void sendHello(session_t peer_id,
			u8 serialization_ver,
			u16 net_proto_version,
			u32 auth_mechs) override;
	void sendAuthAccept(session_t peer_id,
			u64 map_seed,
			float dedicated_server_step,
			u32 allowed_auth_mechs) override;
	void sendAcceptSudoMode(session_t peer_id,
			u32 sudo_auth_mechs) override;
	void sendSrpBytesSandB(session_t peer_id,
			const std::string &salt,
			const char *bytes_B, size_t len_B) override;

	// ---- mod channels ----
	void sendModChannelSignal(session_t peer_id,
			ModChannelSignal signal,
			const std::string &channel) override;
	void sendModChannelMsg(session_t peer_id,
			const std::string &channel,
			const std::string &sender,
			const std::string &message) override;

private:
	// `send` and `sendToAll` are protected on the interface and
	// used by typed helpers in this class (e.g. `sendHP`,
	// `sendChatMessage`, `sendTimeOfDay`,
	// `sendDeleteParticleSpawner`, `sendPlayerListUpdate`).
	// Re-declared as private overrides so they stay out of the
	// public API.
	void send(session_t peer_id, NetworkPacket &pkt) override;
	void sendToAll(NetworkPacket &pkt, ClientState min_state) override;

	con::IConnection &m_con;
	ClientInterface  &m_clients;
};

} // namespace server
