// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include "joystick_controller.h"
#include <list>
#include <unordered_set>
#include "keycode.h"

class InputHandler;

enum class PointerType {
	Mouse,
	Touch,
};

/****************************************************************************
 Fast key cache for main game loop
 ****************************************************************************/

/* This is faster than using getKeySetting with the tradeoff that functions
 * using it must make sure that it's initialised before using it and there is
 * no error handling (for example bounds checking). This is really intended for
 * use only in the main running loop of the client (the_game()) where the faster
 * (up to 10x faster) key lookup is an asset. Other parts of the codebase
 * (e.g. formspecs) should continue using getKeySetting().
 */
struct KeyCache
{

	KeyCache()
	{
		handler = NULL;
		populate();
		populate_nonchanging();
	}

	void populate();

	// Keys that are not settings dependent
	void populate_nonchanging();

	KeyPress key[KeyType::INTERNAL_ENUM_COUNT];
	InputHandler *handler;
};

class MyEventReceiver : public IEventReceiver
{
public:
	// This is the one method that we have to implement
	virtual bool OnEvent(const SEvent &event);

	bool IsKeyDown(const KeyPress &keyCode) const { return keyIsDown.find(keyCode) != keyIsDown.end(); }

	// Checks whether a key was down and resets the state
	bool WasKeyDown(const KeyPress &keyCode)
	{
		bool b = keyWasDown.find(keyCode) != keyWasDown.end();
		if (b)
			keyWasDown.erase(keyCode);
		return b;
	}

	// Checks whether a key was just pressed. State will be cleared
	// in the subsequent iteration of Game::processPlayerInteraction
	bool WasKeyPressed(const KeyPress &keycode) const { return keyWasPressed.find(keycode) != keyWasPressed.end(); }

	// Checks whether a key was just released. State will be cleared
	// in the subsequent iteration of Game::processPlayerInteraction
	bool WasKeyReleased(const KeyPress &keycode) const { return keyWasReleased.find(keycode) != keyWasReleased.end(); }

	void listenForKey(const KeyPress &keyCode)
	{
		keysListenedFor.insert(keyCode);
	}
	void dontListenForKeys()
	{
		keysListenedFor.clear();
	}

	s32 getMouseWheel()
	{
		s32 a = mouse_wheel;
		mouse_wheel = 0;
		return a;
	}

	void clearInput()
	{
		keyIsDown.clear();
		keyWasDown.clear();
		keyWasPressed.clear();
		keyWasReleased.clear();

		mouse_wheel = 0;
	}

	void releaseAllKeys()
	{
		keyWasReleased.merge(keyIsDown);
		keyIsDown.clear();
	}

	void clearWasKeyPressed()
	{
		keyWasPressed.clear();
	}

	void clearWasKeyReleased()
	{
		keyWasReleased.clear();
	}

	JoystickController *joystick = nullptr;

	PointerType getLastPointerType() { return last_pointer_type; }

private:
	s32 mouse_wheel = 0;

	// The current state of keys
	std::unordered_set<KeyPress> keyIsDown;

	// Like keyIsDown but only reset when that key is read
	std::unordered_set<KeyPress> keyWasDown;

	// Whether a key has just been pressed
	std::unordered_set<KeyPress> keyWasPressed;

	// Whether a key has just been released
	std::unordered_set<KeyPress> keyWasReleased;

	// List of keys we listen for
	std::unordered_set<KeyPress> keysListenedFor;

	// Intentionally not reset by clearInput/releaseAllKeys.
	bool fullscreen_is_down = false;

	PointerType last_pointer_type = PointerType::Mouse;
};

class InputHandler
{
public:
	InputHandler()
	{
		keycache.handler = this;
		keycache.populate();
	}

	virtual ~InputHandler() = default;

	virtual bool isRandom() const
	{
		return false;
	}

	virtual bool isKeyDown(GameKeyType k) = 0;
	virtual bool wasKeyDown(GameKeyType k) = 0;
	virtual bool wasKeyPressed(GameKeyType k) = 0;
	virtual bool wasKeyReleased(GameKeyType k) = 0;
	virtual bool cancelPressed() = 0;

	virtual float getJoystickSpeed() = 0;
	virtual float getJoystickDirection() = 0;

	virtual void clearWasKeyPressed() {}
	virtual void clearWasKeyReleased() {}

	virtual void listenForKey(const KeyPress &keyCode) {}
	virtual void dontListenForKeys() {}

	virtual v2s32 getMousePos() = 0;
	virtual void setMousePos(s32 x, s32 y) = 0;

	virtual s32 getMouseWheel() = 0;

	virtual void step(float dtime) {}

	virtual void clear() {}
	virtual void releaseAllKeys() {}

	JoystickController joystick;
	KeyCache keycache;
};

/*
	Separated input handler implementations
*/

class RealInputHandler final : public InputHandler
{
public:
	RealInputHandler(MyEventReceiver *receiver) : m_receiver(receiver)
	{
		m_receiver->joystick = &joystick;
	}

	virtual ~RealInputHandler()
	{
		m_receiver->joystick = nullptr;
	}

	virtual bool isKeyDown(GameKeyType k)
	{
		return m_receiver->IsKeyDown(keycache.key[k]) || joystick.isKeyDown(k);
	}
	virtual bool wasKeyDown(GameKeyType k)
	{
		return m_receiver->WasKeyDown(keycache.key[k]) || joystick.wasKeyDown(k);
	}
	virtual bool wasKeyPressed(GameKeyType k)
	{
		return m_receiver->WasKeyPressed(keycache.key[k]) || joystick.wasKeyPressed(k);
	}
	virtual bool wasKeyReleased(GameKeyType k)
	{
		return m_receiver->WasKeyReleased(keycache.key[k]) || joystick.wasKeyReleased(k);
	}

	virtual float getJoystickSpeed();

	virtual float getJoystickDirection();

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

	virtual void listenForKey(const KeyPress &keyCode)
	{
		m_receiver->listenForKey(keyCode);
	}
	virtual void dontListenForKeys()
	{
		m_receiver->dontListenForKeys();
	}

	virtual v2s32 getMousePos();
	virtual void setMousePos(s32 x, s32 y);

	virtual s32 getMouseWheel()
	{
		return m_receiver->getMouseWheel();
	}

	void clear()
	{
		joystick.clear();
		m_receiver->clearInput();
	}

	void releaseAllKeys()
	{
		joystick.releaseAllKeys();
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

	virtual bool isKeyDown(GameKeyType k) { return keydown[k]; }
	virtual bool wasKeyDown(GameKeyType k) { return false; }
	virtual bool wasKeyPressed(GameKeyType k) { return false; }
	virtual bool wasKeyReleased(GameKeyType k) { return false; }
	virtual bool cancelPressed() { return false; }
	virtual float getJoystickSpeed() { return joystickSpeed; }
	virtual float getJoystickDirection() { return joystickDirection; }
	virtual v2s32 getMousePos() { return mousepos; }
	virtual void setMousePos(s32 x, s32 y) { mousepos = v2s32(x, y); }

	virtual s32 getMouseWheel() { return 0; }

	virtual void step(float dtime);

	s32 Rand(s32 min, s32 max);

private:
	std::bitset<GameKeyType::INTERNAL_ENUM_COUNT> keydown;
	v2s32 mousepos;
	v2s32 mousespeed;
	float joystickSpeed;
	float joystickDirection;
};
