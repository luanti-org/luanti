// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "gettext.h"
#include "keycode.h"
#include "settings.h"
#include "log.h"
#include "debug.h"
#include "util/hex.h"
#include "util/string.h"
#include "util/basic_macros.h"

struct table_key {
	const char *Name;
	irr::EKEY_CODE Key;
	wchar_t Char; // L'\0' means no character assigned
	const char *LangName; // NULL means it doesn't have a human description
};

#define DEFINEKEY1(x, lang) /* Irrlicht key without character */ \
	{ #x, irr::x, L'\0', lang },
#define DEFINEKEY2(x, ch, lang) /* Irrlicht key with character */ \
	{ #x, irr::x, ch, lang },
#define DEFINEKEY3(ch) /* single Irrlicht key (e.g. KEY_KEY_X) */ \
	{ "KEY_KEY_" TOSTRING(ch), irr::KEY_KEY_ ## ch, (wchar_t) *TOSTRING(ch), TOSTRING(ch) },
#define DEFINEKEY4(ch) /* single Irrlicht function key (e.g. KEY_F3) */ \
	{ "KEY_F" TOSTRING(ch), irr::KEY_F ## ch, L'\0', "F" TOSTRING(ch) },
#define DEFINEKEY5(ch) /* key without Irrlicht keycode */ \
	{ ch, irr::KEY_KEY_CODES_COUNT, (wchar_t) *ch, ch },

static const struct table_key table[] = {
	// Keys that can be reliably mapped between Char and Key
	DEFINEKEY3(0)
	DEFINEKEY3(1)
	DEFINEKEY3(2)
	DEFINEKEY3(3)
	DEFINEKEY3(4)
	DEFINEKEY3(5)
	DEFINEKEY3(6)
	DEFINEKEY3(7)
	DEFINEKEY3(8)
	DEFINEKEY3(9)
	DEFINEKEY3(A)
	DEFINEKEY3(B)
	DEFINEKEY3(C)
	DEFINEKEY3(D)
	DEFINEKEY3(E)
	DEFINEKEY3(F)
	DEFINEKEY3(G)
	DEFINEKEY3(H)
	DEFINEKEY3(I)
	DEFINEKEY3(J)
	DEFINEKEY3(K)
	DEFINEKEY3(L)
	DEFINEKEY3(M)
	DEFINEKEY3(N)
	DEFINEKEY3(O)
	DEFINEKEY3(P)
	DEFINEKEY3(Q)
	DEFINEKEY3(R)
	DEFINEKEY3(S)
	DEFINEKEY3(T)
	DEFINEKEY3(U)
	DEFINEKEY3(V)
	DEFINEKEY3(W)
	DEFINEKEY3(X)
	DEFINEKEY3(Y)
	DEFINEKEY3(Z)
	DEFINEKEY2(KEY_PLUS, L'+', "+")
	DEFINEKEY2(KEY_COMMA, L',', ",")
	DEFINEKEY2(KEY_MINUS, L'-', "-")
	DEFINEKEY2(KEY_PERIOD, L'.', ".")

	// Keys without a Char
	DEFINEKEY1(KEY_LBUTTON, N_("Left Button"))
	DEFINEKEY1(KEY_RBUTTON, N_("Right Button"))
	//~ Usually paired with the Pause key
	DEFINEKEY1(KEY_CANCEL, N_("Break Key"))
	DEFINEKEY1(KEY_MBUTTON, N_("Middle Button"))
	DEFINEKEY1(KEY_XBUTTON1, N_("X Button 1"))
	DEFINEKEY1(KEY_XBUTTON2, N_("X Button 2"))
	DEFINEKEY1(KEY_BACK, N_("Backspace"))
	DEFINEKEY1(KEY_TAB, N_("Tab"))
	DEFINEKEY1(KEY_CLEAR, N_("Clear Key"))
	DEFINEKEY1(KEY_RETURN, N_("Return Key"))
	DEFINEKEY1(KEY_SHIFT, N_("Shift Key"))
	DEFINEKEY1(KEY_CONTROL, N_("Control Key"))
	//~ Key name, common on Windows keyboards
	DEFINEKEY1(KEY_MENU, N_("Menu Key"))
	//~ Usually paired with the Break key
	DEFINEKEY1(KEY_PAUSE, N_("Pause Key"))
	DEFINEKEY1(KEY_CAPITAL, N_("Caps Lock"))
	DEFINEKEY1(KEY_SPACE, N_("Space"))
	DEFINEKEY1(KEY_PRIOR, N_("Page Up"))
	DEFINEKEY1(KEY_NEXT, N_("Page Down"))
	DEFINEKEY1(KEY_END, N_("End"))
	DEFINEKEY1(KEY_HOME, N_("Home"))
	DEFINEKEY1(KEY_LEFT, N_("Left Arrow"))
	DEFINEKEY1(KEY_UP, N_("Up Arrow"))
	DEFINEKEY1(KEY_RIGHT, N_("Right Arrow"))
	DEFINEKEY1(KEY_DOWN, N_("Down Arrow"))
	//~ Key name
	DEFINEKEY1(KEY_SELECT, N_("Select"))
	//~ "Print screen" key
	DEFINEKEY1(KEY_PRINT, N_("Print"))
	DEFINEKEY1(KEY_EXECUT, N_("Execute"))
	DEFINEKEY1(KEY_SNAPSHOT, N_("Snapshot"))
	DEFINEKEY1(KEY_INSERT, N_("Insert"))
	DEFINEKEY1(KEY_DELETE, N_("Delete Key"))
	DEFINEKEY1(KEY_HELP, N_("Help"))
	DEFINEKEY1(KEY_LWIN, N_("Left Windows"))
	DEFINEKEY1(KEY_RWIN, N_("Right Windows"))
	DEFINEKEY1(KEY_NUMPAD0, N_("Numpad 0")) // These are not assigned to a char
	DEFINEKEY1(KEY_NUMPAD1, N_("Numpad 1")) // to prevent interference with KEY_KEY_[0-9].
	DEFINEKEY1(KEY_NUMPAD2, N_("Numpad 2"))
	DEFINEKEY1(KEY_NUMPAD3, N_("Numpad 3"))
	DEFINEKEY1(KEY_NUMPAD4, N_("Numpad 4"))
	DEFINEKEY1(KEY_NUMPAD5, N_("Numpad 5"))
	DEFINEKEY1(KEY_NUMPAD6, N_("Numpad 6"))
	DEFINEKEY1(KEY_NUMPAD7, N_("Numpad 7"))
	DEFINEKEY1(KEY_NUMPAD8, N_("Numpad 8"))
	DEFINEKEY1(KEY_NUMPAD9, N_("Numpad 9"))
	DEFINEKEY1(KEY_MULTIPLY, N_("Numpad *"))
	DEFINEKEY1(KEY_ADD, N_("Numpad +"))
	DEFINEKEY1(KEY_SEPARATOR, N_("Numpad ."))
	DEFINEKEY1(KEY_SUBTRACT, N_("Numpad -"))
	DEFINEKEY1(KEY_DECIMAL, NULL)
	DEFINEKEY1(KEY_DIVIDE, N_("Numpad /"))
	DEFINEKEY4(1)
	DEFINEKEY4(2)
	DEFINEKEY4(3)
	DEFINEKEY4(4)
	DEFINEKEY4(5)
	DEFINEKEY4(6)
	DEFINEKEY4(7)
	DEFINEKEY4(8)
	DEFINEKEY4(9)
	DEFINEKEY4(10)
	DEFINEKEY4(11)
	DEFINEKEY4(12)
	DEFINEKEY4(13)
	DEFINEKEY4(14)
	DEFINEKEY4(15)
	DEFINEKEY4(16)
	DEFINEKEY4(17)
	DEFINEKEY4(18)
	DEFINEKEY4(19)
	DEFINEKEY4(20)
	DEFINEKEY4(21)
	DEFINEKEY4(22)
	DEFINEKEY4(23)
	DEFINEKEY4(24)
	DEFINEKEY1(KEY_NUMLOCK, N_("Num Lock"))
	DEFINEKEY1(KEY_SCROLL, N_("Scroll Lock"))
	DEFINEKEY1(KEY_LSHIFT, N_("Left Shift"))
	DEFINEKEY1(KEY_RSHIFT, N_("Right Shift"))
	DEFINEKEY1(KEY_LCONTROL, N_("Left Control"))
	DEFINEKEY1(KEY_RCONTROL, N_("Right Control"))
	DEFINEKEY1(KEY_LMENU, N_("Left Menu"))
	DEFINEKEY1(KEY_RMENU, N_("Right Menu"))

	// Rare/weird keys
	DEFINEKEY1(KEY_KANA, "Kana")
	DEFINEKEY1(KEY_HANGUEL, "Hangul")
	DEFINEKEY1(KEY_HANGUL, "Hangul")
	DEFINEKEY1(KEY_JUNJA, "Junja")
	DEFINEKEY1(KEY_FINAL, "Final")
	DEFINEKEY1(KEY_KANJI, "Kanji")
	DEFINEKEY1(KEY_HANJA, "Hanja")
	DEFINEKEY1(KEY_ESCAPE, N_("IME Escape"))
	DEFINEKEY1(KEY_CONVERT, N_("IME Convert"))
	DEFINEKEY1(KEY_NONCONVERT, N_("IME Nonconvert"))
	DEFINEKEY1(KEY_ACCEPT, N_("IME Accept"))
	DEFINEKEY1(KEY_MODECHANGE, N_("IME Mode Change"))
	DEFINEKEY1(KEY_APPS, N_("Apps"))
	DEFINEKEY1(KEY_SLEEP, N_("Sleep"))
	DEFINEKEY1(KEY_OEM_1, "OEM 1") // KEY_OEM_[0-9] and KEY_OEM_102 are assigned to multiple
	DEFINEKEY1(KEY_OEM_2, "OEM 2") // different chars (on different platforms too) and thus w/o char
	DEFINEKEY1(KEY_OEM_3, "OEM 3")
	DEFINEKEY1(KEY_OEM_4, "OEM 4")
	DEFINEKEY1(KEY_OEM_5, "OEM 5")
	DEFINEKEY1(KEY_OEM_6, "OEM 6")
	DEFINEKEY1(KEY_OEM_7, "OEM 7")
	DEFINEKEY1(KEY_OEM_8, "OEM 8")
	DEFINEKEY1(KEY_OEM_AX, "OEM AX")
	DEFINEKEY1(KEY_OEM_102, "OEM 102")
	DEFINEKEY1(KEY_ATTN, "Attn")
	DEFINEKEY1(KEY_CRSEL, "CrSel")
	DEFINEKEY1(KEY_EXSEL, "ExSel")
	DEFINEKEY1(KEY_EREOF, N_("Erase EOF"))
	DEFINEKEY1(KEY_PLAY, N_("Play"))
	DEFINEKEY1(KEY_ZOOM, N_("Zoom Key"))
	DEFINEKEY1(KEY_PA1, "PA1")
	DEFINEKEY1(KEY_OEM_CLEAR, N_("OEM Clear"))

	// Keys without Irrlicht keycode
	DEFINEKEY5("!")
	DEFINEKEY5("\"")
	DEFINEKEY5("#")
	DEFINEKEY5("$")
	DEFINEKEY5("%")
	DEFINEKEY5("&")
	DEFINEKEY5("'")
	DEFINEKEY5("(")
	DEFINEKEY5(")")
	DEFINEKEY5("*")
	DEFINEKEY5("/")
	DEFINEKEY5(":")
	DEFINEKEY5(";")
	DEFINEKEY5("<")
	DEFINEKEY5("=")
	DEFINEKEY5(">")
	DEFINEKEY5("?")
	DEFINEKEY5("@")
	DEFINEKEY5("[")
	DEFINEKEY5("\\")
	DEFINEKEY5("]")
	DEFINEKEY5("^")
	DEFINEKEY5("_")
};

#undef N_


static const table_key &lookup_keyname(const std::string_view &name)
{
	for (const auto &table_key : table) {
		if (table_key.Name == name)
			return table_key;
	}

	throw UnknownKeycode(name);
}

static const table_key &lookup_keykey(irr::EKEY_CODE key)
{
	for (const auto &table_key : table) {
		if (table_key.Key == key)
			return table_key;
	}

	std::ostringstream os;
	os << "<Keycode " << (int) key << ">";
	throw UnknownKeycode(os.str().c_str());
}

static const table_key &lookup_keychar(wchar_t Char)
{
	for (const auto &table_key : table) {
		if (table_key.Char == Char)
			return table_key;
	}

	std::ostringstream os;
	os << "<Char " << hex_encode((char*) &Char, sizeof(wchar_t)) << ">";
	throw UnknownKeycode(os.str().c_str());
}

std::string_view KeyPress::parseModifiers(const std::string_view &name, bool merge_modifiers)
{
	if (str_starts_with(name, "KEY_CONTROL-")) {
		control = true;
		return parseModifiers(name.substr(12), merge_modifiers);
	} else if (str_starts_with(name, "KEY_SHIFT-")) {
		shift = true;
		return parseModifiers(name.substr(10), merge_modifiers);
	}
	if (merge_modifiers) {
		if (is_control_base(name)) {
			control = true;
			return "";
		} else if (is_shift_base(name)) {
			shift = true;
			return "";
		}
	}
	return name;
}

KeyPress::KeyPress(const std::string_view &name, bool merge_modifiers)
{
	auto keyname = parseModifiers(name, merge_modifiers);
	auto wkeyname = utf8_to_wide(keyname);
	if (wkeyname.empty()) {
		Key = irr::KEY_KEY_CODES_COUNT;
		Char = L'\0';
		m_name = "";
		return;
	} else if (wkeyname.size() == 1) {
		// Lookup by resulting character
		Char = wkeyname[0];
		try {
			auto &k = lookup_keychar(Char);
			m_name = k.Name;
			Key = k.Key;
			return;
		} catch (UnknownKeycode &e) {};
	} else {
		// Lookup by name
		m_name = keyname;
		try {
			auto &k = lookup_keyname(keyname);
			Key = k.Key;
			Char = k.Char;
			return;
		} catch (UnknownKeycode &e) {};
	}

	// It's not a known key, complain and try to do something
	Key = irr::KEY_KEY_CODES_COUNT;
	Char = wkeyname[0];
	m_name = "";
	warningstream << "KeyPress: Unknown key '" << keyname
		<< "', falling back to first char." << std::endl;
}

KeyPress::KeyPress(const irr::SEvent::SKeyInput &in, bool prefer_character, bool merge_modifiers):
	shift(in.Shift), control(in.Control)
{
	if (merge_modifiers) {
		switch (in.Key) {
			case irr::KEY_SHIFT: case irr::KEY_LSHIFT: case irr::KEY_RSHIFT:
				shift = true;
				return;
			case irr::KEY_CONTROL: case irr::KEY_LCONTROL: case irr::KEY_RCONTROL:
				control = true;
				return;
			default:
				break;
		}
	}

	if (prefer_character)
		Key = irr::KEY_KEY_CODES_COUNT;
	else
		Key = in.Key;
	Char = in.Char;

	try {
		if (valid_kcode(Key))
			m_name = lookup_keykey(Key).Name;
		else
			m_name = lookup_keychar(Char).Name;
	} catch (UnknownKeycode &e) {
		m_name.clear();
	};
}

const std::string KeyPress::sym() const
{
	std::string sym = m_name;
	if (shift)
		sym = sym.empty() ? "KEY_SHIFT" : "KEY_SHIFT-" + sym;
	if (control)
		sym = sym.empty() ? "KEY_CONTROL" : "KEY_CONTROL-" + sym;
	return sym;
}

const std::string KeyPress::name() const
{
	if (m_name.empty() && !has_modifier())
		return "";
	std::string ret;
	if (valid_kcode(Key))
		ret = lookup_keykey(Key).LangName;
	else if (Char != L'\0')
		ret = lookup_keychar(Char).LangName;
	if (!ret.empty())
		ret = strgettext(ret);
	if (shift)
		ret = ret.empty() ? gettext("Shift Key") : fmtgettext("Shift-%s", ret.c_str());
	if (control)
		ret = ret.empty() ? gettext("Control Key") : fmtgettext("Control-%s", ret.c_str());
	if (ret.empty())
		ret = gettext("<Unnamed key>");
	return ret;
}

int KeyPress::matches(const KeyPress &p) const {
	if (!basic_equals(p) && p.valid_base())
		return 0;
	if ((p.shift && !shift) || (p.control && !control))
		return 0;
	int score = basic_equals(p);
	if (p.shift == shift)
		score++;
	if (p.control == control)
		score++;
	return score;
}

const KeyPress EscapeKey("KEY_ESCAPE");

const KeyPress LMBKey("KEY_LBUTTON");
const KeyPress MMBKey("KEY_MBUTTON");
const KeyPress RMBKey("KEY_RBUTTON");

/*
	Key config
*/

// A simple cache for quicker lookup
static std::unordered_map<std::string, KeyPress> g_key_setting_cache;

const KeyPress &getKeySetting(const char *settingname)
{
	auto n = g_key_setting_cache.find(settingname);
	if (n != g_key_setting_cache.end())
		return n->second;

	auto &ref = g_key_setting_cache[settingname];
	ref = g_settings->get(settingname).c_str();
	return ref;
}

void clearKeyCache()
{
	g_key_setting_cache.clear();
}

irr::EKEY_CODE keyname_to_keycode(const char *name)
{
	return lookup_keyname(name).Key;
}
