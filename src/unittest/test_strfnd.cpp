// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"

#include "util/strfnd.h"

TEST_CASE("strfnd") {
SECTION("create") {
	Strfnd fnd = "test";

	CHECK(fnd.at_end() == false);
}

SECTION("next") {
	Strfnd fnd = "hello:world:foo";

	CHECK(fnd.next(":") == "hello");
	CHECK(fnd.next(":") == "world");
	CHECK(fnd.next(":") == "foo");
	CHECK(fnd.next(":") == "");
	CHECK(fnd.at_end() == true);
}

SECTION("start") {
	Strfnd fnd = "test foo bar d";

	fnd.start("Hello world");

	CHECK(fnd.next("d") == "Hello worl");
}

SECTION("next_esc") {
	Strfnd fnd = "hello\\:world:foo\\:bar:baz";

	CHECK(fnd.next_esc(":") == "hello\\:world");
	CHECK(fnd.next_esc(":") == "foo\\:bar");
	CHECK(fnd.next_esc(":") == "baz");

	CHECK(fnd.at_end() == true);
}

SECTION("skip_over") {
	Strfnd fnd = "   hello world";
	fnd.skip_over(" ");
	CHECK(fnd.next(" ") == "hello");
}

SECTION("to") {
	Strfnd fnd = "abcdef qwerty";
	fnd.skip_over("abcdef");
	fnd.to(1);

	CHECK(fnd.next("def") == "bc");
}
}
