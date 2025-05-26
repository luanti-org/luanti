// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#include "test.h"

#include "mapgen/mg_schematic.h"
#include "gamedef.h"
#include "nodedef.h"

class TestSchematic : public TestBase {
public:
	TestSchematic() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSchematic"; }

	void runTests(IGameDef *gamedef);

	void testMtsSerializeDeserialize(const NodeDefManager *ndef);
	void testLuaTableSerialize(const NodeDefManager *ndef);
	void testFileSerializeDeserialize(const NodeDefManager *ndef);

	static const content_t test_schem1_data[7 * 6 * 4];
	static const content_t test_schem2_data[3 * 3 * 3];
	static const u8 test_schem2_prob[3 * 3 * 3];
	static const char *expected_lua_output;
};

static TestSchematic g_test_instance;

void TestSchematic::runTests(IGameDef *gamedef)
{
	NodeDefManager *ndef =
		(NodeDefManager *)gamedef->getNodeDefManager();

	ndef->setNodeRegistrationStatus(true);

	TEST(testMtsSerializeDeserialize, ndef);
	TEST(testLuaTableSerialize, ndef);
	TEST(testFileSerializeDeserialize, ndef);

	ndef->resetNodeResolveState();
}

////////////////////////////////////////////////////////////////////////////////

void TestSchematic::testMtsSerializeDeserialize(const NodeDefManager *ndef)
{
	static const v3s16 size(7, 6, 4);
	static const u32 volume = size.X * size.Y * size.Z;

	std::stringstream ss(std::ios_base::binary |
		std::ios_base::in | std::ios_base::out);

	Schematic schem;
	{
		std::vector<std::string> &names = schem.m_nodenames;
		names.emplace_back("foo");
		names.emplace_back("bar");
		names.emplace_back("baz");
		names.emplace_back("qux");
	}

	schem.flags       = 0;
	schem.size        = size;
	schem.schemdata   = new MapNode[volume];
	schem.slice_probs = new u8[size.Y];
	for (size_t i = 0; i != volume; i++)
		schem.schemdata[i] = MapNode(test_schem1_data[i], MTSCHEM_PROB_ALWAYS, 0);
	for (s16 y = 0; y != size.Y; y++)
		schem.slice_probs[y] = MTSCHEM_PROB_ALWAYS;

	UASSERT(schem.serializeToMts(&ss));

	ss.seekg(0);

	Schematic schem2;
	UASSERT(schem2.deserializeFromMts(&ss));

	{
		std::vector<std::string> &names = schem2.m_nodenames;
		UASSERTEQ(size_t, names.size(), 4);
		UASSERTEQ(std::string, names[0], "foo");
		UASSERTEQ(std::string, names[1], "bar");
		UASSERTEQ(std::string, names[2], "baz");
		UASSERTEQ(std::string, names[3], "qux");
	}

	UASSERT(schem2.size == size);
	for (size_t i = 0; i != volume; i++)
		UASSERT(schem2.schemdata[i] == schem.schemdata[i]);
	for (s16 y = 0; y != size.Y; y++)
		UASSERTEQ(u8, schem2.slice_probs[y], schem.slice_probs[y]);
}


void TestSchematic::testLuaTableSerialize(const NodeDefManager *ndef)
{
	static const v3s16 size(3, 3, 3);
	static const u32 volume = size.X * size.Y * size.Z;

	Schematic schem;

	schem.flags       = 0;
	schem.size        = size;
	schem.schemdata   = new MapNode[volume];
	schem.slice_probs = new u8[size.Y];
	for (size_t i = 0; i != volume; i++)
		schem.schemdata[i] = MapNode(test_schem2_data[i], test_schem2_prob[i], 0);
	for (s16 y = 0; y != size.Y; y++)
		schem.slice_probs[y] = MTSCHEM_PROB_ALWAYS;

	std::vector<std::string> &names = schem.m_nodenames;
	names.emplace_back("air");
	names.emplace_back("default:lava_source");
	names.emplace_back("default:glass");

	std::ostringstream ss(std::ios_base::binary);

	UASSERT(schem.serializeToLua(&ss, false, 0));
	UASSERTEQ(std::string, ss.str(), expected_lua_output);
}


