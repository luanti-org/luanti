// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "networkprotocol.h" // session_t
#include "server/clientiface.h" // ClientState
#include "hud_element.h" // HudElementStat
#include "network/networkprotocol.h"
#include "mapnode.h" // MapNode

#include <string>
#include <string_view>
#include <vector>

class NetworkPacket;
class IItemDefManager;
class NodeDefManager;

enum ModChannelSignal : u8;

struct ChatMessage;
struct CloudParams;
struct HudElement;
struct Lighting;
struct MinimapMode;
struct MoonParams;
struct ParticleParameters;
struct ParticleSpawnerParameters;
struct SkyboxParams;
struct StarParams;
struct SunParams;

namespace server {

/**
 * Outgoing-network interface, used by `Server` (and any future
 * services) to deliver `TOCLIENT_*` packets to peers.
 *
 * Goals:
 *   - Decouple `Server` from the concrete transport
 *     (`con::IConnection`) and from the session bookkeeping
 *     (`ClientInterface`).
 *   - Concentrate **all packet serialization** in one place
 *     (`netsender.cpp`).
 *   - Make every per-packet send site testable in isolation by
 *     substituting this interface with a mock.
 *
 * Design rules:
 *   - No method ever takes a `NetworkPacket*` as parameter. The
 *     caller passes typed, read-only data (or value) and the
 *     implementation builds the wire-format packet.
 *   - All parameters are `const`, value, or `std::string_view`.
 *   - The interface is **complete**: every per-packet send
 *     helper that used to live on `Server` is a virtual method
 *     here. The production implementation in `ConnectionNetSender`
 *     is final and non-virtual on the concrete class so the
 *     hot path is a single `m_con->Send(...)` call.
 *
 * See `.kilo/plans/luanti-server-refactor-modular.md` §10.5 and
 * the step-9 follow-up (typed sender API).
 */
class INetSender
{
public:
	virtual ~INetSender() = default;

	// ---- low-level building blocks (kept for back-compat / tests) ----

	/// Send a pre-built packet on a custom channel (rare).
	virtual void sendCustom(session_t peer_id, u8 channel,
			NetworkPacket &pkt, bool reliable) = 0;

	/// Disconnect a peer.
	virtual void disconnectPeer(session_t peer_id) = 0;

	/// Protocol version the given peer is on, or 0 if unknown.
	virtual u16 getProtocolVersion(session_t peer_id) = 0;


	// ---- TOCLIENT_UPDATE_PLAYER_LIST ---------------------------------
	//
	// Built here (not in the call sites) so that the wire format
	// lives in a single place. Broadcasts to every active peer
	// (the only peer state that knows about the player list).

	/// TOCLIENT_UPDATE_PLAYER_LIST. Sends a list of `names` (each
	/// a single u16-prefixed string) with the given modifier to
	/// every active client. Used to add/remove players from the
	/// client's player list.
	virtual void sendPlayerListUpdate(PlayerListModifer action,
			const std::vector<std::string> &names) = 0;

	/// Per-peer variant of `sendPlayerListUpdate`. Sent during
	/// the join handshake (PLAYER_LIST_INIT) to a single peer.
	virtual void sendPlayerListUpdateTo(session_t peer_id,
			PlayerListModifer action,
			const std::vector<std::string> &names) = 0;

	// ---- TOCLIENT_MOVEMENT ------------------------------------------

	/// Build & send the movement parameters packet. Caller is
	/// responsible for reading the values out of `g_settings`
	/// and passing them in (so this method doesn't depend on
	/// `g_settings` and is testable in isolation).
	virtual void sendMovement(session_t peer_id,
			float accel_default, float accel_air, float accel_fast,
			float speed_walk, float speed_crouch, float speed_fast,
			float speed_climb, float speed_jump,
			float liquid_fluidity, float liquid_fluidity_smooth,
			float liquid_sink, float gravity) = 0;

	// ---- small typed packets ----------------------------------------

	virtual void sendHP(session_t peer_id, u16 hp, bool effect) = 0;
	virtual void sendBreath(session_t peer_id, u16 breath) = 0;

	virtual void sendAccessDenied(session_t peer_id,
			AccessDeniedCode reason, std::string_view custom_reason,
			bool reconnect) = 0;

	/// TOCLIENT_CHAT_MESSAGE. If `peer_id == PEER_ID_INEXISTENT`
	/// the message is broadcast to every peer that has reached
	/// CS_InitDone.
	virtual void sendChatMessage(session_t peer_id,
			u8 type, const std::wstring &sender,
			const std::wstring &message, u64 timestamp) = 0;

