// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 est31, <MTest31@outlook.com>

#include "joystick_controller.h"
#include "keys.h"
#include "settings.h"
#include "gettime.h"
#include "porting.h"
#include "util/string.h"
#include "util/numeric.h"

bool JoystickButtonCmb::isTriggered(const SEvent::SJoystickEvent &ev) const
{
	u32 buttons = ev.ButtonStates;

	buttons &= filter_mask;
	return buttons == compare_mask;
}

bool JoystickAxisCmb::isTriggered(const SEvent::SJoystickEvent &ev) const
{
	s16 ax_val = ev.Axis[axis_to_compare];

	return (ax_val * direction < -thresh);
}

// spares many characters
#define JLO_B_PB(A, B, C)    jlo.button_keys.emplace_back(A, B, C)
#define JLO_A_PB(A, B, C, D) jlo.axis_keys.emplace_back(A, B, C, D)

JoystickLayout create_default_layout()
{
	JoystickLayout jlo;

	jlo.axes_deadzone = g_settings->getU16("joystick_deadzone");

	const JoystickAxisLayout axes[JA_COUNT] = {
		{0, 1}, // JA_SIDEWARD_MOVE
		{1, 1}, // JA_FORWARD_MOVE
		{3, 1}, // JA_FRUSTUM_HORIZONTAL
		{4, 1}, // JA_FRUSTUM_VERTICAL
	};
	memcpy(jlo.axes, axes, sizeof(jlo.axes));

	u32 sb = 1 << 7; // START button mask
	u32 fb = 1 << 3; // FOUR button mask
	u32 bm = sb | fb; // Mask for Both Modifiers

	// The back button means "ESC".
	JLO_B_PB(KeyType::ESC,        1 << 6,      1 << 6);

	// The start button counts as modifier as well as use key.
	// JLO_B_PB(KeyType::USE,        sb,          sb));

	// Accessible without start modifier button pressed
	// regardless whether four is pressed or not
	JLO_B_PB(KeyType::SNEAK,      sb | 1 << 2, 1 << 2);

	// Accessible without four modifier button pressed
	// regardless whether start is pressed or not
	JLO_B_PB(KeyType::DIG,        fb | 1 << 4, 1 << 4);
	JLO_B_PB(KeyType::PLACE,      fb | 1 << 5, 1 << 5);

	// Accessible without any modifier pressed
	JLO_B_PB(KeyType::JUMP,       bm | 1 << 0, 1 << 0);
	JLO_B_PB(KeyType::AUX1,       bm | 1 << 1, 1 << 1);

	// Accessible with start button not pressed, but four pressed
	// TODO find usage for button 0
	JLO_B_PB(KeyType::DROP,        bm | 1 << 1, fb | 1 << 1);
	JLO_B_PB(KeyType::HOTBAR_PREV, bm | 1 << 4, fb | 1 << 4);
	JLO_B_PB(KeyType::HOTBAR_NEXT, bm | 1 << 5, fb | 1 << 5);

	// Accessible with start button and four pressed
	// TODO find usage for buttons 0, 1 and 4, 5

	// Now about the buttons simulated by the axes

	// Movement buttons, important for vessels
	JLO_A_PB(KeyType::FORWARD,  1,  1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::BACKWARD, 1, -1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::LEFT,     0,  1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::RIGHT,    0, -1, jlo.axes_deadzone);

	// Scroll buttons
	JLO_A_PB(KeyType::HOTBAR_PREV, 2, -1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::HOTBAR_NEXT, 5, -1, jlo.axes_deadzone);

	return jlo;
}

