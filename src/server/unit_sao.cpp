// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2020 Minetest core developers & community

#include "unit_sao.h"
#include "scripting_server.h"
#include "serverenvironment.h"
#include "util/serialize.h"

UnitSAO::UnitSAO(ServerEnvironment *env, v3f pos) : ServerActiveObject(env, pos)
{
	// Initialize something to armor groups
	m_armor_groups["fleshy"] = 100;
}

ServerActiveObject *UnitSAO::getParent() const
{
	if (!m_attachment_parent_id)
		return nullptr;
	// Check if the parent still exists
	ServerActiveObject *obj = m_env->getActiveObject(m_attachment_parent_id);

	return obj;
}

void UnitSAO::setArmorGroups(const ItemGroupList &armor_groups)
{
	if (m_armor_groups == armor_groups)
		return;
	m_armor_groups = armor_groups;
	m_armor_groups_sent = false;
}

const ItemGroupList &UnitSAO::getArmorGroups() const
{
	return m_armor_groups;
}

void UnitSAO::setAnimation(
		v2f frame_range, float frame_speed, float frame_blend, bool frame_loop)
{
	// Note: Always resend (even if parameters are unchanged) to restart animations.
	m_animation_range = frame_range;
	m_animation_speed = frame_speed;
	m_animation_blend = frame_blend;
	m_animation_loop = frame_loop;
	m_animation_sent = false;
}

void UnitSAO::getAnimation(v2f *frame_range, float *frame_speed, float *frame_blend,
		bool *frame_loop)
{
	*frame_range = m_animation_range;
	*frame_speed = m_animation_speed;
	*frame_blend = m_animation_blend;
	*frame_loop = m_animation_loop;
}

void UnitSAO::setAnimationSpeed(float frame_speed)
{
	if (m_animation_speed == frame_speed)
		return;
	m_animation_speed = frame_speed;
	m_animation_speed_sent = false;
}

void UnitSAO::setBoneOverride(const std::string &bone, const BoneOverride &props)
{
	// store these so they can be updated to clients
	m_bone_override[bone] = props;
	m_bone_override_sent = false;
}

BoneOverride UnitSAO::getBoneOverride(const std::string &bone)
{
	auto it = m_bone_override.find(bone);
	BoneOverride props;
	if (it != m_bone_override.end())
		props = it->second;
	return props;
}

void UnitSAO::sendOutdatedData()
{
	if (!m_armor_groups_sent) {
		m_armor_groups_sent = true;
		m_messages_out.emplace(getId(), true, generateUpdateArmorGroupsCommand());
	}

	if (!m_animation_sent) {
		m_animation_sent = true;
		m_animation_speed_sent = true;
		m_messages_out.emplace(getId(), true, generateUpdateAnimationCommand());
	} else if (!m_animation_speed_sent) {
		// Animation speed is also sent when 'm_animation_sent == false'
		m_animation_speed_sent = true;
		m_messages_out.emplace(getId(), true, generateUpdateAnimationSpeedCommand());
	}

	if (!m_bone_override_sent) {
		m_bone_override_sent = true;
		for (const auto &bone_pos : m_bone_override) {
			m_messages_out.emplace(getId(), true, generateUpdateBoneOverrideCommand(
				bone_pos.first, bone_pos.second));
		}
	}

	if (!m_attachment_sent) {
		m_attachment_sent = true;
		m_messages_out.emplace(getId(), true, generateUpdateAttachmentCommand());
	}
}

