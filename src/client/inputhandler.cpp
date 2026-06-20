// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "porting.h"
#include "settings.h"
#include "util/numeric.h"
#include "inputhandler.h"
#include "gui/mainmenumanager.h"
#include "gui/touchcontrols.h"
#include "hud_element.h"
#include "log_internal.h"
#include "client/renderingengine.h"

static const std::array input_settings = {
	"keyboard_camera_speed",
	"joystick_frustum_sensitivity",
	"repeat_joystick_button_time"
};

InputHandler::InputHandler()
{
	for (const auto &name: Settings::getLayer(SL_DEFAULTS)->getNames())
		if (str_starts_with(name, "keymap_"))
			g_settings->registerChangedCallback(name, &settingChangedCallback, this);
	for (const auto &name: input_settings)
		g_settings->registerChangedCallback(name, &settingChangedCallback, this);
}

void MyEventReceiver::reloadKeybindings()
{
	clearKeyCache();

	keybindings[KeyType::FORWARD] = getKeySetting("keymap_forward");
	keybindings[KeyType::BACKWARD] = getKeySetting("keymap_backward");
	keybindings[KeyType::LEFT] = getKeySetting("keymap_left");
	keybindings[KeyType::RIGHT] = getKeySetting("keymap_right");
	keybindings[KeyType::JUMP] = getKeySetting("keymap_jump");
	keybindings[KeyType::AUX1] = getKeySetting("keymap_aux1");
	keybindings[KeyType::SNEAK] = getKeySetting("keymap_sneak");
	keybindings[KeyType::DIG] = getKeySetting("keymap_dig");
	keybindings[KeyType::PLACE] = getKeySetting("keymap_place");

	keybindings[KeyType::ESC] = getKeySetting("keymap_pause");
	keybindings[KeyType::ESC].keys.emplace_back(EscapeKey);

	keybindings[KeyType::AUTOFORWARD] = getKeySetting("keymap_autoforward");

	keybindings[KeyType::DROP] = getKeySetting("keymap_drop");
	keybindings[KeyType::INVENTORY] = getKeySetting("keymap_inventory");
	keybindings[KeyType::CHAT] = getKeySetting("keymap_chat");
	keybindings[KeyType::CMD] = getKeySetting("keymap_cmd");
	keybindings[KeyType::CMD_LOCAL] = getKeySetting("keymap_cmd_local");
	keybindings[KeyType::CONSOLE] = getKeySetting("keymap_console");
	keybindings[KeyType::MINIMAP] = getKeySetting("keymap_minimap");
	keybindings[KeyType::FREEMOVE] = getKeySetting("keymap_freemove");
	keybindings[KeyType::PITCHMOVE] = getKeySetting("keymap_pitchmove");
	keybindings[KeyType::FASTMOVE] = getKeySetting("keymap_fastmove");
	keybindings[KeyType::NOCLIP] = getKeySetting("keymap_noclip");
	keybindings[KeyType::HOTBAR_PREV] = getKeySetting("keymap_hotbar_previous");
	keybindings[KeyType::HOTBAR_NEXT] = getKeySetting("keymap_hotbar_next");
	keybindings[KeyType::MUTE] = getKeySetting("keymap_mute");
	keybindings[KeyType::INC_VOLUME] = getKeySetting("keymap_increase_volume");
	keybindings[KeyType::DEC_VOLUME] = getKeySetting("keymap_decrease_volume");
	keybindings[KeyType::CINEMATIC] = getKeySetting("keymap_cinematic");
	keybindings[KeyType::SCREENSHOT] = getKeySetting("keymap_screenshot");
	keybindings[KeyType::TOGGLE_BLOCK_BOUNDS] = getKeySetting("keymap_toggle_block_bounds");
	keybindings[KeyType::TOGGLE_HUD] = getKeySetting("keymap_toggle_hud");
	keybindings[KeyType::TOGGLE_CHAT] = getKeySetting("keymap_toggle_chat");
	keybindings[KeyType::TOGGLE_FOG] = getKeySetting("keymap_toggle_fog");
	keybindings[KeyType::TOGGLE_UPDATE_CAMERA] = getKeySetting("keymap_toggle_update_camera");
	keybindings[KeyType::TOGGLE_DEBUG] = getKeySetting("keymap_toggle_debug");
	keybindings[KeyType::TOGGLE_PROFILER] = getKeySetting("keymap_toggle_profiler");
	keybindings[KeyType::CAMERA_MODE] = getKeySetting("keymap_camera_mode");
	keybindings[KeyType::INCREASE_VIEWING_RANGE] =
			getKeySetting("keymap_increase_viewing_range_min");
	keybindings[KeyType::DECREASE_VIEWING_RANGE] =
			getKeySetting("keymap_decrease_viewing_range_min");
	keybindings[KeyType::RANGESELECT] = getKeySetting("keymap_rangeselect");
	keybindings[KeyType::ZOOM] = getKeySetting("keymap_zoom");

	keybindings[KeyType::CAMERA_YAW_LEFT] = getKeySetting("keymap_camera_yaw_left");
	keybindings[KeyType::CAMERA_YAW_RIGHT] = getKeySetting("keymap_camera_yaw_right");
	keybindings[KeyType::CAMERA_PITCH_UP] = getKeySetting("keymap_camera_pitch_up");
	keybindings[KeyType::CAMERA_PITCH_DOWN] = getKeySetting("keymap_camera_pitch_down");

	keybindings[KeyType::QUICKTUNE_NEXT] = getKeySetting("keymap_quicktune_next");
	keybindings[KeyType::QUICKTUNE_PREV] = getKeySetting("keymap_quicktune_prev");
	keybindings[KeyType::QUICKTUNE_INC] = getKeySetting("keymap_quicktune_inc");
	keybindings[KeyType::QUICKTUNE_DEC] = getKeySetting("keymap_quicktune_dec");

	for (int i = 0; i < HUD_HOTBAR_ITEMCOUNT_MAX; i++) {
		std::string slot_key_name = "keymap_slot" + std::to_string(i + 1);
		keybindings[KeyType::SLOT_1 + i] = getKeySetting(slot_key_name.c_str());
	}

	// First clear all keys, then re-add the ones we listen for
	keysListenedFor.clear();
	for (int i = 0; i < KeyType::INTERNAL_ENUM_COUNT; i++) {
		GameKeyType game_key = static_cast<GameKeyType>(i);
		keybindings[i].keys.emplace_back(game_key);
		for (auto key: keybindings[i].keys) {
			listenForKey(key, game_key);
		}
	}

	repeat_joystick_button_time = g_settings->getFloat("repeat_joystick_button_time");

	static const std::array camera_rotation_actions = {
		KeyType::CAMERA_YAW_LEFT,
		KeyType::CAMERA_YAW_RIGHT,
		KeyType::CAMERA_PITCH_UP,
		KeyType::CAMERA_PITCH_DOWN
	};
	for (const auto action: camera_rotation_actions) {
		auto &keybinding = keybindings[action];
		keybinding.scale.keyboard_mouse = g_settings->getFloat("keyboard_camera_speed", 0.001f, 720.0f);
		keybinding.scale.joystick = g_settings->getFloat("joystick_frustum_sensitivity", 0.001f, 720.0f);
	}

}

