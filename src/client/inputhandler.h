// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include <array>
#include <bitset>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>
#include "keycode.h"
#include "settings.h"
#include "util/string.h"

class InputHandler;

enum class PointerType {
	Mouse,
	Touch,
};

class MyEventReceiver : public IEventReceiver
{
public:
	// This is the one method that we have to implement
	virtual bool OnEvent(const SEvent &event);

	// Gets the analog value corresponding to a key
	float GetAxisValue(GameKeyType key) const { return axisValues[key]; }

	// Checks whether a key is held down
	bool IsKeyDown(GameKeyType key) const
	{
		return GetAxisValue(key) > 0;
	}

	// Checks whether a key was down and resets the state
	bool WasKeyDown(GameKeyType key);

	// Checks whether a key was just pressed. State will be cleared
	// in the subsequent iteration of Game::processPlayerInteraction
	bool WasKeyPressed(GameKeyType key) const { return keyWasPressed[key]; }

	// Checks whether a key was just released. State will be cleared
	// in the subsequent iteration of Game::processPlayerInteraction
	bool WasKeyReleased(GameKeyType key) const { return keyWasReleased[key]; }

	void reloadKeybindings();

	s32 getMouseWheel()
	{
		s32 a = mouse_wheel;
		mouse_wheel = 0;
		return a;
	}

	void clearInput()
	{
		physicalKeyDown.clear();
		axisValues.fill(0);
		keyWasDown.reset();
		keyWasPressed.reset();
		keyWasReleased.reset();

		mouse_wheel = 0;
	}

	void releaseAllKeys()
	{
		physicalKeyDown.clear();
		for (size_t i = 0; i < KeyType::INTERNAL_ENUM_COUNT; i++)
			keyWasReleased[i] = keyWasReleased[i] || axisValues[i] > 0;
		axisValues.fill(0);
	}

	void clearWasKeyPressed()
	{
		keyWasPressed.reset();
	}

	void clearWasKeyReleased()
	{
		keyWasReleased.reset();
	}

	PointerType getLastPointerType() { return last_pointer_type; }

private:
	void listenForKey(KeyPress keyCode, GameKeyType action)
	{
		if (keyCode)
			keysListenedFor[keyCode] = action;
	}

	bool setKeyDown(KeyPress keyCode, float value);
	void setKeyDown(GameKeyType action, std::pair<float, bool> new_state);
	std::pair<float, bool> checkKeyDown(GameKeyType action) const;

	struct Keybinding {
		std::vector<KeyPress> keys;
		struct {
			float keyboard_mouse = 1.0f;
			float joystick = 1.0f;
		} scale;

		Keybinding() = default;
		Keybinding(const std::vector<KeyPress> &in_keys): keys(in_keys) {}

		inline float getScale(KeyPress::InputType input_type) const
		{
			switch (input_type) {
			case KeyPress::InputType::KEYBOARD:
			case KeyPress::InputType::MOUSE_BUTTON:
				return scale.keyboard_mouse;
			case KeyPress::InputType::GAMEPAD_BUTTON:
			case KeyPress::InputType::GAMEPAD_AXIS_PLUS:
			case KeyPress::InputType::GAMEPAD_AXIS_MINUS:
				return scale.joystick;
			default:
				return 1.0f;
			}
		}
	};

	struct PhysicalKeyState {
		float analog_value;
		f64 last_binary_update; // The last time the binary state ("is this pressed?") is updated
	};

	/* This is faster than using getKeySetting with the tradeoff that functions
	 * using it must make sure that it's initialised before using it and there is
	 * no error handling (for example bounds checking). This is useful here as the
	 * faster (up to 10x faster) key lookup is an asset.
	 */
	std::array<Keybinding, KeyType::INTERNAL_ENUM_COUNT> keybindings;

	// Repetition interval for joystick input
	float repeat_joystick_button_time = 0.0f;

	s32 mouse_wheel = 0;

	// The current state of physical keys.
	std::map<KeyPress, PhysicalKeyState> physicalKeyDown;

	// The current state of keys
	std::array<float, GameKeyType::INTERNAL_ENUM_COUNT> axisValues;

	// Like axisValues but only reset when that key is read
	std::bitset<GameKeyType::INTERNAL_ENUM_COUNT> keyWasDown;

	// Whether a key has just been pressed
	std::bitset<GameKeyType::INTERNAL_ENUM_COUNT> keyWasPressed;

