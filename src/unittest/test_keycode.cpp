// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 sfan5 <sfan5@live.de>

#include "test.h"

#include <string>
#include "exceptions.h"
#include "client/keycode.h"

class TestKeycode : public TestBase {
public:
	TestKeycode() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestKeycode"; }

	void runTests(IGameDef *gamedef);

	void testCreateFromString();
	void testCreateFromSKeyInput();
	void testCompare();
};

static TestKeycode g_test_instance;

void TestKeycode::runTests(IGameDef *gamedef)
{
	TEST(testCreateFromString);
	TEST(testCreateFromSKeyInput);
	TEST(testCompare);
}

////////////////////////////////////////////////////////////////////////////////

#define UASSERT_HAS_NAME(key) UASSERTCMP(size_t, >, key.name().size(), 0)

void TestKeycode::testCreateFromString()
{
	KeyPress k;

	// Character key, from char
	k = KeyPress("R");
	UASSERT(k.sym() == "KEY_KEY_R");
	UASSERT_HAS_NAME(k); // should have human description

	// Character key, from identifier
	k = KeyPress("KEY_KEY_B");
	UASSERT(k.sym() == "KEY_KEY_B");
	UASSERT_HAS_NAME(k);

	// Non-Character key, from identifier
	k = KeyPress("KEY_UP");
	UASSERT(k.sym() == "KEY_UP");
	UASSERT_HAS_NAME(k);

	k = KeyPress("KEY_F6");
	UASSERT(k.sym() == "KEY_F6");
	UASSERT_HAS_NAME(k);

	// Irrlicht-unknown key, from char
	k = KeyPress("/");
	UASSERT(k.sym() == "/");
	UASSERT_HAS_NAME(k);

	// Key with modifier
	k = KeyPress("KEY_SHIFT-KEY_CONTROL-2");
	UASSERT(k.sym() == "KEY_CONTROL-KEY_SHIFT-KEY_KEY_2");
}

void TestKeycode::testCreateFromSKeyInput()
{
	KeyPress k;
	irr::SEvent::SKeyInput in;
	in.Control = in.Shift = false;

	// Character key
	in.Key = irr::KEY_KEY_3;
	in.Char = L'3';
	k = KeyPress(in);
	UASSERT(k.sym() == "KEY_KEY_3");

	// Non-Character key
	in.Key = irr::KEY_RSHIFT;
	in.Char = L'\0';
	k = KeyPress(in);
	UASSERT(k.sym() == "KEY_SHIFT");

	// Irrlicht-unknown key
	in.Key = irr::KEY_KEY_CODES_COUNT;
	in.Char = L'?';
	k = KeyPress(in);
	UASSERT(k.sym() == "?");

	// prefer_character mode
	in.Key = irr::KEY_COMMA;
	in.Char = L'G';
	k = KeyPress(in, true);
	UASSERT(k.sym() == "KEY_KEY_G");
}

void TestKeycode::testCompare()
{
	// Basic comparison
	UASSERT(KeyPress("5") == KeyPress("KEY_KEY_5"));
	UASSERT(!(KeyPress("5") == KeyPress("KEY_NUMPAD5")));

	// Test modifiers
	UASSERT(KeyPress("KEY_CONTROL-KEY_SHIFT-5") == KeyPress("KEY_SHIFT-KEY_CONTROL-KEY_KEY_5"));
	UASSERT(KeyPress("KEY_SHIFT--") == KeyPress("KEY_SHIFT-KEY_MINUS"));
	UASSERT(KeyPress("KEY_CONTROL-KEY_LSHIFT") == KeyPress("KEY_SHIFT-KEY_RCONTROL"));
	UASSERT(KeyPress("KEY_RSHIFT") == KeyPress("KEY_SHIFT-"));

	// Test matching
	UASSERT(!KeyPress("5").matches(KeyPress("KEY_CONTROL-5")));
	UASSERT(!KeyPress("5").matches(KeyPress("KEY_CONTROL")));
	UASSERT(KeyPress("KEY_SHIFT-5").matches(KeyPress("KEY_SHIFT")));
	UASSERT(KeyPress("KEY_SHIFT-5").matches(KeyPress("5")));
	UASSERT(KeyPress("KEY_SHIFT-5").matches(KeyPress("KEY_SHIFT-5")) > KeyPress("KEY_SHIFT-5").matches("5"));
	UASSERT(!KeyPress("5").matches(KeyPress("$")));
	UASSERT(!KeyPress("5").matches(KeyPress("KEY_F10")));

	// Matching char suffices
	// note: This is a real-world example, Irrlicht maps XK_equal to irr::KEY_PLUS on Linux
	irr::SEvent::SKeyInput in;
	in.Shift = in.Control = false;
	in.Key = irr::KEY_PLUS;
	in.Char = L'=';
	UASSERT(KeyPress("=") == KeyPress(in));

	// Matching keycode suffices
	irr::SEvent::SKeyInput in2;
	in.Key = in2.Key = irr::KEY_OEM_CLEAR;
	in2.Shift = in2.Control = false;
	in.Char = L'\0';
	in2.Char = L';';
	UASSERT(KeyPress(in) == KeyPress(in2));
}
