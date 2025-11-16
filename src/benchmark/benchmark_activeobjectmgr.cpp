// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"
#include "server/activeobjectmgr.h"
#include "util/numeric.h"

namespace {

class TestObject : public ServerActiveObject {
public:
	TestObject(v3f pos) : ServerActiveObject(nullptr, pos)
	{}

	ActiveObjectType getType() const {
		return ACTIVEOBJECT_TYPE_TEST;
	}
	bool getCollisionBox(aabb3f *toset) const {
		return false;
	}
	bool getSelectionBox(aabb3f *toset) const {
		return false;
	}
	bool collideWithObjects() const {
		return true;
	}
	std::string getGUID() const {
		return "";
	}
};

constexpr float POS_RANGE = 2001;

inline v3f randpos()
{
	return v3f(myrand_range(-POS_RANGE, POS_RANGE),
		myrand_range(-20, 60),
		myrand_range(-POS_RANGE, POS_RANGE));
}

inline void fill(server::ActiveObjectMgr &mgr, size_t n)
{
	mgr.clear();
	for (size_t i = 0; i < n; i++) {
		auto obj = std::make_unique<TestObject>(randpos());
		bool ok = mgr.registerObject(std::move(obj));
		REQUIRE(ok);
	}
}

}

template <size_t N>
void benchGetObjectsInsideRadius(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	size_t x;
	std::vector<ServerActiveObject*> result;

	auto cb = [&x] (ServerActiveObject *obj) -> bool {
		x += obj->m_static_exists ? 0 : 1;
		return false;
	};
	fill(mgr, N);
	meter.measure([&] {
		x = 0;
		mgr.getObjectsInsideRadius(randpos(), 30.0f, result, cb);
		return x;
	});
	REQUIRE(result.empty());

	mgr.clear(); // implementation expects this
}

template <size_t N>
void benchGetObjectsInArea(Catch::Benchmark::Chronometer &meter)
{
	server::ActiveObjectMgr mgr;
	size_t x;
	std::vector<ServerActiveObject*> result;

	auto cb = [&x] (ServerActiveObject *obj) -> bool {
		x += obj->m_static_exists ? 0 : 1;
		return false;
	};
	fill(mgr, N);
	meter.measure([&] {
		x = 0;
		v3f pos = randpos();
		v3f off(50, 50, 50);
		off[myrand_range(0, 2)] = 10;
		mgr.getObjectsInArea({pos, pos + off}, result, cb);
		return x;
	});
	REQUIRE(result.empty());

	mgr.clear(); // implementation expects this
}

#define BENCH_INSIDE_RADIUS(_count) \
	BENCHMARK_ADVANCED("inside_radius_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchGetObjectsInsideRadius<_count>(meter); };

#define BENCH_IN_AREA(_count) \
	BENCHMARK_ADVANCED("in_area_" #_count)(Catch::Benchmark::Chronometer meter) \
	{ benchGetObjectsInArea<_count>(meter); };

TEST_CASE("ActiveObjectMgr") {
	BENCH_INSIDE_RADIUS(200)
	BENCH_INSIDE_RADIUS(1450)
	BENCH_INSIDE_RADIUS(10000)

	BENCH_IN_AREA(200)
	BENCH_IN_AREA(1450)
	BENCH_IN_AREA(10000)
}

// TODO benchmark active object manager update costs
