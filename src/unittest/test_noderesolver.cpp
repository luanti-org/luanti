// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#include "test.h"

#include "util/numeric.h"
#include "exceptions.h"
#include "gamedef.h"
#include "nodedef.h"

#include <algorithm>


class TestNodeResolver : public TestBase {
public:
	TestNodeResolver() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestNodeResolver"; }

	void runTests(IGameDef *gamedef);

	void testNodeResolving(NodeDefManager *ndef);
	void testPendingResolveCancellation(NodeDefManager *ndef);
	void testDirectResolveMethod(NodeDefManager *ndef);
	void testNoneResolveMethod(NodeDefManager *ndef);
};

static TestNodeResolver g_test_instance;

void TestNodeResolver::runTests(IGameDef *gamedef)
{
	NodeDefManager *ndef =
		(NodeDefManager *)gamedef->getNodeDefManager();

	ndef->resetNodeResolveState();
	TEST(testNodeResolving, ndef);

	ndef->resetNodeResolveState();
	TEST(testPendingResolveCancellation, ndef);
}

class Foobar : public NodeResolver {
public:
	friend class TestNodeResolver; // m_ndef

	void resolveNodeNames();

	content_t test_nr_node1;
	content_t test_nr_node2;
	content_t test_nr_node3;
	content_t test_nr_node4;
	content_t test_nr_node5;
	std::vector<content_t> test_nr_list;
	std::vector<content_t> test_nr_list_group;
	std::vector<content_t> test_nr_list_required;
	std::vector<content_t> test_nr_list_empty;
};

class Foobaz : public NodeResolver {
public:
	void resolveNodeNames();

	content_t test_content1;
	content_t test_content2;
};

////////////////////////////////////////////////////////////////////////////////

void Foobar::resolveNodeNames()
{
	UASSERT(getIdFromNrBacklog(&test_nr_node1, "", CONTENT_IGNORE) == true);
	UASSERT(getIdsFromNrBacklog(&test_nr_list) == true);
	UASSERT(getIdsFromNrBacklog(&test_nr_list_group) == true);
	UASSERT(getIdsFromNrBacklog(&test_nr_list_required,
		true, CONTENT_AIR) == false);
	UASSERT(getIdsFromNrBacklog(&test_nr_list_empty) == true);

	UASSERT(getIdFromNrBacklog(&test_nr_node2, "", CONTENT_IGNORE) == true);
	UASSERT(getIdFromNrBacklog(&test_nr_node3,
		"default:brick", CONTENT_IGNORE) == true);
	UASSERT(getIdFromNrBacklog(&test_nr_node4,
		"default:gobbledygook", CONTENT_AIR) == false);
	UASSERT(getIdFromNrBacklog(&test_nr_node5, "", CONTENT_IGNORE) == false);
}


void Foobaz::resolveNodeNames()
{
	UASSERT(getIdFromNrBacklog(&test_content1, "", CONTENT_IGNORE) == true);
	UASSERT(getIdFromNrBacklog(&test_content2, "", CONTENT_IGNORE) == false);
}


