// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <Keycodes.h>
#include <IEventReceiver.h>
#include <string>
#include <variant>

/* A key press, consisting of a scancode or a keycode.
 * This fits into 64 bits, so prefer passing this by value.
*/
class KeyPress
{
public:
	KeyPress() = default;

	KeyPress(const std::string &name);

	KeyPress(const SEvent::SKeyInput &in);

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

	// Check whether the keypress is valid
	operator bool() const
	{
		return std::holds_alternative<EKEY_CODE>(scancode) ?
			Keycode::isValid(std::get<EKEY_CODE>(scancode)) :
			std::get<u32>(scancode) != 0;
	}

	static KeyPress getSpecialKey(const std::string &name);

private:
	using value_type = std::variant<u32, EKEY_CODE>;
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
KeyPress getKeySetting(const std::string &settingname);

// Clear fast lookup cache
void clearKeyCache();