void UnitSAO::setAttachment(const object_t new_parent, const std::string &bone, v3f position,
		v3f rotation, bool force_visible)
{
	const auto call_count = ++m_attachment_call_counter;

	const auto check_nesting = [&] (const char *descr) -> bool {
		// The counter is never decremented, so if it differs that means
		// a nested call to setAttachment() has happened.
		if (m_attachment_call_counter == call_count)
			return false;
		verbosestream << "UnitSAO::setAttachment() id=" << m_id <<
			" nested call detected (" << descr << ")." << std::endl;
		return true;
	};

	// Do checks to avoid circular references
	// See similar check in `GenericCAO::setAttachment` (but with different types).
	{
		auto *obj = m_env->getActiveObject(new_parent);
		if (obj == this) {
			assert(false);
			return;
		}
		bool problem = false;
		if (obj) {
			// The chain of wanted parent must not refer or contain "this"
			for (obj = obj->getParent(); obj; obj = obj->getParent()) {
				if (obj == this) {
					problem = true;
					break;
				}
			}
		}
		if (problem) {
			warningstream << "Mod bug: "
				<< "Attempted to attach object " << m_id << " to parent "
				<< new_parent << " but former is an (in)direct parent of latter." << std::endl;
			return;
		}
	}

	// Detach first
	// Note: make sure to apply data changes before running callbacks.
	const auto old_parent = m_attachment_parent_id;
	m_attachment_parent_id = 0;
	m_attachment_sent = false;

	if (old_parent && old_parent != new_parent) {
		auto *parent = m_env->getActiveObject(old_parent);
		if (parent) {
			onDetach(parent);
		} else {
			warningstream << "UnitSAO::setAttachment() id=" << m_id <<
				" is attached to nonexistent parent. This is a bug." << std::endl;
			// we can pretend it never happened
		}
	}

	if (check_nesting("onDetach")) {
		// Don't touch anything after the other call has completed.
		return;
	}

	if (isGone())
		return;

	// Now attach to new parent
	m_attachment_parent_id = new_parent;
	m_attachment_bone = bone;
	m_attachment_position = position;
	m_attachment_rotation = rotation;
	m_force_visible = force_visible;

	if (new_parent && old_parent != new_parent) {
		auto *parent = m_env->getActiveObject(new_parent);
		if (parent) {
			onAttach(parent);
		} else {
			warningstream << "UnitSAO::setAttachment() id=" << m_id <<
				" tried to attach to nonexistent parent. This is a bug." << std::endl;
			m_attachment_parent_id = 0; // detach
		}
	}

	check_nesting("onAttach");
}

void UnitSAO::getAttachment(object_t *parent_id, std::string *bone, v3f *position,
		v3f *rotation, bool *force_visible) const
{
	*parent_id = m_attachment_parent_id;
	*bone = m_attachment_bone;
	*position = m_attachment_position;
	*rotation = m_attachment_rotation;
	*force_visible = m_force_visible;
}

void UnitSAO::clearAnyAttachments()
{
	// This is called before this SAO is marked for removal/deletion and unlinks
	// any parent or child relationships.
	// This is done at this point and not in ~UnitSAO() so attachments to
	// "phantom objects" don't stay around while we're waiting to be actually deleted.
	// (which can take several server steps)
	clearParentAttachment();
	clearChildAttachments();
}

void UnitSAO::clearChildAttachments()
{
	// Cannot use for-loop here: setAttachment() modifies 'm_attachment_child_ids'!
	while (!m_attachment_child_ids.empty()) {
		const auto child_id = *m_attachment_child_ids.begin();

		if (auto *child = m_env->getActiveObject(child_id)) {
			child->clearParentAttachment();
		} else {
			// should not happen but we need to handle it to prevent an infinite loop
			removeAttachmentChild(child_id);
		}
	}
}

void UnitSAO::addAttachmentChild(object_t child_id)
{
	m_attachment_child_ids.insert(child_id);
}

void UnitSAO::removeAttachmentChild(object_t child_id)
{
	m_attachment_child_ids.erase(child_id);
}

void UnitSAO::onAttach(ServerActiveObject *parent)
{
	assert(parent);

	parent->addAttachmentChild(m_id);

	// Do not try to notify soon gone parent
	if (!parent->isGone()) {
		if (parent->getType() == ACTIVEOBJECT_TYPE_LUAENTITY)
			m_env->getScriptIface()->luaentity_on_attach_child(parent->getId(), this);
	}
}

