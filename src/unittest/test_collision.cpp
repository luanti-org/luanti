// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"
#include "dummymap.h"
#include "environment.h"
#include "irrlicht_changes/printing.h"

#include "collision.h"

class TestCollision : public TestBase {
public:
	TestCollision() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestCollision"; }

	void runTests(IGameDef *gamedef);

	void testAxisAlignedCollision();
	void testCollisionMoveSimple(IGameDef *gamedef);
};

static TestCollision g_test_instance;

void TestCollision::runTests(IGameDef *gamedef)
{
	TEST(testAxisAlignedCollision);
	TEST(testCollisionMoveSimple, gamedef);
}

namespace {
	class TestEnvironment : public Environment {
		DummyMap map;
	public:
		TestEnvironment(IGameDef *gamedef)
			: Environment(gamedef), map(gamedef, {-1, -1, -1}, {1, 1, 1})
		{
			map.fill({-1, -1, -1}, {1, 1, 1}, MapNode(CONTENT_AIR));
		}

		void step(f32 dtime) override {}

		Map &getMap() override { return map; }

		void getSelectedActiveObjects(const core::line3d<f32> &shootline_on_map,
			std::vector<PointedThing> &objects,
			const std::optional<Pointabilities> &pointabilities) override {}
	};
}

#define UASSERTEQ_F(actual, expected) do { \
		f32 a = (actual); \
		f32 e = (expected); \
		UTEST(fabsf(a - e) <= 0.0001f, "actual: %.f expected: %.f", a, e) \
	} while (0)

#define UASSERTEQ_V3F(actual, expected) do { \
		v3f va = (actual); \
		v3f ve = (expected); \
		UASSERTEQ_F(va.X, ve.X); UASSERTEQ_F(va.Y, ve.Y); UASSERTEQ_F(va.Z, ve.Z); \
	} while (0)


////////////////////////////////////////////////////////////////////////////////

void TestCollision::testAxisAlignedCollision()
{
	for (s16 bx = -3; bx <= 3; bx++)
	for (s16 by = -3; by <= 3; by++)
	for (s16 bz = -3; bz <= 3; bz++) {
		// X-
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by, bz, bx-1, by+1, bz+1);
			v3f v(1, 0, 0);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 0);
			UASSERT(fabs(dtime - 1.000) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by, bz, bx-1, by+1, bz+1);
			v3f v(-1, 0, 0);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by+1.5, bz, bx-1, by+2.5, bz-1);
			v3f v(1, 0, 0);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by-1.5, bz, bx-1.5, by+0.5, bz+1);
			v3f v(0.5, 0.1, 0);
			f32 dtime = 3.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 0);
			UASSERT(fabs(dtime - 3.000) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx-2, by-1.5, bz, bx-1.5, by+0.5, bz+1);
			v3f v(0.5, 0.1, 0);
			f32 dtime = 3.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 0);
			UASSERT(fabs(dtime - 3.000) < 0.001);
		}

		// X+
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by, bz, bx+3, by+1, bz+1);
			v3f v(-1, 0, 0);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 0);
			UASSERT(fabs(dtime - 1.000) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by, bz, bx+3, by+1, bz+1);
			v3f v(1, 0, 0);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by, bz+1.5, bx+3, by+1, bz+3.5);
			v3f v(-1, 0, 0);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == -1);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by-1.5, bz, bx+2.5, by-0.5, bz+1);
			v3f v(-0.5, 0.2, 0);
			f32 dtime = 2.5f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 1);  // Y, not X!
			UASSERT(fabs(dtime - 2.500) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+1, by+1, bz+1);
			aabb3f m(bx+2, by-1.5, bz, bx+2.5, by-0.5, bz+1);
			v3f v(-0.5, 0.3, 0);
			f32 dtime = 2.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 0);
			UASSERT(fabs(dtime - 2.000) < 0.001);
		}

		// TODO: Y-, Y+, Z-, Z+

		// misc
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx+2.3, by+2.29, bz+2.29, bx+4.2, by+4.2, bz+4.2);
			v3f v(-1./3, -1./3, -1./3);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 0);
			UASSERT(fabs(dtime - 0.9) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx+2.29, by+2.3, bz+2.29, bx+4.2, by+4.2, bz+4.2);
			v3f v(-1./3, -1./3, -1./3);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 1);
			UASSERT(fabs(dtime - 0.9) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx+2.29, by+2.29, bz+2.3, bx+4.2, by+4.2, bz+4.2);
			v3f v(-1./3, -1./3, -1./3);
			f32 dtime = 1.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 2);
			UASSERT(fabs(dtime - 0.9) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx-4.2, by-4.2, bz-4.2, bx-2.3, by-2.29, bz-2.29);
			v3f v(1./7, 1./7, 1./7);
			f32 dtime = 17.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 0);
			UASSERT(fabs(dtime - 16.1) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx-4.2, by-4.2, bz-4.2, bx-2.29, by-2.3, bz-2.29);
			v3f v(1./7, 1./7, 1./7);
			f32 dtime = 17.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 1);
			UASSERT(fabs(dtime - 16.1) < 0.001);
		}
		{
			aabb3f s(bx, by, bz, bx+2, by+2, bz+2);
			aabb3f m(bx-4.2, by-4.2, bz-4.2, bx-2.29, by-2.29, bz-2.3);
			v3f v(1./7, 1./7, 1./7);
			f32 dtime = 17.0f;
			UASSERT(axisAlignedCollision(s, m, v, &dtime) == 2);
			UASSERT(fabs(dtime - 16.1) < 0.001);
		}
	}
}