JoystickLayout create_xbox_layout()
{
	JoystickLayout jlo;

	jlo.axes_deadzone = 7000;

	const JoystickAxisLayout axes[JA_COUNT] = {
		{0, 1}, // JA_SIDEWARD_MOVE
		{1, 1}, // JA_FORWARD_MOVE
		{2, 1}, // JA_FRUSTUM_HORIZONTAL
		{3, 1}, // JA_FRUSTUM_VERTICAL
	};
	memcpy(jlo.axes, axes, sizeof(jlo.axes));

	// The back button means "ESC".
	JLO_B_PB(KeyType::ESC,        1 << 8,  1 << 8); // back
	JLO_B_PB(KeyType::ESC,        1 << 9,  1 << 9); // start

	// 4 Buttons
	JLO_B_PB(KeyType::JUMP,        1 << 0,  1 << 0); // A/green
	JLO_B_PB(KeyType::ESC,         1 << 1,  1 << 1); // B/red
	JLO_B_PB(KeyType::AUX1,        1 << 2,  1 << 2); // X/blue
	JLO_B_PB(KeyType::INVENTORY,   1 << 3,  1 << 3); // Y/yellow

	// Analog Sticks
	JLO_B_PB(KeyType::AUX1,        1 << 11, 1 << 11); // left
	JLO_B_PB(KeyType::SNEAK,       1 << 12, 1 << 12); // right

	// Triggers
	JLO_B_PB(KeyType::DIG,         1 << 6,  1 << 6); // lt
	JLO_B_PB(KeyType::PLACE,       1 << 7,  1 << 7); // rt
	JLO_B_PB(KeyType::HOTBAR_PREV, 1 << 4,  1 << 4); // lb
	JLO_B_PB(KeyType::HOTBAR_NEXT, 1 << 5,  1 << 5); // rb

	// D-PAD
	JLO_B_PB(KeyType::ZOOM,        1 << 15, 1 << 15); // up
	JLO_B_PB(KeyType::DROP,        1 << 13, 1 << 13); // left
	JLO_B_PB(KeyType::SCREENSHOT,  1 << 14, 1 << 14); // right
	JLO_B_PB(KeyType::FREEMOVE,    1 << 16, 1 << 16); // down

	// Movement buttons, important for vessels
	JLO_A_PB(KeyType::FORWARD,  1,  1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::BACKWARD, 1, -1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::LEFT,     0,  1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::RIGHT,    0, -1, jlo.axes_deadzone);

	return jlo;
}

JoystickLayout create_dragonrise_gamecube_layout()
{
	JoystickLayout jlo;

	jlo.axes_deadzone = 7000;

	const JoystickAxisLayout axes[JA_COUNT] = {
		// Control Stick
		{0, 1}, // JA_SIDEWARD_MOVE
		{1, 1}, // JA_FORWARD_MOVE

		// C-Stick
		{3, 1}, // JA_FRUSTUM_HORIZONTAL
		{4, 1}, // JA_FRUSTUM_VERTICAL
	};
	memcpy(jlo.axes, axes, sizeof(jlo.axes));

	// The center button
	JLO_B_PB(KeyType::ESC, 1 << 9, 1 << 9); // Start/Pause Button

	// Front right buttons
	JLO_B_PB(KeyType::JUMP,  1 << 2, 1 << 2); // A Button
	JLO_B_PB(KeyType::SNEAK, 1 << 3, 1 << 3); // B Button
	JLO_B_PB(KeyType::DROP,  1 << 0, 1 << 0); // Y Button
	JLO_B_PB(KeyType::AUX1,  1 << 1, 1 << 1); // X Button

	// Triggers
	JLO_B_PB(KeyType::DIG,       1 << 4, 1 << 4); // L Trigger
	JLO_B_PB(KeyType::PLACE,     1 << 5, 1 << 5); // R Trigger
	JLO_B_PB(KeyType::INVENTORY, 1 << 6, 1 << 6); // Z Button

	// D-Pad
	JLO_A_PB(KeyType::HOTBAR_PREV, 5,  1, jlo.axes_deadzone); // left
	JLO_A_PB(KeyType::HOTBAR_NEXT, 5, -1, jlo.axes_deadzone); // right
	// Axis are hard to actuate independently, best to leave up and down unused.
	//JLO_A_PB(0, 6,  1, jlo.axes_deadzone); // up
	//JLO_A_PB(0, 6, -1, jlo.axes_deadzone); // down

	// Movements tied to Control Stick, important for vessels
	JLO_A_PB(KeyType::LEFT,     0,  1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::RIGHT,    0, -1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::FORWARD,  1,  1, jlo.axes_deadzone);
	JLO_A_PB(KeyType::BACKWARD, 1, -1, jlo.axes_deadzone);

	return jlo;
}


