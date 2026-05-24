// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include "test.h"

#include "client/gameui.h"
#include "gui/statusTextHelper.h"

class TestGameUI : public TestBase
{
public:
	TestGameUI() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestGameUI"; }

	void runTests(IGameDef *gamedef);

	void testInit();
	void testInfoText();
	void testStatusText();
};

static TestGameUI g_test_instance;

void TestGameUI::runTests(IGameDef *gamedef)
{
	TEST(testInit);
	TEST(testInfoText);
	TEST(testStatusText);
}

void TestGameUI::testInit()
{
	GameUI gui{};
	// Ensure flags on GameUI init
	UASSERT(gui.getFlags().show_chat)
	UASSERT(gui.getFlags().show_hud)
	UASSERT(!gui.getFlags().show_profiler_graph)

	// And after the initFlags init stage
	gui.initFlags();
	UASSERT(gui.getFlags().show_chat)
	UASSERT(gui.getFlags().show_hud)
	UASSERT(!gui.getFlags().show_profiler_graph)

	// @TODO verify if we can create non UI nulldevice to test this function
	// gui.init();
}

void TestGameUI::testStatusText()
{
	StatusTextHelper status_text(nullptr);

	UASSERT(status_text.getStatusText().empty());
	UASSERT(status_text.getStatusTextTime() == 0.0f);

	status_text.showStatusText(L"test status");
	UASSERT(status_text.getStatusText() == L"test status");
	UASSERT(status_text.getStatusTextTime() == 0.0f);

	status_text.clearStatusText();
	UASSERT(status_text.getStatusText().empty());
	UASSERT(status_text.getStatusTextTime() == 0.0f);
}

void TestGameUI::testInfoText()
{
	GameUI gui{};
	gui.setInfoText(L"test info");

	UASSERT(gui.m_infotext == L"test info");
}
