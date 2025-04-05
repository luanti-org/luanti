// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "test.h"
#include "mock_serveractiveobject.h"
#include <algorithm>
#include <queue>

#include "server/activeobjectmgr.h"

#include "profiler.h"

class TestServerActiveObjectMgr : public TestBase
{
public:
	TestServerActiveObjectMgr() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestServerActiveObjectMgr"; }

	void runTests(IGameDef *gamedef);

	void testFreeID();
	void testRegisterObject();
	void testRemoveObject();
	void testGetObjectsInsideRadius();
	void testGetAddedActiveObjectsAroundPos();
};

static TestServerActiveObjectMgr g_test_instance;

void TestServerActiveObjectMgr::runTests(IGameDef *gamedef)
{
	TEST(testFreeID);
	TEST(testRegisterObject)
	TEST(testRemoveObject)
	TEST(testGetObjectsInsideRadius);
	TEST(testGetAddedActiveObjectsAroundPos);
}

////////////////////////////////////////////////////////////////////////////////

void TestServerActiveObjectMgr::testFreeID()
{
	server::ActiveObjectMgr saomgr;
	std::vector<u16> aoids;

	u16 aoid = saomgr.getFreeId();
	// Ensure it's not the same id
	UASSERT(saomgr.getFreeId() != aoid);

	aoids.push_back(aoid);

	// Register basic objects, ensure we never found
	for (u8 i = 0; i < UINT8_MAX; i++) {
		// Register an object
		auto sao_u = std::make_unique<MockServerActiveObject>();
		auto sao = sao_u.get();
		saomgr.registerObject(std::move(sao_u));
		aoids.push_back(sao->getId());

		// Ensure next id is not in registered list
		UASSERT(std::find(aoids.begin(), aoids.end(), saomgr.getFreeId()) ==
				aoids.end());
	}

	saomgr.clear();
}

void TestServerActiveObjectMgr::testRegisterObject()
{
	server::ActiveObjectMgr saomgr;
	auto sao_u = std::make_unique<MockServerActiveObject>();
	auto sao = sao_u.get();
	UASSERT(saomgr.registerObject(std::move(sao_u)));

	u16 id = sao->getId();

	auto saoToCompare = saomgr.getActiveObject(id);
	UASSERT(saoToCompare->getId() == id);
	UASSERT(saoToCompare == sao);

	sao_u = std::make_unique<MockServerActiveObject>();
	sao = sao_u.get();
	UASSERT(saomgr.registerObject(std::move(sao_u)));
	UASSERT(saomgr.getActiveObject(sao->getId()) == sao);
	UASSERT(saomgr.getActiveObject(sao->getId()) != saoToCompare);

	saomgr.clear();
}

void TestServerActiveObjectMgr::testRemoveObject()
{
	server::ActiveObjectMgr saomgr;
	auto sao_u = std::make_unique<MockServerActiveObject>();
	auto sao = sao_u.get();
	UASSERT(saomgr.registerObject(std::move(sao_u)));

	u16 id = sao->getId();
	UASSERT(saomgr.getActiveObject(id) != nullptr)

	saomgr.removeObject(sao->getId());
	UASSERT(saomgr.getActiveObject(id) == nullptr);

	saomgr.clear();
}

void TestServerActiveObjectMgr::testGetObjectsInsideRadius()
{
	server::ActiveObjectMgr saomgr;
	static const v3f sao_pos[] = {
			v3f(10, 40, 10),
			v3f(740, 100, -304),
			v3f(-200, 100, -304),
			v3f(740, -740, -304),
			v3f(1500, -740, -304),
	};

	for (const auto &p : sao_pos) {
		saomgr.registerObject(std::make_unique<MockServerActiveObject>(nullptr, p));
	}

	std::vector<ServerActiveObject *> result;
	saomgr.getObjectsInsideRadius(v3f(), 50, result, nullptr);
	UASSERTCMP(int, ==, result.size(), 1);

	result.clear();
	saomgr.getObjectsInsideRadius(v3f(), 750, result, nullptr);
	UASSERTCMP(int, ==, result.size(), 2);

	result.clear();
	saomgr.getObjectsInsideRadius(v3f(), 750000, result, nullptr);
	UASSERTCMP(int, ==, result.size(), 5);

	result.clear();
	auto include_obj_cb = [](ServerActiveObject *obj) {
		return (obj->getBasePosition().X != 10);
	};

	saomgr.getObjectsInsideRadius(v3f(), 750000, result, include_obj_cb);
	UASSERTCMP(int, ==, result.size(), 4);

	saomgr.clear();
}

void TestServerActiveObjectMgr::testGetAddedActiveObjectsAroundPos()
{
	server::ActiveObjectMgr saomgr;
	static const v3f sao_pos[] = {
			v3f(10, 40, 10),
			v3f(740, 100, -304),
			v3f(-200, 100, -304),
			v3f(740, -740, -304),
			v3f(1500, -740, -304),
	};

	for (const auto &p : sao_pos) {
		saomgr.registerObject(std::make_unique<MockServerActiveObject>(nullptr, p));
	}

	std::vector<u16> result;
	std::set<u16> cur_objects;
	saomgr.getAddedActiveObjectsAroundPos(v3f(), "singleplayer", 100, 50, cur_objects, result);
	UASSERTCMP(int, ==, result.size(), 1);

	result.clear();
	cur_objects.clear();
	saomgr.getAddedActiveObjectsAroundPos(v3f(), "singleplayer", 740, 50, cur_objects, result);
	UASSERTCMP(int, ==, result.size(), 2);

	saomgr.clear();
}
