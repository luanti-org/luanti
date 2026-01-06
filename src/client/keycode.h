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

// Auxiliary struct used for keycode lookups
struct table_key;

/* A key press, consisting of a scancode or a keycode.
 * This fits into 64 bits, so prefer passing this by value.
*/
class KeyPress
{
public:
	enum class InputType {
		KEYBOARD, // Keyboard input (scancodes)
		LEGACY_KEYCODE, // (Deprecated) keyboard and mouse input based on EKEY_CODE
		GAME_ACTION, // GameKeyType input passed by touchscreen buttons
	};

	KeyPress() = default;

	KeyPress(const std::string &name);

	KeyPress(const SEvent::SKeyInput &in);

	KeyPress(GameKeyType key) : value(key) {}

	// Get a string representation that is suitable for use in minetest.conf
	std::string sym() const;

	// Get a human-readable string representation
	std::string name() const;

	// Get the scancode or 0 is one is not available
	u32 getScancode() const
	{
		if (auto pv = getIf<InputType::KEYBOARD>())
			return *pv;
		return 0;
	}

	bool operator==(KeyPress o) const {
		return value == o.value;
	}
	bool operator!=(KeyPress o) const {
		return !(*this == o);
	}

	// Used for e.g. std::set
	bool operator<(KeyPress o) const {
		return value < o.value;
	}

	// Get the type of input
	InputType getType() const {
		return static_cast<InputType>(value.index());
	}

	// Check whether the keypress is valid
	operator bool() const;

	static KeyPress getSpecialKey(const std::string &name);

private:
	using value_type = std::variant<u32, EKEY_CODE, GameKeyType>;
	template<InputType I>
	using value_alternative_t = std::variant_alternative_t<static_cast<size_t>(I), value_type>;

	bool loadFromScancode(const std::string &name);
	void loadFromKey(EKEY_CODE keycode, wchar_t keychar);
	const table_key &lookupScancode() const;

	value_type value = KEY_UNKNOWN;

	template<InputType I>
	value_alternative_t<I> get() const {
		return std::get<static_cast<size_t>(I)>(value);
	}

	template<InputType I>
	std::add_pointer_t<const value_alternative_t<I>> getIf() const {
		return std::get_if<static_cast<size_t>(I)>(&value);
	}

	template<InputType I>
	void emplace(value_alternative_t<I> newValue) {
		value.emplace<static_cast<size_t>(I)>(newValue);
	}

	friend std::hash<KeyPress>;
};

template <>
struct std::hash<KeyPress>
{
	size_t operator()(KeyPress kp) const noexcept {
		return std::hash<KeyPress::value_type>{}(kp.value);
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
