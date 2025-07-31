// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <memory>

#include "gamedef.h"
#include "log.h"
#include "voxel.h"
#include "dummymap.h"
#include "irrlicht_changes/printing.h"

class TestVoxelManipulator : public TestBase {
public:
	TestVoxelManipulator() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestVoxelManipulator"; }

	void runTests(IGameDef *gamedef);

	void testBasic(const NodeDefManager *nodedef);
	void testEmerge(IGameDef *gamedef);
	void testBlitBack(IGameDef *gamedef);
	void testBlitBack2(IGameDef *gamedef);
};

static TestVoxelManipulator g_test_instance;

void TestVoxelManipulator::runTests(IGameDef *gamedef)
{
	TEST(testBasic, gamedef->ndef());
	TEST(testEmerge, gamedef);
	TEST(testBlitBack, gamedef);
	TEST(testBlitBack2, gamedef);
}

////////////////////////////////////////////////////////////////////////////////

void TestVoxelManipulator::testBasic(const NodeDefManager *nodedef)
{
	VoxelManipulator v;

	v.print(infostream, nodedef);
	UASSERT(v.m_area.hasEmptyExtent());

	infostream << "*** Setting (-1,0,-1) ***" << std::endl;
	v.setNode(v3s16(-1,0,-1), MapNode(t_CONTENT_GRASS));

	v.print(infostream, nodedef);
	UASSERT(v.getNode(v3s16(-1,0,-1)).getContent() == t_CONTENT_GRASS);

	infostream << "*** Reading from inexistent (0,0,-1) ***" << std::endl;

	EXCEPTION_CHECK(InvalidPositionException, v.getNode(v3s16(0,0,-1)));
	v.print(infostream, nodedef);

	infostream << "*** Adding area ***" << std::endl;

	VoxelArea a(v3s16(-1,-1,-1), v3s16(1,1,1));
	v.addArea(a);
	v.print(infostream, nodedef);

	UASSERT(v.getNode(v3s16(-1,0,-1)).getContent() == t_CONTENT_GRASS);
	EXCEPTION_CHECK(InvalidPositionException, v.getNode(v3s16(0,1,1)));
}

void TestVoxelManipulator::testEmerge(IGameDef *gamedef)
{
	constexpr int bs = MAP_BLOCKSIZE;

	DummyMap map(gamedef, {0,0,0}, {1,1,1});
	map.fill({0,0,0}, {1,1,1}, CONTENT_AIR);

	MMVManip vm(&map);
	UASSERT(!vm.isOrphan());

	// emerge something
	vm.initialEmerge({0,0,0}, {0,0,0});
	UASSERTEQ(auto, vm.m_area.MinEdge, v3s16(0));
	UASSERTEQ(auto, vm.m_area.MaxEdge, v3s16(bs-1));
	UASSERTEQ(auto, vm.getNodeNoExNoEmerge({0,0,0}).getContent(), CONTENT_AIR);

	map.setNode({0,   1,0}, t_CONTENT_BRICK);
	map.setNode({0,bs+1,0}, t_CONTENT_BRICK);

	// emerge top block: this should not re-read the first one
	vm.initialEmerge({0,0,0}, {0,1,0});
	UASSERTEQ(auto, vm.m_area.getExtent(), v3s32(bs,2*bs,bs));

	UASSERTEQ(auto, vm.getNodeNoExNoEmerge({0,   1,0}).getContent(), CONTENT_AIR);
	UASSERTEQ(auto, vm.getNodeNoExNoEmerge({0,bs+1,0}).getContent(), t_CONTENT_BRICK);

	// emerge out of bounds: should produce empty data
	vm.initialEmerge({0,2,0}, {0,2,0}, false);
	UASSERTEQ(auto, vm.m_area.getExtent(), v3s32(bs,3*bs,bs));

	UASSERTEQ(auto, vm.getNodeNoExNoEmerge({0,2*bs,0}).getContent(), CONTENT_IGNORE);
	UASSERT(!vm.exists({0,2*bs,0}));

	// clear
	vm.clear();
	UASSERT(vm.m_area.hasEmptyExtent());
}

void TestVoxelManipulator::testBlitBack(IGameDef *gamedef)
{
	DummyMap map(gamedef, {-1,-1,-1}, {1,1,1});
	map.fill({0,0,0}, {0,0,0}, CONTENT_AIR);

	std::unique_ptr<MMVManip> vm2;

	{
		MMVManip vm(&map);
		vm.initialEmerge({0,0,0}, {0,0,0});
		UASSERT(vm.exists({0,0,0}));
		vm.setNodeNoEmerge({0,0,0}, t_CONTENT_STONE);
		vm.setNodeNoEmerge({1,1,1}, t_CONTENT_GRASS);
		vm.setNodeNoEmerge({2,2,2}, CONTENT_IGNORE);
		// test out clone and reparent too
		vm2.reset(vm.clone());
	}

	UASSERT(vm2);
	UASSERT(vm2->isOrphan());
	vm2->reparent(&map);

	std::map<v3s16, MapBlock*> modified;
	vm2->blitBackAll(&modified);
	UASSERTEQ(size_t, modified.size(), 1);
	UASSERTEQ(auto, modified.begin()->first, v3s16(0,0,0));

	UASSERTEQ(auto, map.getNode({0,0,0}).getContent(), t_CONTENT_STONE);
	UASSERTEQ(auto, map.getNode({1,1,1}).getContent(), t_CONTENT_GRASS);
	// ignore nodes are not written (is this an intentional feature?)
	UASSERTEQ(auto, map.getNode({2,2,2}).getContent(), CONTENT_AIR);
}

void TestVoxelManipulator::testBlitBack2(IGameDef *gamedef)
{
	constexpr int bs = MAP_BLOCKSIZE;

	DummyMap map(gamedef, {0,0,0}, {1,1,1});
	map.fill({0,0,0}, {1,1,1}, CONTENT_AIR);

	// Create a vmanip "manually" without using initialEmerge
	MMVManip vm(&map);
	vm.addArea(VoxelArea({0,0,0}, v3s16(1,2,1) * bs - v3s16(1)));

	// Lower block is initialized with ignore, upper with lava
	for(s16 z=0; z<bs; z++)
	for(s16 y=0; y<2*bs; y++)
	for(s16 x=0; x<bs; x++) {
		auto c = y >= bs ? t_CONTENT_LAVA : CONTENT_IGNORE;
		vm.setNodeNoEmerge({x,y,z}, c);
	}
	// But pretend the upper block was not actually initialized
	vm.setFlags(VoxelArea({0,bs,0}, v3s16(1,2,1) * bs - v3s16(1)), VOXELFLAG_NO_DATA);
	// Add a node to the lower one
	vm.setNodeNoEmerge({0,1,0}, t_CONTENT_TORCH);

	// Verify covered blocks
	{
		auto cov = vm.getCoveredBlocks();
		UASSERTEQ(size_t, cov.size(), 2);
		auto it = cov.find({0,0,0});
		UASSERT(it != cov.end() && it->second);
		it = cov.find({0,1,0});
		UASSERT(it != cov.end() && !it->second);
	}

	// Now blit it back
	std::map<v3s16, MapBlock*> modified;
	vm.blitBackAll(&modified);
	// The lower block data should have been written
	UASSERTEQ(size_t, modified.size(), 1);
	UASSERTEQ(auto, modified.begin()->first, v3s16(0,0,0));
	UASSERTEQ(auto, map.getNode({0,1,0}).getContent(), t_CONTENT_TORCH);
	// The upper one should not!
	UASSERTEQ(auto, map.getNode({0,bs,0}).getContent(), CONTENT_AIR);
}
