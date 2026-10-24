// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include <sstream>
#include <map>
#include "gamedef.h"
#include "dummygamedef.h"
#include "nodedef.h"
#include "mapblock.h"
#include "serialization.h"
#include "noise.h"
#include "inventory.h"
#include "ididmapping.h"
#include "util/serialize.h"
#include "voxel.h"

class TestMapBlock : public TestBase
{
public:
	TestMapBlock() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMapBlock"; }

	void runTests(IGameDef *gamedef);

	void testSaveLoad(IGameDef *gamedef, u8 ver);
	inline void testSaveLoadLowest(IGameDef *gamedef) {
		testSaveLoad(gamedef, SER_FMT_VER_LOWEST_WRITE);
	}

	void testSave29(IGameDef *gamedef);
	void testModifierTiles();
	void testNodeModifiers();
	void testSetNodeModifiers();
	void testPruneNodeModifiers();
	void testPackedNodeModifiers();
	void testModifierIdWidths();
	void testModifierEntryCount();
	void testResolveNodeModifiers();
	void testCollectNodeModifiers();
	void testModifierSnapshot();
	void testAdaptiveModifiers();

	void testLoad29(IGameDef *gamedef);

	// Tests loading a MapBlock from Minetest-c55 0.3
	void testLoad20(IGameDef *gamedef);

	// Tests loading a non-standard MapBlock
	void testLoadNonStd(IGameDef *gamedef);

	// Tests blocks with a single recurring node
	void testMonoblock(IGameDef *gamedef);
};

static TestMapBlock g_test_instance;

void TestMapBlock::runTests(IGameDef *gamedef)
{
	TEST(testSaveLoad, gamedef, SER_FMT_VER_HIGHEST_WRITE);
	TEST(testSaveLoadLowest, gamedef);
	TEST(testSave29, gamedef);
	TEST(testModifierTiles);
	TEST(testNodeModifiers);
	TEST(testSetNodeModifiers);
	TEST(testPruneNodeModifiers);
	TEST(testPackedNodeModifiers);
	TEST(testModifierIdWidths);
	TEST(testModifierEntryCount);
	TEST(testResolveNodeModifiers);
	TEST(testCollectNodeModifiers);
	TEST(testModifierSnapshot);
	TEST(testAdaptiveModifiers);
	TEST(testLoad29, gamedef);
	TEST(testLoad20, gamedef);
	TEST(testLoadNonStd, gamedef);
	TEST(testMonoblock, gamedef);
}

////////////////////////////////////////////////////////////////////////////////

// To do: investigate what Codex has generated here
void TestMapBlock::testCollectNodeModifiers()
{
	DummyGameDef game;
	auto *manager = game.getWritableNodeModifierManager();
	u16 first = manager->add(NodeModifier("test:first"));
	u16 second = manager->add(NodeModifier("test:second"));
	u16 dummy = manager->add(NodeModifier("test:missing", true));
	MapBlock central({2, 0, 0}, &game), neighbor({1, 0, 0}, &game);
	const v3s16 pos(3, 4, 5);
	central.addNodeModifier(pos, first);
	central.addNodeModifier(pos, second);
	central.addNodeModifier(pos, first);
	central.addNodeModifier(pos, dummy);
	central.addNodeModifier({0, 0, 0}, 60000); // Unknown client-side ID.
	neighbor.addNodeModifier(pos, second);
	AppliedNodeModifiersSnapshot collected;
	const v3s16 origin = central.getPos();
	central.copyNodeModifiersToSnapshot(collected, origin);
	neighbor.copyNodeModifiersToSnapshot(collected, origin);
	UASSERTEQ(size_t, collected.size(), 5);
	auto [begin, end] = collected.findRange(pos);
	UASSERTEQ(size_t, end - begin, 3);
	UASSERTEQ(u16, begin[0].id, second);
	UASSERTEQ(u16, begin[1].id, first);
	UASSERT(manager->get(begin[2].id).m_is_dummy);
	auto [neighbor_begin, neighbor_end] = collected.findRange(pos - v3s16(MAP_BLOCKSIZE, 0, 0));
	UASSERTEQ(size_t, neighbor_end - neighbor_begin, 1);
	UASSERTEQ(u16, neighbor_begin->id, second);
	auto [unknown, unknown_end] = collected.findRange({0, 0, 0});
	UASSERTEQ(size_t, unknown_end - unknown, 1);
	UASSERT(manager->get(unknown->id).m_is_dummy);
	auto [missing, missing_end] = collected.findRange({2, 0, 0});
	UASSERT(missing == missing_end);
	central.removeNodeModifier(pos, first);
	UASSERTEQ(size_t, collected.findRange(pos).second - collected.findRange(pos).first, 3);

}

void TestMapBlock::testModifierTiles()
{
	NodeModifier source("test:tiles");
	source.m_modified_props_mask = MP_LightSource;
	source.m_light_source = 7;
	std::array<TileDef, 6> tiles;
	tiles[0].name = "top.png";
	tiles[5].name = "front.png";
	tiles[5].scale = 4;
	tiles[5].align_style = ALIGN_STYLE_WORLD;

	for (bool explicit_alpha : {false, true}) {
		if (explicit_alpha)
			source.m_alpha = ALPHAMODE_BLEND;
		if (explicit_alpha)
			source.m_modified_props_mask |= MP_Alpha;
		source.setTiles(tiles);
		UASSERTEQ(u64, source.m_modified_props_mask, MP_LightSource | MP_TileFirst | (u64{MP_TileFirst} << 5) |
				(explicit_alpha ? u64(MP_Alpha) : 0));

		std::stringstream stream;
		source.serialize(stream, 54);
		NodeModifier loaded("test:tiles");
		loaded.deSerialize(stream, 54);
		UASSERTEQ(u8, loaded.m_light_source, 7);
		UASSERTEQ(u64, loaded.m_modified_props_mask, source.m_modified_props_mask);
		UASSERT(loaded.getTiles());
		UASSERT(!(loaded.m_modified_props_mask & MP_Alpha) || loaded.m_alpha == source.m_alpha);
		UASSERTEQ(std::string, (*loaded.getTiles())[0].name, "top.png");
		UASSERT((*loaded.getTiles())[1].name.empty());
		UASSERTEQ(std::string, (*loaded.getTiles())[5].name, "front.png");
		UASSERTEQ(u8, (*loaded.getTiles())[5].scale, 4);
		UASSERT((*loaded.getTiles())[5].align_style == ALIGN_STYLE_WORLD);
	}

	// Replacing a definition drops stale face bits without losing other properties.
	tiles[0].name.clear();
	source.setTiles(tiles);
	UASSERTEQ(u64, source.m_modified_props_mask, MP_LightSource | MP_Alpha | (u64{MP_TileFirst} << 5));
	source.setTiles({});
	UASSERT(!source.hasTiles());
	UASSERT(!source.getTiles());
	UASSERTEQ(u64, source.m_modified_props_mask, MP_LightSource | MP_Alpha);

	NodeModifierManager manager;
	NodeModifier both("test:both"), top("test:top"), front("test:front");
	tiles[0].name = "top.png";
	both.setTiles(tiles);
	UASSERT(both.hasTiles());
	tiles[5].name.clear();
	top.setTiles(tiles);
	tiles[0].name.clear();
	tiles[5].name = "front.png";
	front.setTiles(tiles);
	u16 both_id = manager.add(std::move(both));
	u16 top_id = manager.add(std::move(top));
	u16 front_id = manager.add(std::move(front));

	AppliedNodeModifiersVector entries;
	UASSERT(entries.add({}, both_id, &manager));
	UASSERT(entries.add({}, top_id, &manager));
	UASSERTEQ(size_t, entries.size(), 2); // The first modifier still supplies the front.
	UASSERT(entries.add({}, front_id, &manager));
	UASSERTEQ(size_t, entries.size(), 2); // Both faces of the first modifier are covered.
	UASSERTEQ(u16, entries.begin()->id, top_id);
	UASSERTEQ(u16, (entries.begin() + 1)->id, front_id);
}