bool MyEventReceiver::WasKeyDown(GameKeyType key)
{
	bool b = keyWasDown[key];
	if (b) {
		keyWasDown.reset(key);
	} else if (IsKeyDown(key)) {
		// Gamepad events are not repeated so these need to be repeated here
		for (auto kp: keybindings[key].keys) {
			if (kp.getSourceType() != KeyPress::InputSourceType::GAMEPAD)
				continue;
			auto down_ent = physicalKeyDown.find(kp);
			if (down_ent == physicalKeyDown.end())
				continue;
			if (auto &keystate = down_ent->second; keystate.analog_value > 0) {
				auto time_now = porting::getTimeMs() / 1000.0;
				if (time_now - keystate.last_binary_update >= repeat_joystick_button_time) {
					b = true;
					keystate.last_binary_update = time_now;
				}
			}
		}
	}
	return b;
}

bool MyEventReceiver::setKeyDown(KeyPress keyCode, float value)
{
	if (keysListenedFor.find(keyCode) == keysListenedFor.end()) // ignore irrelevant key input
		return false;
	auto action = keysListenedFor[keyCode];
	if (value > 0) {
		if (physicalKeyDown.find(keyCode) == physicalKeyDown.end())
			physicalKeyDown[keyCode].last_binary_update = porting::getTimeMs() / 1000.0;
		physicalKeyDown[keyCode].analog_value = value;
	} else {
		physicalKeyDown.erase(keyCode);
	}
	setKeyDown(action, checkKeyDown(action));
	return true;
}