void TestSchematic::testFileSerializeDeserialize(const NodeDefManager *ndef)
{
	static const v3s16 size(3, 3, 3);
	static const u32 volume = size.X * size.Y * size.Z;
	static const content_t content_map[] = {
		CONTENT_AIR,
		t_CONTENT_STONE,
		t_CONTENT_LAVA,
	};
	static const content_t content_map2[] = {
		CONTENT_AIR,
		t_CONTENT_STONE,
		t_CONTENT_WATER,
	};
	StringMap replace_names;
	replace_names["default:lava"] = "default:water";

	Schematic schem1, schem2;

	//// Construct the schematic to save
	schem1.flags          = 0;
	schem1.size           = size;
	schem1.schemdata      = new MapNode[volume];
	schem1.slice_probs    = new u8[size.Y];
	schem1.slice_probs[0] = 80;
	schem1.slice_probs[1] = 160;
	schem1.slice_probs[2] = 240;
	// Node resolving happened manually.
	schem1.m_resolve_done = true;

	for (size_t i = 0; i != volume; i++) {
		content_t c = content_map[test_schem2_data[i]];
		schem1.schemdata[i] = MapNode(c, test_schem2_prob[i], 0);
	}

	std::string temp_file = getTestTempFile();
	UASSERT(schem1.saveSchematicToFile(temp_file, ndef));
	UASSERT(schem2.loadSchematicFromFile(temp_file, ndef, &replace_names));

	UASSERT(schem2.size == size);
	UASSERT(schem2.slice_probs[0] == 80);
	UASSERT(schem2.slice_probs[1] == 160);
	UASSERT(schem2.slice_probs[2] == 240);

	for (size_t i = 0; i != volume; i++) {
		content_t c = content_map2[test_schem2_data[i]];
		UASSERT(schem2.schemdata[i] == MapNode(c, test_schem2_prob[i], 0));
	}
}


// Should form a cross-shaped-thing...?
const content_t TestSchematic::test_schem1_data[7 * 6 * 4] = {
	3, 3, 1, 1, 1, 3, 3, // Y=0, Z=0
	3, 0, 1, 2, 1, 0, 3, // Y=1, Z=0
	3, 0, 1, 2, 1, 0, 3, // Y=2, Z=0
	3, 1, 1, 2, 1, 1, 3, // Y=3, Z=0
	3, 2, 2, 2, 2, 2, 3, // Y=4, Z=0
	3, 1, 1, 2, 1, 1, 3, // Y=5, Z=0

	0, 0, 1, 1, 1, 0, 0, // Y=0, Z=1
	0, 0, 1, 2, 1, 0, 0, // Y=1, Z=1
	0, 0, 1, 2, 1, 0, 0, // Y=2, Z=1
	1, 1, 1, 2, 1, 1, 1, // Y=3, Z=1
	1, 2, 2, 2, 2, 2, 1, // Y=4, Z=1
	1, 1, 1, 2, 1, 1, 1, // Y=5, Z=1

	0, 0, 1, 1, 1, 0, 0, // Y=0, Z=2
	0, 0, 1, 2, 1, 0, 0, // Y=1, Z=2
	0, 0, 1, 2, 1, 0, 0, // Y=2, Z=2
	1, 1, 1, 2, 1, 1, 1, // Y=3, Z=2
	1, 2, 2, 2, 2, 2, 1, // Y=4, Z=2
	1, 1, 1, 2, 1, 1, 1, // Y=5, Z=2

	3, 3, 1, 1, 1, 3, 3, // Y=0, Z=3
	3, 0, 1, 2, 1, 0, 3, // Y=1, Z=3
	3, 0, 1, 2, 1, 0, 3, // Y=2, Z=3
	3, 1, 1, 2, 1, 1, 3, // Y=3, Z=3
	3, 2, 2, 2, 2, 2, 3, // Y=4, Z=3
	3, 1, 1, 2, 1, 1, 3, // Y=5, Z=3
};

const content_t TestSchematic::test_schem2_data[3 * 3 * 3] = {
	0, 0, 0,
	0, 2, 0,
	0, 0, 0,

	0, 2, 0,
	2, 1, 2,
	0, 2, 0,

	0, 0, 0,
	0, 2, 0,
	0, 0, 0,
};

const u8 TestSchematic::test_schem2_prob[3 * 3 * 3] = {
	0x00, 0x00, 0x00,
	0x00, 0xFF, 0x00,
	0x00, 0x00, 0x00,

	0x00, 0xFF, 0x00,
	0xFF, 0xFF, 0xFF,
	0x00, 0xFF, 0x00,

	0x00, 0x00, 0x00,
	0x00, 0xFF, 0x00,
	0x00, 0x00, 0x00,
};

const char *TestSchematic::expected_lua_output =
	"schematic = {\n"
	"\tsize = {x=3, y=3, z=3},\n"
	"\tyslice_prob = {\n"
	"\t\t{ypos=0, prob=254},\n"
	"\t\t{ypos=1, prob=254},\n"
	"\t\t{ypos=2, prob=254},\n"
	"\t},\n"
	"\tdata = {\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"default:glass\", prob=254, param2=0, force_place=true},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"default:glass\", prob=254, param2=0, force_place=true},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"default:glass\", prob=254, param2=0, force_place=true},\n"
	"\t\t{name=\"default:lava_source\", prob=254, param2=0, force_place=true},\n"
	"\t\t{name=\"default:glass\", prob=254, param2=0, force_place=true},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"default:glass\", prob=254, param2=0, force_place=true},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"default:glass\", prob=254, param2=0, force_place=true},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t\t{name=\"air\", prob=0, param2=0},\n"
	"\t},\n"
	"}\n";
