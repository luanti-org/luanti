// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013-2020 Minetest core developers & community

#pragma once

#include "object_properties.h"
#include "serveractiveobject.h"
#include <quaternion.h>
#include "util/numeric.h"

class UnitSAO : public ServerActiveObject
{
public:
	UnitSAO(ServerEnvironment *env, v3f pos);
	virtual ~UnitSAO() = default;

	u16 getHP() const override { return m_hp; }
	// Use a function, if isDead can be defined by other conditions
	bool isDead() const { return m_hp == 0; }

	// Rotation
	void setRotation(v3f rotation) { m_rotation = rotation; }
	const v3f &getRotation() const { return m_rotation; }
	const v3f getTotalRotation() const {
		// This replicates what happens clientside serverside
		core::matrix4 rot;
		setPitchYawRoll(rot, -m_rotation);
		v3f res;
		// First rotate by m_rotation, then rotate by the automatic rotate yaw
		(core::quaternion(v3f(0, -m_rotation_add_yaw * core::DEGTORAD, 0))
				* core::quaternion(rot.getRotationRadians()))
				.toEuler(res);
		return res * core::RADTODEG;
	}
	v3f getRadRotation() { return m_rotation * core::DEGTORAD; }

	// Deprecated
	f32 getRadYawDep() const { return (m_rotation.Y + 90.) * core::DEGTORAD; }

	// Armor groups
	inline bool isImmortal() const
	{
		return itemgroup_get(getArmorGroups(), "immortal");
	}
	void setArmorGroups(const ItemGroupList &armor_groups) override;
	const ItemGroupList &getArmorGroups() const override;

	// Animation
	void setAnimation(v2f frame_range, float frame_speed, float frame_blend,
			bool frame_loop) override;
	void getAnimation(v2f *frame_range, float *frame_speed, float *frame_blend,
			bool *frame_loop) override;
	void setAnimationSpeed(float frame_speed) override;

	// Bone position
	void setBoneOverride(const std::string &bone, const BoneOverride &props) override;
	BoneOverride getBoneOverride(const std::string &bone) override;
	const std::unordered_map<std::string, BoneOverride>
			&getBoneOverrides() const override { return m_bone_override; };

	// Attachments
	ServerActiveObject *getParent() const override;
	inline bool isAttached() const { return m_attachment_parent_id != 0; }
	void setAttachment(object_t parent_id, const std::string &bone, v3f position,
			v3f rotation, bool force_visible) override;
	void getAttachment(object_t *parent_id, std::string *bone, v3f *position,
			v3f *rotation, bool *force_visible) const override;
	void clearChildAttachments() override;
	void addAttachmentChild(object_t child_id) override;
	void removeAttachmentChild(object_t child_id) override;
	const std::unordered_set<object_t> &getAttachmentChildIds() const override {
		return m_attachment_child_ids;
	}

	// Object properties
	ObjectProperties *accessObjectProperties() override;
	void notifyObjectPropertiesModified() override;
	void sendOutdatedData();

	// Update packets
	std::string generateUpdateAttachmentCommand() const;
	std::string generateUpdateAnimationSpeedCommand() const;
	std::string generateUpdateAnimationCommand() const;
	std::string generateUpdateArmorGroupsCommand() const;
	static std::string generateUpdatePositionCommand(const v3f &position,
			const v3f &velocity, const v3f &acceleration, const v3f &rotation,
			bool do_interpolate, bool is_movement_end, f32 update_interval);
	std::string generateSetPropertiesCommand(const ObjectProperties &prop) const;
	static std::string generateUpdateBoneOverrideCommand(
			const std::string &bone, const BoneOverride &props);
	void sendPunchCommand();

protected:
	u16 m_hp = 1;

	v3f m_rotation;
	f32 m_rotation_add_yaw = 0;

	ItemGroupList m_armor_groups;

	// Object properties
	bool m_properties_sent = true;
	ObjectProperties m_prop;

	// Stores position and rotation for each bone name
	std::unordered_map<std::string, BoneOverride> m_bone_override;

	object_t m_attachment_parent_id = 0;

	void clearAnyAttachments();
	virtual void onMarkedForDeactivation() override {
		ServerActiveObject::onMarkedForDeactivation();
		clearAnyAttachments();
	}
	virtual void onMarkedForRemoval() override {
		ServerActiveObject::onMarkedForRemoval();
		clearAnyAttachments();
	}

private:
	void onAttach(ServerActiveObject *parent);
	void onDetach(ServerActiveObject *parent);

	std::string generatePunchCommand(u16 result_hp) const;

	// Used to detect nested calls to setAttachments(), which can happen due to
	// Lua callbacks
	u8 m_attachment_call_counter = 0;

	// Armor groups
	bool m_armor_groups_sent = false;

	// Animation
	v2f m_animation_range;
	float m_animation_speed = 0.0f;
	float m_animation_blend = 0.0f;
	bool m_animation_loop = true;
	bool m_animation_sent = false;
	bool m_animation_speed_sent = false;

	// Bone positions
	bool m_bone_override_sent = false;

	// Attachments
	std::unordered_set<object_t> m_attachment_child_ids;
	std::string m_attachment_bone = "";
	v3f m_attachment_position;
	v3f m_attachment_rotation;
	bool m_attachment_sent = false;
	bool m_force_visible = false;
};