void TestMapBlock::testNodeModifiers()
{
	DummyGameDef source_game, target_game;
	auto *source_manager = source_game.getWritableNodeModifierManager();
	auto *target_manager = target_game.getWritableNodeModifierManager();
	u16 first = source_manager->add(NodeModifier("test:first"));
	u16 second = source_manager->add(NodeModifier("test:second"));
	// Deliberately give the same name a different runtime ID on load.
	target_manager->add(NodeModifier("test:other"));
	u16 target_first = target_manager->add(NodeModifier("test:first"));
	MapBlock source({}, &source_game);
	source.resetModified();
	const v3s16 pos(1, 2, 3);
	UASSERT(source.addNodeModifier(pos, first));
	UASSERT(source.addNodeModifier(pos, second));
	UASSERT(source.addNodeModifier(pos, first)); // Move first after second.
	UASSERT(!source.addNodeModifier({MAP_BLOCKSIZE, 0, 0}, first));
	UASSERT(!source.addNodeModifier(pos, MODIFIER_IGNORE));
	UASSERTEQ(u32, source.getModified(), MOD_STATE_WRITE_NEEDED);

	for (bool disk : {false, true}) {
		std::stringstream stream;
		source.serialize(stream, 30, disk, -1);
		MapBlock loaded({}, disk ? &target_game : &source_game);
		loaded.deSerialize(stream, 30, disk);
		AppliedNodeModifiersSnapshot entries;
		loaded.copyNodeModifiersToSnapshot(entries, loaded.getPos());
		UASSERTEQ(size_t, entries.size(), 2);
		auto [begin, end] = entries.findRange(pos);
		UASSERTEQ(size_t, end - begin, 2);
		u16 expected_second = disk ? target_manager->getId("test:second") : second;
		UASSERTEQ(u16, begin->id, expected_second);
		UASSERTEQ(u16, (begin + 1)->id, disk ? target_first : first);
		if (disk)
			UASSERT(target_manager->get(expected_second).m_is_dummy);

		// Older formats load into a fresh block without modifiers.
		std::stringstream legacy;
		source.serialize(legacy, 29, disk, -1);
		MapBlock legacy_block({}, disk ? &target_game : &source_game);
		legacy_block.deSerialize(legacy, 29, disk);
		AppliedNodeModifiersSnapshot empty;
		legacy_block.copyNodeModifiersToSnapshot(empty, legacy_block.getPos());
		UASSERT(empty.empty());
	}

	source.removeNodeModifier(pos, first);
	source.resetModified();
	source.removeNodeModifier(pos, second);
	UASSERTEQ(u32, source.getModified(), MOD_STATE_WRITE_NEEDED);
	std::stringstream stream;
	source.serialize(stream, 30, true, -1);
	MapBlock empty({}, &target_game);
	empty.deSerialize(stream, 30, true);
	AppliedNodeModifiersSnapshot entries;
	empty.copyNodeModifiersToSnapshot(entries, empty.getPos());
	UASSERT(entries.empty());
}

void TestMapBlock::testMonoblock(IGameDef *gamedef)
{
	MapBlock block({}, gamedef);
	UASSERT(!block.m_is_mono_block);

	// make the array is expanded
	block.expandNodesIfNeeded();
	UASSERT(std::all_of(block.data, block.data + MapBlock::nodecount, [](MapNode &n) { return n == MapNode(CONTENT_IGNORE); }));

	// convert to monoblock
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);
	UASSERT(block.data[0].param0 == CONTENT_IGNORE);

	block.data[0] = MapNode(CONTENT_AIR);
	UASSERT(block.m_is_mono_block);
	UASSERT(block.data[0].param0 == CONTENT_AIR);

	block.expandNodesIfNeeded();
	UASSERT(!block.m_is_mono_block);
	UASSERT(std::all_of(block.data, block.data + MapBlock::nodecount, [](MapNode &n) { return n == MapNode(CONTENT_AIR); }));

	// convert back to mono block
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);

	// deconvert explicitly
	block.expandNodesIfNeeded();
	UASSERT(!block.m_is_mono_block);

	// convert back to mono block
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);

	static_assert(CONTENT_AIR != 42);
	// set a node, should deconvert the block
	block.setNode(5,5,5, MapNode(42));
	UASSERT(!block.m_is_mono_block);

	// cannot convert to mono block
	block.tryShrinkNodes();
	UASSERT(!block.m_is_mono_block);

	// set all nodes to 42
	for (size_t i = 0; i < MapBlock::nodecount; ++i) {
		block.data[i] = MapNode(42);
	}

	// can convert to mono block
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);
	UASSERT(block.data[0].param0 == 42);

	VoxelManipulator vmm;
	v3s16 data_size(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE);
	vmm.addArea(VoxelArea(block.getPosRelative(), block.getPosRelative() + data_size + v3s16(1,1,1)));
	block.copyTo(vmm);
	UASSERT(block.m_is_mono_block);
	UASSERT(vmm.getNode({5,5,5}).param0 == 42);

	block.setNode(5,5,5,MapNode(23));

	block.copyFrom(vmm);
	UASSERT(block.m_is_mono_block);
	UASSERT(block.data[0].param0 == 42);

	vmm.setNode({5,5,5}, MapNode(23));
	block.copyFrom(vmm);
	UASSERT(!block.m_is_mono_block);

	vmm.setNode({5,5,5}, MapNode(42,1,0));
	block.copyFrom(vmm);
	UASSERT(!block.m_is_mono_block);

	vmm.setNode({5,5,5}, MapNode(42,0,1));
	block.copyFrom(vmm);
	UASSERT(!block.m_is_mono_block);

	vmm.setNode({5,5,5}, MapNode(42));
	block.copyFrom(vmm);
	UASSERT(block.m_is_mono_block);

	block.setNode(5,5,5,MapNode(23));
	block.tryShrinkNodes();
	UASSERT(!block.m_is_mono_block);

	block.setNode(5,5,5,MapNode(42, 1, 0));
	block.tryShrinkNodes();
	UASSERT(!block.m_is_mono_block);

	block.setNode(5,5,5,MapNode(42, 0, 1));
	block.tryShrinkNodes();
	UASSERT(!block.m_is_mono_block);

	block.setNode(5,5,5,MapNode(42));
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);
}