	/// TOCLIENT_SHOW_FORMSPEC. `formspec` may be empty (closes
	/// any open formspec on the client).
	virtual void sendShowFormspec(session_t peer_id,
			const std::string &formspec, const std::string &formname) = 0;

	/// TOCLIENT_TIME_OF_DAY. If `peer_id == PEER_ID_INEXISTENT`
	/// the message is broadcast to every active peer.
	virtual void sendTimeOfDay(session_t peer_id, u16 time, f32 speed) = 0;

	virtual void sendOverrideDayNightRatio(session_t peer_id,
			bool do_override, f32 ratio) = 0;

	virtual void sendCSMRestrictionFlags(session_t peer_id,
			u64 flags, u32 noderange) = 0;

	virtual void sendMovePlayer(session_t peer_id,
			const v3f &pos, f32 pitch, f32 yaw) = 0;
	virtual void sendMovePlayerRel(session_t peer_id, const v3f &added_pos) = 0;
	virtual void sendPlayerSpeed(session_t peer_id, const v3f &vel) = 0;
	virtual void sendPlayerFov(session_t peer_id,
			f32 fov, bool is_multiplier, f32 transition_time) = 0;

	virtual void sendEyeOffset(session_t peer_id,
			const v3f &first, const v3f &third,
			const v3f &third_front) = 0;

	virtual void sendLocalPlayerAnimations(session_t peer_id,
			const v2f frames[4], f32 speed) = 0;

	virtual void sendCamera(session_t peer_id, u8 allowed_camera_mode) = 0;

	// ---- inventory (full) -------------------------------------------

	/// `serialized_inventory` is the inventory as serialized by
	/// the caller (the inventory also needs to be marked
	/// not-modified on success, which the netsender cannot do).
	virtual void sendInventory(session_t peer_id,
			std::string &&serialized_inventory,
			bool incremental, bool skip_wield_anim) = 0;

	// ---- HUD --------------------------------------------------------

	virtual void sendHUDAdd(session_t peer_id, u32 id,
			const HudElement &element) = 0;
	virtual void sendHUDRemove(session_t peer_id, u32 id) = 0;
	virtual void sendHUDChange(session_t peer_id, u32 id,
			HudElementStat stat, const void *value) = 0;
	virtual void sendHUDSetFlags(session_t peer_id, u32 flags, u32 mask) = 0;
	virtual void sendHUDSetParam(session_t peer_id, u16 param,
			std::string_view value) = 0;

	// ---- sky / world visuals ---------------------------------------

	virtual void sendSetSky(session_t peer_id, const SkyboxParams &params) = 0;
	virtual void sendSetSun(session_t peer_id, const SunParams &params) = 0;
	virtual void sendSetMoon(session_t peer_id, const MoonParams &params) = 0;
	virtual void sendSetStars(session_t peer_id, const StarParams &params) = 0;
	virtual void sendCloudParams(session_t peer_id, const CloudParams &params) = 0;
	virtual void sendSetLighting(session_t peer_id, const Lighting &lighting) = 0;

	// ---- privilege / formspec / inventory-formspec -----------------

	virtual void sendPlayerPrivileges(session_t peer_id,
			const std::vector<std::string> &privs) = 0;
	virtual void sendPlayerInventoryFormspec(session_t peer_id,
			const std::string &formspec) = 0;
	virtual void sendPlayerFormspecPrepend(session_t peer_id,
			const std::string &prepend) = 0;

	// ---- particles --------------------------------------------------

	/// TOCLIENT_SPAWN_PARTICLE (one packet, no batching). Caller
	/// is responsible for any pre-filtering (range, gone-check).
	virtual void sendSpawnParticle(session_t peer_id,
			const ParticleParameters &p) = 0;

	/// TOCLIENT_SPAWN_PARTICLE_BATCH. The caller pre-serializes
	/// each particle and concatenates them with a length-prefix
	/// into `serialized` (so the per-particle serialize only
	/// happens once even if the batch is rebroadcast).
	virtual void sendSpawnParticleBatch(session_t peer_id,
			std::string &&serialized) = 0;

	virtual void sendAddParticleSpawner(session_t peer_id, u16 proto_version,
			const ParticleSpawnerParameters &p, u16 attached_id, u32 id) = 0;

