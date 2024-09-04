// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "activeobjectmgr.h"
#include "catch.h"
#include "mock_serveractiveobject.h"
#include <algorithm>
#include <utility>

#include "server/activeobjectmgr.h"
#include "server/serveractiveobject.h"

class TestServerActiveObjectMgr {
	server::ActiveObjectMgr saomgr;

public:

	u16 getFreeId() const {
		return saomgr.getFreeId();
	}

	bool registerObject(std::unique_ptr<ServerActiveObject> obj) {
		return saomgr.registerObject(std::move(obj));
	}

	void removeObject(u16 id) {
		saomgr.removeObject(id);
	}

	void clear() {
		saomgr.clear();
	}

	ServerActiveObject *getActiveObject(u16 id) {
		return saomgr.getActiveObject(id);
	}

	template<class T>
	void getObjectsInsideRadius(T&& arg) {
		saomgr.getObjectsInsideRadius(std::forward(arg));
	}

	template<class T>
	void getAddedActiveObjectsAroundPos(T&& arg) {
		saomgr.getAddedActiveObjectsAroundPos(std::forward(arg));
	}
};


TEST_CASE("server active object manager")
{
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
}