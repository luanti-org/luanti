// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2020 Minetest core developers & community

#include "player_sao.h"
#include "nodedef.h"
#include "remoteplayer.h"
#include "scripting_server.h"
#include "server.h"
#include "serverenvironment.h"

PlayerSAO::PlayerSAO(ServerEnvironment *env_, RemotePlayer *player_, session_t peer_id_,
		bool is_singleplayer):
	UnitSAO(env_, v3f(0,0,0)),
	m_player(player_),
	m_player_name(player_->getName()),
	m_peer_id_initial(peer_id_),
	m_is_singleplayer(is_singleplayer)
{
	SANITY_CHECK(m_peer_id_initial != PEER_ID_INEXISTENT);

	m_prop.hp_max = PLAYER_MAX_HP_DEFAULT;
	m_prop.breath_max = PLAYER_MAX_BREATH_DEFAULT;
	m_prop.physical = false;
	m_prop.collisionbox = aabb3f(-0.3f, 0.0f, -0.3f, 0.3f, 1.77f, 0.3f);
	m_prop.selectionbox = aabb3f(-0.3f, 0.0f, -0.3f, 0.3f, 1.77f, 0.3f);
	m_prop.pointable = PointabilityType::POINTABLE;
	// Start of default appearance, this should be overwritten by Lua
	m_prop.visual = OBJECTVISUAL_UPRIGHT_SPRITE;
	m_prop.visual_size = v3f(1, 2, 1);
	m_prop.textures.clear();
	m_prop.textures.emplace_back("player.png");
	m_prop.textures.emplace_back("player_back.png");
	m_prop.colors.clear();
	m_prop.spritediv = v2s16(1,1);
	m_prop.eye_height = 1.625f;
	// End of default appearance
	m_prop.is_visible = true;
	m_prop.backface_culling = false;
	m_prop.makes_footstep_sound = true;
	m_prop.stepheight = PLAYER_DEFAULT_STEPHEIGHT * BS;
	m_prop.show_on_minimap = true;
	m_hp = m_prop.hp_max;
	m_breath = m_prop.breath_max;
	// Disable zoom in survival mode using a value of 0
	m_prop.zoom_fov = g_settings->getBool("creative_mode") ? 15.0f : 0.0f;

	if (!g_settings->getBool("enable_damage"))
		m_armor_groups["immortal"] = 1;
}

void PlayerSAO::finalize(RemotePlayer *player, const std::set<std::string> &privs)
{
	assert(player);
	m_player = player;
	m_privs = privs;
}

v3f PlayerSAO::getEyeOffset() const
{
	return v3f(0, BS * m_prop.eye_height, 0);
}

std::string PlayerSAO::getDescription()
{
	return std::string("player ") + m_player->getName();
}

// Called after id has been set and has been inserted in environment
void PlayerSAO::addedToEnvironment(u32 dtime_s)
{
	ServerActiveObject::addedToEnvironment(dtime_s);
	m_player->setPlayerSAO(this);
	m_player->setPeerId(m_peer_id_initial);
	m_peer_id_initial = PEER_ID_INEXISTENT; // don't try to use it again.
	m_last_good_position = getBasePosition();
}

// Called before removing from environment
void PlayerSAO::removingFromEnvironment()
{
	ServerActiveObject::removingFromEnvironment();

	// If this fails, fix the ActiveObjectMgr code in ServerEnvironment
	SANITY_CHECK(m_player->getPlayerSAO() == this);

	unlinkPlayerSessionAndSave();
	for (u32 attached_particle_spawner : m_attached_particle_spawners) {
		m_env->deleteParticleSpawner(attached_particle_spawner, false);
	}
}

std::string PlayerSAO::getClientInitializationData(u16 protocol_version)
{
	std::ostringstream os(std::ios::binary);

	// Protocol >= 15
	writeU8(os, 1); // version
	os << serializeString16(m_player->getName()); // name
	writeU8(os, 1); // is_player
	writeS16(os, getId()); // id
	writeV3F32(os, getBasePosition());
	writeV3F32(os, m_rotation);
	writeU16(os, getHP());

	std::ostringstream msg_os(std::ios::binary);
	msg_os << serializeString32(getPropertyPacket()); // message 1
	msg_os << serializeString32(generateUpdateArmorGroupsCommand()); // 2
	msg_os << serializeString32(generateUpdateAnimationCommand()); // 3
	for (const auto &it : m_bone_override) {
		msg_os << serializeString32(generateUpdateBoneOverrideCommand(
			it.first, it.second)); // 3 + N
	}
	msg_os << serializeString32(generateUpdateAttachmentCommand()); // 4 + m_bone_override.size
	msg_os << serializeString32(generateUpdatePhysicsOverrideCommand()); // 5 + m_bone_override.size

	int message_count = 5 + m_bone_override.size();

	for (const auto &id : getAttachmentChildIds()) {
		if (ServerActiveObject *obj = m_env->getActiveObject(id)) {
			message_count++;
			msg_os << serializeString32(obj->generateUpdateInfantCommand(
				id, protocol_version));
		}
	}

	writeU8(os, message_count);
	std::string serialized = msg_os.str();
	os.write(serialized.c_str(), serialized.size());

	// return result
	return os.str();
}