	/// TOCLIENT_DELETE_PARTICLESPAWNER. If `peer_id ==
	/// PEER_ID_INEXISTENT` the message is broadcast to every
	/// active peer.
	virtual void sendDeleteParticleSpawner(session_t peer_id, u32 id) = 0;

	// ---- active objects --------------------------------------------

	/// TOCLIENT_ACTIVE_OBJECT_MESSAGES on the channel chosen by
	/// `reliable` (uses the cmd-factory channel when reliable,
	/// channel 1 when not).
	virtual void sendActiveObjectMessages(session_t peer_id,
			const std::string &datas, bool reliable) = 0;

	struct ActiveObjectRef {
		u16 id = 0;
		u8  type = 0;             ///< only meaningful for `added`
		std::string init_data;    ///< only meaningful for `added`
	};

	virtual void sendActiveObjectRemoveAdd(session_t peer_id,
			const std::vector<u16> &removed,
			const std::vector<ActiveObjectRef> &added) = 0;

	// ---- map edits (per-peer list) ----------------------------------

	/// Send a TOCLIENT_REMOVENODE for `pos` to each peer in
	/// `peers` that has reached CS_Active. Peers that are not
	/// active (or no longer exist) are silently skipped.
	virtual void sendRemoveNode(const std::vector<session_t> &peers,
			const v3s16 &pos) = 0;

	/// `keep_metadata` == false means "remove metadata" on the
	/// client; `metadata_serialized` is ignored in that case.
	virtual void sendAddNode(const std::vector<session_t> &peers,
			const v3s16 &pos, const MapNode &n,
			u16 type, bool keep_metadata,
			const std::string &metadata_serialized) = 0;

	virtual void sendNodeMetaChanged(session_t peer_id, u16 type,
			const std::string &metadata_serialized) = 0;

	// ---- inventory (detached) --------------------------------------

	/// When `update_inventory == false` the client is told to
	/// remove the detached inventory; `inventory_serialized` is
	/// then ignored.
	virtual void sendDetachedInventory(session_t peer_id,
			const std::string &name,
			const std::string &inventory_serialized,
			bool update_inventory) = 0;

	// ---- definitions ------------------------------------------------

	virtual void sendItemDef(session_t peer_id,
			IItemDefManager &itemdef, u16 protocol_version) = 0;
	virtual void sendNodeDef(session_t peer_id,
			const NodeDefManager &nodedef, u16 protocol_version) = 0;

	// ---- map blocks ------------------------------------------------

	/// `serialized_block` is the block already serialized by
	/// `MapBlock::serialize` + `serializeNetworkSpecific`.
	virtual void sendBlockData(session_t peer_id,
			const v3s16 &pos, const std::string &serialized_block) = 0;

	// ---- sounds ----------------------------------------------------

	/// TOCLIENT_PLAY_SOUND. The caller resolves the sound's world
	/// position via `ServerPlayingSound::getPos()` (which needs
	/// the env) and passes the primitives here. The `handle`
	/// field is the sound id (`-1` for ephemeral sounds). The
	/// `type` field is the sound's spatial scope
	/// (`SoundLocation`).
	virtual void sendPlaySound(session_t peer_id, s32 handle,
			const std::string &spec_name, f32 gain, u8 type,
			const v3f &pos, u16 object, bool loop, f32 fade,
			f32 pitch, bool ephemeral, f32 start_time) = 0;

	virtual void sendStopSound(session_t peer_id, s32 handle) = 0;
	virtual void sendFadeSound(session_t peer_id, s32 handle,
			f32 step, f32 gain) = 0;

	// ---- media ------------------------------------------------------

	virtual void sendMediaAnnouncement(session_t peer_id,
			const std::vector<std::string> &filenames,
			const std::vector<std::string> &sha1_hashes,
			const std::string &remote_media,
			u16 protocol_version) = 0;

	virtual void sendMedia(session_t peer_id,
			const std::string &filename, std::string_view data) = 0;

	/// TOCLIENT_MEDIA bunch (one of N packets in a file transfer).
	/// `files` is the list of (filename, data) pairs to embed in
	/// this bunch packet. The wire format mirrors the legacy
	/// `Server::sendRequestedMedia` bunch format:
	///   u16 num_bunches, u16 bunch_index, u32 bunch_size,
	///   for each file: u16-prefixed name + long-string data.
	/// Built here (not at the call site) so the wire format lives
	/// in a single place.
	virtual void sendMediaBunch(session_t peer_id,
			u16 num_bunches, u16 bunch_index,
			const std::vector<std::pair<std::string_view, std::string_view>> &files) = 0;