void TestMapBlock::testSaveLoad(IGameDef *gamedef, const u8 version)
{
	// Use the bottom node ids for this test
	content_t max = 0;
	{
		auto *ndef = gamedef->getNodeDefManager();
		const auto &unknown_name = ndef->get(CONTENT_UNKNOWN).name;
		while (ndef->get(max).name != unknown_name)
			max++;
	}
	UASSERT(max >= 2);
	UASSERT(max <= CONTENT_UNKNOWN);

	constexpr u64 seed = 0x207366616e3520ULL;
	std::stringstream ss;
	{
		MapBlock block({}, gamedef);
		// Fill with data
		PcgRandom r(seed);
		for (s16 z=0; z < MAP_BLOCKSIZE; z++)
		for (s16 y=0; y < MAP_BLOCKSIZE; y++)
		for (s16 x=0; x < MAP_BLOCKSIZE; x++) {
			u32 rval = r.next();
			block.setNodeNoCheck(x, y, z,
					MapNode(rval % max, (rval >> 16) & 0xff, (rval >> 24) & 0xff));
		}

		// Serialize
		block.serialize(ss, version, true, -1);
	}

	{
		MapBlock block({}, gamedef);
		// Deserialize
		block.deSerialize(ss, version, true);

		// Check data
		PcgRandom r(seed);
		for (s16 z=0; z < MAP_BLOCKSIZE; z++)
		for (s16 y=0; y < MAP_BLOCKSIZE; y++)
		for (s16 x=0; x < MAP_BLOCKSIZE; x++) {
			u32 rval = r.next();
			auto expect =
				MapNode(rval % max, (rval >> 16) & 0xff, (rval >> 24) & 0xff);
			UASSERT(block.getNodeNoCheck(x, y, z) == expect);
		}
	}
}

#define SS2_CHECK() UASSERT(!ss2.fail())

void TestMapBlock::testSave29(IGameDef *gamedef)
{
	auto *ndef = gamedef->getNodeDefManager();
	std::stringstream ss;

	{
		// Prepare test block
		MapBlock block({}, gamedef);
		for (s16 z=0; z < MAP_BLOCKSIZE; z++)
		for (s16 y=0; y < MAP_BLOCKSIZE; y++)
		for (s16 x=0; x < MAP_BLOCKSIZE; x++) {
			block.setNodeNoCheck(x, y, z, MapNode(CONTENT_AIR));
		}
		block.setNode({0, 0, 0}, MapNode(t_CONTENT_STONE));

		block.serialize(ss, 29, true, -1);
	}

	// Pick it apart a bit:
	std::stringstream ss2;
	decompressZstd(ss, ss2); // first zstd

	u8 flags = readU8(ss2);
	SS2_CHECK();
	UASSERT(flags & 0x02); // !is_air == true
	UASSERT(flags & 0x08); // !is_genereated == true

	ss2.seekg(2+4, std::ios_base::cur); // lighting_complete & timestamp
	SS2_CHECK();

	// nimap
	u8 ver = readU8(ss2);
	u16 count = readU16(ss2);
	SS2_CHECK();
	UASSERTEQ(int, ver, 0);
	// Invariant 1: map only contains nodes that are actually present in the data
	//              and no duplicates.
	UASSERTEQ(int, count, 2);
	std::string nn[2];
	content_t ii[2];
	for (int i = 0; i < 2; i++) {
		ii[i] = readU16(ss2);
		nn[i] = deSerializeString16(ss2);
	}
	// no particular order is guaranteed
	if (nn[0] > nn[1])
		std::swap(nn[0], nn[1]);
	UASSERTEQ(auto, nn[0], ndef->get(CONTENT_AIR).name);
	UASSERTEQ(auto, nn[1], ndef->get(t_CONTENT_STONE).name);
	// Invariant 2: IDs must start from zero
	if (ii[0] > ii[1])
		std::swap(ii[0], ii[1]);
	UASSERTEQ(int, ii[0], 0x0000);
	UASSERTEQ(int, ii[1], 0x0001);

	/*
	 * Quick note:
	 * Minetest deals with MapBlocks that violate these invariants (mostly) fine,
	 * but we should still be careful about them as third-party tools may rely on them.
	 */

	u8 content_width, params_width;
	content_width = readU8(ss2);
	params_width = readU8(ss2);
	SS2_CHECK();
	UASSERT(content_width == 2 && params_width == 2);
}

#undef SS2_CHECK

// The array was generated with: minetestmapper -i testworld --dumpblock 6,0,0 |
// python -c 'import sys;d=bytes.fromhex(sys.stdin.read().strip());print(",".join("%d"%c for c in d))'

