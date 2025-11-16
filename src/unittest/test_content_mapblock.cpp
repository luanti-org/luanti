// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Vitaliy Lobachevskiy

#include "test.h"

#include <algorithm>
#include <numeric>

#include "gamedef.h"
#include "inventory.h" // ItemStack
#include "dummygamedef.h"
#include "client/content_mapblock.h"
#include "client/mapblock_mesh.h"
#include "client/meshgen/collector.h"
#include "client/node_visuals.h"
#include "mesh_compare.h"

namespace {

class MockGameDef : public DummyGameDef {
public:
	IWritableItemDefManager *item_mgr() noexcept {
		return static_cast<IWritableItemDefManager *>(m_itemdef);
	}

	NodeDefManager *node_mgr() noexcept {
		return const_cast<NodeDefManager *>(m_nodedef);
	}

	// ContentFeatures that doesn't destroy the visuals
	// Needed because nodedef.set(feature) creates a copy of the ContentFeatures and since
	// the NodeDefManager destructs its ContentFeatures, this prevents double free.
	// Should only be used if the visuals get freed somewhere else.
	struct CContentFeatures : public ContentFeatures {
		~CContentFeatures() { visuals = nullptr; }
	};

	content_t registerNode(ItemDefinition itemdef, const CContentFeatures &nodedef) {
		item_mgr()->registerItem(itemdef);
		return node_mgr()->set(nodedef.name, nodedef);
	}

	void finalize() {
		node_mgr()->resolveCrossrefs();

		// Need to fill node visuals for predefined nodes
		node_mgr()->applyFunction([] (ContentFeatures &f) {
			if (!f.visuals)
				f.visuals = constructNodeVisuals(&f);
		});
	}

	MeshMakeData makeSingleNodeMMD(bool smooth_lighting = true)
	{
		MeshMakeData data{ndef(), 1, MeshGrid{1}};
		data.m_generate_minimap = false;
		data.m_smooth_lighting = smooth_lighting;
		data.m_enable_water_reflections = false;
		data.m_blockpos = {0, 0, 0};
		for (s16 x = -1; x <= 1; x++)
		for (s16 y = -1; y <= 1; y++)
		for (s16 z = -1; z <= 1; z++)
			data.m_vmanip.setNode({x, y, z}, {CONTENT_AIR, 0, 0});
		return data;
	}

	content_t addSimpleNode(std::string name, u32 texture)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name;
		itemdef.description = name;

		CContentFeatures f;
		f.visuals = constructNodeVisuals(&f);
		f.name = itemdef.name;
		f.drawtype = NDT_NORMAL;
		f.visuals->solidness = 2;
		f.alpha = ALPHAMODE_OPAQUE;
		for (TileDef &tiledef : f.tiledef)
			tiledef.name = name + ".png";
		for (TileSpec &tile : f.visuals->tiles)
			tile.layers[0].texture_id = texture;

		return registerNode(itemdef, f);
	}

	content_t addLiquidSource(std::string name, u32 texture)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name + "_source";
		itemdef.description = name;

		CContentFeatures f;
		f.visuals = constructNodeVisuals(&f);
		f.name = itemdef.name;
		f.drawtype = NDT_LIQUID;
		f.visuals->solidness = 1;
		f.alpha = ALPHAMODE_BLEND;
		f.light_propagates = true;
		f.param_type = CPT_LIGHT;
		f.liquid_type = LIQUID_SOURCE;
		f.liquid_viscosity = 4;
		f.groups["liquids"] = 3;
		f.liquid_alternative_source = "test:" + name + "_source";
		f.liquid_alternative_flowing = "test:" + name + "_flowing";
		for (TileDef &tiledef : f.tiledef)
			tiledef.name = name + ".png";
		for (TileSpec &tile : f.visuals->tiles)
			tile.layers[0].texture_id = texture;

		return registerNode(itemdef, f);
	}

	content_t addLiquidFlowing(std::string name, u32 texture_top, u32 texture_side)
	{
		ItemDefinition itemdef;
		itemdef.type = ITEM_NODE;
		itemdef.name = "test:" + name + "_flowing";
		itemdef.description = name;

		CContentFeatures f;
		f.visuals = constructNodeVisuals(&f);
		f.name = itemdef.name;
		f.drawtype = NDT_FLOWINGLIQUID;
		f.visuals->solidness = 0;
		f.alpha = ALPHAMODE_BLEND;
		f.light_propagates = true;
		f.param_type = CPT_LIGHT;
		f.liquid_type = LIQUID_FLOWING;
		f.liquid_viscosity = 4;
		f.groups["liquids"] = 3;
		f.liquid_alternative_source = "test:" + name + "_source";
		f.liquid_alternative_flowing = "test:" + name + "_flowing";
		f.tiledef_special[0].name = name + "_top.png";
		f.tiledef_special[1].name = name + "_side.png";
		f.visuals->special_tiles[0].layers[0].texture_id = texture_top;
		f.visuals->special_tiles[1].layers[0].texture_id = texture_side;

		return registerNode(itemdef, f);
	}
};

