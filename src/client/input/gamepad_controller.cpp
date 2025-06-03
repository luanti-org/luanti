/*
Minetest
Copyright (C) 2016 est31, <MTest31@outlook.com>
Copyright (C) 2022 rubenwardy

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "gamepad_controller.h"
#include "irrlichttypes_bloated.h"
#include "keys.h"
#include "settings.h"
#include "porting.h"
#include "util/string.h"
#include "util/numeric.h"


class GamepadInputButton : public GamepadInput
{
	SDL_GameControllerButton button;

public:
	explicit GamepadInputButton(SDL_GameControllerButton button): button(button) {}

	~GamepadInputButton() override = default;
	bool operator()(SDL_GameController *gameController) const override {
		return SDL_GameControllerGetButton(gameController, button);
	};
};


class GamepadInputAxis : public GamepadInput
{
	SDL_GameControllerAxis axis;

public:
	explicit GamepadInputAxis(SDL_GameControllerAxis axis): axis(axis) {}

	~GamepadInputAxis() override = default;
	bool operator()(SDL_GameController *gameController) const override {
		return SDL_GameControllerGetAxis(gameController, axis) > 9000;
	};
};


#define BTN(button) std::make_unique<GamepadInputButton>(button)
#define AXIS(axis) std::make_unique<GamepadInputAxis>(axis)


GamepadController::GamepadController() :
		doubling_dtime(std::max(g_settings->getFloat("repeat_joystick_button_time"), 0.001f))
{
	for (float &i : m_past_pressed_time) {
		i = 0;
	}
	clear();

	m_layout.axes_deadzone = g_settings->getU16("joystick_deadzone");

	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_BACK), KeyType::ESC );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_START), KeyType::ESC );

	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A), KeyType::JUMP );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_B), KeyType::ESC );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_X), KeyType::AUX1 );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_Y), KeyType::INVENTORY );

	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSTICK), KeyType::AUX1 );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSTICK), KeyType::SNEAK );

	m_button_bindings.emplace_back( AXIS(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT), KeyType::DIG );
	m_button_bindings.emplace_back( AXIS(SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT), KeyType::PLACE );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_LEFTSHOULDER), KeyType::HOTBAR_PREV );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), KeyType::HOTBAR_NEXT );

	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP), KeyType::ZOOM );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT), KeyType::DROP );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT), KeyType::SCREENSHOT );
	m_button_bindings.emplace_back( BTN(SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN), KeyType::FREEMOVE );
}


void GamepadController::connectToJoystick()
{
	s32 id = g_settings->getS32("joystick_id");

	int joystick_count = SDL_NumJoysticks();

	if (id < 0 || id >= joystick_count) {
		id = 0;

		for (int joystick_id = 0; joystick_id < joystick_count; joystick_id++) {
			if (SDL_IsGameController(joystick_id)) {
				id = joystick_id;
				break;
			}
		}
	}

	m_joystick_id = id;

	if (gameController)
		SDL_GameControllerClose(gameController);
	gameController = SDL_GameControllerOpen(m_joystick_id);
}

void GamepadController::update(float dtime, bool is_in_gui)
{
	if (!gameController)
		return;

	m_internal_time = porting::getTimeMs() / 1000.f;

	std::bitset<KeyType::INTERNAL_ENUM_COUNT> keys_pressed;

	for (const auto &pair : m_button_bindings) {
		if ((*pair.first)(gameController)) {
			keys_pressed.set(pair.second);
		}
	}

	// Then update the values

	for (size_t i = 0; i < KeyType::INTERNAL_ENUM_COUNT; i++) {
		if (keys_pressed[i]) {
			if (!m_past_keys_pressed[i] &&
					m_past_pressed_time[i] < m_internal_time - doubling_dtime) {
				m_past_keys_pressed[i] = true;
				m_past_pressed_time[i] = m_internal_time;
			}
		} else if (m_keys_down[i]) {
			m_keys_released[i] = true;
		}

		if (keys_pressed[i] && !(m_keys_down[i]))
			m_keys_pressed[i] = true;

		m_keys_down[i] = keys_pressed[i];
	}
}

bool GamepadController::handleEvent(const irr::SEvent::SJoystickEvent &ev)
{
	return true;
}

void GamepadController::clear()
{
	m_keys_pressed.reset();
	m_keys_down.reset();
	m_past_keys_pressed.reset();
	m_keys_released.reset();
	memset(m_axes_vals, 0, sizeof(m_axes_vals));
}

float GamepadController::getAxisWithoutDead(GamepadAxis axis)
{
	SDL_GameControllerAxis sdlAxis;
	switch (axis) {
	case JA_SIDEWARD_MOVE:
		sdlAxis = SDL_CONTROLLER_AXIS_LEFTX;
		break;
	case JA_FORWARD_MOVE:
		sdlAxis = SDL_CONTROLLER_AXIS_LEFTY;
		break;
	case JA_FRUSTUM_HORIZONTAL:
		sdlAxis = SDL_CONTROLLER_AXIS_RIGHTX;
		break;
	case JA_FRUSTUM_VERTICAL:
		sdlAxis = SDL_CONTROLLER_AXIS_RIGHTY;
		break;
	case JA_COUNT:
		break;
	}

	s16 v = SDL_GameControllerGetAxis(gameController, sdlAxis);
	if (abs(v) < m_layout.axes_deadzone)
		return 0.0f;

	v += (v < 0 ? m_layout.axes_deadzone : -m_layout.axes_deadzone);

	return (float)v / ((float)(INT16_MAX - m_layout.axes_deadzone));
}

float GamepadController::getMovementDirection()
{
	return atan2(getAxisWithoutDead(JA_SIDEWARD_MOVE), -getAxisWithoutDead(JA_FORWARD_MOVE));
}

float GamepadController::getMovementSpeed()
{
	float speed = sqrt(pow(getAxisWithoutDead(JA_FORWARD_MOVE), 2) + pow(getAxisWithoutDead(JA_SIDEWARD_MOVE), 2));
	if (speed > 1.0f)
		speed = 1.0f;
	return speed;
}

v2f32 GamepadController::getLookRotation()
{
	return {
		-getAxisWithoutDead(JA_FRUSTUM_HORIZONTAL),
		getAxisWithoutDead(JA_FRUSTUM_VERTICAL),
	};
}
