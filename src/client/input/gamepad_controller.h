// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 rubenwardy <rw@rubenwardy.com>


#pragma once

#include "irrlicht.h"
#include "irrlichttypes_bloated.h"
#include "keys.h"
#include <bitset>
#include <vector>
#include <memory>

#include <SDL2/SDL_gamecontroller.h>


enum GamepadAxis
{
	JA_SIDEWARD_MOVE,
	JA_FORWARD_MOVE,

	JA_FRUSTUM_HORIZONTAL,
	JA_FRUSTUM_VERTICAL,

	// To know the count of enum values
	JA_COUNT,
};


struct GamepadLayout
{
	s16 axes_deadzone;
};


class GamepadInput
{
public:
	virtual ~GamepadInput() = default;
	virtual bool operator()(SDL_GameController *gameController) const = 0;
};


class GamepadController
{
public:
	GamepadController();

	void connectToJoystick();

	void update(float dtime, bool is_in_gui);
	bool handleEvent(const irr::SEvent::SJoystickEvent &ev);
	void clear();

	void releaseAllKeys()
	{
		m_keys_released |= m_keys_down;
		m_keys_down.reset();
	}

	bool wasKeyDown(GameKeyType b)
	{
		bool r = m_past_keys_pressed[b];
		m_past_keys_pressed[b] = false;
		return r;
	}

	bool wasKeyReleased(GameKeyType b)
	{
		return m_keys_released[b];
	}

	void clearWasKeyReleased(GameKeyType b)
	{
		m_keys_released[b] = false;
	}

	bool wasKeyPressed(GameKeyType b)
	{
		return m_keys_pressed[b];
	}

	void clearWasKeyPressed(GameKeyType b)
	{
		m_keys_pressed[b] = false;
	}

	bool isKeyDown(GameKeyType b)
	{
		return m_keys_down[b];
	}

	float getMovementDirection();
	float getMovementSpeed();
	v2f32 getLookRotation();

	inline u8 getJoystickId() {
		return m_joystick_id;
	}

private:

	float getAxisWithoutDead(GamepadAxis axis);

	f32 doubling_dtime;
	SDL_GameController *gameController = nullptr;

	GamepadLayout m_layout;
	std::vector<std::pair<std::unique_ptr<GamepadInput>, KeyType::T>> m_button_bindings;

	s16 m_axes_vals[JA_COUNT];

	u8 m_joystick_id = 0;

	std::bitset<KeyType::INTERNAL_ENUM_COUNT> m_keys_down;
	std::bitset<KeyType::INTERNAL_ENUM_COUNT> m_keys_pressed;

	f32 m_internal_time;

	f32 m_past_pressed_time[KeyType::INTERNAL_ENUM_COUNT];

	std::bitset<KeyType::INTERNAL_ENUM_COUNT> m_past_keys_pressed;
	std::bitset<KeyType::INTERNAL_ENUM_COUNT> m_keys_released;
};