	// Whether a key has just been released
	std::bitset<GameKeyType::INTERNAL_ENUM_COUNT> keyWasReleased;

	// List of keys we listen for
	std::unordered_map<KeyPress, GameKeyType> keysListenedFor;

	// Intentionally not reset by clearInput/releaseAllKeys.
	bool fullscreen_is_down = false;

	bool close_world_down = false;
	bool esc_down = false;

	PointerType last_pointer_type = PointerType::Mouse;
};

class InputHandler
{
public:
	InputHandler();

	virtual ~InputHandler() = default;

	virtual bool isRandom() const
	{
		return false;
	}

	static inline s16 analogToInt(float value)
	{
		return value > 0 ? std::min(1.0f, value) * INT16_MAX : 0;
	}

	static inline float intToAnalog(s16 value)
	{
		return value > 0 ? value / (f32)INT16_MAX : 0.0f;
	}

	virtual float getAxisValue(GameKeyType k) = 0;
	virtual bool isKeyDown(GameKeyType k)
	{
		return getAxisValue(k) > 0;
	}
	virtual bool wasKeyDown(GameKeyType k) = 0;
	virtual bool wasKeyPressed(GameKeyType k) = 0;
	virtual bool wasKeyReleased(GameKeyType k) = 0;
	virtual bool cancelPressed() = 0;

	virtual void clearWasKeyPressed() {}
	virtual void clearWasKeyReleased() {}

	virtual void reloadKeybindings() {}

	virtual v2s32 getMousePos() = 0;
	virtual void setMousePos(s32 x, s32 y) = 0;

	virtual s32 getMouseWheel() = 0;

	virtual void step(float dtime) {}

	virtual void clear() {}
	virtual void releaseAllKeys() {}

	static void settingChangedCallback(const std::string &name, void *data)
	{
		static_cast<InputHandler *>(data)->reloadKeybindings();
	}
};

/*
	Separated input handler implementations
*/

class RealInputHandler final : public InputHandler
{
public:
	RealInputHandler(MyEventReceiver *receiver) : m_receiver(receiver)
	{
		m_receiver->reloadKeybindings();
	}

	virtual float getAxisValue(GameKeyType k)
	{
		return m_receiver->GetAxisValue(k);
	}
	virtual bool wasKeyDown(GameKeyType k)
	{
		return m_receiver->WasKeyDown(k);
	}
	virtual bool wasKeyPressed(GameKeyType k)
	{
		return m_receiver->WasKeyPressed(k);
	}
	virtual bool wasKeyReleased(GameKeyType k)
	{
		return m_receiver->WasKeyReleased(k);
	}

	virtual bool cancelPressed()
	{
		return wasKeyDown(KeyType::ESC);
	}

	virtual void clearWasKeyPressed()
	{
		m_receiver->clearWasKeyPressed();
	}
	virtual void clearWasKeyReleased()
	{
		m_receiver->clearWasKeyReleased();
	}

	virtual void reloadKeybindings()
	{
		m_receiver->reloadKeybindings();
	}

	virtual v2s32 getMousePos();
	virtual void setMousePos(s32 x, s32 y);

	virtual s32 getMouseWheel()
	{
		return m_receiver->getMouseWheel();
	}

	void clear()
	{
		m_receiver->clearInput();
	}

	void releaseAllKeys()
	{
		m_receiver->releaseAllKeys();
	}

private:
	MyEventReceiver *m_receiver = nullptr;
	v2s32 m_mousepos;
};

class RandomInputHandler final : public InputHandler
{
public:
	RandomInputHandler() = default;

	bool isRandom() const
	{
		return true;
	}

	virtual float getAxisValue(GameKeyType k) { return keydown[k]; }
	virtual bool wasKeyDown(GameKeyType k) { return false; }
	virtual bool wasKeyPressed(GameKeyType k) { return false; }
	virtual bool wasKeyReleased(GameKeyType k) { return false; }
	virtual bool cancelPressed() { return false; }
	virtual v2s32 getMousePos() { return mousepos; }
	virtual void setMousePos(s32 x, s32 y) { mousepos = v2s32(x, y); }

	virtual s32 getMouseWheel() { return 0; }

	virtual void step(float dtime);

	s32 Rand(s32 min, s32 max);

private:
	std::bitset<GameKeyType::INTERNAL_ENUM_COUNT> keydown;
	v2s32 mousepos;
	v2s32 mousespeed;
};