static const u8 coded_mapblock29[] = {
	29,40,181,47,253,0,88,77,11,0,2,138,32,38,160,23,36,29,255,255,103,85,85,
	213,136,105,255,110,103,27,53,43,218,89,210,205,184,194,207,24,226,254,135,
	7,16,191,92,232,249,141,144,20,175,126,149,175,29,143,211,164,99,16,206,80,
	28,115,77,190,197,176,52,212,147,14,12,130,34,88,75,42,45,65,21,24,44,215,
	16,102,229,107,199,191,192,232,52,26,47,182,136,216,22,91,108,177,45,182,
	88,44,98,199,127,148,30,253,41,49,36,205,81,120,167,33,197,232,124,108,73,
	14,178,225,1,89,96,74,192,233,226,7,34,0,78,36,138,124,168,193,182,41,40,42,
	148,182,14,48,131,17,74,155,12,99,18,0,113,80,150,98,137,8,98,56,144,8,20,
	33,66,66,136,128,8,8,17,32,225,7,205,42,26,14,52,88,86,216,141,181,243,179,
	3,4,215,245,139,22,83,172,157,47,73,82,216,114,3,60,78,223,0,155,247,80,5,
	12,253,12,187,13,205,139,12,84,48,218,228,35,240,86,70,203,121,239,228,210,
	96,86,116,12,5,10,97,49,111,52,238,79,12,36,119,59,39,118,53,49,89,216,181,
	248,68,147,193,146,59,21,88,196,217,4,120,51,69,7,12,32,80,227,49,80,232,6,
	163,221,171,38,126,130,81,154,216,165,93,120,40,186,136,115,223,1,231,48,
	101,208,254,192,23,23,206,172,130,253,9,225,127,116,192,192,13,149,20,201,
	60,244,151,226,114,71,81,114,126,198,247,177,98,70,70,62,181,18,74,140,239,
	245,248,62,71,1,107,238,209,77,222,10,141,7,71,29,14,62,168,83,126,138,139,
	26,106
};

void TestMapBlock::testLoad29(IGameDef *gamedef)
{
	UASSERT(MAP_BLOCKSIZE == 16);
	const std::string_view buf(reinterpret_cast<const char*>(coded_mapblock29), sizeof(coded_mapblock29));

	// this node is not part of the test gamedef, so we also test handling of
	// unknown nodes here.
	auto *ndef = gamedef->getNodeDefManager();
	UASSERT(ndef->getId("default:chest") == CONTENT_IGNORE);

	std::istringstream iss;
	iss.str(std::string(buf));
	u8 version = readU8(iss);
	UASSERTEQ(int, version, 29);
	MapBlock block({}, gamedef);
	block.deSerialize(iss, version, true);

	auto content_chest = ndef->getId("default:chest");
	UASSERT(content_chest != CONTENT_IGNORE);

	// there are bricks at each corner
	const v3s16 pl[] = {
		{0, 0, 0}, {15, 0, 0}, {0, 15, 0}, {0, 0, 15},
		{15, 15, 0}, {15, 0, 15}, {0, 15, 15}, {15, 15, 15},
	};
	for (auto p : pl)
		UASSERTEQ(int, block.getNodeNoEx(p).getContent(), t_CONTENT_BRICK);

	// and a chest with an item inside
	UASSERTEQ(int, block.getNodeNoEx({0, 1, 0}).getContent(), content_chest);
	auto *meta = block.m_node_metadata.get({0, 1, 0});
	UASSERT(meta);
	auto *inv = meta->getInventory();
	UASSERT(inv);
	auto *ilist = inv->getList("main");
	UASSERT(ilist);
	UASSERTEQ(int, ilist->getSize(), 32);
	UASSERTEQ(auto, ilist->getItem(1).name, "default:stone");
}

static const u8 coded_mapblock20[] = {
	20,2,120,156,237,150,91,114,131,48,12,69,197,63,30,88,2,75,242,138,24,47,
	189,230,145,196,186,184,22,170,12,161,33,183,51,105,78,41,182,142,95,48,142,
	33,132,113,250,152,126,17,45,95,95,159,143,43,75,22,30,199,132,89,144,173,
	57,186,253,49,209,29,39,125,98,190,4,254,1,252,3,212,179,97,185,247,242,253,
	156,195,134,197,246,229,36,109,210,234,255,98,169,127,99,125,106,95,105,184,
	132,254,202,161,217,159,115,218,98,128,122,180,220,199,217,54,206,183,106,
	248,149,33,240,39,240,15,224,179,139,203,93,234,252,181,203,79,153,156,191,
	218,23,248,215,206,254,114,176,73,203,223,22,2,255,0,62,74,206,20,91,222,43,
	21,20,44,137,229,208,72,172,126,240,3,37,129,5,127,235,98,189,124,222,241,
	40,187,82,132,13,126,194,139,141,34,31,63,27,66,46,119,26,157,156,204,163,
	250,94,67,112,119,255,111,238,154,206,181,212,186,206,77,233,58,162,149,187,
	132,219,233,219,146,137,95,52,51,11,178,53,105,95,57,174,209,197,236,55,91,
	57,226,190,11,59,135,252,188,149,184,239,150,121,185,206,109,216,9,220,150,
	175,203,122,98,92,58,187,220,143,192,71,207,45,175,183,181,50,140,31,178,
	109,60,8,234,111,26,206,120,93,203,77,220,89,41,187,29,236,56,151,135,7,215,
	151,46,4,245,82,3,140,215,247,48,175,191,117,86,238,74,108,240,143,119,178,
	250,43,113,218,62,219,254,18,175,62,42,182,36,174,45,94,191,153,29,172,93,
	231,138,92,255,96,215,101,94,172,201,140,17,247,137,103,121,203,86,247,14,
	230,139,19,185,230,236,217,99,63,154,213,71,121,253,71,153,33,219,71,201,
	209,220,21,249,220,28,242,98,241,143,114,185,211,232,228,76,47,154,151,90,
	144,103,103,123,26,223,203,255,155,187,134,104,232,135,248,49,101,121,227,
	137,188,254,129,200,251,244,202,131,137,18,62,52,105,95,57,174,210,197,170,
	155,240,179,23,240,247,224,239,193,95,63,26,3,167,161,200,195,134,213,253,
	101,42,40,245,201,253,188,151,184,102,223,57,223,226,191,31,177,58,82,35,15,
	190,34,199,159,158,181,214,35,15,69,94,118,34,99,134,194,112,213,72,226,227,
	193,47,203,186,214,117,243,47,249,30,224,47,250,2,239,110,248,47,7,155,180,
	252,235,198,131,159,204,197,226,50,115,201,217,92,176,45,139,74,234,3,167,
	123,47,50,11,236,213,237,94,62,118,246,222,158,119,60,202,174,20,97,131,159,
	240,98,163,200,199,207,134,144,203,157,70,39,7,253,233,110,67,112,119,255,
	111,238,154,31,244,135,233,2,120,156,99,96,100,96,100,206,102,16,96,232,242,
	201,44,46,81,72,43,77,205,81,48,228,114,205,45,40,169,228,114,205,75,241,
	204,43,75,205,43,201,47,170,4,201,114,129,149,20,23,37,19,80,145,2,196,38,
	48,21,152,36,186,30,100,1,46,3,5,3,5,0,187,133,49,88,0,0,0,0,0,0,210,62,6
};

