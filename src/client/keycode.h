// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "keys.h"
#include <Keycodes.h>
#include <IEventReceiver.h>
#include <string>
#include <variant>
#include <vector>

/* A key press, consisting of a scancode or a keycode.
 * This fits into 64 bits, so prefer passing this by value.
*/
class KeyPress
{
public:
	enum InputType {
		SCANCODE_INPUT, // Keyboard input (scancodes)
		KEYCODE_INPUT, // (Deprecated) EKEY_CODE-based keyboard and mouse input
		GAME_ACTION_INPUT, // GameKeyType input passed by touchscreen buttons
	};

	KeyPress() = default;

	KeyPress(const std::string &name);

	KeyPress(const SEvent::SKeyInput &in);

	KeyPress(GameKeyType key) : scancode(key) {}

	// Get a string representation that is suitable for use in minetest.conf
	std::string sym() const;

	// Get a human-readable string representation
	std::string name() const;

	// Get the corresponding keycode or KEY_UNKNOWN if one is not available
	EKEY_CODE getKeycode() const;

	// Get the corresponding keychar or '\0' if one is not available
	wchar_t getKeychar() const;

	// Get the scancode or 0 is one is not available
	u32 getScancode() const
	{
		if (auto pv = std::get_if<u32>(&scancode))
			return *pv;
		return 0;
	}

	bool operator==(KeyPress o) const {
		return scancode == o.scancode;
	}
	bool operator!=(KeyPress o) const {
		return !(*this == o);
	}

	// Used for e.g. std::set
	bool operator<(KeyPress o) const {
		return scancode < o.scancode;
	}

	InputType getType() const {
		return static_cast<InputType>(scancode.index());
	}

	// Check whether the keypress is valid
	operator bool() const;

	static KeyPress getSpecialKey(const std::string &name);

private:
	struct table_key { // internal keycode lookup table
		std::string Name; // An EKEY_CODE 'symbol' name as a string
		EKEY_CODE Key;
		wchar_t Char; // L'\0' means no character assigned
		std::string LangName; // empty string means it doesn't have a human description
	};
	static const table_key invalid_key;
	static std::vector<table_key> keycode_table;
	static const table_key &lookupKeychar(wchar_t Char);
	static const table_key &lookupKeykey(EKEY_CODE key);
	static const table_key &lookupKeyname(std::string_view name);
	static const table_key &lookupScancode(const u32 scancode);
	const table_key &lookupScancode() const;

	using value_type = std::variant<u32, EKEY_CODE, GameKeyType>;

	bool loadFromScancode(const std::string &name);
	void loadFromKey(EKEY_CODE keycode, wchar_t keychar);
	std::string formatScancode() const;

	value_type scancode = KEY_UNKNOWN;

	friend std::hash<KeyPress>;
};

template <>
struct std::hash<KeyPress>
{
	size_t operator()(KeyPress kp) const noexcept {
		return std::hash<KeyPress::value_type>{}(kp.scancode);
	}
};

// Global defines for convenience
// This implementation defers creation of the objects to make sure that the
// IrrlichtDevice is initialized.
#define EscapeKey KeyPress::getSpecialKey("KEY_ESCAPE")
#define LMBKey KeyPress::getSpecialKey("KEY_LBUTTON")
#define MMBKey KeyPress::getSpecialKey("KEY_MBUTTON") // Middle Mouse Button
#define RMBKey KeyPress::getSpecialKey("KEY_RBUTTON")

// Key configuration getter
// Note that the reference may be invalidated by a next call to getKeySetting
// or a related function, so the value should either be used immediately or
// copied elsewhere before calling this again.
const std::vector<KeyPress> &getKeySetting(const std::string &settingname);

// Check whether the key setting includes a key.
bool keySettingHasMatch(const std::string &settingname, KeyPress kp);

// Clear fast lookup cache
void clearKeyCache();
