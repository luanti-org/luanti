// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include "test.h"

#include "mock_activeobject.h"

class TestActiveObject : public TestBase
{
public:
	TestActiveObject() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestActiveObject"; }

	void runTests(IGameDef *gamedef);

	void testAOAttributes();
};

static TestActiveObject g_test_instance;

void TestActiveObject::runTests(IGameDef *gamedef)
{
	TEST(testAOAttributes);
}

void TestActiveObject::testAOAttributes()
{
	MockActiveObject ao(44);
	UASSERT(ao.getId() == 44);

	ao.setId(558);
	UASSERT(ao.getId() == 558);
}
