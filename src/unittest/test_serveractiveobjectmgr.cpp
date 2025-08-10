// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "activeobjectmgr.h"
#include "catch.h"
#include "irrTypes.h"
#include "irr_aabb3d.h"
#include "mock_serveractiveobject.h"
#include <algorithm>
#include <iterator>
#include <random>
#include <utility>

#include "server/activeobjectmgr.h"
#include "server/serveractiveobject.h"

class TestServerActiveObjectMgr {
	server::ActiveObjectMgr saomgr;
	std::vector<u16> ids;

public:

	u16 getFreeId() const { return saomgr.getFreeId(); }

	bool registerObject(std::unique_ptr<ServerActiveObject> obj)
	{
		auto *ptr = obj.get();
		if (!saomgr.registerObject(std::move(obj)))
			return false;
		ids.push_back(ptr->getId());
		return true;
	}

	void removeObject(u16 id)
	{
		const auto it = std::find(ids.begin(), ids.end(), id);
		REQUIRE(it != ids.end());
		ids.erase(it);
		saomgr.removeObject(id);
	}

	void updateObjectPos(u16 id, const v3f &pos)
	{
		auto *obj = saomgr.getActiveObject(id);
		REQUIRE(obj != nullptr);
		obj->setPos(pos);
		saomgr.updateObjectPos(id, pos); // HACK work around m_env == nullptr
	}

	void clear()
	{
		saomgr.clear();
		ids.clear();
	}

	ServerActiveObject *getActiveObject(u16 id)
	{
		return saomgr.getActiveObject(id);
	}

	template<class T>
	void getObjectsInsideRadius(T&& arg)
	{
		saomgr.getObjectsInsideRadius(std::forward(arg));
	}

	template<class T>
	void getAddedActiveObjectsAroundPos(T&& arg)
	{
		saomgr.getAddedActiveObjectsAroundPos(std::forward(arg));
	}

	// Testing

	bool empty() { return ids.empty(); }

	template<class T>
	u16 randomId(T &random)
	{
		REQUIRE(!ids.empty());
		std::uniform_int_distribution<u16> index(0, ids.size() - 1);
		return ids[index(random)];
	}

	void getObjectsInsideRadiusNaive(const v3f &pos, float radius,
			std::vector<ServerActiveObject *> &result)
	{
		for (const auto &[id, obj] : saomgr.m_active_objects.iter()) {
			if (obj->getBasePosition().getDistanceFromSQ(pos) <= radius * radius) {
				result.push_back(obj.get());
			}
		}
	}

	void getObjectsInAreaNaive(const aabb3f &box,
			std::vector<ServerActiveObject *> &result)
	{
		for (const auto &[id, obj] : saomgr.m_active_objects.iter()) {
			if (box.isPointInside(obj->getBasePosition())) {
				result.push_back(obj.get());
			}
		}
	}

	constexpr static auto compare_by_id = [](auto *sao1, auto *sao2) -> bool {
		return sao1->getId() < sao2->getId();
	};

	static void sortById(std::vector<ServerActiveObject *> &saos)
	{
		std::sort(saos.begin(), saos.end(), compare_by_id);
	}

	void compareObjects(std::vector<ServerActiveObject *> &actual,
			std::vector<ServerActiveObject *> &expected)
	{
		std::vector<ServerActiveObject *> unexpected, missing;
		sortById(actual);
		sortById(expected);

		std::set_difference(actual.begin(), actual.end(),
			expected.begin(), expected.end(),
			std::back_inserter(unexpected), compare_by_id);

		assert(unexpected.empty());

		std::set_difference(expected.begin(), expected.end(),
			actual.begin(), actual.end(),
			std::back_inserter(missing), compare_by_id);
		assert(missing.empty());
	}

	void compareObjectsInsideRadius(const v3f &pos, float radius)
	{
		std::vector<ServerActiveObject *> actual, expected;
		saomgr.getObjectsInsideRadius(pos, radius, actual, nullptr);
		getObjectsInsideRadiusNaive(pos, radius, expected);
		compareObjects(actual, expected);
	}

	void compareObjectsInArea(const aabb3f &box)
	{
		std::vector<ServerActiveObject *> actual, expected;
		saomgr.getObjectsInArea(box, actual, nullptr);
		getObjectsInAreaNaive(box, expected);
		compareObjects(actual, expected);
	}
};


