/*
Minetest

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include "test.h"

#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include "mapblock.h"
#include "dummymap.h"

class TestMap : public TestBase
{
public:
	TestMap() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMap"; }

	void runTests(IGameDef *gamedef);

	void testMaxMapgenLimit();
	void testForEachNodeInArea(IGameDef *gamedef);
	void testForEachNodeInAreaBlank(IGameDef *gamedef);
	void testForEachNodeInAreaEmpty(IGameDef *gamedef);
};

static TestMap g_test_instance;

void TestMap::runTests(IGameDef *gamedef)
{
	TEST(testMaxMapgenLimit);
	TEST(testForEachNodeInArea, gamedef);
	TEST(testForEachNodeInAreaBlank, gamedef);
	TEST(testForEachNodeInAreaEmpty, gamedef);
}

////////////////////////////////////////////////////////////////////////////////

void TestMap::testMaxMapgenLimit()
{
	// limit must end on a mapblock boundary
	UASSERTEQ(int, MAX_MAP_GENERATION_LIMIT % MAP_BLOCKSIZE, MAP_BLOCKSIZE - 1);

	// objectpos_over_limit should do exactly this except the last node
	// actually spans from LIMIT-0.5 to LIMIT+0.5
	float limit_times_bs = MAX_MAP_GENERATION_LIMIT * BS;
	UASSERT(objectpos_over_limit(v3f(limit_times_bs-BS/2)) == false);
	UASSERT(objectpos_over_limit(v3f(limit_times_bs)) == false);
	UASSERT(objectpos_over_limit(v3f(limit_times_bs+BS/2)) == false);
	UASSERT(objectpos_over_limit(v3f(limit_times_bs+BS)) == true);

	UASSERT(objectpos_over_limit(v3f(-limit_times_bs+BS/2)) == false);
	UASSERT(objectpos_over_limit(v3f(-limit_times_bs)) == false);
	UASSERT(objectpos_over_limit(v3f(-limit_times_bs-BS/2)) == false);
	UASSERT(objectpos_over_limit(v3f(-limit_times_bs-BS)) == true);

	// blockpos_over_max_limit
	s16 limit_block = MAX_MAP_GENERATION_LIMIT / MAP_BLOCKSIZE;
	UASSERT(blockpos_over_max_limit(v3s16(limit_block)) == false);
	UASSERT(blockpos_over_max_limit(v3s16(limit_block+1)) == true);
	UASSERT(blockpos_over_max_limit(v3s16(-limit_block)) == false);
	UASSERT(blockpos_over_max_limit(v3s16(-limit_block-1)) == true);
}

void TestMap::testForEachNodeInArea(IGameDef *gamedef)
{
	v3s16 minp_visit(-10, -10, -10);
	v3s16 maxp_visit(20, 20, 10);
	v3s16 dims_visit = maxp_visit - minp_visit + v3s16(1, 1, 1);
	s32 volume_visit = (s32)dims_visit.X * (s32)dims_visit.Y * (s32)dims_visit.Z;

	v3s16 minp = minp_visit - v3s16(1, 1, 1);
	v3s16 maxp = maxp_visit + v3s16(1, 1, 1);
	DummyMap map(gamedef, getNodeBlockPos(minp), getNodeBlockPos(maxp));

	v3s16 p1(0, 10, 5);
	MapNode n1(t_CONTENT_STONE);
	map.setNode(p1, n1);

	v3s16 p2(-1, 15, 5);
	MapNode n2(t_CONTENT_TORCH);
	map.setNode(p2, n2);

	v3s16 p3 = minp_visit;
	MapNode n3(CONTENT_AIR);
	map.setNode(p3, n3);

	v3s16 p4 = maxp_visit;
	MapNode n4(t_CONTENT_LAVA);
	map.setNode(p4, n4);

	// These positions should not be visited.
	map.setNode(minp, MapNode(t_CONTENT_WATER));
	map.setNode(maxp, MapNode(t_CONTENT_WATER));

	s32 n_visited = 0;
	std::unordered_set<v3s16> visited;
	v3s16 minp_visited(0, 0, 0);
	v3s16 maxp_visited(0, 0, 0);
	std::unordered_map<v3s16, MapNode> found;
	map.forEachNodeInArea(minp_visit, maxp_visit, [&](v3s16 p, MapNode n) -> bool {
		n_visited++;
		visited.insert(p);
		minp_visited.X = std::min(minp_visited.X, p.X);
		minp_visited.Y = std::min(minp_visited.Y, p.Y);
		minp_visited.Z = std::min(minp_visited.Z, p.Z);
		maxp_visited.X = std::max(maxp_visited.X, p.X);
		maxp_visited.Y = std::max(maxp_visited.Y, p.Y);
		maxp_visited.Z = std::max(maxp_visited.Z, p.Z);

		if (n.getContent() != CONTENT_IGNORE)
			found[p] = n;

		return true;
	});

	UASSERTEQ(s32, n_visited, volume_visit);
	UASSERTEQ(s32, (s32)visited.size(), volume_visit);
	UASSERT(minp_visited == minp_visit);
	UASSERT(maxp_visited == maxp_visit);

	UASSERTEQ(size_t, found.size(), 4);
	UASSERT(found.find(p1) != found.end());
	UASSERTEQ(content_t, found[p1].getContent(), n1.getContent());
	UASSERT(found.find(p2) != found.end());
	UASSERTEQ(content_t, found[p2].getContent(), n2.getContent());
	UASSERT(found.find(p3) != found.end());
	UASSERTEQ(content_t, found[p3].getContent(), n3.getContent());
	UASSERT(found.find(p4) != found.end());
	UASSERTEQ(content_t, found[p4].getContent(), n4.getContent());
}

void TestMap::testForEachNodeInAreaBlank(IGameDef *gamedef)
{
	DummyMap map(gamedef, v3s16(0, 0, 0), v3s16(-1, -1, -1));

	v3s16 invalid_p(0, 0, 0);
	bool visited = false;
	map.forEachNodeInArea(invalid_p, invalid_p, [&](v3s16 p, MapNode n) -> bool {
		bool is_valid_position = true;
		UASSERT(n == map.getNode(p, &is_valid_position));
		UASSERT(!is_valid_position);
		UASSERT(!visited);
		visited = true;
		return true;
	});
	UASSERT(visited);
}

void TestMap::testForEachNodeInAreaEmpty(IGameDef *gamedef)
{
	DummyMap map(gamedef, v3s16(), v3s16());
	map.forEachNodeInArea(v3s16(0, 0, 0), v3s16(-1, -1, -1), [&](v3s16 p, MapNode n) -> bool {
		UASSERT(false); // Should be unreachable
		return true;
	});
}
