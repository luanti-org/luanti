// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include <unordered_map>
#include <memory>
#include "test.h"
#include "client/event_manager.h"

class TestEventManager : public TestBase
{
public:
	TestEventManager() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestEventManager"; }

	void runTests(IGameDef *gamedef) override;

	void testRegister();
	void testDeregister();
	void testRealEvent();
	void testRealEventAfterDereg();
};

// EventManager test class
class EventManagerTest : public EventManager
{
public:
	static void eventTest(MtEvent *e, void *data)
	{
		UASSERT(e->getType() >= 0);
		UASSERT(e->getType() < MtEvent::TYPE_MAX);
		EventManagerTest *emt = (EventManagerTest *)data;
		emt->m_test_value = e->getType();
	}

	u64 getTestValue() const { return m_test_value; }
	void resetValue() { m_test_value = 0; }

private:
	u64 m_test_value = 0;
};

static TestEventManager g_test_instance;

void TestEventManager::runTests(IGameDef *gamedef)
{
	TEST(testRegister);
	TEST(testDeregister);
	TEST(testRealEvent);
	TEST(testRealEventAfterDereg);
}

void TestEventManager::testRegister()
{
	EventManager ev;
	ev.reg(MtEvent::PLAYER_DAMAGE, nullptr, nullptr);
	ev.reg(MtEvent::PLAYER_DAMAGE, nullptr, nullptr);
}

void TestEventManager::testDeregister()
{
	EventManager ev;
	ev.dereg(MtEvent::NODE_DUG, nullptr, nullptr);
	ev.reg(MtEvent::PLAYER_DAMAGE, nullptr, nullptr);
	ev.dereg(MtEvent::PLAYER_DAMAGE, nullptr, nullptr);
}

void TestEventManager::testRealEvent()
{
	EventManager ev;
	auto emt = std::make_unique<EventManagerTest>();
	ev.reg(MtEvent::PLAYER_REGAIN_GROUND, EventManagerTest::eventTest, emt.get());

	// Put event & verify event value
	ev.put(new SimpleTriggerEvent(MtEvent::PLAYER_REGAIN_GROUND));
	UASSERT(emt->getTestValue() == MtEvent::PLAYER_REGAIN_GROUND);
}

void TestEventManager::testRealEventAfterDereg()
{
	EventManager ev;
	auto emt = std::make_unique<EventManagerTest>();
	ev.reg(MtEvent::PLAYER_REGAIN_GROUND, EventManagerTest::eventTest, emt.get());

	// Put event & verify event value
	ev.put(new SimpleTriggerEvent(MtEvent::PLAYER_REGAIN_GROUND));
	UASSERT(emt->getTestValue() == MtEvent::PLAYER_REGAIN_GROUND);

	// Reset internal value
	emt->resetValue();

	// Remove the registered event
	ev.dereg(MtEvent::PLAYER_REGAIN_GROUND, EventManagerTest::eventTest, emt.get());

	// Push the new event & ensure we target the default value
	ev.put(new SimpleTriggerEvent(MtEvent::PLAYER_REGAIN_GROUND));
	UASSERT(emt->getTestValue() == 0);
}