void TestNodeResolver::testNodeResolving(NodeDefManager *ndef)
{
	Foobar foobar;
	size_t i;

	foobar.m_nodenames.emplace_back("default:torch");

	foobar.m_nodenames.emplace_back("default:dirt_with_grass");
	foobar.m_nodenames.emplace_back("default:water");
	foobar.m_nodenames.emplace_back("default:abloobloobloo");
	foobar.m_nodenames.emplace_back("default:stone");
	foobar.m_nodenames.emplace_back("default:shmegoldorf");
	foobar.m_nnlistsizes.push_back(5);

	foobar.m_nodenames.emplace_back("group:liquids");
	foobar.m_nnlistsizes.push_back(1);

	foobar.m_nodenames.emplace_back("default:warf");
	foobar.m_nodenames.emplace_back("default:stone");
	foobar.m_nodenames.emplace_back("default:bloop");
	foobar.m_nnlistsizes.push_back(3);

	foobar.m_nnlistsizes.push_back(0);

	foobar.m_nodenames.emplace_back("default:brick");
	foobar.m_nodenames.emplace_back("default:desert_stone");
	foobar.m_nodenames.emplace_back("default:shnitzle");

	ndef->pendNodeResolve(&foobar);
	UASSERT(foobar.m_ndef == ndef);

	ndef->setNodeRegistrationStatus(true);
	ndef->runNodeResolveCallbacks();

	// Check that we read single nodes successfully
	UASSERTEQ(content_t, foobar.test_nr_node1, t_CONTENT_TORCH);
	UASSERTEQ(content_t, foobar.test_nr_node2, t_CONTENT_BRICK);
	UASSERTEQ(content_t, foobar.test_nr_node3, t_CONTENT_BRICK);
	UASSERTEQ(content_t, foobar.test_nr_node4, CONTENT_AIR);
	UASSERTEQ(content_t, foobar.test_nr_node5, CONTENT_IGNORE);

	// Check that we read all the regular list items
	static const content_t expected_test_nr_list[] = {
		t_CONTENT_GRASS,
		t_CONTENT_WATER,
		t_CONTENT_STONE,
	};
	UASSERTEQ(size_t, foobar.test_nr_list.size(), 3);
	for (i = 0; i != foobar.test_nr_list.size(); i++)
		UASSERTEQ(content_t, foobar.test_nr_list[i], expected_test_nr_list[i]);

	// Check that we read all the list items that were from a group entry
	static const content_t expected_test_nr_list_group[] = {
		t_CONTENT_WATER,
		t_CONTENT_LAVA,
	};
	UASSERTEQ(size_t, foobar.test_nr_list_group.size(), 2);
	for (i = 0; i != foobar.test_nr_list_group.size(); i++) {
		UASSERT(CONTAINS(foobar.test_nr_list_group,
			expected_test_nr_list_group[i]));
	}

	// Check that we read all the items we're able to in a required list
	static const content_t expected_test_nr_list_required[] = {
		CONTENT_AIR,
		t_CONTENT_STONE,
		CONTENT_AIR,
	};
	UASSERTEQ(size_t, foobar.test_nr_list_required.size(), 3);
	for (i = 0; i != foobar.test_nr_list_required.size(); i++)
		UASSERTEQ(content_t, foobar.test_nr_list_required[i],
			expected_test_nr_list_required[i]);

	// Check that the edge case of 0 is successful
	UASSERTEQ(size_t, foobar.test_nr_list_empty.size(), 0);
}


void TestNodeResolver::testPendingResolveCancellation(NodeDefManager *ndef)
{
	Foobaz foobaz1;
	foobaz1.test_content1 = 1234;
	foobaz1.test_content2 = 5678;
	foobaz1.m_nodenames.emplace_back("default:dirt_with_grass");
	foobaz1.m_nodenames.emplace_back("default:abloobloobloo");
	ndef->pendNodeResolve(&foobaz1);

	Foobaz foobaz2;
	foobaz2.test_content1 = 1234;
	foobaz2.test_content2 = 5678;
	foobaz2.m_nodenames.emplace_back("default:dirt_with_grass");
	foobaz2.m_nodenames.emplace_back("default:abloobloobloo");
	ndef->pendNodeResolve(&foobaz2);

	ndef->cancelNodeResolveCallback(&foobaz1);

	ndef->setNodeRegistrationStatus(true);
	ndef->runNodeResolveCallbacks();

	UASSERT(foobaz1.test_content1 == 1234);
	UASSERT(foobaz1.test_content2 == 5678);
	UASSERT(foobaz2.test_content1 == t_CONTENT_GRASS);
	UASSERT(foobaz2.test_content2 == CONTENT_IGNORE);
}
