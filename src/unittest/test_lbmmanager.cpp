// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 sfan5

#include "test.h"

#include <sstream>

#include "server/blockmodifier.h"

class TestLBMManager : public TestBase
{
public:
	TestLBMManager() { TestManager::registerTestModule(this); }
	const char *getName() {	return "TestLBMManager"; }

	void runTests(IGameDef *gamedef);

	void testNew(IGameDef *gamedef);
	void testExisting(IGameDef *gamedef);
	void testDiscard(IGameDef *gamedef);
};

static TestLBMManager g_test_instance;

void TestLBMManager::runTests(IGameDef *gamedef)
{
	TEST(testNew, gamedef);
	TEST(testExisting, gamedef);
	TEST(testDiscard, gamedef);
}

namespace {
	struct FakeLBM : LoadingBlockModifierDef {
		FakeLBM(const std::string &name, bool every_load) {
			this->name = name;
			this->run_at_every_load = every_load;
			trigger_contents.emplace_back("air");
		}
	};
}

void TestLBMManager::testNew(IGameDef *gamedef)
{
	LBMManager mgr;

	mgr.addLBMDef(new FakeLBM(":foo:bar", false));
	mgr.addLBMDef(new FakeLBM("not:this", true));

	mgr.loadIntroductionTimes("", gamedef, 1234);

	auto str = mgr.createIntroductionTimesString();
	// name of first lbm should have been stripped
	// the second should not appear at all
	UASSERTEQ(auto, str, "foo:bar~1234;");
}

void TestLBMManager::testExisting(IGameDef *gamedef)
{
	LBMManager mgr;

	mgr.addLBMDef(new FakeLBM("foo:bar", false));

	// colon should also be stripped when loading (due to old versions)
	mgr.loadIntroductionTimes(":foo:bar~22;", gamedef, 1234);

	auto str = mgr.createIntroductionTimesString();
	UASSERTEQ(auto, str, "foo:bar~22;");
}

void TestLBMManager::testDiscard(IGameDef *gamedef)
{
	LBMManager mgr;

	// LBMs that no longer exist are dropped
	mgr.loadIntroductionTimes("some:thing~2;", gamedef, 10);

	auto str = mgr.createIntroductionTimesString();
	UASSERTEQ(auto, str, "");
}

// We should also test LBMManager::applyLBMs in the future.