void TestMapBlock::testLoad20(IGameDef *gamedef)
{
	UASSERT(MAP_BLOCKSIZE == 16);
	const std::string_view buf(reinterpret_cast<const char*>(coded_mapblock20), sizeof(coded_mapblock20));

	// Conversion of minerals does not work if these nodes are not already
	// defined at load time. (Is this a bug? Does anyone even care?)
	gamedef->allocateUnknownNodeId("default:stone_with_coal");
	gamedef->allocateUnknownNodeId("default:stone_with_iron");

	std::istringstream iss;
	iss.str(std::string(buf));
	u8 version = readU8(iss);
	UASSERTEQ(int, version, 20);
	MapBlock block({}, gamedef);
	block.deSerialize(iss, version, true);

	auto *ndef = gamedef->getNodeDefManager();
	auto get_node = [&] (s16 x, s16 y, s16 z) -> std::string_view {
		MapNode n = block.getNodeNoEx({x, y, z});
		return ndef->get(n).name;
	};

	// These names come from content_mapnode.cpp and are hardcoded in the engine.
	UASSERTEQ(auto, get_node(11, 0, 3), "default:stone");
	UASSERTEQ(auto, get_node(11, 1, 3), "default:stone_with_coal");
	UASSERTEQ(auto, get_node(11, 2, 3), "default:dirt");
	UASSERTEQ(auto, get_node(10, 5, 4), "default:dirt_with_grass");
	UASSERTEQ(auto, get_node(10, 6, 4), "air");
	UASSERTEQ(auto, get_node(11, 6, 3), "default:furnace");

	for (s16 z=0; z < MAP_BLOCKSIZE; z++)
	for (s16 y=0; y < MAP_BLOCKSIZE; y++)
	for (s16 x=0; x < MAP_BLOCKSIZE; x++) {
		UASSERT(block.getNodeNoCheck(x, y, z).getContent() != CONTENT_IGNORE);
	}

	// metadata is also translated
	auto *meta = block.m_node_metadata.get({11, 6, 3});
	UASSERT(meta);
	UASSERT(meta->contains("formspec"));
	auto *inv = meta->getInventory();
	UASSERT(inv);
	auto *ilist = inv->getList("dst");
	UASSERT(ilist);
	UASSERTEQ(int, ilist->getSize(), 4);
}

static const u8 coded_mapblock_nonstd[] = {
	27,0,255,255,1,2,120,218,237,208,177,13,130,64,0,0,192,127,21,5,31,176,182,
	182,34,177,103,3,18,6,176,115,20,72,76,156,137,45,88,199,222,26,38,128,228,
	110,132,139,113,233,179,18,0,0,0,0,0,0,0,128,205,75,227,235,59,63,223,237,
	227,222,76,67,247,235,243,99,125,169,138,243,33,102,101,186,93,195,201,16,0,
	0,0,236,223,31,36,149,14,142,120,218,99,0,0,0,1,0,1,0,0,0,255,255,255,255,0,
	0,3,0,0,0,3,97,105,114,0,1,0,8,116,101,115,116,58,111,110,101,8,0,0,8,116,
	101,115,116,58,116,119,111,10,0,0
};

void TestMapBlock::testLoadNonStd(IGameDef *gamedef)
{
	/*
	 * Node IDs were originally 8-bit, then some special format that allowed exactly
	 * 2176 IDs (but only the first 128 could use the full range of param2), and
	 * finally 16-bit like today.
	 * The content_width field in MapBlocks was used around that time during the
	 * transition to 16-bit: <https://github.com/luanti-org/luanti/commit/ea62ee4b61371107ef3d693bda4c410ba02ca7e6>
	 * A content_width of 1 would normally never appear with a version > 24, but
	 * it is in fact perfectly possible to serialize a block in todays format with
	 * shortened IDs.
	 *
	 * This test checks that such a block is deserialized correctly.
	 * To future readers: I think it's worth not breaking this quirk (~sfan5)
	 */

	UASSERT(MAP_BLOCKSIZE == 16);
	const std::string_view buf(reinterpret_cast<const char*>(coded_mapblock_nonstd), sizeof(coded_mapblock_nonstd));

	std::istringstream iss;
	iss.str(std::string(buf));
	u8 version = readU8(iss);
	UASSERT(version > 24);
	MapBlock block({}, gamedef);
	block.deSerialize(iss, version, true);

	auto *ndef = gamedef->getNodeDefManager();
	UASSERTEQ(int, block.getNodeNoEx({0, 0, 0}).getContent(), ndef->getId("test:one"));
	UASSERTEQ(int, block.getNodeNoEx({0, 1, 0}).getContent(), ndef->getId("test:two"));

	// Check that param2 was handeled correctly
	const static u8 data_lo[] = {8, 3, 14, 7, 13, 9, 6, 2, 1, 5, 12, 11, 15, 10, 0, 4};
	const static u8 data_hi[] = {11, 125, 85, 131, 204, 44, 92, 55, 35, 25, 41, 181, 124, 70, 245, 73};
	for (s16 i = 0; i < 16; i++)
		UASSERTEQ(int, block.getNodeNoEx({i, 0, 0}).param2, data_hi[i]);
	for (s16 i = 0; i < 16; i++)
		UASSERTEQ(int, block.getNodeNoEx({i, 1, 0}).param2, data_lo[i]);
}

void TestMapBlock::testSetNodeModifiers()
{
	AppliedNodeModifiersStorage list;
	const v3s16 before(0, 0, 0), pos(1, 0, 0), after(2, 0, 0);
	UASSERT(list.add(before, 10));
	UASSERT(list.add(after, 20));
	auto check = [&](const std::vector<u16> &expected) {
		UASSERT(list.size() == expected.size() + 2);
		UASSERTEQ(u16, list.findRange(before).first->id, 10);
		UASSERTEQ(u16, list.findRange(after).first->id, 20);
		auto [first, last] = list.findRange(pos);
		std::vector<u16> actual;
		for (auto it = first; it != last; ++it)
			actual.push_back(it->id);
		UASSERT(actual == expected);
	};
	UASSERT(list.set(pos, {1, 2, 1, 3, 2}));
	check({1, 3, 2});
	UASSERT(list.set(pos, {4, 5, 6}));
	check({4, 5, 6});
	UASSERT(list.set(pos, {7}));
	check({7});
	UASSERT(list.set(pos, {1, 2, 3, 4}));
	check({1, 2, 3, 4});
	UASSERT(!list.set(pos, {5, MODIFIER_IGNORE}));
	check({1, 2, 3, 4});
	std::vector<u16> full;
	for (u32 id = 0; id < MODIFIER_IGNORE; ++id)
		full.push_back(id);
	UASSERT(!list.set(pos, full));
	check({1, 2, 3, 4});
	full.resize(MAX_NODE_MODIFIERS);
	UASSERT(list.set(pos, full));
	check(full);
	UASSERT(list.set(pos, {}));
	check({});
}