JoystickController::JoystickController()
{
	doubling_dtime = std::max(g_settings->getFloat("repeat_joystick_button_time"), 0.001f);
	for (float &i : m_past_pressed_time) {
		i = 0;
	}
	m_layout.axes_deadzone = 0;
	clear();
}

void JoystickController::onJoystickConnect(const std::vector<SJoystickInfo> &joystick_infos)
{
	s32         id     = g_settings->getS32("joystick_id");
	std::string layout = g_settings->get("joystick_type");

	if (id < 0 || id >= (s32)joystick_infos.size()) {
		// TODO: auto detection
		id = 0;
	}

	if (id >= 0 && id < (s32)joystick_infos.size()) {
		if (layout.empty() || layout == "auto")
			setLayoutFromControllerName(joystick_infos[id].Name.c_str());
		else
			setLayoutFromControllerName(layout);
	}

	// Irrlicht restriction.
	m_joystick_id = rangelim(id, 0, UINT8_MAX);
}

void JoystickController::setLayoutFromControllerName(const std::string &name)
{
	if (lowercase(name).find("xbox") != std::string::npos) {
		m_layout = create_xbox_layout();
	} else if (lowercase(name).find("dragonrise_gamecube") != std::string::npos) {
		m_layout = create_dragonrise_gamecube_layout();
	} else {
		m_layout = create_default_layout();
	}
}

bool JoystickController::handleEvent(const SEvent::SJoystickEvent &ev)
{
	if (ev.Joystick != m_joystick_id)
		return false;

	m_internal_time = porting::getTimeMs() / 1000.f;

	std::bitset<KeyType::INTERNAL_ENUM_COUNT> keys_pressed;

	// First generate a list of keys pressed

	for (const auto &button_key : m_layout.button_keys) {
		if (button_key.isTriggered(ev)) {
			keys_pressed.set(button_key.key);
		}
	}

	for (const auto &axis_key : m_layout.axis_keys) {
		if (axis_key.isTriggered(ev)) {
			keys_pressed.set(axis_key.key);
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

	for (size_t i = 0; i < JA_COUNT; i++) {
		const JoystickAxisLayout &ax_la = m_layout.axes[i];
		m_axes_vals[i] = ax_la.invert * ev.Axis[ax_la.axis_id];
	}

	return true;
}

void JoystickController::clear()
{
	m_keys_pressed.reset();
	m_keys_down.reset();
	m_past_keys_pressed.reset();
	m_keys_released.reset();
	memset(m_axes_vals, 0, sizeof(m_axes_vals));
}

float JoystickController::getAxisWithoutDead(JoystickAxis axis)
{
	s16 v = m_axes_vals[axis];

	if (abs(v) < m_layout.axes_deadzone)
		return 0.0f;

	v += (v < 0 ? m_layout.axes_deadzone : -m_layout.axes_deadzone);

	return (float)v / ((float)(INT16_MAX - m_layout.axes_deadzone));
}

float JoystickController::getMovementDirection()
{
	return std::atan2(getAxisWithoutDead(JA_SIDEWARD_MOVE),
			-getAxisWithoutDead(JA_FORWARD_MOVE));
}

float JoystickController::getMovementSpeed()
{
	float speed = std::sqrt(std::pow(getAxisWithoutDead(JA_FORWARD_MOVE), 2) +
			std::pow(getAxisWithoutDead(JA_SIDEWARD_MOVE), 2));
	if (speed > 1.0f)
		speed = 1.0f;
	return speed;
}
