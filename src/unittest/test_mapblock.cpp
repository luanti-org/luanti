// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"

#include <sstream>
#include "gamedef.h"
#include "nodedef.h"
#include "mapblock.h"
#include "serialization.h"
#include "noise.h"
#include "inventory.h"
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
	TEST(testLoad29, gamedef);
	TEST(testLoad20, gamedef);
	TEST(testLoadNonStd, gamedef);
	TEST(testMonoblock, gamedef);
}

////////////////////////////////////////////////////////////////////////////////

void TestMapBlock::testMonoblock(IGameDef *gamedef)
{
	MapBlock block({}, gamedef);
	UASSERT(!block.m_is_mono_block);

	// make the array is expanded
	block.expandNodesIfNeeded();
	UASSERT(std::all_of(block.data, block.data + MapBlock::nodecount, [](MapNode &n) { return n == MapNode(CONTENT_IGNORE); }));

	// covert to monoblock
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);
	UASSERT(block.data[0].param0 == CONTENT_IGNORE);

	block.data[0] = MapNode(CONTENT_AIR);
	UASSERT(block.m_is_mono_block);
	UASSERT(block.data[0].param0 == CONTENT_AIR);

	block.expandNodesIfNeeded();
	UASSERT(!block.m_is_mono_block);
	UASSERT(std::all_of(block.data, block.data + MapBlock::nodecount, [](MapNode &n) { return n == MapNode(CONTENT_AIR); }));

	// covert back to mono block
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);

	// deconvert explicitly
	block.expandNodesIfNeeded();
	UASSERT(!block.m_is_mono_block);

	// covert back to mono block
	block.tryShrinkNodes();
	UASSERT(block.m_is_mono_block);

	static_assert(CONTENT_AIR != 42);
	// set a node, should deconvert the block
	block.setNode(5,5,5, MapNode(42));
	UASSERT(!block.m_is_mono_block);

	// cannot covert to mono block
	block.tryShrinkNodes();
	UASSERT(!block.m_is_mono_block);

	// set all nodes to 42
	for (size_t i = 0; i < MapBlock::nodecount; ++i) {
		block.data[i] = MapNode(42);
	}

	// can covert to mono block
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