void TestMapBlock::testResolveNodeModifiers()
{
	NodeModifierManager manager;
	auto add = [&](const char *name, u8 light, bool overrides, bool dummy = false) {
		NodeModifier modifier(name, dummy);
		modifier.m_light_source = light;
		modifier.m_modified_props_mask = overrides ? static_cast<u64>(MP_LightSource) : 0;
		return manager.add(std::move(modifier));
	};
	u16 bright = add("test:bright", 12, true);
	u16 dim = add("test:dim", 3, true);
	u16 off = add("test:off", 0, true);
	u16 unrelated = add("test:unrelated", 8, false);
	u16 dummy = add("test:dummy", 14, true, true);
	ContentLightingFlags base{};
	base.light_source = 7;
	base.has_light = true;
	base.light_propagates = true;
	base.sunlight_propagates = false;

	AppliedNodeModifiersStorage list;
	const v3s16 p(0, 0, 0);
	UASSERT(list.resolveLighting(p, manager).light_source == 0);
	UASSERT(list.resolveLighting(p, manager, base) == base);
	UASSERT(list.set(p, {bright, unrelated, dim, dummy}));
	ContentLightingFlags value = list.resolveLighting(p, manager, base);
	UASSERT(value.light_source == 3);
	ContentLightingFlags expected = base;
	expected.light_source = 3;
	UASSERT(value == expected);
	UASSERT(base.light_source == 7);
	UASSERT(list.add(p, bright)); // Reapplying changes precedence.
	UASSERT(list.resolveLighting(p, manager).light_source == 12);
	UASSERT(list.add(p, off));
	expected.light_source = 0;
	UASSERT(list.resolveLighting(p, manager, base) == expected);
	UASSERT(list.set(v3s16(0, 0, 2), {dummy, unrelated, 60000}));
	UASSERT(list.resolveLighting(v3s16(0, 0, 2), manager).light_source == 0);
	UASSERT(list.set(v3s16(0, 1, 2), {bright}));
	UASSERT(list.set(v3s16(1, 2, 2), {dim}));
}

void TestMapBlock::testPackedNodeModifiers()
{
	AppliedNodeModifiersStorage list;
	NameIdMapping names;
	for (u16 id = 0; id < 16; ++id)
		UASSERT(list.add({15, 15, 15}, id));
	UASSERT(!list.add({15, 15, 15}, 16));
	UASSERT(list.add({15, 15, 15}, 0));
	for (bool disk : {false, true}) {
		for (u16 id = 0; id < 16; ++id)
			names.set(id, "test:modifier_" + std::to_string(id));
		std::stringstream stream;
		list.serialize(stream, 30, disk, names);
		if (!disk) {
			UASSERTEQ(size_t, stream.str().size(), 38);
			UASSERTEQ(u32, readU32(stream), 16);
			UASSERTEQ(u16, readU16(stream), 0xffff);
			stream.seekg(0);
		}
		AppliedNodeModifiersStorage loaded;
		loaded.deSerialize(stream, 30, disk, names);
		UASSERTEQ(size_t, loaded.size(), 16);
		UASSERTEQ(u16, loaded.findRange({15, 15, 15}).first->id, 1);
		UASSERTEQ(u16, (loaded.findRange({15, 15, 15}).second - 1)->id, 0);
	}
	// Nonzero first position, consecutive groups, and coordinate-boundary crossings.
	AppliedNodeModifiersStorage deltas;
	for (v3s16 pos : {v3s16(0, 0, 14), v3s16(0, 0, 15), v3s16(0, 1, 0),
			v3s16(1, 0, 0), v3s16(15, 15, 15)})
		UASSERT(deltas.add(pos, 0));
	std::stringstream delta_stream;
	deltas.serialize(delta_stream, 30, false, names);
	UASSERTEQ(u32, readU32(delta_stream), 5);
	for (u16 delta : {14, 1, 1, 240, 3839}) {
		UASSERTEQ(u16, readU16(delta_stream), delta);
		UASSERTEQ(u16, readU16(delta_stream), 0);
	}
	delta_stream.seekg(0);
	AppliedNodeModifiersStorage delta_loaded;
	delta_loaded.deSerialize(delta_stream, 30, false, names);
	deltas.forEach([&](const AppliedNodeModifier &entry) {
		auto range = delta_loaded.findRange(entry.pos);
		UASSERTEQ(size_t, range.second - range.first, 1);
		UASSERTEQ(u16, range.first->id, entry.id);
	});
	std::stringstream overflow;
	writeU32(overflow, 2);
	writeU16(overflow, 0x0fff);
	writeU16(overflow, 0);
	writeU16(overflow, 1); // 4095 + 1 is outside the block.
	writeU16(overflow, 0);
	EXCEPTION_CHECK(SerializationError, delta_loaded.deSerialize(overflow, 30, false, names));

	// The full block exceeds the old 16-bit total-entry limit.
	for (s16 x = 0; x < 16; ++x)
	for (s16 y = 0; y < 16; ++y)
	for (s16 z = 0; z < 16; ++z)
		for (u16 id = 0; id < 16; ++id)
			UASSERT(list.add({x, y, z}, id));
	std::stringstream stream;
	list.serialize(stream, 30, false, names);
	AppliedNodeModifiersStorage loaded;
	loaded.deSerialize(stream, 30, false, names);
	UASSERTEQ(size_t, loaded.size(), 65536);
	std::stringstream invalid;
	writeU32(invalid, 2);
	for (int i = 0; i < 2; ++i) {
		writeU16(invalid, 0);
		writeU16(invalid, 1);
	}
	EXCEPTION_CHECK(SerializationError, loaded.deSerialize(invalid, 30, false, names));
	std::stringstream truncated;
	writeU32(truncated, 16);
	writeU16(truncated, 0xf000);
	writeU16(truncated, 1);
	EXCEPTION_CHECK(SerializationError, loaded.deSerialize(truncated, 30, false, names));
	std::stringstream overrun;
	writeU32(overrun, 1);
	writeU16(overrun, 0x1000); // Two entries when only one remains.
	EXCEPTION_CHECK(SerializationError, loaded.deSerialize(overrun, 30, false, names));
	std::stringstream oversized;
	writeU32(oversized, 65537);
	EXCEPTION_CHECK(SerializationError, loaded.deSerialize(oversized, 30, false, names));
	AppliedNodeModifiersStorage empty;
	std::stringstream empty_stream;
	empty.serialize(empty_stream, 30, false, names);
	UASSERTEQ(size_t, empty_stream.str().size(), 4);
	loaded.deSerialize(empty_stream, 30, false, names);
	UASSERT(loaded.empty());

}

