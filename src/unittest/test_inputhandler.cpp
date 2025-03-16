// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "client/inputhandler.h"
#include "catch.h"

TEST_CASE("test KeyList")
{
	SECTION("KeyList inclusion")
	{
		KeyList list{"KEY_KEY_5", KeyPress("KEY_LSHIFT", false)};
		CHECK(list["KEY_KEY_5"]);
		CHECK(list["KEY_SHIFT"]);
		CHECK(list["KEY_SHIFT-KEY_KEY_5"]);
		CHECK(!list["KEY_NUMPAD5"]);
		CHECK(!list["KEY_CONTROL-KEY_KEY_5"]);
	}
	SECTION("KeyList insertion")
	{
		KeyList list;
		list.set("KEY_KEY_5");
		list.set(KeyPress("KEY_LSHIFT", false));
		list.unset(KeyPress("KEY_RSHIFT", false));
		CHECK(list["KEY_SHIFT-KEY_KEY_5"]);
		list.toggle(KeyPress("KEY_RSHIFT", false));
		list.toggle(KeyPress("KEY_LSHIFT", false));
		list.set("KEY_KEY_6");
		CHECK(list["KEY_SHIFT-KEY_KEY_6"]);
		list.toggle(KeyPress("KEY_RSHIFT", false));
		CHECK(list["KEY_KEY_5"]);
		CHECK(!list["KEY_SHIFT-KEY_KEY_5"]);
		list.toggle(KeyPress("KEY_SHIFT", false));
		list.clear();
		CHECK(!list["KEY_KEY_5"]);
		CHECK(!list[KeyPress("KEY_SHIFT", false)]);
	}
}