void PlayerSAO::getStaticData(std::string * result) const
{
	FATAL_ERROR("This function shall not be called for PlayerSAO");
}

void PlayerSAO::step(float dtime, bool send_recommended)
{
	bool not_immortal = !isImmortal();

	if (not_immortal && m_flags.drowning
			&& m_drowning_interval.step(dtime, 2.0f)) {
		// Get nose/mouth position, approximate with eye position
		v3s16 p = floatToInt(getEyePosition(), BS);
		MapNode n = m_env->getMap().getNode(p);
		const ContentFeatures &c = m_env->getGameDef()->ndef()->get(n);
		// If node generates drown
		if (c.drowning > 0 && m_hp > 0) {
			if (m_breath > 0)
				setBreath(m_breath - 1);

			// No more breath, damage player
			if (m_breath == 0) {
				PlayerHPChangeReason reason(PlayerHPChangeReason::DROWNING);
				setHP(m_hp - c.drowning, reason);
			}
		}
	}

	if (not_immortal && m_flags.breathing
			&& m_breathing_interval.step(dtime, 0.5f)) {
		// Get nose/mouth position, approximate with eye position
		v3s16 p = floatToInt(getEyePosition(), BS);
		MapNode n = m_env->getMap().getNode(p);
		const ContentFeatures &c = m_env->getGameDef()->ndef()->get(n);
		// If player is alive & not drowning & not in ignore & not immortal, breathe
		if (m_breath < m_prop.breath_max && c.drowning == 0 &&
				n.getContent() != CONTENT_IGNORE && m_hp > 0)
			setBreath(m_breath + 1);
	}

	if (not_immortal && m_flags.node_damage
			&& m_node_hurt_interval.step(dtime, 1.0f)) {
		u32 damage_per_second = 0;
		std::string nodename;
		v3s16 node_pos;
		// Lowest and highest damage points are 0.1 within collisionbox
		float dam_top = m_prop.collisionbox.MaxEdge.Y - 0.1f;

		// Sequence of damage points, starting 0.1 above feet and progressing
		// upwards in 1 node intervals, stopping below top damage point.
		for (float dam_height = 0.1f; dam_height < dam_top; dam_height++) {
			v3s16 p = floatToInt(getBasePosition() +
				v3f(0.0f, dam_height * BS, 0.0f), BS);
			MapNode n = m_env->getMap().getNode(p);
			const ContentFeatures &c = m_env->getGameDef()->ndef()->get(n);
			if (c.damage_per_second > damage_per_second) {
				damage_per_second = c.damage_per_second;
				nodename = c.name;
				node_pos = p;
			}
		}

		// Top damage point
		v3s16 ptop = floatToInt(getBasePosition() +
			v3f(0.0f, dam_top * BS, 0.0f), BS);
		MapNode ntop = m_env->getMap().getNode(ptop);
		const ContentFeatures &c = m_env->getGameDef()->ndef()->get(ntop);
		if (c.damage_per_second > damage_per_second) {
			damage_per_second = c.damage_per_second;
			nodename = c.name;
			node_pos = ptop;
		}

		if (damage_per_second != 0 && m_hp > 0) {
			s32 newhp = (s32)m_hp - (s32)damage_per_second;
			PlayerHPChangeReason reason(PlayerHPChangeReason::NODE_DAMAGE, nodename, node_pos);
			setHP(newhp, reason);
		}
	}

	if (!m_properties_sent) {
		m_properties_sent = true;
		std::string str = getPropertyPacket();
		// create message and add to list
		m_messages_out.emplace(getId(), true, str);
		m_env->getScriptIface()->player_event(this, "properties_changed");
	}

	// If attached, check that our parent is still there. If it isn't, detach.
	if (m_attachment_parent_id && !getParent()) {
		// This is handled when objects are removed from the map
		warningstream << "PlayerSAO::step() id=" << m_id <<
			" is attached to nonexistent parent. This is a bug." << std::endl;
		clearParentAttachment();
		setPos(m_last_good_position);
	}

	//dstream<<"PlayerSAO::step: dtime: "<<dtime<<std::endl;

	// Set lag pool maximums based on estimated lag
	const float LAG_POOL_MIN = 5.0f;
	float lag_pool_max = m_env->getMaxLagEstimate() * 2.0f;
	if(lag_pool_max < LAG_POOL_MIN)
		lag_pool_max = LAG_POOL_MIN;
	m_dig_pool.setMax(lag_pool_max);
	m_move_pool.setMax(lag_pool_max);

	// Increment cheat prevention timers
	m_dig_pool.add(dtime);
	m_move_pool.add(dtime);
	m_time_from_last_teleport += dtime;
	m_time_from_last_punch += dtime;
	m_nocheat_dig_time += dtime;
	m_max_speed_override_time = MYMAX(m_max_speed_override_time - dtime, 0.0f);

	// Each frame, parent position is copied if the object is attached,
	// otherwise it's calculated normally.
	// If the object gets detached this comes into effect automatically from
	// the last known origin.
	if (auto *parent = getParent()) {
		v3f pos = parent->getBasePosition();
		m_last_good_position = pos;
		setBasePosition(pos);

		if (m_player)
			m_player->setSpeed(v3f());
	}

	if (!send_recommended)
		return;

	if (m_position_not_sent) {
		m_position_not_sent = false;
		float update_interval = m_env->getSendRecommendedInterval();
		v3f pos;
		// When attached, the position is only sent to clients where the
		// parent isn't known
		if (isAttached())
			pos = m_last_good_position;
		else
			pos = getBasePosition();

		std::string str = generateUpdatePositionCommand(
			pos,
			v3f(0.0f, 0.0f, 0.0f),
			v3f(0.0f, 0.0f, 0.0f),
			m_rotation,
			true,
			false,
			update_interval
		);
		// create message and add to list
		m_messages_out.emplace(getId(), false, str);
	}

	if (!m_physics_override_sent) {
		m_physics_override_sent = true;
		// create message and add to list
		m_messages_out.emplace(getId(), true, generateUpdatePhysicsOverrideCommand());
	}

	sendOutdatedData();
}