void TestMapBlock::testModifierEntryCount()
{
	auto check = [](const AppliedNodeModifiersStorage &list, u32 expected) {
		std::stringstream stream;
		NameIdMapping names;
		list.serialize(stream, 30, false, names);
		UASSERTEQ(u32, readU32(stream), expected);
		stream.seekg(0);
		AppliedNodeModifiersStorage loaded;
		loaded.deSerialize(stream, 30, false, names);
		UASSERTEQ(size_t, loaded.size(), list.size());
		UASSERT(stream.peek() == std::char_traits<char>::eof());
	};
	const v3s16 a(0, 0, 0), b(1, 0, 0);
	AppliedNodeModifiersStorage list;
	check(list, 0);
	list.add(a, 1);
	list.add(a, 2);
	list.add(a, 1);
	check(list, 2);
	UASSERT(!list.add(b, MODIFIER_IGNORE));
	check(list, 2);
	list.set(b, {3, 4, 3});
	check(list, 4);
	list.remove(a, 2);
	check(list, 3);
	list.remove(a, 1);
	check(list, 2);
	list.set(b, {});
	check(list, 0);
	list.set(a, {});
	list.remove(b);
	check(list, 0);
	list.set(a, {1});
	AppliedNodeModifiersStorage other;
	other.set(a, {1});
	other.add(b, 2);
	check(other, 2);
	other.remove(a);
	check(other, 1);
	check(list, 1);
	list.clear();
	check(list, 0);
	list.clear();
	UASSERT(list.add(a, 3));
	check(list, 1);
}

void TestMapBlock::testModifierIdWidths()
{
	for (u16 count : {1, 256, 257}) {
		for (bool disk : {false, true}) {
			NameIdMapping names;
			AppliedNodeModifiersStorage list;
			for (u16 id = 0; id < count; ++id) {
				names.set(id, "test:modifier_" + std::to_string(id));
				UASSERT(list.add({s16(id / 256), s16((id / 16) % 16),
						s16(id % 16)}, id));
			}
			std::stringstream mapping;
			if (disk)
				names.serialize(mapping);
			std::stringstream stream;
			list.serialize(stream, 30, disk, names);
			const size_t width = disk && count <= 256 ? 1 : 2;
			UASSERTEQ(size_t, stream.str().size(), 4 + mapping.str().size() + count * (2 + width));
			writeU8(stream, 0xab); // Decoding must stop at the next field.
			AppliedNodeModifiersStorage loaded;
			loaded.deSerialize(stream, 30, disk, names);
			UASSERTEQ(size_t, loaded.size(), count);
			u16 id = 0;
			loaded.forEach([&](const AppliedNodeModifier &entry) {
				UASSERTEQ(u16, entry.id, id);
				UASSERT(entry.pos == v3s16(id / 256, (id / 16) % 16, id % 16));
				++id;
			});
			UASSERTEQ(u16, id, count);
			UASSERTEQ(u8, readU8(stream), 0xab);
		}
	}
}

void TestMapBlock::testModifierSnapshot()
{
	NodeModifierManager manager;
	NodeModifier bright("test:bright"), dim("test:dim");
	bright.m_modified_props_mask = dim.m_modified_props_mask = MP_LightSource;
	bright.m_light_source = 14;
	dim.m_light_source = 3;
	const u16 a = manager.add(std::move(bright));
	const u16 b = manager.add(std::move(dim));
	AppliedNodeModifiersStorage first, second;
	UASSERT(first.set({1, 0, 0}, {a, b}));
	UASSERT(second.add({1, 0, 0}, a));
	AppliedNodeModifiersSnapshot snapshot;
	UASSERT(snapshot.empty());
	snapshot.append(first, {});
	snapshot.append(second, {}); // Overlapping positions preserve append order.
	snapshot.append(second, {-16, 0, 0});
	snapshot.append(second, {16, 0, 0});
	UASSERTEQ(size_t, snapshot.size(), 5);
	auto [begin, end] = snapshot.findRange({1, 0, 0});
	UASSERTEQ(size_t, end - begin, 3);
	UASSERT(begin[0].id == a && begin[1].id == b && begin[2].id == a);
	first.remove({1, 0, 0});
	second.set({1, 0, 0}, {b});
	UASSERTEQ(u8, snapshot.resolveLighting({1, 0, 0}, manager).light_source, 14);
}

