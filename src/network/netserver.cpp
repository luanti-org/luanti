// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "netserver.h"

#include "config.h" // CHECK_CLIENT_BUILD
#include "server/clientiface.h" // ClientInterface
#include "networkexceptions.h" // SendFailedException
#include "networkpacket.h"
#include "serveropcodes.h" // clientCommandFactoryTable

#include "log.h" // FATAL_ERROR, verbosestream
#include "network/connection.h" // IConnection, mtp/impl

#include "hud_element.h"
#include "itemdef.h" // IItemDefManager
#include "lighting.h" // Lighting
#include "mapnode.h" // MapNode
#include "nodedef.h" // NodeDefManager
#include "particles.h"
#include "serialization.h" // compressZstd, compressZlib
#include "server.h"
#include "skyparams.h"
#include "util/base64.h"
#include "util/serialize.h"

#include <sstream>

namespace server {

// =====================================================================
//  ConnectionNetServer — production implementation of INetServer.
//  Every per-packet helper below builds ONE NetworkPacket and
//  sends it. No business logic (player lookup, envlock, settings).
// =====================================================================

ConnectionNetServer::ConnectionNetServer(con::IConnection &con, ClientInterface &clients) :
	m_con(con), m_clients(clients)
{
}

// ---- low-level building blocks -------------------------------------

void ConnectionNetServer::send(session_t peer_id, NetworkPacket &pkt)
{
	const auto &ccf = clientCommandFactoryTable[pkt.getCommand()];
	FATAL_ERROR_IF(!ccf.name, "packet type missing in table");

	m_con.Send(peer_id, ccf.channel, &pkt, ccf.reliable);
}

void ConnectionNetServer::sendToAll(NetworkPacket &pkt, ClientState min_state)
{
	const auto &ccf = clientCommandFactoryTable[pkt.getCommand()];
	FATAL_ERROR_IF(!ccf.name, "packet type missing in table");

	// Reuse the client interface's broadcast path so that the
	// recursive mutex is taken in the correct order, exactly as
	// it was before the refactor. This keeps the locking
	// contract (`env -> sessions -> con`) intact.
	m_clients.sendToAll(&pkt, min_state);
}

// ---- TOCLIENT_UPDATE_PLAYER_LIST ------------------------------------
//
// Wire format (see also `clientpackethandler.cpp`):
//   u8  modifier        (PlayerListModifer)
//   u16 count
//   count * (u16 len + bytes)  (each entry a u16-prefixed string)
//
// Broadcast to every active peer: only peers that have reached
// CS_Active know about the player list (and would just discard
// it otherwise). The modifier+count framing is identical for
// ADD/REMOVE/INIT, so this single helper covers all three.

void ConnectionNetServer::sendPlayerListUpdate(PlayerListModifer action,
		const std::vector<std::string> &names)
{
	NetworkPacket pkt(TOCLIENT_UPDATE_PLAYER_LIST, 0, PEER_ID_INEXISTENT);
	pkt << (u8)action << (u16)names.size();
	for (const std::string &name : names)
		pkt << name;
	sendToAll(pkt, CS_Active);
}

void ConnectionNetServer::sendPlayerListUpdateTo(session_t peer_id,
		PlayerListModifer action,
		const std::vector<std::string> &names)
{
	NetworkPacket pkt(TOCLIENT_UPDATE_PLAYER_LIST, 0, peer_id);
	pkt << (u8)action << (u16)names.size();
	for (const std::string &name : names)
		pkt << name;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendCustom(session_t peer_id, u8 channel,
		NetworkPacket &pkt, bool reliable)
{
	// Validate that the command is in the table even though
	// we're using a custom channel. This matches the
	// pre-refactor behavior in `ClientInterface::sendCustom`.
	FATAL_ERROR_IF(!clientCommandFactoryTable[pkt.getCommand()].name,
		"packet type missing in table");

	m_con.Send(peer_id, channel, &pkt, reliable);
}

void ConnectionNetServer::disconnectPeer(session_t peer_id)
{
	m_con.DisconnectPeer(peer_id);
}

u16 ConnectionNetServer::getProtocolVersion(session_t peer_id)
{
	return m_clients.getProtocolVersion(peer_id);
}

// ---- TOCLIENT_MOVEMENT ----------------------------------------------

void ConnectionNetServer::sendMovement(session_t peer_id,
		float accel_default, float accel_air, float accel_fast,
		float speed_walk, float speed_crouch, float speed_fast,
		float speed_climb, float speed_jump,
		float liquid_fluidity, float liquid_fluidity_smooth,
		float liquid_sink, float gravity)
{
	NetworkPacket pkt(TOCLIENT_MOVEMENT, 12 * sizeof(float), peer_id);
	pkt << accel_default << accel_air << accel_fast
		<< speed_walk << speed_crouch << speed_fast
		<< speed_climb << speed_jump
		<< liquid_fluidity << liquid_fluidity_smooth
		<< liquid_sink << gravity;
	send(peer_id, pkt);
}

// ---- small typed packets -------------------------------------------

void ConnectionNetServer::sendHP(session_t peer_id, u16 hp, bool effect)
{
	NetworkPacket pkt(TOCLIENT_HP, 3, peer_id);
	pkt << hp << effect;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendBreath(session_t peer_id, u16 breath)
{
	NetworkPacket pkt(TOCLIENT_BREATH, 2, peer_id);
	pkt << (u16)breath;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendAccessDenied(session_t peer_id,
		AccessDeniedCode reason, std::string_view custom_reason,
		bool reconnect)
{
	assert(reason < SERVER_ACCESSDENIED_MAX);
	NetworkPacket pkt(TOCLIENT_ACCESS_DENIED, 1, peer_id);
	pkt << (u8)reason << custom_reason << (u8)reconnect;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendChatMessage(session_t peer_id,
		u8 type, const std::wstring &sender,
		const std::wstring &message, u64 timestamp)
{
	NetworkPacket pkt(TOCLIENT_CHAT_MESSAGE, 0, peer_id);
	const u8 version = 1;
	pkt << version << type << sender << message << timestamp;
	if (peer_id != PEER_ID_INEXISTENT)
		send(peer_id, pkt);
	else
		// If a client has completed auth but is still
		// joining, still send chat.
		sendToAll(pkt, CS_InitDone);
}

void ConnectionNetServer::sendShowFormspec(session_t peer_id,
		const std::string &formspec, const std::string &formname)
{
	NetworkPacket pkt(TOCLIENT_SHOW_FORMSPEC, 0, peer_id);
	pkt.putLongString(formspec);
	pkt << formname;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendTimeOfDay(session_t peer_id, u16 time, f32 speed)
{
	NetworkPacket pkt(TOCLIENT_TIME_OF_DAY, 0, peer_id);
	pkt << time << speed;
	if (peer_id == PEER_ID_INEXISTENT)
		sendToAll(pkt, CS_Active);
	else
		send(peer_id, pkt);
}

void ConnectionNetServer::sendOverrideDayNightRatio(session_t peer_id,
		bool do_override, f32 ratio)
{
	NetworkPacket pkt(TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO, 1 + 2, peer_id);
	pkt << do_override << (u16)(ratio * 65535);
	send(peer_id, pkt);
}

void ConnectionNetServer::sendCSMRestrictionFlags(session_t peer_id,
		u64 flags, u32 noderange)
{
	NetworkPacket pkt(TOCLIENT_CSM_RESTRICTION_FLAGS,
		sizeof(flags) + sizeof(noderange), peer_id);
	pkt << flags << noderange;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendMovePlayer(session_t peer_id,
		const v3f &pos, f32 pitch, f32 yaw)
{
	NetworkPacket pkt(TOCLIENT_MOVE_PLAYER,
		sizeof(v3f) + sizeof(f32) * 2, peer_id);
	pkt << pos << pitch << yaw;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendMovePlayerRel(session_t peer_id, const v3f &added_pos)
{
	NetworkPacket pkt(TOCLIENT_MOVE_PLAYER_REL, 0, peer_id);
	pkt << added_pos;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendPlayerSpeed(session_t peer_id, const v3f &vel)
{
	NetworkPacket pkt(TOCLIENT_PLAYER_SPEED, 0, peer_id);
	pkt << vel;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendPlayerFov(session_t peer_id,
		f32 fov, bool is_multiplier, f32 transition_time)
{
	NetworkPacket pkt(TOCLIENT_FOV, 4 + 1 + 4, peer_id);
	pkt << fov << is_multiplier << transition_time;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendEyeOffset(session_t peer_id,
		const v3f &first, const v3f &third, const v3f &third_front)
{
	NetworkPacket pkt(TOCLIENT_EYE_OFFSET, 0, peer_id);
	pkt << first << third << third_front;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendLocalPlayerAnimations(session_t peer_id,
		const v2f frames[4], f32 speed)
{
	const u16 proto = getProtocolVersion(peer_id);
	NetworkPacket pkt(TOCLIENT_LOCAL_PLAYER_ANIMATIONS, 0, peer_id);
	for (int i = 0; i < 4; ++i) {
		if (proto >= 46)
			pkt << frames[i];
		else
			pkt << v2s32::from(frames[i]);
	}
	pkt << speed;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendCamera(session_t peer_id, u8 allowed_camera_mode)
{
	NetworkPacket pkt(TOCLIENT_CAMERA, 1, peer_id);
	pkt << allowed_camera_mode;
	send(peer_id, pkt);
}

// ---- inventory (full) ----------------------------------------------

void ConnectionNetServer::sendInventory(session_t peer_id,
		std::string &&serialized_inventory,
		bool incremental, bool skip_wield_anim)
{
	// Old clients (< 52) use the raw-string encoding and don't
	// have skip_wield_anim. Older clients (< 38) cannot do
	// incremental inventory; the caller is expected to have
	// downgraded `incremental` already.
	NetworkPacket pkt(TOCLIENT_INVENTORY, 0, peer_id);
	if (getProtocolVersion(peer_id) >= 52) {
		pkt.putLongString(serialized_inventory);
		pkt << skip_wield_anim;
	} else {
		(void)incremental;
		pkt.putRawString(serialized_inventory);
	}
	send(peer_id, pkt);
}

// ---- HUD ----------------------------------------------------------

void ConnectionNetServer::sendHUDAdd(session_t peer_id, u32 id,
		const HudElement &e)
{
	NetworkPacket pkt(TOCLIENT_HUDADD, 0, peer_id);
	pkt << id << (u8)e.type << e.pos << e.name << e.scale
		<< e.text << e.number << e.item << e.dir
		<< e.align << e.offset << e.world_pos;

	if (getProtocolVersion(peer_id) >= 52)
		pkt << e.size;
	else
		pkt << v2s32::from(e.size);

	const u8 flags = e.hideable ? 1 : 0;
	pkt << e.z_index << e.text2 << e.style << flags;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendHUDRemove(session_t peer_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_HUDRM, 4, peer_id);
	pkt << id;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendHUDChange(session_t peer_id, u32 id,
		HudElementStat stat, const void *value)
{
	NetworkPacket pkt(TOCLIENT_HUDCHANGE, 0, peer_id);
	pkt << id << (u8)stat;

	switch (stat) {
	case HUD_STAT_POS:
	case HUD_STAT_SCALE:
	case HUD_STAT_ALIGN:
	case HUD_STAT_OFFSET:
		pkt << *(const v2f *)value;
		break;
	case HUD_STAT_NAME:
	case HUD_STAT_TEXT:
	case HUD_STAT_TEXT2:
		pkt << *(const std::string *)value;
		break;
	case HUD_STAT_WORLD_POS:
		pkt << *(const v3f *)value;
		break;
	case HUD_STAT_SIZE: {
		const v2f *v = (const v2f *)value;
		if (getProtocolVersion(peer_id) >= 52)
			pkt << *v;
		else
			pkt << v2s32::from(*v);
		break;
	}
	case HUD_STAT_HIDEABLE:
		pkt << u32{*(const bool *)value};
		break;
	default: // all other types
		pkt << *(const u32 *)value;
		break;
	}
	send(peer_id, pkt);
}

void ConnectionNetServer::sendHUDSetFlags(session_t peer_id, u32 flags, u32 mask)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_FLAGS, 4 + 4, peer_id);
	pkt << flags << mask;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendHUDSetParam(session_t peer_id, u16 param,
		std::string_view value)
{
	NetworkPacket pkt(TOCLIENT_HUD_SET_PARAM, 0, peer_id);
	pkt << param << value;
	send(peer_id, pkt);
}

// ---- sky / world visuals ------------------------------------------

void ConnectionNetServer::sendSetSky(session_t peer_id, const SkyboxParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_SKY, 0, peer_id);

	if (getProtocolVersion(peer_id) < 39) {
		pkt << params.bgcolor << params.type << (u16)params.textures.size();
		for (const std::string &t : params.textures)
			pkt << t;
		pkt << params.clouds;
	} else {
		pkt << params.bgcolor << params.type
			<< params.clouds << params.fog_sun_tint
			<< params.fog_moon_tint << params.fog_tint_type;

		if (params.type == "skybox") {
			pkt << (u16)params.textures.size();
			for (const std::string &t : params.textures)
				pkt << t;
		} else if (params.type == "regular") {
			auto &c = params.sky_color;
			pkt << c.day_sky << c.day_horizon << c.dawn_sky
				<< c.dawn_horizon << c.night_sky << c.night_horizon
				<< c.indoors;
		}
		pkt << params.body_orbit_tilt << params.fog_distance
			<< params.fog_start << params.fog_color;
		pkt << params.auto_dim_skybox;
	}
	send(peer_id, pkt);
}

void ConnectionNetServer::sendSetSun(session_t peer_id, const SunParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_SUN, 0, peer_id);
	pkt << params.visible << params.texture
		<< params.tonemap << params.sunrise
		<< params.sunrise_visible << params.scale;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendSetMoon(session_t peer_id, const MoonParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_MOON, 0, peer_id);
	pkt << params.visible << params.texture
		<< params.tonemap << params.scale;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendSetStars(session_t peer_id, const StarParams &params)
{
	NetworkPacket pkt(TOCLIENT_SET_STARS, 0, peer_id);
	pkt << params.visible << params.count
		<< params.starcolor << params.scale
		<< params.day_opacity << params.star_seed;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendCloudParams(session_t peer_id,
		const CloudParams &params)
{
	NetworkPacket pkt(TOCLIENT_CLOUD_PARAMS, 0, peer_id);
	pkt << params.density << params.color_bright << params.color_ambient
		<< params.height << params.thickness << params.speed
		<< params.color_shadow;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendSetLighting(session_t peer_id,
		const Lighting &lighting)
{
	NetworkPacket pkt(TOCLIENT_SET_LIGHTING, 4, peer_id);
	pkt << lighting.shadow_intensity;
	pkt << lighting.saturation;
	pkt << lighting.exposure.luminance_min
		<< lighting.exposure.luminance_max
		<< lighting.exposure.exposure_correction
		<< lighting.exposure.speed_dark_bright
		<< lighting.exposure.speed_bright_dark
		<< lighting.exposure.center_weight_power;
	pkt << lighting.volumetric_light_strength << lighting.shadow_tint;
	pkt << lighting.bloom_intensity << lighting.bloom_strength_factor
		<< lighting.bloom_radius;
	pkt << lighting.shadow_direction;
	send(peer_id, pkt);
}

// ---- privilege / formspec -----------------------------------------

void ConnectionNetServer::sendPlayerPrivileges(session_t peer_id,
		const std::vector<std::string> &privs)
{
	NetworkPacket pkt(TOCLIENT_PRIVILEGES, 0, peer_id);
	pkt << (u16)privs.size();
	for (const std::string &p : privs)
		pkt << p;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendPlayerInventoryFormspec(session_t peer_id,
		const std::string &formspec)
{
	NetworkPacket pkt(TOCLIENT_INVENTORY_FORMSPEC, 0, peer_id);
	pkt.putLongString(formspec);
	send(peer_id, pkt);
}

void ConnectionNetServer::sendPlayerFormspecPrepend(session_t peer_id,
		const std::string &prepend)
{
	NetworkPacket pkt(TOCLIENT_FORMSPEC_PREPEND, 0, peer_id);
	pkt << prepend;
	send(peer_id, pkt);
}

// ---- particles ----------------------------------------------------

void ConnectionNetServer::sendSpawnParticle(session_t peer_id,
		const ParticleParameters &p)
{
	std::ostringstream os(std::ios_base::binary);
	p.serialize(os, getProtocolVersion(peer_id));
	std::string blob = os.str();
	SANITY_CHECK(blob.size() < U32_MAX);

	NetworkPacket pkt(TOCLIENT_SPAWN_PARTICLE, blob.size(), peer_id);
	pkt.putRawString(blob);
	send(peer_id, pkt);
}

void ConnectionNetServer::sendSpawnParticleBatch(session_t peer_id,
		std::string &&serialized)
{
	std::ostringstream compressed(std::ios_base::binary);
	compressZstd(serialized, compressed);
	NetworkPacket pkt(TOCLIENT_SPAWN_PARTICLE_BATCH,
		4 + compressed.tellp(), peer_id);
	pkt.putLongString(compressed.str());
	send(peer_id, pkt);
}

void ConnectionNetServer::sendAddParticleSpawner(session_t peer_id, u16 proto,
		const ParticleSpawnerParameters &p, u16 attached_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_ADD_PARTICLESPAWNER, 100, peer_id);
	pkt << p.amount << p.time;

	std::ostringstream os(std::ios_base::binary);
	if (proto >= 42) {
		p.pos.serialize(os);
		p.vel.serialize(os);
		p.acc.serialize(os);
		p.exptime.serialize(os);
		p.size.serialize(os);
	} else {
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
		p.animation.serialize(os, proto);
		pkt.putRawString(os.str());
	}
	pkt << p.glow << p.object_collision;
	pkt << p.node.param0 << p.node.param2 << p.node_tile;
	{
		os.str("");
		if (proto < 42) {
			pkt << p.pos.start.bias
				<< p.vel.start.bias
				<< p.acc.start.bias
				<< p.exptime.start.bias
				<< p.size.start.bias;
			p.pos.end.serialize(os);
			p.vel.end.serialize(os);
			p.acc.end.serialize(os);
			p.exptime.end.serialize(os);
			p.size.end.serialize(os);
		}
		p.texture.serialize(os, proto, true);
		p.drag.serialize(os);
		p.jitter.serialize(os);
		p.bounce.serialize(os);
		ParticleParamTypes::serializeParameterValue(os, p.attractor_kind);
		if (p.attractor_kind != ParticleParamTypes::AttractorKind::none) {
			p.attract.serialize(os);
			p.attractor_origin.serialize(os);
			writeU16(os, p.attractor_attachment);
			writeU8(os, p.attractor_kill);
			if (p.attractor_kind != ParticleParamTypes::AttractorKind::point) {
				p.attractor_direction.serialize(os);
				writeU16(os, p.attractor_direction_attachment);
			}
		}
		p.radius.serialize(os);

		ParticleParamTypes::serializeParameterValue(os, (u16)p.texpool.size());
		for (const auto &tex : p.texpool)
			tex.serialize(os, proto);
		pkt.putRawString(os.str());
	}
	send(peer_id, pkt);
}

void ConnectionNetServer::sendDeleteParticleSpawner(session_t peer_id, u32 id)
{
	NetworkPacket pkt(TOCLIENT_DELETE_PARTICLESPAWNER, 4, peer_id);
	pkt << id;
	if (peer_id != PEER_ID_INEXISTENT)
		send(peer_id, pkt);
	else
		sendToAll(pkt, CS_Active);
}

// ---- active objects -----------------------------------------------

void ConnectionNetServer::sendActiveObjectMessages(session_t peer_id,
		const std::string &datas, bool reliable)
{
	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_MESSAGES, datas.size(), peer_id);
	pkt.putRawString(datas);
	const auto &ccf = clientCommandFactoryTable[pkt.getCommand()];
	sendCustom(peer_id, reliable ? ccf.channel : 1, pkt, reliable);
}

void ConnectionNetServer::sendActiveObjectRemoveAdd(session_t peer_id,
		const std::vector<u16> &removed,
		const std::vector<ActiveObjectRef> &added)
{
	NetworkPacket pkt(TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD,
		2 * removed.size() + 32 * added.size(), peer_id);
	pkt << (u16)removed.size();
	for (u16 id : removed)
		pkt << id;
	pkt << (u16)added.size();
	for (const auto &a : added) {
		pkt << a.id << a.type;
		pkt.putLongString(a.init_data);
	}
	send(peer_id, pkt);
}

// ---- map edits ----------------------------------------------------

void ConnectionNetServer::sendRemoveNode(const std::vector<session_t> &peers,
		const v3s16 &pos)
{
	// Caller (Server) has already filtered `peers` to active
	// clients in the right state. We just send.
	NetworkPacket pkt(TOCLIENT_REMOVENODE, 6);
	pkt << pos;
	for (session_t peer_id : peers)
		send(peer_id, pkt);
}

void ConnectionNetServer::sendAddNode(const std::vector<session_t> &peers,
		const v3s16 &pos, const MapNode &n,
		u16 type, bool keep_metadata, const std::string &metadata_serialized)
{
	NetworkPacket pkt(TOCLIENT_ADDNODE, 6 + 2 + 1 + 1 + 1);
	pkt << pos << n.param0 << n.param1 << n.param2
		<< (u8)(keep_metadata ? 1 : 0);
	pkt.putLongString(keep_metadata ? metadata_serialized : "");
	for (session_t peer_id : peers)
		send(peer_id, pkt);
}

void ConnectionNetServer::sendNodeMetaChanged(session_t peer_id, u16 type,
		const std::string &metadata_serialized)
{
	NetworkPacket pkt(TOCLIENT_NODEMETA_CHANGED, 0, peer_id);
	pkt.putLongString(metadata_serialized);
	send(peer_id, pkt);
}

// ---- inventory (detached) ----------------------------------------

void ConnectionNetServer::sendDetachedInventory(session_t peer_id,
		const std::string &name, const std::string &inventory_serialized,
		bool update_inventory)
{
	NetworkPacket pkt(TOCLIENT_DETACHED_INVENTORY, 0, peer_id);
	pkt << name;
	pkt << update_inventory;
	if (update_inventory) {
		// Old clients (proto < 52) only know how to read a u16-prefixed
		// raw string for detached inventories; the client side reads the
		// u16 size and then deSerialize()s the remaining bytes.
		pkt << static_cast<u16>(inventory_serialized.size());
		pkt.putRawString(inventory_serialized);
	}
	send(peer_id, pkt);
}

// ---- definitions --------------------------------------------------

void ConnectionNetServer::sendItemDef(session_t peer_id,
		IItemDefManager &itemdef, u16 protocol_version)
{
	std::ostringstream tmp_os2(std::ios::binary);
	{
		std::ostringstream tmp_os(std::ios::binary);
		itemdef.serialize(tmp_os, protocol_version);
		if (protocol_version >= 48)
			compressZstd(tmp_os.str(), tmp_os2);
		else
			compressZlib(tmp_os.str(), tmp_os2);
	}
	NetworkPacket pkt(TOCLIENT_ITEMDEF, 0, peer_id);
	pkt.putLongString(tmp_os2.str());
	verbosestream << "Server: Sending item definitions to id("
		<< peer_id << "): size=" << pkt.getSize() << std::endl;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendNodeDef(session_t peer_id,
		const NodeDefManager &nodedef, u16 protocol_version)
{
	std::ostringstream tmp_os2(std::ios::binary);
	{
		std::ostringstream tmp_os(std::ios::binary);
		nodedef.serialize(tmp_os, protocol_version);
		if (protocol_version >= 48)
			compressZstd(tmp_os.str(), tmp_os2);
		else
			compressZlib(tmp_os.str(), tmp_os2);
	}
	NetworkPacket pkt(TOCLIENT_NODEDEF, 0, peer_id);
	pkt.putLongString(tmp_os2.str());
	verbosestream << "Server: Sending node definitions to id("
		<< peer_id << "): size=" << pkt.getSize() << std::endl;
	send(peer_id, pkt);
}

// ---- map blocks ---------------------------------------------------

void ConnectionNetServer::sendBlockData(session_t peer_id,
		const v3s16 &pos, const std::string &serialized_block)
{
	NetworkPacket pkt(TOCLIENT_BLOCKDATA,
		2 + 2 + 2 + serialized_block.size(), peer_id);
	pkt << pos;
	pkt.putRawString(serialized_block);
	send(peer_id, pkt);
}

// ---- sounds -------------------------------------------------------

void ConnectionNetServer::sendPlaySound(session_t peer_id, s32 handle,
		const std::string &spec_name, f32 gain, u8 type,
		const v3f &pos, u16 object, bool loop, f32 fade,
		f32 pitch, bool ephemeral, f32 start_time)
{
	NetworkPacket pkt(TOCLIENT_PLAY_SOUND, 0);
	pkt << handle << spec_name << gain << type << pos << object
		<< loop << fade << pitch << ephemeral << start_time;
	const bool as_reliable = !ephemeral;
	sendCustom(peer_id, 0, pkt, as_reliable);
}

void ConnectionNetServer::sendStopSound(session_t peer_id, s32 handle)
{
	NetworkPacket pkt(TOCLIENT_STOP_SOUND, 4);
	pkt << handle;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendFadeSound(session_t peer_id, s32 handle,
		f32 step, f32 gain)
{
	NetworkPacket pkt(TOCLIENT_FADE_SOUND, 4);
	pkt << handle << step << gain;
	send(peer_id, pkt);
}

// ---- media --------------------------------------------------------

void ConnectionNetServer::sendMediaAnnouncement(session_t peer_id,
		const std::vector<std::string> &filenames,
		const std::vector<std::string> &sha1_hashes,
		const std::string &remote_media,
		u16 protocol_version)
{
	FATAL_ERROR_IF(filenames.size() != sha1_hashes.size(),
		"sendMediaAnnouncement: filenames/sha1 size mismatch");
	NetworkPacket pkt(TOCLIENT_ANNOUNCE_MEDIA, 0, peer_id);

	if (protocol_version < 48) {
		pkt << (u16)filenames.size();
		for (size_t i = 0; i < filenames.size(); ++i) {
			pkt << filenames[i];
			pkt << base64_encode(sha1_hashes[i]);
		}
	} else {
		// Compressed table of media names (legacy field,
		// still required by old clients to know what to ask for).
		std::ostringstream oss(std::ios::binary);
		compressZstd(serializeString16Array(filenames), oss);
		pkt.putLongString(oss.str());
		// then the raw 20-byte SHA1 for each file
		for (const auto &hash : sha1_hashes) {
			assert(hash.size() == 20);
			pkt.putRawString(hash);
		}
	}

	// and the remote media server(s)
	pkt << remote_media;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendMedia(session_t peer_id,
		const std::string &filename, std::string_view data)
{
	NetworkPacket pkt(TOCLIENT_MEDIA, 4, peer_id);
	pkt << filename;
	pkt.putRawString(data);
	send(peer_id, pkt);
}

void ConnectionNetServer::sendMediaBunch(session_t peer_id,
		u16 num_bunches, u16 bunch_index,
		const std::vector<std::pair<std::string_view, std::string_view>> &files)
{
	NetworkPacket pkt(TOCLIENT_MEDIA, 4 + 0, peer_id);
	pkt << num_bunches << bunch_index << (u32)files.size();
	for (const auto &file : files) {
		pkt << file.first;
		pkt.putLongString(file.second);
	}
	send(peer_id, pkt);
}

void ConnectionNetServer::sendMediaPush(const std::vector<session_t> &peers,
		const std::string &raw_hash,
		const std::string &filename,
		bool cached, u32 token)
{
	NetworkPacket pkt(TOCLIENT_MEDIA_PUSH, 0, PEER_ID_INEXISTENT);
	pkt << raw_hash << filename << cached << token;
	for (session_t peer_id : peers)
		send(peer_id, pkt);
}

// ---- minimap ------------------------------------------------------

void ConnectionNetServer::sendMinimapModes(session_t peer_id,
		const std::vector<MinimapMode> &modes, size_t wanted_mode)
{
	NetworkPacket pkt(TOCLIENT_MINIMAP_MODES, 0, peer_id);
	pkt << (u16)modes.size() << (u16)wanted_mode;
	for (const auto &mode : modes)
		pkt << (u16)mode.type << mode.label << mode.size
			<< mode.texture << mode.scale;
	send(peer_id, pkt);
}

// ---- protocol / connection control --------------------------------

void ConnectionNetServer::sendDenySudoMode(session_t peer_id)
{
	NetworkPacket pkt(TOCLIENT_DENY_SUDO_MODE, 0, peer_id);
	send(peer_id, pkt);
}

// ---- auth handshake -----------------------------------------------
//
// Wire formats (see also `clientpackethandler.cpp`):
//
// TOCLIENT_HELLO:
//   u8  serialization_ver
//   u16 unused (always 0)
//   u16 net_proto_version
//   u32 auth_mechs
//   string (u16 len, always empty)  -- reserved
//
// TOCLIENT_AUTH_ACCEPT:
//   v3f  spawn_pos       (always 0,0,0 on the wire for the
//                         initial login; reserved)
//   u64  map_seed
//   f32  dedicated_server_step
//   u32  allowed_auth_mechs
//
// TOCLIENT_ACCEPT_SUDO_MODE:
//   u32 sudo_auth_mechs
//
// TOCLIENT_SRP_BYTES_S_B:
//   string (u16 len) salt
//   string (u16 len) bytes_B   (raw, 256 bytes for SRP-6a)

void ConnectionNetServer::sendHello(session_t peer_id,
		u8 serialization_ver,
		u16 net_proto_version,
		u32 auth_mechs)
{
	NetworkPacket pkt(TOCLIENT_HELLO, 0, peer_id);
	pkt << serialization_ver << u16(0) /* unused */
		<< net_proto_version
		<< auth_mechs << std::string_view() /* unused */;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendAuthAccept(session_t peer_id,
		u64 map_seed,
		float dedicated_server_step,
		u32 allowed_auth_mechs)
{
	NetworkPacket pkt(TOCLIENT_AUTH_ACCEPT, 0, peer_id);
	pkt << v3f() << map_seed
		<< dedicated_server_step
		<< allowed_auth_mechs;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendAcceptSudoMode(session_t peer_id,
		u32 sudo_auth_mechs)
{
	NetworkPacket pkt(TOCLIENT_ACCEPT_SUDO_MODE, 0, peer_id);
	pkt << sudo_auth_mechs;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendSrpBytesSandB(session_t peer_id,
		const std::string &salt,
		const char *bytes_B, size_t len_B)
{
	// Wire format (see clientpackethandler.cpp):
	//   u16 salt_len + salt bytes
	//   u16 B_len    + B bytes          (typically 256 for SRP-6a)
	NetworkPacket pkt(TOCLIENT_SRP_BYTES_S_B, 0, peer_id);
	pkt << salt << std::string(bytes_B, len_B);
	send(peer_id, pkt);
}

// ---- mod channels ------------------------------------------------
//
// TOCLIENT_MODCHANNEL_SIGNAL:
//   u8  signal (ModChannelSignal)
//   string (u16 len) channel

void ConnectionNetServer::sendModChannelSignal(session_t peer_id,
		ModChannelSignal signal, const std::string &channel)
{
	NetworkPacket pkt(TOCLIENT_MODCHANNEL_SIGNAL, 0, peer_id);
	pkt << (u8)signal << channel;
	send(peer_id, pkt);
}

void ConnectionNetServer::sendModChannelMsg(session_t peer_id,
		const std::string &channel,
		const std::string &sender,
		const std::string &message)
{
	NetworkPacket pkt(TOCLIENT_MODCHANNEL_MSG, 0, peer_id);
	pkt << channel << sender << message;
	send(peer_id, pkt);
}

} // namespace server