std::string PlayerSAO::generateUpdatePhysicsOverrideCommand() const
{
	if (!m_player) {
		// Will output a format warning client-side
		return "";
	}

	const auto &phys = m_player->physics_override;
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_SET_PHYSICS_OVERRIDE);
	// parameters
	writeF32(os, phys.speed);
	writeF32(os, phys.jump);
	writeF32(os, phys.gravity);
	// MT 0.4.10 legacy: send inverted for detault `true` if the server sends nothing
	writeU8(os, !phys.sneak);
	writeU8(os, !phys.sneak_glitch);
	writeU8(os, !phys.new_move);
	// new physics overrides since 5.8.0
	writeF32(os, phys.speed_climb);
	writeF32(os, phys.speed_crouch);
	writeF32(os, phys.liquid_fluidity);
	writeF32(os, phys.liquid_fluidity_smooth);
	writeF32(os, phys.liquid_sink);
	writeF32(os, phys.acceleration_default);
	writeF32(os, phys.acceleration_air);
	writeF32(os, phys.speed_fast);
	writeF32(os, phys.acceleration_fast);
	writeF32(os, phys.speed_walk);
	return os.str();
}

void PlayerSAO::setBasePosition(v3f position)
{
	if (m_player && position != getBasePosition())
		m_player->setDirty(true);

	// This needs to be ran for attachments too
	ServerActiveObject::setBasePosition(position);

	// Updating is not wanted/required for player migration
	if (m_env) {
		m_position_not_sent = true;
	}
}

void PlayerSAO::setPos(const v3f &pos)
{
	if (isAttached())
		return;

	// Send mapblock of target location
	v3s16 blockpos = v3s16(pos.X / MAP_BLOCKSIZE, pos.Y / MAP_BLOCKSIZE, pos.Z / MAP_BLOCKSIZE);
	m_env->getGameDef()->SendBlock(getPeerID(), blockpos);

	setBasePosition(pos);
	// Movement caused by this command is always valid
	m_last_good_position = getBasePosition();
	m_move_pool.empty();
	m_time_from_last_teleport = 0.0;
	m_env->getGameDef()->SendMovePlayer(this);
}

