// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 sfan5

#include "test.h"

#include "util/container.h"

class TestDataStructures : public TestBase
{
public:
	TestDataStructures() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestDataStructures"; }

	void runTests(IGameDef *gamedef);

	void testMap1();
	void testMap2();
	void testMap3();
	void testMap4();
	void testMap5();
};

static TestDataStructures g_test_instance;

void TestDataStructures::runTests(IGameDef *gamedef)
{
	rawstream << "-------- ModifySafeMap" << std::endl;
	TEST(testMap1);
	TEST(testMap2);
	TEST(testMap3);
	TEST(testMap4);
	TEST(testMap5);
}

namespace {

struct TrackerState {
	bool copied = false;
	bool deleted = false;
};

class Tracker {
	TrackerState *res = nullptr;

	inline void trackDeletion() { res && (res->deleted = true); }

public:
	Tracker() {}
	Tracker(TrackerState &res) : res(&res) {}

	operator bool() const { return !!res; }

	Tracker(const Tracker &other) { *this = other; }
	Tracker &operator=(const Tracker &other) {
		trackDeletion();
		res = other.res;
		res && (res->copied = true);
		return *this;
	}
	Tracker(Tracker &&other) { *this = std::move(other); }
	Tracker &operator=(Tracker &&other) {
		trackDeletion();
		res = other.res;
		other.res = nullptr;
		return *this;
	}

	~Tracker() { trackDeletion(); }
};

}

void TestDataStructures::testMap1()
{
	ModifySafeMap<int, Tracker> map;
	TrackerState t0, t1;

	// no-copy put
	map.put(1, Tracker(t0));
	UASSERT(!t0.copied);
	UASSERT(!t0.deleted);

	// overwrite during iter
	bool once = false;
	for (auto &it : map.iter()) {
		(void)it;
		map.put(1, Tracker(t1));
		UASSERT(t0.deleted);
		UASSERT(!t1.copied);
		UASSERT(!t1.deleted);
		if ((once |= 1))
			break;
	}
	UASSERT(once);

	map.clear(); // ASan complains about stack-use-after-scope otherwise
}

void TestDataStructures::testMap2()
{
	ModifySafeMap<int, Tracker> map;
	TrackerState t0, t1;

	// overwrite
	map.put(1, Tracker(t0));
	map.put(1, Tracker(t1));
	UASSERT(t0.deleted);
	UASSERT(!t1.copied);
	UASSERT(!t1.deleted);

	map.clear();
}

void TestDataStructures::testMap3()
{
	ModifySafeMap<int, Tracker> map;
	TrackerState t0, t1;

	// take
	map.put(1, Tracker(t0));
	{
		auto v = map.take(1);
		UASSERT(!t0.copied);
		UASSERT(!t0.deleted);
	}
	UASSERT(t0.deleted);

	// remove during iter
	map.put(1, Tracker(t1));
	for (auto &it : map.iter()) {
		(void)it;
		map.remove(1);
		UASSERT(t1.deleted);
		break;
	}
}

void TestDataStructures::testMap4()
{
	ModifySafeMap<int, u32> map;

	// overwrite + take during iter
	map.put(1, 100);
	for (auto &it : map.iter()) {
		(void)it;
		map.put(1, 200);
		u32 taken = map.take(1);
		UASSERTEQ(u32, taken, 200);
		break;
	}

	UASSERT(map.get(1) == u32());
	UASSERTEQ(size_t, map.size(), 0);
}

void TestDataStructures::testMap5()
{
	ModifySafeMap<int, u32> map;

	// overwrite 2x during iter
	map.put(9001, 9001);
	for (auto &it : map.iter()) {
		(void)it;
		map.put(1, 100);
		map.put(1, 200);
		UASSERTEQ(u32, map.get(1), 200);
		break;
	}
}
