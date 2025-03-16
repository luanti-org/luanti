// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "exceptions.h"
#include "irrlichttypes.h"
#include <Keycodes.h>
#include <IEventReceiver.h>
#include <string>

class UnknownKeycode : public BaseException
{
public:
	UnknownKeycode(const std::string_view &s):
		BaseException(std::string(s)) {}
};

/* A key press, consisting of either an Irrlicht keycode
   or an actual char */

class KeyPress
{
public:
	KeyPress() = default;

	KeyPress(const std::string_view &name, bool merge_modifiers = true);

	KeyPress(const char *name, bool merge_modifiers = true): KeyPress(std::string_view(name), merge_modifiers) {};

	KeyPress(const irr::SEvent::SKeyInput &in, bool prefer_character = false, bool merge_modifiers = true);

	bool operator==(const KeyPress &o) const
	{
		return basic_equals(o) && shift == o.shift && control == o.control;
	}
	bool operator!=(const KeyPress &o) const
	{
		return !operator==(o);
	}

	bool basic_equals(const KeyPress &o) const
	{
		return (Char > 0 && Char == o.Char) || (valid_kcode(Key) && Key == o.Key)
			|| (Char == o.Char && Key == o.Key);
	}

	bool has_modifier() const
	{
		return shift || control;
	}

	bool valid_base() const
	{
		return valid_kcode(Key) || Char > 0;
	}

	KeyPress base() const
	{
		KeyPress kp = *this;
		kp.control = kp.shift = false;
		return kp;
	}

	bool is_shift_base() const
	{
		return is_shift_base(m_name);
	}

	static bool is_shift_base(const std::string_view &name)
	{
		return (name == "KEY_LSHIFT" || name == "KEY_RSHIFT" || name == "KEY_SHIFT");
	}

	bool is_control_base() const
	{
		return is_control_base(m_name);
	}

	static bool is_control_base(const std::string_view &name)
	{
		return (name == "KEY_LCONTROL" || name == "KEY_RCONTROL" || name == "KEY_CONTROL");
	}

	bool empty() const
	{
		return !(valid_base() || has_modifier());
	}

	int matches(const KeyPress &p) const;

	const std::string sym() const;
	const std::string name() const;

	bool shift = false;
	bool control = false;

protected:
	std::string_view parseModifiers(const std::string_view &str, bool merge_modifiers);

	static bool valid_kcode(irr::EKEY_CODE k)
	{
		return k > 0 && k < irr::KEY_KEY_CODES_COUNT;
	}

	irr::EKEY_CODE Key = irr::KEY_KEY_CODES_COUNT;
	wchar_t Char = L'\0';
	std::string m_name = "";
};

// Global defines for convenience

extern const KeyPress EscapeKey;

extern const KeyPress LMBKey;
extern const KeyPress MMBKey; // Middle Mouse Button
extern const KeyPress RMBKey;

// Key configuration getter
const KeyPress &getKeySetting(const char *settingname);

// Clear fast lookup cache
void clearKeyCache();

irr::EKEY_CODE keyname_to_keycode(const char *name);