TEST_CASE("server active object manager") {

SECTION("free ID") {
	TestServerActiveObjectMgr saomgr;
	std::vector<u16> aoids;

	u16 aoid = saomgr.getFreeId();
	// Ensure it's not the same id
	REQUIRE(saomgr.getFreeId() != aoid);

	aoids.push_back(aoid);

	// Register basic objects, ensure we never found
	for (u8 i = 0; i < UINT8_MAX; i++) {
		// Register an object
		auto sao_u = std::make_unique<MockServerActiveObject>();
		auto sao = sao_u.get();
		saomgr.registerObject(std::move(sao_u));
		aoids.push_back(sao->getId());

		// Ensure next id is not in registered list
		REQUIRE(std::find(aoids.begin(), aoids.end(), saomgr.getFreeId()) ==
				aoids.end());
	}

	saomgr.clear();
}

SECTION("register object") {
	TestServerActiveObjectMgr saomgr;
	auto sao_u = std::make_unique<MockServerActiveObject>();
	auto sao = sao_u.get();
	REQUIRE(saomgr.registerObject(std::move(sao_u)));

	u16 id = sao->getId();

	auto saoToCompare = saomgr.getActiveObject(id);
	REQUIRE(saoToCompare->getId() == id);
	REQUIRE(saoToCompare == sao);

	sao_u = std::make_unique<MockServerActiveObject>();
	sao = sao_u.get();
	REQUIRE(saomgr.registerObject(std::move(sao_u)));
	REQUIRE(saomgr.getActiveObject(sao->getId()) == sao);
	REQUIRE(saomgr.getActiveObject(sao->getId()) != saoToCompare);

	saomgr.clear();
}

SECTION("remove object") {
	TestServerActiveObjectMgr saomgr;
	auto sao_u = std::make_unique<MockServerActiveObject>();
	auto sao = sao_u.get();
	REQUIRE(saomgr.registerObject(std::move(sao_u)));

	u16 id = sao->getId();
	REQUIRE(saomgr.getActiveObject(id) != nullptr);

	saomgr.removeObject(sao->getId());
	REQUIRE(saomgr.getActiveObject(id) == nullptr);

	saomgr.clear();
}

SECTION("get objects inside radius") {
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
	CHECK(result.size() == 1);

	result.clear();
	saomgr.getObjectsInsideRadius(v3f(), 750, result, nullptr);
	CHECK(result.size() == 2);

	result.clear();
	saomgr.getObjectsInsideRadius(v3f(), 750000, result, nullptr);
	CHECK(result.size() == 5);

	result.clear();
	auto include_obj_cb = [](ServerActiveObject *obj) {
		return (obj->getBasePosition().X != 10);
	};

	saomgr.getObjectsInsideRadius(v3f(), 750000, result, include_obj_cb);
	CHECK(result.size() == 4);

	saomgr.clear();
}

SECTION("get added active objects around pos") {
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
	CHECK(result.size() == 1);

	result.clear();
	cur_objects.clear();
	saomgr.getAddedActiveObjectsAroundPos(v3f(), "singleplayer", 740, 50, cur_objects, result);
	CHECK(result.size() == 2);

	saomgr.clear();
}

SECTION("spatial index") {
	TestServerActiveObjectMgr saomgr;
	std::mt19937 gen(0xABCDEF);
	std::uniform_int_distribution<s32> coordinate(-1000, 1000);
	const auto random_pos = [&]() {
		return v3f(coordinate(gen), coordinate(gen), coordinate(gen));
	};

	std::uniform_int_distribution<u32> percent(0, 99);
	const auto modify = [&](u32 p_insert, u32 p_delete, u32 p_update) {
		const auto p = percent(gen);
		if (p < p_insert) {
			saomgr.registerObject(std::make_unique<MockServerActiveObject>(nullptr, random_pos()));
		} else if (p < p_insert + p_delete) {
			if (!saomgr.empty())
				saomgr.removeObject(saomgr.randomId(gen));
		} else if (p < p_insert + p_delete + p_update) {
			if (!saomgr.empty())
				saomgr.updateObjectPos(saomgr.randomId(gen), random_pos());
		}
	};

	const auto test_queries = [&]() {
		std::uniform_real_distribution<f32> radius(0, 100);
		saomgr.compareObjectsInsideRadius(random_pos(), radius(gen));

		aabb3f box(random_pos(), random_pos());
		box.repair();
		saomgr.compareObjectsInArea(box);
	};

	// Grow: Insertion twice as likely as deletion
	for (u32 i = 0; i < 3000; ++i) {
		modify(50, 25, 25);
		test_queries();
	}

	// Stagnate: Insertion and deletion equally likely
	for (u32 i = 0; i < 3000; ++i) {
		modify(25, 25, 50);
		test_queries();
	}

	// Shrink: Deletion twice as likely as insertion
	while (!saomgr.empty()) {
		modify(25, 50, 25);
		test_queries();
	}
}

}
