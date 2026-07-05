// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "inventory.h"
#include "util/basic_macros.h"
#include <memory>
#include <string>
#include <string_view>

#define PLAYERNAME_SIZE 20

#define PLAYERNAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
#define PLAYERNAME_ALLOWED_CHARS_USER_EXPL "'a' to 'z', 'A' to 'Z', '0' to '9', '-', '_'"

bool is_valid_player_name(std::string_view name);

struct PlayerFovSpec
{
	f32 fov;

	// Whether to multiply the client's FOV or to override it
	bool is_multiplier;

	// The time to be take to transition to the new FOV value.
	// Transition is instantaneous if omitted. Omitted by default.
	f32 transition_time = 0;

	inline bool operator==(const PlayerFovSpec &other) const {
		// transition_time is compared here since that could be relevant
		// when aborting a running transition.
		return fov == other.fov && is_multiplier == other.is_multiplier &&
			transition_time == other.transition_time;
	}
	inline bool operator!=(const PlayerFovSpec &other) const {
		return !(*this == other);
	}
};

struct PlayerControl
{
	PlayerControl() = default;

	PlayerControl(
		float a_up, float a_down, float a_left, float a_right,
		bool a_jump, bool a_aux1, bool a_sneak,
		bool a_zoom,
		bool a_dig, bool a_place,
		float a_pitch, float a_yaw
	)
	{
		up = a_up;
		down = a_down;
		left = a_left;
		right = a_right;
		jump = a_jump;
		aux1 = a_aux1;
		sneak = a_sneak;
		zoom = a_zoom;
		dig = a_dig;
		place = a_place;
		pitch = a_pitch;
		yaw = a_yaw;
	}

	// Sets movement_speed and movement_direction according to direction_keys
	// if direction_keys != 0, otherwise leaves them unchanged to preserve
	// joystick input.
	void setMovementFromKeys();

	// For client use
	u32 getKeysPressed() const;
	inline bool isMoving() const { return movement_speed > 0.001f; }

	// For server use
	void unpackKeysPressed(u32 keypress_bits);
	v2f getMovement() const;

	float up = 0;
	float down = 0;
	float left = 0;
	float right = 0;
	bool jump = false;
	bool aux1 = false;
	bool sneak = false;
	bool zoom = false;
	bool dig = false;
	bool place = false;
	// Note: These two are NOT available on the server
	float pitch = 0.0f;
	float yaw = 0.0f;
	float movement_speed = 0.0f;
	float movement_direction = 0.0f;
};

struct PlayerPhysicsOverride
{
	float speed = 1.f;
	float jump = 1.f;
	float gravity = 1.f;

	bool sneak = true;
	bool sneak_glitch = false;
	// "Temporary" option for old move code
	bool new_move = true;

	float speed_climb = 1.f;
	float speed_crouch = 1.f;
	float liquid_fluidity = 1.f;
	float liquid_fluidity_smooth = 1.f;
	float liquid_sink = 1.f;
	float acceleration_default = 1.f;
	float acceleration_air = 1.f;
	float speed_fast = 1.f;
	float acceleration_fast = 1.f;
	float speed_walk = 1.f;

	bool operator==(const PlayerPhysicsOverride &other) const;
	bool operator!=(const PlayerPhysicsOverride &other) const {
		return !(*this == other);
	}
};

struct HudElement;

struct PlayerHud
{
	const auto &getElements() const { return m_elements; }
	/// Returns `nullptr` if not found
	HudElement *get(u32 id);
	/// Returns the ID of the added element
	u32         add(std::unique_ptr<HudElement> e);
	/// Returns whether removal succeeded
	bool        remove(u32 id);
	void        clear();

private:
	u32 getFreeID();

	std::vector<std::unique_ptr<HudElement>> m_elements;
};

/// @note numeric values are part of network protocol
enum CameraMode : int {
	// not a mode. indicates that any may be used.
	CAMERA_MODE_ANY = 0,
	CAMERA_MODE_FIRST,
	CAMERA_MODE_THIRD,
	CAMERA_MODE_THIRD_FRONT,

	CameraMode_END // Dummy for validity check
};

extern const struct EnumString es_CameraMode[];

class Player
{
public:

	Player(const std::string &name, IItemDefManager *idef);
	virtual ~Player() = 0;

	DISABLE_CLASS_COPY(Player);

	// in BS-space
	inline void setSpeed(v3f speed)
	{
		m_speed = speed;
	}

	// in BS-space
	v3f getSpeed() const { return m_speed; }

	const std::string& getName() const { return m_name; }

	CameraMode allowed_camera_mode = CAMERA_MODE_ANY;

	v3f eye_offset_first;
	v3f eye_offset_third;
	v3f eye_offset_third_front;

	Inventory inventory;

	f32 movement_acceleration_default;
	f32 movement_acceleration_air;
	f32 movement_acceleration_fast;
	f32 movement_speed_walk;
	f32 movement_speed_crouch;
	f32 movement_speed_fast;
	f32 movement_speed_climb;
	f32 movement_speed_jump;
	f32 movement_liquid_fluidity;
	f32 movement_liquid_fluidity_smooth;
	f32 movement_liquid_sink;
	f32 movement_gravity;

	v2f local_animations[4];
	float local_animation_speed;

	std::string inventory_formspec;
	std::string formspec_prepend;

	PlayerControl control;
	const PlayerControl& getPlayerControl() { return control; }

	PlayerPhysicsOverride physics_override;

	// Returns non-empty `selected` ItemStack. `hand` is a fallback, if specified
	ItemStack &getWieldedItem(ItemStack *selected, ItemStack *hand) const;
	void setWieldIndex(u16 index);
	u16 getWieldIndex();

	bool setFov(const PlayerFovSpec &spec)
	{
		if (m_fov_override_spec == spec)
			return false;
		m_fov_override_spec = spec;
		return true;
	}

	const PlayerFovSpec &getFov() const
	{
		return m_fov_override_spec;
	}

	PlayerHud hud;

	u32 hud_flags;
	s32 hud_hotbar_itemcount;

	// Get actual usable number of hotbar items (clamped to size of "main" list)
	u16 getMaxHotbarItemcount();

protected:
	std::string m_name;
	v3f m_speed; // velocity; in BS-space
	u16 m_wield_index = 0;
	PlayerFovSpec m_fov_override_spec = { 0.0f, false, 0.0f };
};
