// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "settings.h"
#include "util/numeric.h"
#include "inputhandler.h"
#include "gui/mainmenumanager.h"
#include "gui/touchcontrols.h"
#include "hud.h"
#include "log_internal.h"
#include "client/renderingengine.h"

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

	keybindings[KeyType::ESC] = EscapeKey;

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
		listenForKey(keybindings[i], static_cast<GameKeyType>(i));
	}
}

bool MyEventReceiver::setKeyDown(KeyPress keyCode, bool is_down)
{
	if (keysListenedFor.find(keyCode) == keysListenedFor.end()) // ignore irrelevant key input
		return false;
	auto action = keysListenedFor[keyCode];
	if (is_down) {
		physicalKeyDown.insert(keyCode);
		setKeyDown(action, true);
	} else {
		physicalKeyDown.erase(keyCode);
		setKeyDown(action, false);
	}
	return true;
}

void MyEventReceiver::setKeyDown(GameKeyType action, bool is_down)
{
	if (is_down) {
		if (!IsKeyDown(action))
			keyWasPressed.set(action);
		keyIsDown.set(action);
		keyWasDown.set(action);
	} else {
		if (IsKeyDown(action))
			keyWasReleased.set(action);
		keyIsDown.reset(action);
	}
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

		if (keyCode == getKeySetting("keymap_fullscreen")) {
			if (event.KeyInput.PressedDown && !fullscreen_is_down) {
				IrrlichtDevice *device = RenderingEngine::get_raw_device();

				bool new_fullscreen = !device->isFullscreen();
				// Only update the setting if toggling succeeds - it always fails
				// if Minetest was built without SDL.
				if (device->setFullscreen(new_fullscreen)) {
					g_settings->setBool("fullscreen", new_fullscreen);
				}
			}
			fullscreen_is_down = event.KeyInput.PressedDown;
			return true;

		} else if (keyCode == getKeySetting("keymap_close_world")) {
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
	if (event.EventType == EET_KEY_INPUT_EVENT) {
		KeyPress keyCode(event.KeyInput);
		if (setKeyDown(keyCode, event.KeyInput.PressedDown))
			return true;
	} else if (g_touchcontrols && event.EventType == EET_TOUCH_INPUT_EVENT) {
		// In case of touchcontrols, we have to handle different events
		g_touchcontrols->translateEvent(event);
		return true;
	} else if (event.EventType == EET_JOYSTICK_INPUT_EVENT) {
		// joystick may be nullptr if game is launched with '--random-input' parameter
		return joystick && joystick->handleEvent(event.JoystickEvent);
	} else if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		// Handle mouse events
		switch (event.MouseInput.Event) {
		case EMIE_LMOUSE_PRESSED_DOWN:
			setKeyDown(LMBKey, true);
			break;
		case EMIE_MMOUSE_PRESSED_DOWN:
			setKeyDown(MMBKey, true);
			break;
		case EMIE_RMOUSE_PRESSED_DOWN:
			setKeyDown(RMBKey, true);
			break;
		case EMIE_LMOUSE_LEFT_UP:
			setKeyDown(LMBKey, false);
			break;
		case EMIE_MMOUSE_LEFT_UP:
			setKeyDown(MMBKey, false);
			break;
		case EMIE_RMOUSE_LEFT_UP:
			setKeyDown(RMBKey, false);
			break;
		case EMIE_MOUSE_WHEEL:
			mouse_wheel += event.MouseInput.Wheel;
			break;
		default:
			break;
		}
	}

	// tell Irrlicht to continue processing this event
	return false;
}

/*
 * RealInputHandler
 */
float RealInputHandler::getJoystickSpeed()
{
	if (g_touchcontrols && g_touchcontrols->getJoystickSpeed())
		return g_touchcontrols->getJoystickSpeed();
	return joystick.getMovementSpeed();
}

float RealInputHandler::getJoystickDirection()
{
	// `getJoystickDirection() == 0` means forward, so we cannot use
	// `getJoystickDirection()` as a condition.
	if (g_touchcontrols && g_touchcontrols->getJoystickSpeed())
		return g_touchcontrols->getJoystickDirection();
	return joystick.getMovementDirection();
}

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
	static bool useJoystick = false;
	{
		static float counterUseJoystick = 0;
		counterUseJoystick -= dtime;
		if (counterUseJoystick < 0.0) {
			counterUseJoystick = 5.0; // switch between joystick and keyboard direction input
			useJoystick = !useJoystick;
		}
	}
	if (useJoystick) {
		static float counterMovement = 0;
		counterMovement -= dtime;
		if (counterMovement < 0.0) {
			counterMovement = 0.1 * Rand(1, 40);
			joystickSpeed = Rand(0,100)*0.01;
			joystickDirection = Rand(-100, 100)*0.01 * M_PI;
		}
	} else {
		joystickSpeed = 0.0f;
		joystickDirection = 0.0f;
	}
}