	/// Dynamic media push (TOCLIENT_MEDIA_PUSH, dynamic_add_media v2).
	/// `raw_hash` is the 20-byte SHA1 of the file (raw, no hex
	/// encoding). `cached` indicates whether the client already
	/// has the file. `token` is a 32-bit id used by the script
	/// API to track delivery per-player.
	///
	/// The packet is built ONCE and fanned out to every peer in
	/// `peers` that has reached CS_Active (others are silently
	/// skipped, matching the per-peer-list semantics of
	/// `sendRemoveNode` / `sendAddNode`).
	virtual void sendMediaPush(const std::vector<session_t> &peers,
			const std::string &raw_hash,
			const std::string &filename,
			bool cached, u32 token) = 0;

	// ---- minimap ----------------------------------------------------

	virtual void sendMinimapModes(session_t peer_id,
			const std::vector<MinimapMode> &modes, size_t wanted_mode) = 0;

	// ---- protocol / connection control -------------------------------

	virtual void sendDenySudoMode(session_t peer_id) = 0;

	// ---- auth handshake ---------------------------------------------
	//
	// These packets are part of the SRP / connection-boot protocol.
	// They're sent to a single peer (the one currently
	// authenticating) so the helper takes only `peer_id`.

	/// TOCLIENT_HELLO. Sent right after Init1 to advertise the
	/// server's serialization version, network protocol version,
	/// and the set of supported auth mechanisms. The trailing
	/// unused string field is preserved (it's part of the
	/// wire format) but always empty.
	virtual void sendHello(session_t peer_id,
			u8 serialization_ver,
			u16 net_proto_version,
			u32 auth_mechs) = 0;

	/// TOCLIENT_AUTH_ACCEPT. Final positive response to a
	/// complete SRP exchange. Carries the spawn position
	/// (currently always (0,0,0)), the map seed, the server
	/// step length and the set of auth mechanisms the server
	/// accepts for THIS peer (`allowed_auth_mechs`).
	virtual void sendAuthAccept(session_t peer_id,
			u64 map_seed,
			float dedicated_server_step,
			u32 allowed_auth_mechs) = 0;

	/// TOCLIENT_ACCEPT_SUDO_MODE. Positive response to a
	/// sudo-mode re-authentication. Only SRP is supported for
	/// re-auth, so the wire format is just the chosen mech.
	virtual void sendAcceptSudoMode(session_t peer_id,
			u32 sudo_auth_mechs) = 0;

	/// TOCLIENT_SRP_BYTES_S_B. Reply to TOSERVER_SRP_BYTES_A
	/// with the salt and the server's B value.
	/// `bytes_B` is a raw byte buffer of length `len_B`
	/// (typically 256 bytes, raw — no length prefix on the wire).
	virtual void sendSrpBytesSandB(session_t peer_id,
			const std::string &salt,
			const char *bytes_B, size_t len_B) = 0;

	// ---- mod channels ------------------------------------------------

	/// TOCLIENT_MODCHANNEL_SIGNAL. Sent as a one-shot
	/// (per-peer) notification: a join/leave ack, a "channel
	/// not registered" error, or a state-change signal.
	virtual void sendModChannelSignal(session_t peer_id,
			ModChannelSignal signal, const std::string &channel) = 0;

	/// TOCLIENT_MODCHANNEL_MSG. Carries a mod-channel
	/// broadcast (the actual message, not a control signal).
	/// `sender` is the player name (or empty for server).
	virtual void sendModChannelMsg(session_t peer_id,
			const std::string &channel,
			const std::string &sender,
			const std::string &message) = 0;

protected:
	// ---- low-level building blocks ----------------------------------
	//
	// `send` and `sendToAll` are implementation details of the
	// netserver (used by typed helpers like `sendHP` / `sendTimeOfDay` /
	// `sendPlayerListUpdate` to push a pre-built packet to one peer
	// or to all peers in a given minimum state). Callers outside of
	// the netsender should prefer a typed helper that builds the
	// wire format itself.

	/// Send a pre-built packet to a single peer.
	virtual void send(session_t peer_id, NetworkPacket &pkt) = 0;

	/// Send a pre-built packet to every peer whose state >= `min`.
	virtual void sendToAll(NetworkPacket &pkt, ClientState min_state) = 0;
};

} // namespace server
