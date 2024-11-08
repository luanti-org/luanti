// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include "test.h"

#include "mock_server.h"

#include "util/string.h"
#include "util/serialize.h"

class TestServerShutdownState : public TestBase
{
public:
	TestServerShutdownState() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestServerShutdownState"; }

	void runTests(IGameDef *gamedef);

	void testInit();
	void testReset();
	void testTrigger();
	void testTick();
};

static TestServerShutdownState g_test_instance;

void TestServerShutdownState::runTests(IGameDef *gamedef)
{
	TEST(testInit);
	TEST(testReset);
	TEST(testTrigger);
	TEST(testTick);
}

void TestServerShutdownState::testInit()
{
	Server::ShutdownState ss;
	UASSERT(!ss.is_requested);
	UASSERT(!ss.should_reconnect);
	UASSERT(ss.message.empty());
	UASSERT(ss.m_timer == 0.0f);
}

void TestServerShutdownState::testReset()
{
	Server::ShutdownState ss;
	ss.reset();
	UASSERT(!ss.is_requested);
	UASSERT(!ss.should_reconnect);
	UASSERT(ss.message.empty());
	UASSERT(ss.m_timer == 0.0f);
}

void TestServerShutdownState::testTrigger()
{
	Server::ShutdownState ss;
	ss.trigger(3.0f, "testtrigger", true);
	UASSERT(!ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 3.0f);
}

void TestServerShutdownState::testTick()
{
	auto server = std::make_unique<MockServer>();
	Server::ShutdownState ss;
	ss.trigger(28.0f, "testtrigger", true);
	ss.tick(0.0f, server.get());

	// Tick with no time should not change anything
	UASSERT(!ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 28.0f);

	// Tick 2 seconds
	ss.tick(2.0f, server.get());
	UASSERT(!ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 26.0f);

	// Tick remaining seconds + additional expire
	ss.tick(26.1f, server.get());
	UASSERT(ss.is_requested);
	UASSERT(ss.should_reconnect);
	UASSERT(ss.message == "testtrigger");
	UASSERT(ss.m_timer == 0.0f);
}