void PlayerSAO::addPos(const v3f &added_pos)
{
	if (isAttached())
		return;

	// Backward compatibility for older clients
	if (m_player->protocol_version < 44) {
		setPos(getBasePosition() + added_pos);
		return;
	}

	// Send mapblock of target location
	v3f pos = getBasePosition() + added_pos;
	v3s16 blockpos = v3s16(pos.X / MAP_BLOCKSIZE, pos.Y / MAP_BLOCKSIZE, pos.Z / MAP_BLOCKSIZE);
	m_env->getGameDef()->SendBlock(getPeerID(), blockpos);

	setBasePosition(pos);
	// Movement caused by this command is always valid
	m_last_good_position = getBasePosition();
	m_move_pool.empty();
	m_time_from_last_teleport = 0.0;
	m_env->getGameDef()->SendMovePlayerRel(getPeerID(), added_pos);
}

void PlayerSAO::moveTo(v3f pos, bool continuous)
{
	if(isAttached())
		return;

	setBasePosition(pos);
	// Movement caused by this command is always valid
	m_last_good_position = getBasePosition();
	m_move_pool.empty();
	m_time_from_last_teleport = 0.0;
	m_env->getGameDef()->SendMovePlayer(this);
}

void PlayerSAO::setPlayerYaw(const float yaw)
{
	v3f rotation(0, yaw, 0);
	if (m_player && yaw != m_rotation.Y)
		m_player->setDirty(true);

	// Set player model yaw, not look view
	UnitSAO::setRotation(rotation);
}

void PlayerSAO::setFov(const float fov)
{
	if (m_player && fov != m_fov)
		m_player->setDirty(true);

	m_fov = fov;
}

void PlayerSAO::setWantedRange(const s16 range)
{
	if (m_player && range != m_wanted_range)
		m_player->setDirty(true);

	m_wanted_range = range;
}

void PlayerSAO::setPlayerYawAndSend(const float yaw)
{
	setPlayerYaw(yaw);
	m_env->getGameDef()->SendMovePlayer(this);
}

void PlayerSAO::setLookPitch(const float pitch)
{
	if (m_player && pitch != m_pitch)
		m_player->setDirty(true);

	m_pitch = pitch;
}

void PlayerSAO::setLookPitchAndSend(const float pitch)
{
	setLookPitch(pitch);
	m_env->getGameDef()->SendMovePlayer(this);
}

u32 PlayerSAO::punch(v3f dir,
	const ToolCapabilities *toolcap,
	ServerActiveObject *puncher,
	float time_from_last_punch,
	u16 initial_wear)
{
	if (!toolcap)
		return 0;

	// No effect if PvP disabled or if immortal
	if (isImmortal() || !g_settings->getBool("enable_pvp")) {
		if (puncher && puncher->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			// create message and add to list
			sendPunchCommand();
			return 0;
		}
	}

	s32 old_hp = getHP();
	HitParams hitparams = getHitParams(m_armor_groups, toolcap,
			time_from_last_punch, initial_wear);

	PlayerSAO *playersao = m_player->getPlayerSAO();

	bool damage_handled = m_env->getScriptIface()->on_punchplayer(playersao,
				puncher, time_from_last_punch, toolcap, dir,
				hitparams.hp);

	if (!damage_handled) {
		setHP((s32)getHP() - (s32)hitparams.hp,
				PlayerHPChangeReason(PlayerHPChangeReason::PLAYER_PUNCH, puncher));
	} else { // override client prediction
		if (puncher->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			// create message and add to list
			sendPunchCommand();
		}
	}

	if (puncher) {
		actionstream << puncher->getDescription() << " (id=" << puncher->getId() <<
				", hp=" << puncher->getHP() << ")";
	} else {
		actionstream << "(none)";
	}
	actionstream << " punched " <<
			getDescription() << " (id=" << m_id << ", hp=" << m_hp <<
			"), damage=" << (old_hp - (s32)getHP()) <<
			(damage_handled ? " (handled by Lua)" : "") << std::endl;

	return hitparams.wear;
}

void PlayerSAO::rightClick(ServerActiveObject *clicker)
{
	m_env->getScriptIface()->on_rightclickplayer(this, clicker);
}

