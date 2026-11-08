// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"
#include "dummymap.h"
#include "environment.h"
#include "gamedef.h"
#include "nodedef.h"
#include "raycast.h"
#include "util/numeric.h"
#include "util/pointedthing.h"
#include <cmath>

namespace {

class RaycastEnvironment : public Environment
{
public:
	RaycastEnvironment(IGameDef *gamedef, v3s16 target, content_t content) :
			Environment(gamedef),
			map(gamedef, getNodeBlockPos(target - v3s16(3)),
					getNodeBlockPos(target + v3s16(3)))
	{
		map.fill(getNodeBlockPos(target - v3s16(3)),
				getNodeBlockPos(target + v3s16(3)), MapNode(CONTENT_AIR));
		map.setNode(target, MapNode(content));
	}

	void step(f32 dtime) override {}
	Map &getMap() override { return map; }
	void getSelectedActiveObjects(const core::line3d<f32> &line,
			std::vector<PointedThing> &objects,
			const std::optional<Pointabilities> &pointabilities) override {}

private:
	DummyMap map;
};

const v3s16 directions[] = {
	{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
};

}

class TestRaycast : public TestBase
{
public:
	TestRaycast() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestRaycast"; }
	void runTests(IGameDef *gamedef)
	{
		TEST(testFullCubeFaces, gamedef);
		TEST(testSelectionBoxInset, gamedef);
	}

	void testFullCubeFaces(IGameDef *gamedef)
	{
		for (s16 edge : {0, -30000, 30000}) {
			const v3s16 target(edge, edge, edge);
			RaycastEnvironment env(gamedef, target, t_CONTENT_STONE);
			for (v3s16 axis : directions) {
				RaycastState state(core::line3d<f32>(
						intToFloat(target + axis * 2, BS),
						intToFloat(target - axis * 2, BS)), false, false, {});
				PointedThing hit;
				env.continueRaycast(&state, &hit);
				UASSERTEQ(int, hit.type, POINTEDTHING_NODE);
				UASSERT(hit.node_undersurface == target);
				UASSERT(hit.node_real_undersurface == target);
				UASSERT(hit.node_abovesurface == target + axis);
				UASSERT(hit.intersection_normal == intToFloat(axis, 1.0f));
				UASSERT(std::fabs(hit.distanceSq - 225.0f) < 0.001f);
			}
		}
	}

	void testSelectionBoxInset(IGameDef *gamedef)
	{
		for (size_t i = 0; i < 6; ++i) {
			const v3s16 axis = directions[i];
			aabb3f box(-4.0f, -4.0f, -4.0f, 4.0f, 4.0f, 4.0f);
			for (size_t component = 0; component < 3; ++component) {
				if (axis[component] > 0) {
					box.MinEdge[component] = 4.975f;
					box.MaxEdge[component] = 6.0f;
				} else if (axis[component] < 0) {
					box.MinEdge[component] = -6.0f;
					box.MaxEdge[component] = -4.975f;
				}
			}
			ContentFeatures features;
			features.name = "test:raycast_inset_" + std::to_string(i);
			features.drawtype = NDT_NODEBOX;
			features.selection_box.type = NODEBOX_FIXED;
			features.selection_box.fixed.push_back(box);
			auto *ndef = const_cast<NodeDefManager *>(gamedef->ndef());
			const content_t content = ndef->set(features.name, std::move(features));

			for (s16 edge : {0, -30000, 30000}) {
				const v3s16 target(edge, edge, edge);
				RaycastEnvironment env(gamedef, target, content);
				RaycastState state(core::line3d<f32>(intToFloat(target, BS),
						intToFloat(target + axis * 2, BS)), false, false, {});
				PointedThing hit;
				env.continueRaycast(&state, &hit);
				UASSERTEQ(int, hit.type, POINTEDTHING_NODE);
				UASSERT(hit.node_undersurface == target);
				UASSERT(hit.node_real_undersurface == target);
				UASSERT(hit.node_abovesurface == target - axis);
				UASSERT(hit.intersection_normal == intToFloat(-axis, 1.0f));
			}
		}
	}
};

static TestRaycast g_test_instance;
