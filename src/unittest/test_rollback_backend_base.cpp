// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 James Dornan <james@catch22.com>

#include "test.h"

#include "server/rollback.h"

#include <ctime>
#include <stdexcept>

class DummyRollbackBackend final : public RollbackMgr
{
public:
	explicit DummyRollbackBackend(IGameDef *gamedef) : RollbackMgr(gamedef) {}

	void pushAction(const RollbackAction &a) { addActionInternal(a); }
	size_t latestSize() const { return action_latest_buffer.size(); }

private:
	void beginSaveActions() override {}
	void endSaveActions() override {}
	void rollbackSaveActions() override {}
	void persistAction(const RollbackAction &a) override { (void)a; }
	void flush() override {}
	std::list<RollbackAction> getActionsSince(time_t, const std::string &) override { return {}; }
	std::list<RollbackAction> getActionsSince_range(time_t, v3s16, int, int) override { return {}; }
};

class TestRollbackBackendBase : public TestBase
{
public:
	TestRollbackBackendBase() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestRollbackBackendBase"; }

	void runTests(IGameDef *gamedef) override;

	void testBufferCapping();
	void testParseNodemetaLocationValid();
	void testParseNodemetaLocationInvalid();

private:
	IGameDef *m_gamedef = nullptr;
};

static TestRollbackBackendBase g_test_instance;

void TestRollbackBackendBase::runTests(IGameDef *gamedef)
{
	m_gamedef = gamedef;
	TEST(testBufferCapping);
	TEST(testParseNodemetaLocationValid);
	TEST(testParseNodemetaLocationInvalid);
}

void TestRollbackBackendBase::testBufferCapping()
{
	DummyRollbackBackend rb(m_gamedef);

	RollbackAction a;
	a.unix_time = time(nullptr);
	a.actor = "tester";
	a.type = RollbackAction::TYPE_SET_NODE;
	a.p = v3s16(0, 0, 0);

	for (int i = 0; i < 2000; i++)
		rb.pushAction(a);

	UASSERTEQ(size_t, rb.latestSize(), 500);
}

void TestRollbackBackendBase::testParseNodemetaLocationValid()
{
	int x, y, z;

	RollbackMgr::parseNodemetaLocation("nodemeta:1,2,3", x, y, z);
	UASSERTEQ(int, x, 1);
	UASSERTEQ(int, y, 2);
	UASSERTEQ(int, z, 3);

	RollbackMgr::parseNodemetaLocation("nodemeta:-1,-2,-3", x, y, z);
	UASSERTEQ(int, x, -1);
	UASSERTEQ(int, y, -2);
	UASSERTEQ(int, z, -3);

	RollbackMgr::parseNodemetaLocation("nodemeta:0,0,0", x, y, z);
	UASSERTEQ(int, x, 0);
	UASSERTEQ(int, y, 0);
	UASSERTEQ(int, z, 0);
}

void TestRollbackBackendBase::testParseNodemetaLocationInvalid()
{
	int x, y, z;

	EXCEPTION_CHECK(std::invalid_argument,
		RollbackMgr::parseNodemetaLocation("invalid:1,2,3", x, y, z));

	EXCEPTION_CHECK(std::invalid_argument,
		RollbackMgr::parseNodemetaLocation("nodemeta:1", x, y, z));

	EXCEPTION_CHECK(std::invalid_argument,
		RollbackMgr::parseNodemetaLocation("nodemeta:1,2", x, y, z));

	EXCEPTION_CHECK(std::invalid_argument,
		RollbackMgr::parseNodemetaLocation("", x, y, z));
}