/* new_state:
 * float: the analog value of the joystick
 * bool: whether keyWasDown should be set
 */
void MyEventReceiver::setKeyDown(GameKeyType action, std::pair<float, bool> new_state)
{
	auto value = new_state.first;
	if (value > 0) {
		if (!IsKeyDown(action)) {
			keyWasPressed.set(action);
			// checkKeyDown does not check whether the key for an action is already down, so we set this unconditionally
			// if the key was previously not yet pressed
			keyWasDown.set(action);
		}
		if (new_state.second)
			keyWasDown.set(action);
		axisValues[action] = value;
	} else {
		if (IsKeyDown(action))
			keyWasReleased.set(action);
		axisValues[action] = 0;
	}
}

std::pair<float, bool> MyEventReceiver::checkKeyDown(GameKeyType action) const
{
	auto value = 0.0f;
	bool setWasKeyDown = false;
	for (const auto &key : keybindings[action].keys) {
		auto p = physicalKeyDown.find(key);
		if (p != physicalKeyDown.end()) {
			value = std::max(value, p->second.analog_value * keybindings[action].getScale(key.getType()));
			setWasKeyDown |= p->first.getSourceType() != KeyPress::InputSourceType::GAMEPAD;
		}
	}
	return std::pair(value, setWasKeyDown);
}