void UnitSAO::onDetach(ServerActiveObject *parent)
{
	assert(parent);

	parent->removeAttachmentChild(m_id);

	if (getType() == ACTIVEOBJECT_TYPE_LUAENTITY)
		m_env->getScriptIface()->luaentity_on_detach(m_id, parent);

	// callback could affect the parent
	if (parent->isGone())
		return;

	if (parent->getType() == ACTIVEOBJECT_TYPE_LUAENTITY)
		m_env->getScriptIface()->luaentity_on_detach_child(parent->getId(), this);
}

ObjectProperties *UnitSAO::accessObjectProperties()
{
	return &m_prop;
}

void UnitSAO::notifyObjectPropertiesModified()
{
	m_properties_sent = false;
}

std::string UnitSAO::generateUpdateAttachmentCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_ATTACH_TO);
	// parameters
	writeS16(os, m_attachment_parent_id);
	os << serializeString16(m_attachment_bone);
	writeV3F32(os, m_attachment_position);
	writeV3F32(os, m_attachment_rotation);
	writeU8(os, m_force_visible);
	return os.str();
}

std::string UnitSAO::generateUpdateBoneOverrideCommand(
		const std::string &bone, const BoneOverride &props)
{
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_SET_BONE_POSITION);
	// parameters
	os << serializeString16(bone);
	writeV3F32(os, props.position.vector);
	v3f euler_rot;
	props.rotation.next.toEuler(euler_rot);
	writeV3F32(os, euler_rot * core::RADTODEG);
	writeV3F32(os, props.scale.vector);
	writeF32(os, props.position.interp_timer);
	writeF32(os, props.rotation.interp_timer);
	writeF32(os, props.scale.interp_timer);
	writeU8(os, (props.position.absolute & 1) << 0
	          | (props.rotation.absolute & 1) << 1
	          | (props.scale.absolute & 1) << 2);
	return os.str();
}

std::string UnitSAO::generateUpdateAnimationSpeedCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_SET_ANIMATION_SPEED);
	// parameters
	writeF32(os, m_animation_speed);
	return os.str();
}

std::string UnitSAO::generateUpdateAnimationCommand() const
{
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_SET_ANIMATION);
	// parameters
	writeV2F32(os, m_animation_range);
	writeF32(os, m_animation_speed);
	writeF32(os, m_animation_blend);
	// these are sent inverted so we get true when the server sends nothing
	writeU8(os, !m_animation_loop);
	return os.str();
}

std::string UnitSAO::generateUpdateArmorGroupsCommand() const
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, AO_CMD_UPDATE_ARMOR_GROUPS);
	writeU16(os, m_armor_groups.size());
	for (const auto &armor_group : m_armor_groups) {
		os << serializeString16(armor_group.first);
		writeS16(os, armor_group.second);
	}
	return os.str();
}

std::string UnitSAO::generateUpdatePositionCommand(const v3f &position,
		const v3f &velocity, const v3f &acceleration, const v3f &rotation,
		bool do_interpolate, bool is_movement_end, f32 update_interval)
{
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_UPDATE_POSITION);
	// pos
	writeV3F32(os, position);
	// velocity
	writeV3F32(os, velocity);
	// acceleration
	writeV3F32(os, acceleration);
	// rotation
	writeV3F32(os, rotation);
	// do_interpolate
	writeU8(os, do_interpolate);
	// is_end_position (for interpolation)
	writeU8(os, is_movement_end);
	// update_interval (for interpolation)
	writeF32(os, update_interval);
	return os.str();
}

std::string UnitSAO::generateSetPropertiesCommand(const ObjectProperties &prop) const
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, AO_CMD_SET_PROPERTIES);
	prop.serialize(os);
	return os.str();
}

std::string UnitSAO::generatePunchCommand(u16 result_hp) const
{
	std::ostringstream os(std::ios::binary);
	// command
	writeU8(os, AO_CMD_PUNCHED);
	// result_hp
	writeU16(os, result_hp);
	return os.str();
}

void UnitSAO::sendPunchCommand()
{
	m_messages_out.emplace(getId(), true, generatePunchCommand(getHP()));
}