void PlayerSAO::setHP(s32 target_hp, const PlayerHPChangeReason &reason, bool from_client)
{
	if (target_hp == m_hp || (m_hp == 0 && target_hp < 0))
		return; // Nothing to do

	// Protect against overflow.
	s32 hp_change = std::max<s64>((s64)target_hp - (s64)m_hp, S32_MIN);

	hp_change = m_env->getScriptIface()->on_player_hpchange(this, hp_change, reason);
	hp_change = std::min<s32>(hp_change, U16_MAX); // Protect against overflow

	s32 hp = (s32)m_hp + hp_change;
	hp = rangelim(hp, 0, U16_MAX);

	if (hp > m_prop.hp_max)
		hp = m_prop.hp_max;

	if (hp < m_hp && isImmortal())
		hp = m_hp; // Do not allow immortal players to be damaged

	// Update properties on death
	if ((hp == 0) != (m_hp == 0))
		m_properties_sent = false;

	if (hp != m_hp) {
		m_hp = hp;
		m_env->getGameDef()->HandlePlayerHPChange(this, reason);
	} else if (from_client)
		m_env->getGameDef()->SendPlayerHP(this, true);
}

void PlayerSAO::setBreath(const u16 breath, bool send)
{
	if (m_player && breath != m_breath)
		m_player->setDirty(true);

	m_breath = rangelim(breath, 0, m_prop.breath_max);

	if (send)
		m_env->getGameDef()->SendPlayerBreath(this);
}

void PlayerSAO::respawn()
{
	infostream << "PlayerSAO::respawn(): Player " << m_player->getName()
			<< " respawns" << std::endl;

	setHP(m_prop.hp_max, PlayerHPChangeReason(PlayerHPChangeReason::RESPAWN));
	setBreath(m_prop.breath_max);

	bool repositioned = m_env->getScriptIface()->on_respawnplayer(this);
	if (!repositioned) {
		// setPos will send the new position to client
		setPos(m_env->getGameDef()->findSpawnPos());
	}
}

Inventory *PlayerSAO::getInventory() const
{
	return m_player ? &m_player->inventory : nullptr;
}

InventoryLocation PlayerSAO::getInventoryLocation() const
{
	InventoryLocation loc;
	loc.setPlayer(m_player->getName());
	return loc;
}

u16 PlayerSAO::getWieldIndex() const
{
	return m_player->getWieldIndex();
}

ItemStack PlayerSAO::getWieldedItem(ItemStack *selected, ItemStack *hand) const
{
	return m_player->getWieldedItem(selected, hand);
}

bool PlayerSAO::setWieldedItem(const ItemStack &item)
{
	InventoryList *mlist = m_player->inventory.getList(getWieldList());
	if (mlist) {
		mlist->changeItem(m_player->getWieldIndex(), item);
		return true;
	}
	return false;
}

void PlayerSAO::disconnected()
{
	markForRemoval();
	m_player->setPeerId(PEER_ID_INEXISTENT);
}

session_t PlayerSAO::getPeerID() const
{
	// Before adding `this` to the server env, m_player is still nullptr.
	return m_player ? m_player->getPeerId() : PEER_ID_INEXISTENT;
}

void PlayerSAO::unlinkPlayerSessionAndSave()
{
	assert(m_player->getPlayerSAO() == this);
	m_env->savePlayer(m_player);
	m_env->removePlayer(m_player);
}

std::string PlayerSAO::getPropertyPacket()
{
	m_prop.is_visible = (true);
	return generateSetPropertiesCommand(m_prop);
}

void PlayerSAO::setMaxSpeedOverride(const v3f &vel)
{
	if (m_max_speed_override_time == 0.0f)
		m_max_speed_override = vel;
	else
		m_max_speed_override += vel;
	if (m_player) {
		float accel = MYMIN(m_player->movement_acceleration_default,
				m_player->movement_acceleration_air);
		m_max_speed_override_time = m_max_speed_override.getLength() / accel / BS;
	}
}