void TestMapBlock::testAdaptiveModifiers()
{
	NodeModifierManager manager;
	for (u16 id = 0; id < 16; ++id) {
		NodeModifier modifier("test:light_" + std::to_string(id));
		modifier.m_modified_props_mask = MP_LightSource;
		modifier.m_light_source = id % 14 + 1;
		UASSERTEQ(u16, manager.add(std::move(modifier)), id);
	}
	using Model = std::map<v3s16, std::vector<u16>>;
	Model model;
	AppliedNodeModifiersStorage list;
	auto verify = [&](const AppliedNodeModifiersStorage &actual, const Model &expected) {
		std::vector<AppliedNodeModifier> flat;
		for (const auto &[pos, ids] : expected)
			for (u16 id : ids)
				flat.push_back({pos, id});
		UASSERTEQ(size_t, actual.size(), flat.size());
		size_t index = 0;
		actual.forEach([&](const AppliedNodeModifier &entry) {
			UASSERT(index < flat.size());
			UASSERT(entry.pos == flat[index].pos && entry.id == flat[index].id);
			++index;
		});
		UASSERTEQ(size_t, index, flat.size());
	};

	// A dense row forces splitting through all three coordinates. Sparse
	// siblings exercise branches containing a mixture of leaves and branches.
	for (s16 z = 0; z < 16; ++z) {
		const v3s16 pos(7, 9, z);
		for (u16 id = 0; id < 16; ++id) {
			UASSERT(list.add(pos, id));
			model[pos].push_back(id);
		}
	}
	for (s16 x = 0; x < 16; ++x)
	for (s16 y = 0; y < 16; ++y) {
		const v3s16 pos(x, y, 0);
		if (!model.count(pos)) {
			UASSERT(list.set(pos, {2, 1, 2}));
			model[pos] = {1, 2};
		}
	}
	verify(list, model);
	UASSERT(!list.add({7, 9, 0}, 16));
	UASSERT(!list.set({7, 9, 0}, {MODIFIER_IGNORE}));

	// Remapping IDs while writing must leave the original tree unchanged.
	NameIdMapping disk_names;
	auto &mapping = IdIdMapping::giveClearedThreadLocalInstance();
	for (u16 id = 0; id < 16; ++id) {
		mapping.set(id, 15 - id);
		disk_names.set(15 - id, "test:light_" + std::to_string(id));
	}
	std::stringstream disk_stream;
	list.serialize(disk_stream, 30, true, disk_names, &mapping);
	AppliedNodeModifiersStorage copy;
	copy.deSerialize(disk_stream, 30, true, disk_names);
	auto remapped = model;
	for (auto &[pos, ids] : remapped)
		for (auto &id : ids)
			id = 15 - id;
	verify(copy, remapped);
	verify(list, model);
	copy.clear();
	verify(copy, {});
	verify(list, model);
	UASSERT(copy.add({1, 2, 3}, 4));
	verify(copy, {{{1, 2, 3}, {4}}});

	// Bulk loading can choose a different shape, but must preserve wire order.
	std::stringstream encoded;
	NameIdMapping names;
	list.serialize(encoded, 30, false, names);
	AppliedNodeModifiersStorage loaded;
	loaded.deSerialize(encoded, 30, false, names);
	verify(loaded, model);
	std::stringstream reencoded;
	loaded.serialize(reencoded, 30, false, names);
	UASSERT(encoded.str() == reencoded.str());

	AppliedNodeModifiersSnapshot snapshot;
	snapshot.append(loaded, {-16, 16, 0});
	for (const auto &[pos, ids] : model) {
		auto [first, last] = snapshot.findRange(pos + v3s16(-16, 16, 0));
		UASSERTEQ(size_t, last - first, ids.size());
		for (u16 id : ids)
			UASSERTEQ(u16, (first++)->id, id);
	}

	// Exercise mixed edits against an independent ordered reference model.
	u32 random = 1234567;
	auto next = [&]() { random = random * 1664525 + 1013904223; return random >> 16; };
	for (size_t i = 0; i < 2000; ++i) {
		const v3s16 pos(next() % 16, next() % 16, next() % 16);
		const u16 id = next() % 16;
		auto found = model.find(pos);
		switch (next() % 4) {
		case 0: {
			auto &ids = model[pos];
			auto it = std::find(ids.begin(), ids.end(), id);
			if (it != ids.end())
				ids.erase(it);
			ids.push_back(id);
			UASSERT(loaded.add(pos, id));
			break;
		}
		case 1:
			UASSERT(loaded.set(pos, {id, u16((id + 1) % 16), id}));
			model[pos] = {u16((id + 1) % 16), id};
			break;
		case 2: {
			bool removed = false;
			if (found != model.end()) {
				auto &ids = found->second;
				auto it = std::find(ids.begin(), ids.end(), id);
				if (it != ids.end()) {
					ids.erase(it);
					removed = true;
				}
				if (ids.empty())
					model.erase(found);
			}
			UASSERT(loaded.remove(pos, id) == removed);
			break;
		}
		case 3:
			UASSERT(loaded.remove(pos) == (found != model.end()));
			model.erase(pos);
			break;
		}
	}
	verify(loaded, model);
	for (const auto &[pos, ids] : model)
		UASSERT(loaded.remove(pos));
	verify(loaded, {}); // All retained branches are now empty.
	UASSERT(loaded.set({15, 15, 15}, {1, 2}));
	verify(loaded, {{{15, 15, 15}, {1, 2}}});
}

void TestMapBlock::testPruneNodeModifiers()
{
	DummyGameDef game;
	auto *manager = game.getWritableNodeModifierManager();
	auto define = [&](const char *name, u64 mask, bool dummy = false) {
		NodeModifier modifier(name, dummy);
		modifier.m_modified_props_mask = mask;
		return manager->add(std::move(modifier));
	};
	// Synthetic bits exercise unions and partial coverage of future properties.
	const u16 both = define("test:both", 3);
	const u16 first = define("test:first", 1);
	const u16 second = define("test:second", 2);
	const u16 zero = define("test:zero", 0);
	const u16 dummy = define("test:dummy", 3, true);
	const u16 unknown = 60000;
	MapBlock block({}, &game);
	const v3s16 pos(7, 9, 1);
	auto check = [&](std::initializer_list<u16> expected) {
		auto [begin, end] = block.getNodeModifiers(pos);
		UASSERTEQ(size_t, end - begin, expected.size());
		for (u16 id : expected)
			UASSERTEQ(u16, (begin++)->id, id);
	};
	UASSERT(block.setNodeModifiers(pos, {both, first}));
	UASSERT(block.addNodeModifier(pos, second));
	check({both, first, second}); // Opt-in only.
	UASSERT(block.addNodeModifier(pos, second, true));
	check({first, second}); // Union of later masks covers both.
	UASSERT(block.setNodeModifiers(pos, {both}));
	UASSERT(block.addNodeModifier(pos, first, true));
	check({both, first}); // Partial coverage must retain the earlier modifier.
	UASSERT(block.addNodeModifier(pos, both, true));
	check({both}); // Reapplication moves the modifier to the end before pruning.
	UASSERT(block.setNodeModifiers(pos, {both, zero, unknown}));
	UASSERT(block.addNodeModifier(pos, dummy, true));
	check({both, zero, unknown, dummy}); // Dummy masks provide no coverage.
	UASSERT(block.addNodeModifier(pos, first, true));
	UASSERT(block.addNodeModifier(pos, second, true));
	check({zero, unknown, dummy, first, second});

	std::vector<u16> full;
	for (u16 i = 0; i < 16; ++i) {
		NodeModifier modifier("test:full_" + std::to_string(i));
		modifier.m_modified_props_mask = 1;
		full.push_back(manager->add(std::move(modifier)));
	}
	UASSERT(block.setNodeModifiers(pos, full));
	UASSERT(block.addNodeModifier(pos, first, true));
	check({first}); // Pruning can make room in an already-full node.
	for (u16 id : full) {
		NodeModifier modifier(manager->get(id).m_name);
		manager->add(std::move(modifier)); // Now all are propertyless.
	}
	UASSERT(block.setNodeModifiers(pos, full));
	UASSERT(!block.addNodeModifier(pos, first, true));
	auto [begin, end] = block.getNodeModifiers(pos);
	UASSERTEQ(size_t, end - begin, full.size());
	for (u16 id : full)
		UASSERTEQ(u16, (begin++)->id, id);
	UASSERT(!block.addNodeModifier(pos, MODIFIER_IGNORE, true));
	auto unchanged = block.getNodeModifiers(pos);
	UASSERTEQ(size_t, unchanged.second - unchanged.first, full.size());
}