#define fpos(x,y,z) (BS * v3f(x, y, z))

void TestCollision::testCollisionMoveSimple(IGameDef *gamedef)
{
	auto env = std::make_unique<TestEnvironment>(gamedef);
	g_collision_problems_encountered = false;

	for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
		env->getMap().setNode({x, 0, z}, MapNode(t_CONTENT_STONE));

	v3f pos, speed, accel;
	const aabb3f box(fpos(-0.1f, 0, -0.1f), fpos(0.1f, 1.4f, 0.1f));
	collisionMoveResult res;

	/* simple movement with accel */
	pos   = fpos(4, 1, 4);
	speed = fpos(0, 0, 0);
	accel = fpos(0, 1, 0);
	res = collisionMoveSimple(env.get(), gamedef, box, 0.0f, 1.0f,
		&pos, &speed, accel);

	UASSERT(!res.touching_ground || !res.collides || !res.standing_on_object);
	UASSERT(res.collisions.empty());
	// FIXME: it's easy to tell that this should be y=1.5f, but our code does it wrong.
	// It's unclear if/how this will be fixed.
	UASSERTEQ_V3F(pos, fpos(4, 2, 4));
	UASSERTEQ_V3F(speed, fpos(0, 1, 0));

	/* standing on ground */
	pos   = fpos(0, 0.5f, 0);
	speed = fpos(0, 0, 0);
	accel = fpos(0, -9.81f, 0);
	res = collisionMoveSimple(env.get(), gamedef, box, 0.0f, 0.04f,
		&pos, &speed, accel);

	UASSERT(res.collides);
	UASSERT(res.touching_ground);
	UASSERT(!res.standing_on_object);
	UASSERTEQ_V3F(pos, fpos(0, 0.5f, 0));
	UASSERTEQ_V3F(speed, fpos(0, 0, 0));
	UASSERT(res.collisions.size() == 1);
	{
		auto &ci = res.collisions.front();
		UASSERTEQ(int, ci.type, COLLISION_NODE);
		UASSERTEQ(int, ci.axis, COLLISION_AXIS_Y);
		UASSERTEQ(v3s16, ci.node_p, v3s16(0, 0, 0));
	}

	/* not moving never collides */
	pos   = fpos(0, -100, 0);
	speed = fpos(0, 0, 0);
	accel = fpos(0, 0, 0);
	res = collisionMoveSimple(env.get(), gamedef, box, 0.0f, 1/60.0f,
		&pos, &speed, accel);
	UASSERT(!res.collides);

	/* collision in ignore */
	pos   = fpos(0, -100, 0);
	speed = fpos(5, 0, 0);
	accel = fpos(0, 0, 0);
	res = collisionMoveSimple(env.get(), gamedef, box, 0.0f, 1/60.0f,
		&pos, &speed, accel);
	UASSERTEQ_V3F(speed, fpos(0, 0, 0));
	UASSERT(!res.collides); // FIXME this is actually inconsistent
	UASSERT(res.collisions.empty());

	// TODO things to test:
	// standing_on_object, multiple collisions, bouncy, stepheight

	// No warnings should have been raised during our test.
	UASSERT(!g_collision_problems_encountered);
}
