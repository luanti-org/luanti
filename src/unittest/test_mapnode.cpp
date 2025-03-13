// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include "gamedef.h"
#include "nodedef.h"
#include "content_mapnode.h"

class TestMapNode : public TestBase
{
public:
	TestMapNode() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMapNode"; }

	void runTests(IGameDef *gamedef);

	void testNodeProperties(const NodeDefManager *nodedef);
};

static TestMapNode g_test_instance;

void TestMapNode::runTests(IGameDef *gamedef)
{
	TEST(testNodeProperties, gamedef->getNodeDefManager());
}

////////////////////////////////////////////////////////////////////////////////

void TestMapNode::testNodeProperties(const NodeDefManager *nodedef)
{
	MapNode n(CONTENT_AIR);

	ContentLightingFlags f = nodedef->getLightingFlags(n);
	UASSERT(n.getContent() == CONTENT_AIR);
	UASSERT(n.getLight(LIGHTBANK_DAY, f) == 0);
	UASSERT(n.getLight(LIGHTBANK_NIGHT, f) == 0);

	// Transparency
	n.setContent(CONTENT_AIR);
	UASSERT(nodedef->get(n).light_propagates == true);
}
