// Copyright (C) 2024 Lars Müller
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "noise.h"
#include "test.h"

#include "util/k_d_tree.h"
#include <algorithm>
#include <unordered_set>

class TestKdTree : public TestBase
{
public:
	TestKdTree() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestKdTree"; }

	void runTests(IGameDef *gamedef);

	// TODO basic small cube test
	void singleUpdate();
	void randomOps();
};

template<uint8_t Dim, typename Component, typename Id>
class ObjectVector {
public:
	using Point = std::array<Component, Dim>;
	void insert(const Point &p, Id id) {
		entries.push_back(Entry{p, id});
	}
	void remove(Id id) {
		const auto it = std::find_if(entries.begin(), entries.end(), [&](const auto &e) {
			return e.id == id;
		});
		assert(it != entries.end());
		entries.erase(it);
	}
	void update(const Point &p, Id id) {
		remove(id);
		insert(p, id);
	}
	template<typename F>
	void rangeQuery(const Point &min, const Point &max, const F &cb) {
		for (const auto &e : entries) {
			for (uint8_t d = 0; d < Dim; ++d)
				if (e.point[d] < min[d] || e.point[d] > max[d])
					goto next;
			cb(e.point, e.id); // TODO check
			next: {}
		}
	}
private:
	struct Entry {
		Point point;
		Id id;
	};
	std::vector<Entry> entries;
};

static TestKdTree g_test_instance;

void TestKdTree::runTests(IGameDef *gamedef)
{
	rawstream << "-------- k-d-tree" << std::endl;
	TEST(singleUpdate);
	TEST(randomOps);
}

void TestKdTree::singleUpdate() {
	DynamicKdTrees<3, u16, u16> kds;
	for (u16 i = 1; i <= 5; ++i)
		kds.insert({i, i, i}, i);
	for (u16 i = 1; i <= 5; ++i) {
		u16 j = i - 1;
		kds.update({j, j, j}, i);
	}
}

// 1: asan again
// 2: asan again
// 3: violates assert
// 5: violates asan

void TestKdTree::randomOps() {
	PseudoRandom pr(814538);

	ObjectVector<3, f32, u16> objvec;
	DynamicKdTrees<3, f32, u16> kds;

	const auto randPos = [&]() {
		std::array<f32, 3> point;
		for (uint8_t d = 0; d < 3; ++d)
			point[d] = pr.range(-1000, 1000);
		return point;
	};

	for (u16 id = 1; id < 1000; ++id) {
		const auto point = randPos();
		objvec.insert(point, id);
		kds.insert(point, id);
	}

	const auto testRandomQueries = [&]() {
		for (int i = 0; i < 1000; ++i) {
			std::array<f32, 3> min, max;
			for (uint8_t d = 0; d < 3; ++d) {
				min[d] = pr.range(-1500, 1500);
				max[d] = min[d] + pr.range(1, 2500);
			}
			std::unordered_set<u16> expected_ids;
			objvec.rangeQuery(min, max, [&](auto _, u16 id) {
				UASSERT(expected_ids.count(id) == 0);
				expected_ids.insert(id);
			});
			kds.rangeQuery(min, max, [&](auto point, u16 id) {
				UASSERT(expected_ids.count(id) == 1);
				expected_ids.erase(id);
			});
			UASSERT(expected_ids.empty());
		}
	};

	testRandomQueries();

	for (u16 id = 1; id < 800; ++id) {
		objvec.remove(id);
		kds.remove(id);
	}

	testRandomQueries();

	for (u16 id = 800; id < 1000; ++id) {
		const auto point = randPos();
		objvec.update(point, id);
		kds.update(point, id);
	}

	testRandomQueries();
}