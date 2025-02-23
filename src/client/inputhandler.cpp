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

void KeyCache::populate_nonchanging()
{
	key[KeyType::ESC] = EscapeKey;
}

void KeyCache::populate()
{
	key[KeyType::FORWARD] = getKeySetting("keymap_forward");
	key[KeyType::BACKWARD] = getKeySetting("keymap_backward");
	key[KeyType::LEFT] = getKeySetting("keymap_left");
	key[KeyType::RIGHT] = getKeySetting("keymap_right");
	key[KeyType::JUMP] = getKeySetting("keymap_jump");
	key[KeyType::AUX1] = getKeySetting("keymap_aux1");
	key[KeyType::SNEAK] = getKeySetting("keymap_sneak");
	key[KeyType::SNEAK_LOCK] = getKeySetting("keymap_sneak_lock");
	key[KeyType::DIG] = getKeySetting("keymap_dig");
	key[KeyType::PLACE] = getKeySetting("keymap_place");

	key[KeyType::AUTOFORWARD] = getKeySetting("keymap_autoforward");

	key[KeyType::DROP] = getKeySetting("keymap_drop");
	key[KeyType::INVENTORY] = getKeySetting("keymap_inventory");
	key[KeyType::CHAT] = getKeySetting("keymap_chat");
	key[KeyType::CMD] = getKeySetting("keymap_cmd");
	key[KeyType::CMD_LOCAL] = getKeySetting("keymap_cmd_local");
	key[KeyType::CONSOLE] = getKeySetting("keymap_console");
	key[KeyType::MINIMAP] = getKeySetting("keymap_minimap");
	key[KeyType::FREEMOVE] = getKeySetting("keymap_freemove");
	key[KeyType::PITCHMOVE] = getKeySetting("keymap_pitchmove");
	key[KeyType::FASTMOVE] = getKeySetting("keymap_fastmove");
	key[KeyType::NOCLIP] = getKeySetting("keymap_noclip");
	key[KeyType::HOTBAR_PREV] = getKeySetting("keymap_hotbar_previous");
	key[KeyType::HOTBAR_NEXT] = getKeySetting("keymap_hotbar_next");
	key[KeyType::MUTE] = getKeySetting("keymap_mute");
	key[KeyType::INC_VOLUME] = getKeySetting("keymap_increase_volume");
	key[KeyType::DEC_VOLUME] = getKeySetting("keymap_decrease_volume");
	key[KeyType::CINEMATIC] = getKeySetting("keymap_cinematic");
	key[KeyType::SCREENSHOT] = getKeySetting("keymap_screenshot");
	key[KeyType::TOGGLE_BLOCK_BOUNDS] = getKeySetting("keymap_toggle_block_bounds");
	key[KeyType::TOGGLE_HUD] = getKeySetting("keymap_toggle_hud");
	key[KeyType::TOGGLE_CHAT] = getKeySetting("keymap_toggle_chat");
	key[KeyType::TOGGLE_FOG] = getKeySetting("keymap_toggle_fog");
	key[KeyType::TOGGLE_UPDATE_CAMERA] = getKeySetting("keymap_toggle_update_camera");
	key[KeyType::TOGGLE_DEBUG] = getKeySetting("keymap_toggle_debug");
	key[KeyType::TOGGLE_PROFILER] = getKeySetting("keymap_toggle_profiler");
	key[KeyType::CAMERA_MODE] = getKeySetting("keymap_camera_mode");
	key[KeyType::INCREASE_VIEWING_RANGE] =
			getKeySetting("keymap_increase_viewing_range_min");
	key[KeyType::DECREASE_VIEWING_RANGE] =
			getKeySetting("keymap_decrease_viewing_range_min");
	key[KeyType::RANGESELECT] = getKeySetting("keymap_rangeselect");
	key[KeyType::ZOOM] = getKeySetting("keymap_zoom");

	key[KeyType::QUICKTUNE_NEXT] = getKeySetting("keymap_quicktune_next");
	key[KeyType::QUICKTUNE_PREV] = getKeySetting("keymap_quicktune_prev");
	key[KeyType::QUICKTUNE_INC] = getKeySetting("keymap_quicktune_inc");
	key[KeyType::QUICKTUNE_DEC] = getKeySetting("keymap_quicktune_dec");

	for (int i = 0; i < HUD_HOTBAR_ITEMCOUNT_MAX; i++) {
		std::string slot_key_name = "keymap_slot" + std::to_string(i + 1);
		key[KeyType::SLOT_1 + i] = getKeySetting(slot_key_name.c_str());
	}

	if (handler) {
		// First clear all keys, then re-add the ones we listen for
		handler->dontListenForKeys();
		for (const KeyPress &k : key) {
			handler->listenForKey(k);
		}
		handler->listenForKey(EscapeKey);
	}
}

