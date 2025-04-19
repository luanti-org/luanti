// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include "util/strfnd.h"

class TestStrfnd : public TestBase
{
public:
	TestStrfnd() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestStrfnd"; }

	void runTests(IGameDef *gamedef);

private:
	void testCreate();
	void testNext();
	void testStart();
	void testNextEsc();
	void testSkipOver();
};

static TestStrfnd g_test_instance;

void TestStrfnd::runTests(IGameDef *gamedef)
{
	TEST(testCreate);

	TEST(testNext);

	TEST(testStart);

	TEST(testNextEsc);

	TEST(testSkipOver);
}

void TestStrfnd::testCreate()
{
	Strfnd fnd = "test";

	UASSERTEQ(std::string, fnd.what(), "test");
	UASSERTEQ(size_t, fnd.where(), 0);
	UASSERTEQ(bool, fnd.at_end(), false);
}

void TestStrfnd::testNext()
{
	Strfnd fnd = "hello:world:foo";

	UASSERTEQ(std::string, fnd.next(":"), "hello");
	UASSERTEQ(std::string, fnd.next(":"), "world");
	UASSERTEQ(std::string, fnd.next(":"), "foo");
	UASSERTEQ(bool, fnd.at_end(), true);
}

void TestStrfnd::testStart()
{
	Strfnd fnd = "test foo bar d";

	fnd.start("Hello world");

	UASSERTEQ(std::string, fnd.next("d"), "Hello worl");
}

void TestStrfnd::testNextEsc()
{
	Strfnd fnd = "hello\\:world:foo\\:bar:baz";

	UASSERTEQ(std::string, fnd.next_esc(":"), "hello\\:world");
	UASSERTEQ(std::string, fnd.next_esc(":"), "foo\\:bar");
	UASSERTEQ(std::string, fnd.next_esc(":"), "baz");

	UASSERTEQ(bool, fnd.at_end(), true);
}

void TestStrfnd::testSkipOver()
{
	Strfnd fnd = "   hello world";
	fnd.skip_over(" ");
	UASSERTEQ(std::string, fnd.next(" "), "hello");
}