void set_light_decode_table()
{
	u8 table[LIGHT_SUN + 1] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	};
	memcpy(const_cast<u8 *>(light_decode_table), table, sizeof(table));
}

class TestMapblockMeshGenerator : public TestBase {
public:
	TestMapblockMeshGenerator() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestMapblockMeshGenerator"; }

	void runTests(IGameDef *gamedef) override;
	void testSimpleNode();
	void testSurroundedNode();
	void testInterliquidSame();
	void testInterliquidDifferent();
};

static TestMapblockMeshGenerator g_test_instance;

void TestMapblockMeshGenerator::runTests(IGameDef *gamedef)
{
	set_light_decode_table();
	TEST(testSimpleNode);
	TEST(testSurroundedNode);
	TEST(testInterliquidSame);
	TEST(testInterliquidDifferent);
}

namespace quad {
	constexpr float h = BS / 2.0f;
	const Quad zp{{{{-h, -h, h}, {0, 0, 1}, 0, {1, 1}}, {{h, -h, h}, {0, 0, 1}, 0, {0, 1}}, {{h, h, h}, {0, 0, 1}, 0, {0, 0}}, {{-h, h, h}, {0, 0, 1}, 0, {1, 0}}}};
	const Quad yp{{{{-h, h, -h}, {0, 1, 0}, 0, {0, 1}}, {{-h, h, h}, {0, 1, 0}, 0, {0, 0}}, {{h, h, h}, {0, 1, 0}, 0, {1, 0}}, {{h, h, -h}, {0, 1, 0}, 0, {1, 1}}}};
	const Quad xp{{{{h, -h, -h}, {1, 0, 0}, 0, {0, 1}}, {{h, h, -h}, {1, 0, 0}, 0, {0, 0}}, {{h, h, h}, {1, 0, 0}, 0, {1, 0}}, {{h, -h, h}, {1, 0, 0}, 0, {1, 1}}}};
	const Quad zn{{{{-h, -h, -h}, {0, 0, -1}, 0, {0, 1}}, {{-h, h, -h}, {0, 0, -1}, 0, {0, 0}}, {{h, h, -h}, {0, 0, -1}, 0, {1, 0}}, {{h, -h, -h}, {0, 0, -1}, 0, {1, 1}}}};
	const Quad yn{{{{-h, -h, -h}, {0, -1, 0}, 0, {0, 0}}, {{h, -h, -h}, {0, -1, 0}, 0, {1, 0}}, {{h, -h, h}, {0, -1, 0}, 0, {1, 1}}, {{-h, -h, h}, {0, -1, 0}, 0, {0, 1}}}};
	const Quad xn{{{{-h, -h, -h}, {-1, 0, 0}, 0, {1, 1}}, {{-h, -h, h}, {-1, 0, 0}, 0, {0, 1}}, {{-h, h, h}, {-1, 0, 0}, 0, {0, 0}}, {{-h, h, -h}, {-1, 0, 0}, 0, {1, 0}}}};
}

void TestMapblockMeshGenerator::testSimpleNode()
{
	MockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::xp, quad::yn, quad::yp, quad::zn, quad::zp}));
}

void TestMapblockMeshGenerator::testSurroundedNode()
{
	MockGameDef gamedef;
	content_t stone = gamedef.addSimpleNode("stone", 42);
	content_t wood = gamedef.addSimpleNode("wood", 13);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {stone, 0, 0});
	data.m_vmanip.setNode({1, 0, 0}, {wood, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::yn, quad::yp, quad::zn, quad::zp}));
}

void TestMapblockMeshGenerator::testInterliquidSame()
{
	MockGameDef gamedef;
	auto water = gamedef.addLiquidSource("water", 42);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {water, 0, 0});
	data.m_vmanip.setNode({1, 0, 0}, {water, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::yn, quad::yp, quad::zn, quad::zp}));
}

void TestMapblockMeshGenerator::testInterliquidDifferent()
{
	MockGameDef gamedef;
	auto water = gamedef.addLiquidSource("water", 42);
	auto lava = gamedef.addLiquidSource("lava", 13);
	gamedef.finalize();

	MeshMakeData data = gamedef.makeSingleNodeMMD();
	data.m_vmanip.setNode({0, 0, 0}, {water, 0, 0});
	data.m_vmanip.setNode({0, 0, 1}, {lava, 0, 0});

	MeshCollector col{{}};
	MapblockMeshGenerator mg{&data, &col};
	mg.generate();
	UASSERTEQ(std::size_t, col.prebuffers[0].size(), 1);
	UASSERTEQ(std::size_t, col.prebuffers[1].size(), 0);

	auto &&buf = col.prebuffers[0][0];
	UASSERTEQ(u32, buf.layer.texture_id, 42);
	UASSERT(checkMeshEqual(buf.vertices, buf.indices, {quad::xn, quad::xp, quad::yn, quad::yp, quad::zn, quad::zp}));
}

}