bool MyEventReceiver::OnEvent(const SEvent &event)
{
	if (event.EventType == EET_LOG_TEXT_EVENT) {
		static const LogLevel irr_loglev_conv[] = {
			LL_VERBOSE, // ELL_DEBUG
			LL_INFO,    // ELL_INFORMATION
			LL_WARNING, // ELL_WARNING
			LL_ERROR,   // ELL_ERROR
			LL_NONE,    // ELL_NONE
		};
		assert(event.LogEvent.Level < ARRLEN(irr_loglev_conv));
		g_logger.log(irr_loglev_conv[event.LogEvent.Level],
				std::string("Irrlicht: ") + event.LogEvent.Text);
		return true;
	}

	if (event.EventType == EET_APPLICATION_EVENT &&
			event.ApplicationEvent.EventType == EAET_DPI_CHANGED) {
		// This is a fake setting so that we can use (de)registerChangedCallback
		// not only to listen for gui/hud_scaling changes, but also for DPI changes.
		g_settings->setU16("dpi_change_notifier",
				g_settings->getU16("dpi_change_notifier") + 1);
		return true;
	}

	// This is separate from other keyboard handling so that it also works in menus.
	if (event.EventType == EET_KEY_INPUT_EVENT) {
		KeyPress keyCode(event.KeyInput);

		if (keySettingHasMatch("keymap_fullscreen", keyCode)) {
			if (event.KeyInput.PressedDown && !fullscreen_is_down) {
				IrrlichtDevice *device = RenderingEngine::get_raw_device();

				bool new_fullscreen = !device->isFullscreen();
				// Only update the setting if toggling succeeds
				if (device->setFullscreen(new_fullscreen)) {
					g_settings->setBool("fullscreen", new_fullscreen);
				}
			}
			fullscreen_is_down = event.KeyInput.PressedDown;
			return true;

		} else if (keySettingHasMatch("keymap_close_world", keyCode)) {
			close_world_down = event.KeyInput.PressedDown;

		} else if (keyCode == EscapeKey) {
			esc_down = event.KeyInput.PressedDown;
		}

		if (esc_down && close_world_down) {
			g_gamecallback->disconnect();
			return true;
		}
	}

	if (event.EventType == EET_MOUSE_INPUT_EVENT && !event.MouseInput.Simulated)
		last_pointer_type = PointerType::Mouse;
	else if (event.EventType == EET_TOUCH_INPUT_EVENT)
		last_pointer_type = PointerType::Touch;

	// Let the menu handle events, if one is active.
	if (isMenuActive()) {
		if (g_touchcontrols)
			g_touchcontrols->setVisible(false);
		return g_menumgr.preprocessEvent(event);
	}

	// Remember whether each key is down or up
	if (g_touchcontrols && event.EventType == EET_TOUCH_INPUT_EVENT) {
		// In case of touchcontrols, we have to handle different events
		g_touchcontrols->translateEvent(event);
		return true;
	} else if (event.EventType == EET_MOUSE_INPUT_EVENT && event.MouseInput.Event == EMIE_MOUSE_WHEEL) {
		mouse_wheel += event.MouseInput.Wheel;
	} else if (event.EventType == EET_USER_EVENT && event.UserEvent.type == EUET_GAME_KEY) {
		KeyPress keyCode(static_cast<GameKeyType>(event.UserEvent.UserData1));
		setKeyDown(keyCode, InputHandler::intToAnalog(event.UserEvent.UserData2));
		return true;
	} else if (KeyPressEvent kpevent(event); kpevent) {
		setKeyDown(kpevent.key, kpevent.analog_value);
		if (auto opposite = kpevent.key.getOppositeAxisDirection(); opposite) {
			setKeyDown(opposite, 0);
		}
		return true;
	}

	// tell Irrlicht to continue processing this event
	return false;
}

/*
 * RealInputHandler
 */
v2s32 RealInputHandler::getMousePos()
{
	auto control = RenderingEngine::get_raw_device()->getCursorControl();
	if (control) {
		return control->getPosition();
	}

	return m_mousepos;
}

void RealInputHandler::setMousePos(s32 x, s32 y)
{
	auto control = RenderingEngine::get_raw_device()->getCursorControl();
	if (control) {
		control->setPosition(x, y);
	} else {
		m_mousepos = v2s32(x, y);
	}
}

/*
 * RandomInputHandler
 */
s32 RandomInputHandler::Rand(s32 min, s32 max)
{
	return (myrand() % (max - min + 1)) + min;
}

struct RandomInputHandlerSimData {
	GameKeyType key;
	float counter;
	int time_max;
};

void RandomInputHandler::step(float dtime)
{
	static RandomInputHandlerSimData rnd_data[] = {
		{ KeyType::JUMP, 0.0f, 40 },
		{ KeyType::AUX1, 0.0f, 40 },
		{ KeyType::FORWARD, 0.0f, 40 },
		{ KeyType::LEFT, 0.0f, 40 },
		{ KeyType::DIG, 0.0f, 30 },
		{ KeyType::PLACE, 0.0f, 15 }
	};

	for (auto &i : rnd_data) {
		i.counter -= dtime;
		if (i.counter < 0.0) {
			i.counter = 0.1 * Rand(1, i.time_max);
			keydown.flip(i.key);
		}
	}
	{
		static float counter1 = 0;
		counter1 -= dtime;
		if (counter1 < 0.0) {
			counter1 = 0.1 * Rand(1, 20);
			mousespeed = v2s32(Rand(-20, 20), Rand(-15, 20));
		}
	}
	mousepos += mousespeed;
}