bool PlayerSAO::checkMovementCheat()
{
	static thread_local const u32 anticheat_flags =
		g_settings->getFlagStr("anticheat_flags", flagdesc_anticheat, nullptr);

	if (m_is_singleplayer ||
			isAttached() ||
			!(anticheat_flags & AC_MOVEMENT)) {
		m_last_good_position = getBasePosition();
		return false;
	}

	bool cheated = false;
	/*
		Check player movements

		NOTE: Actually the server should handle player physics like the
		client does and compare player's position to what is calculated
		on our side. This is required when eg. players fly due to an
		explosion. Although a node-based alternative might be possible
		too, and much more lightweight.
	*/

	float override_max_H, override_max_V;
	if (m_max_speed_override_time > 0.0f) {
		override_max_H = MYMAX(fabs(m_max_speed_override.X), fabs(m_max_speed_override.Z));
		override_max_V = fabs(m_max_speed_override.Y);
	} else {
		override_max_H = override_max_V = 0.0f;
	}

	float player_max_walk = 0; // horizontal movement
	float player_max_jump = 0; // vertical upwards movement

	float speed_walk = m_player->movement_speed_walk * m_player->physics_override.speed_walk;
	float speed_fast = m_player->movement_speed_fast * m_player->physics_override.speed_fast;
	float speed_crouch = m_player->movement_speed_crouch * m_player->physics_override.speed_crouch;
	float speed_climb = m_player->movement_speed_climb * m_player->physics_override.speed_climb;

	speed_walk *= m_player->physics_override.speed;
	speed_fast *= m_player->physics_override.speed;
	speed_crouch *= m_player->physics_override.speed;
	speed_climb *= m_player->physics_override.speed;

	// Get permissible max. speed
	if (m_privs.count("fast") != 0) {
		// Fast priv: Get the highest speed of fast, walk or crouch
		// (it is not forbidden the 'fast' speed is
		// not actually the fastest)
		player_max_walk = MYMAX(speed_crouch, speed_fast);
		player_max_walk = MYMAX(player_max_walk, speed_walk);
	} else {
		// Get the highest speed of walk or crouch
		// (it is not forbidden the 'walk' speed is
		// lower than the crouch speed)
		player_max_walk = MYMAX(speed_crouch, speed_walk);
	}

	player_max_walk = MYMAX(player_max_walk, override_max_H);

	player_max_jump = m_player->movement_speed_jump * m_player->physics_override.jump;
	// FIXME: Bouncy nodes cause practically unbound increase in Y speed,
	//        until this can be verified correctly, tolerate higher jumping speeds
	player_max_jump *= 2.0;
	player_max_jump = MYMAX(player_max_jump, speed_climb);
	player_max_jump = MYMAX(player_max_jump, override_max_V);

	// Don't divide by zero!
	if (player_max_walk < 0.0001f)
		player_max_walk = 0.0001f;
	if (player_max_jump < 0.0001f)
		player_max_jump = 0.0001f;

	v3f diff = (getBasePosition() - m_last_good_position);
	float d_vert = diff.Y;
	diff.Y = 0;
	float d_horiz = diff.getLength();
	float required_time = d_horiz / player_max_walk;

	// FIXME: Checking downwards movement is not easily possible currently,
	//        the server could calculate speed differences to examine the gravity
	if (d_vert > 0) {
		// In certain cases (swimming, climbing, flying) walking speed is applied
		// vertically
		float s = MYMAX(player_max_jump, player_max_walk);
		required_time = MYMAX(required_time, d_vert / s);
	}

	static thread_local float anticheat_movement_tolerance =
		std::max(g_settings->getFloat("anticheat_movement_tolerance"), 1.0f);

	required_time /= anticheat_movement_tolerance;

	if (m_move_pool.grab(required_time)) {
		m_last_good_position = getBasePosition();
	} else {
		const float LAG_POOL_MIN = 5.0;
		float lag_pool_max = m_env->getMaxLagEstimate() * 2.0;
		lag_pool_max = MYMAX(lag_pool_max, LAG_POOL_MIN);
		if (m_time_from_last_teleport > lag_pool_max) {
			actionstream << "Server: " << m_player->getName()
					<< " moved too fast: V=" << d_vert << ", H=" << d_horiz
					<< "; resetting position." << std::endl;
			cheated = true;
		}
		setBasePosition(m_last_good_position);
	}
	return cheated;
}

bool PlayerSAO::getCollisionBox(aabb3f *toset) const
{
	//update collision box
	toset->MinEdge = m_prop.collisionbox.MinEdge * BS;
	toset->MaxEdge = m_prop.collisionbox.MaxEdge * BS;

	toset->MinEdge += getBasePosition();
	toset->MaxEdge += getBasePosition();
	return true;
}

bool PlayerSAO::getSelectionBox(aabb3f *toset) const
{
	if (!m_prop.is_visible) {
		return false;
	}

	toset->MinEdge = m_prop.selectionbox.MinEdge * BS;
	toset->MaxEdge = m_prop.selectionbox.MaxEdge * BS;

	return true;
}

float PlayerSAO::getZoomFOV() const
{
	return m_prop.zoom_fov;
}