bool MyEventReceiver::OnEvent(const SEvent &event)
{
	if (event.EventType == irr::EET_LOG_TEXT_EVENT) {
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
		const KeyPress keyCode(event.KeyInput);
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
	if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
		const KeyPress keyCode(event.KeyInput);
		if (keysListenedFor[keyCode]) {
			if (event.KeyInput.PressedDown) {
				if (!IsKeyDown(keyCode))
					keyWasPressed.set(keyCode);

				keyIsDown.set(keyCode);
				keyWasDown.set(keyCode);
			} else {
				if (IsKeyDown(keyCode))
					keyWasReleased.set(keyCode);

				keyIsDown.unset(keyCode);
			}

			return true;
		}

	} else if (g_touchcontrols && event.EventType == irr::EET_TOUCH_INPUT_EVENT) {
		// In case of touchcontrols, we have to handle different events
		g_touchcontrols->translateEvent(event);
		return true;
	} else if (event.EventType == irr::EET_JOYSTICK_INPUT_EVENT) {
		// joystick may be nullptr if game is launched with '--random-input' parameter
		return joystick && joystick->handleEvent(event.JoystickEvent);
	} else if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
		// Handle mouse events
		switch (event.MouseInput.Event) {
		case EMIE_LMOUSE_PRESSED_DOWN:
			keyIsDown.set(LMBKey);
			keyWasDown.set(LMBKey);
			keyWasPressed.set(LMBKey);
			break;
		case EMIE_MMOUSE_PRESSED_DOWN:
			keyIsDown.set(MMBKey);
			keyWasDown.set(MMBKey);
			keyWasPressed.set(MMBKey);
			break;
		case EMIE_RMOUSE_PRESSED_DOWN:
			keyIsDown.set(RMBKey);
			keyWasDown.set(RMBKey);
			keyWasPressed.set(RMBKey);
			break;
		case EMIE_LMOUSE_LEFT_UP:
			keyIsDown.unset(LMBKey);
			keyWasReleased.set(LMBKey);
			break;
		case EMIE_MMOUSE_LEFT_UP:
			keyIsDown.unset(MMBKey);
			keyWasReleased.set(MMBKey);
			break;
		case EMIE_RMOUSE_LEFT_UP:
			keyIsDown.unset(RMBKey);
			keyWasReleased.set(RMBKey);
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
	std::string key;
	float counter;
	int time_max;
};

void RandomInputHandler::step(float dtime)
{
	static RandomInputHandlerSimData rnd_data[] = {
		{ "keymap_jump", 0.0f, 40 },
		{ "keymap_aux1", 0.0f, 40 },
		{ "keymap_forward", 0.0f, 40 },
		{ "keymap_left", 0.0f, 40 },
		{ "keymap_dig", 0.0f, 30 },
		{ "keymap_place", 0.0f, 15 }
	};

	for (auto &i : rnd_data) {
		i.counter -= dtime;
		if (i.counter < 0.0) {
			i.counter = 0.1 * Rand(1, i.time_max);
			keydown.toggle(getKeySetting(i.key.c_str()));
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
