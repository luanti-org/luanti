/*
Minetest
Copyright (C) 2024 sfan5

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

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

	// TODO void cube();
	void randomInserts();
};

template<uint8_t Dim, typename Component, typename Id>
class ObjectVector {
public:
	using Point = std::array<Component, Dim>;
	void insert(const Point &p, Id id) {
		entries.push_back(Entry{p, id});
	}
	void remove(Id id) {
		entries.erase(std::find_if(entries.begin(), entries.end(), [&](const auto &e) {
			return e.id == id;
		}));
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
	TEST(randomInserts);
}

void TestKdTree::randomInserts() {
	PseudoRandom pr(814538);

	ObjectVector<3, f32, u16> objvec;
	DynamicKdTrees<3, f32, u16> kds;
	for (u16 id = 1; id < 1000; ++id) {
		std::array<f32, 3> point;
		for (uint8_t d = 0; d < 3; ++d)
			point[d] = pr.range(-1000, 1000);
		objvec.insert(point, id);
		kds.insert(point, id);
	}

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
}