// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "catch.h"

#include "test.h"

#include "nodedef.h"
#include "itemdef.h"
#include "dummygamedef.h"
#include "log_internal.h"
#include "modchannels.h"
#include "util/numeric.h"
#include "porting.h"
#include "debug.h"

#include <iostream>

#include "catch.h"

content_t t_CONTENT_STONE;
content_t t_CONTENT_GRASS;
content_t t_CONTENT_TORCH;
content_t t_CONTENT_WATER;
content_t t_CONTENT_LAVA;
content_t t_CONTENT_BRICK;

////////////////////////////////////////////////////////////////////////////////

////
//// TestGameDef
////

class TestGameDef : public DummyGameDef {
public:
	TestGameDef();
	~TestGameDef() = default;

	void defineSomeNodes();

	bool joinModChannel(const std::string &channel);
	bool leaveModChannel(const std::string &channel);
	bool sendModChannelMessage(const std::string &channel, const std::string &message);
	ModChannel *getModChannel(const std::string &channel)
	{
		return m_modchannel_mgr->getModChannel(channel);
	}

private:
	std::unique_ptr<ModChannelMgr> m_modchannel_mgr;
};


TestGameDef::TestGameDef() :
	DummyGameDef(),
	m_modchannel_mgr(new ModChannelMgr())
{
	defineSomeNodes();
}


void TestGameDef::defineSomeNodes()
{
	IWritableItemDefManager *idef = (IWritableItemDefManager *)m_itemdef;
	NodeDefManager *ndef = (NodeDefManager *)m_nodedef;

	ItemDefinition itemdef;
	ContentFeatures f;

	//// Stone
	itemdef = ItemDefinition();
	itemdef.type = ITEM_NODE;
	itemdef.name = "default:stone";
	itemdef.description = "Stone";
	itemdef.groups["cracky"] = 3;
	itemdef.inventory_image = "[inventorycube"
		"{default_stone.png"
		"{default_stone.png"
		"{default_stone.png";
	f = ContentFeatures();
	f.name = itemdef.name;
	for (TileDef &tiledef : f.tiledef)
		tiledef.name = "default_stone.png";
	f.is_ground_content = true;
	idef->registerItem(itemdef);
	t_CONTENT_STONE = ndef->set(f.name, f);

	//// Grass
	itemdef = ItemDefinition();
	itemdef.type = ITEM_NODE;
	itemdef.name = "default:dirt_with_grass";
	itemdef.description = "Dirt with grass";
	itemdef.groups["crumbly"] = 3;
	itemdef.inventory_image = "[inventorycube"
		"{default_grass.png"
		"{default_dirt.png&default_grass_side.png"
		"{default_dirt.png&default_grass_side.png";
	f = ContentFeatures();
	f.name = itemdef.name;
	f.tiledef[0].name = "default_grass.png";
	f.tiledef[1].name = "default_dirt.png";
	for(int i = 2; i < 6; i++)
		f.tiledef[i].name = "default_dirt.png^default_grass_side.png";
	f.is_ground_content = true;
	idef->registerItem(itemdef);
	t_CONTENT_GRASS = ndef->set(f.name, f);

	//// Torch (minimal definition for lighting tests)
	itemdef = ItemDefinition();
	itemdef.type = ITEM_NODE;
	itemdef.name = "default:torch";
	f = ContentFeatures();
	f.name = itemdef.name;
	f.param_type = CPT_LIGHT;
	f.light_propagates = true;
	f.sunlight_propagates = true;
	f.light_source = LIGHT_MAX-1;
	idef->registerItem(itemdef);
	t_CONTENT_TORCH = ndef->set(f.name, f);

	//// Water
	itemdef = ItemDefinition();
	itemdef.type = ITEM_NODE;
	itemdef.name = "default:water";
	itemdef.description = "Water";
	itemdef.inventory_image = "[inventorycube"
		"{default_water.png"
		"{default_water.png"
		"{default_water.png";
	f = ContentFeatures();
	f.name = itemdef.name;
	f.alpha = ALPHAMODE_BLEND;
	f.light_propagates = true;
	f.param_type = CPT_LIGHT;
	f.liquid_type = LIQUID_SOURCE;
	f.liquid_viscosity = 4;
	f.is_ground_content = true;
	f.groups["liquids"] = 3;
	for (TileDef &tiledef : f.tiledef)
		tiledef.name = "default_water.png";
	idef->registerItem(itemdef);
	t_CONTENT_WATER = ndef->set(f.name, f);

	//// Lava
	itemdef = ItemDefinition();
	itemdef.type = ITEM_NODE;
	itemdef.name = "default:lava";
	itemdef.description = "Lava";
	itemdef.inventory_image = "[inventorycube"
		"{default_lava.png"
		"{default_lava.png"
		"{default_lava.png";
	f = ContentFeatures();
	f.name = itemdef.name;
	f.alpha = ALPHAMODE_OPAQUE;
	f.liquid_type = LIQUID_SOURCE;
	f.liquid_viscosity = 7;
	f.light_source = LIGHT_MAX-1;
	f.is_ground_content = true;
	f.groups["liquids"] = 3;
	for (TileDef &tiledef : f.tiledef)
		tiledef.name = "default_lava.png";
	idef->registerItem(itemdef);
	t_CONTENT_LAVA = ndef->set(f.name, f);


	//// Brick
	itemdef = ItemDefinition();
	itemdef.type = ITEM_NODE;
	itemdef.name = "default:brick";
	itemdef.description = "Brick";
	itemdef.groups["cracky"] = 3;
	itemdef.inventory_image = "[inventorycube"
		"{default_brick.png"
		"{default_brick.png"
		"{default_brick.png";
	f = ContentFeatures();
	f.name = itemdef.name;
	for (TileDef &tiledef : f.tiledef)
		tiledef.name = "default_brick.png";
	f.is_ground_content = true;
	idef->registerItem(itemdef);
	t_CONTENT_BRICK = ndef->set(f.name, f);
}

bool TestGameDef::joinModChannel(const std::string &channel)
{
	return m_modchannel_mgr->joinChannel(channel, PEER_ID_SERVER);
}

bool TestGameDef::leaveModChannel(const std::string &channel)
{
	return m_modchannel_mgr->leaveChannel(channel, PEER_ID_SERVER);
}

bool TestGameDef::sendModChannelMessage(const std::string &channel,
	const std::string &message)
{
	if (!m_modchannel_mgr->channelRegistered(channel))
		return false;

	return true;
}

////
//// run_tests
////

bool run_tests()
{
	u64 t1 = porting::getTimeMs();
	TestGameDef gamedef;

	u32 num_modules_failed     = 0;
	u32 num_total_tests_failed = 0;
	u32 num_total_tests_run    = 0;
	std::vector<TestBase *> &testmods = TestManager::getTestModules();
	for (auto *testmod: testmods) {
		if (!testmod->testModule(&gamedef))
			num_modules_failed++;

		num_total_tests_failed += testmod->num_tests_failed;
		num_total_tests_run += testmod->num_tests_run;
	}

	rawstream << "Catch test results: " << std::endl;
	Catch::Session session{};
	auto config = session.configData();
	config.skipBenchmarks = true;
	config.allowZeroTests = true;
	session.useConfigData(config);
	auto exit_code = session.run();
	// We count all the Catch tests as one test for Minetest's own logging
	// because we don't have a way to tell how many individual tests Catch ran.
	++num_total_tests_run;
	if (exit_code != 0) {
		++num_modules_failed;
		++num_total_tests_failed;
	}

	u64 tdiff = porting::getTimeMs() - t1;

	const char *overall_status = (num_modules_failed == 0) ? "PASSED" : "FAILED";

	rawstream << "\n"
		<< "++++++++++++++++++++++++++++++++++++++++"
		<< "++++++++++++++++++++++++++++++++++++++++" << std::endl
		<< "Unit Test Results: " << overall_status << std::endl
		<< "    " << num_modules_failed << " / " << testmods.size()
		<< " failed modules (" << num_total_tests_failed << " / "
		<< num_total_tests_run << " failed individual tests)." << std::endl
		<< "    Testing took " << tdiff << "ms total." << std::endl
		<< "++++++++++++++++++++++++++++++++++++++++"
		<< "++++++++++++++++++++++++++++++++++++++++" << std::endl;

	return num_modules_failed == 0;
}

static TestBase *findTestModule(const std::string &module_name) {
	std::vector<TestBase *> &testmods = TestManager::getTestModules();
	for (auto *testmod: testmods) {
		if (module_name == testmod->getName())
			return testmod;
	}
	return nullptr;
}

bool run_tests(const std::string &module_name)
{
	TestGameDef gamedef;

	auto testmod = findTestModule(module_name);
	if (!testmod) {
		rawstream << "Did not find module, searching Catch tests: " << module_name << std::endl;
		Catch::Session session;
		session.configData().testsOrTags.push_back(module_name);
		auto catch_test_failures = session.run();
		return catch_test_failures == 0;
	}

	u64 t1 = porting::getTimeMs();

	bool ok = testmod->testModule(&gamedef);

	u64 tdiff = porting::getTimeMs() - t1;

	const char *overall_status = ok ? "PASSED" : "FAILED";

	rawstream << "\n"
		<< "++++++++++++++++++++++++++++++++++++++++"
		<< "++++++++++++++++++++++++++++++++++++++++" << std::endl
		<< "Unit Test Results: " << overall_status << std::endl
		<< "    " << testmod->num_tests_failed << " / "
		<< testmod->num_tests_run << " failed tests." << std::endl
		<< "    Testing took " << tdiff << "ms." << std::endl
		<< "++++++++++++++++++++++++++++++++++++++++"
		<< "++++++++++++++++++++++++++++++++++++++++" << std::endl;

	return ok;
}

////
//// TestBase
////

bool TestBase::testModule(IGameDef *gamedef)
{
	rawstream << "======== Testing module " << getName() << std::endl;
	u64 t1 = porting::getTimeMs();


	runTests(gamedef);

	u64 tdiff = porting::getTimeMs() - t1;
	rawstream << "======== Module " << getName() << " "
		<< (num_tests_failed ? "failed" : "passed") << " (" << num_tests_failed
		<< " failures / " << num_tests_run << " tests) - " << tdiff
		<< "ms" << std::endl;

	if (!m_test_dir.empty())
		fs::RecursiveDelete(m_test_dir);

	return num_tests_failed == 0;
}

std::string TestBase::getTestTempDirectory()
{
	if (!m_test_dir.empty())
		return m_test_dir;

	m_test_dir = fs::CreateTempDir();
	UASSERT(!m_test_dir.empty());
	return m_test_dir;
}

std::string TestBase::getTestTempFile()
{
	char buf[32];
	porting::mt_snprintf(buf, sizeof(buf), "%08X", myrand());

	return getTestTempDirectory() + DIR_DELIM + buf + ".tmp";
}

void TestBase::runTest(const char *name, std::function<void()> &&test)
{
	u64 t1 = porting::getTimeMs();
	try {
		test();
		rawstream << "[PASS] ";
	} catch (TestFailedException &e) {
		rawstream << "Test assertion failed: " << e.message << std::endl;
		rawstream << "    at " << e.file << ":" << e.line << std::endl;
		rawstream << "[FAIL] ";
		num_tests_failed++;
	}
#if CATCH_UNHANDLED_EXCEPTIONS == 1
	catch (std::exception &e) {
		rawstream << "Caught unhandled exception: " << e.what() << std::endl;
		rawstream << "[FAIL] ";
		num_tests_failed++;
	}
#endif
	num_tests_run++;
	u64 tdiff = porting::getTimeMs() - t1;
	rawstream << name << " - " << tdiff << "ms" << std::endl;
}

/*
	NOTE: These tests became non-working then NodeContainer was removed.
	      These should be redone, utilizing some kind of a virtual
		  interface for Map (IMap would be fine).
*/
#if 0
struct TestMapBlock: public TestBase
{
	class TC : public NodeContainer
	{
	public:

		MapNode node;
		bool position_valid;
		std::list<v3s16> validity_exceptions;

		TC()
		{
			position_valid = true;
		}

		virtual bool isValidPosition(v3s16 p)
		{
			//return position_valid ^ (p==position_valid_exception);
			bool exception = false;
			for(std::list<v3s16>::iterator i=validity_exceptions.begin();
					i != validity_exceptions.end(); i++)
			{
				if(p == *i)
				{
					exception = true;
					break;
				}
			}
			return exception ? !position_valid : position_valid;
		}

		virtual MapNode getNode(v3s16 p)
		{
			if(isValidPosition(p) == false)
				throw InvalidPositionException();
			return node;
		}

		virtual void setNode(v3s16 p, MapNode & n)
		{
			if(isValidPosition(p) == false)
				throw InvalidPositionException();
		};

		virtual u16 nodeContainerId() const
		{
			return 666;
		}
	};

	void Run()
	{
		TC parent;

		MapBlock b(&parent, v3s16(1,1,1));
		v3s16 relpos(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE);

		UASSERT(b.getPosRelative() == relpos);

		UASSERT(b.getBox().MinEdge.X == MAP_BLOCKSIZE);
		UASSERT(b.getBox().MaxEdge.X == MAP_BLOCKSIZE*2-1);
		UASSERT(b.getBox().MinEdge.Y == MAP_BLOCKSIZE);
		UASSERT(b.getBox().MaxEdge.Y == MAP_BLOCKSIZE*2-1);
		UASSERT(b.getBox().MinEdge.Z == MAP_BLOCKSIZE);
		UASSERT(b.getBox().MaxEdge.Z == MAP_BLOCKSIZE*2-1);

		UASSERT(b.isValidPosition(v3s16(0,0,0)) == true);
		UASSERT(b.isValidPosition(v3s16(-1,0,0)) == false);
		UASSERT(b.isValidPosition(v3s16(-1,-142,-2341)) == false);
		UASSERT(b.isValidPosition(v3s16(-124,142,2341)) == false);
		UASSERT(b.isValidPosition(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1)) == true);
		UASSERT(b.isValidPosition(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE,MAP_BLOCKSIZE-1)) == false);

		/*
			TODO: this method should probably be removed
			if the block size isn't going to be set variable
		*/
		/*UASSERT(b.getSizeNodes() == v3s16(MAP_BLOCKSIZE,
				MAP_BLOCKSIZE, MAP_BLOCKSIZE));*/

		// Changed flag should be initially set
		UASSERT(b.getModified() == MOD_STATE_WRITE_NEEDED);
		b.resetModified();
		UASSERT(b.getModified() == MOD_STATE_CLEAN);

		// All nodes should have been set to
		// .d=CONTENT_IGNORE and .getLight() = 0
		for(u16 z=0; z<MAP_BLOCKSIZE; z++)
		for(u16 y=0; y<MAP_BLOCKSIZE; y++)
		for(u16 x=0; x<MAP_BLOCKSIZE; x++)
		{
			//UASSERT(b.getNode(v3s16(x,y,z)).getContent() == CONTENT_AIR);
			UASSERT(b.getNode(v3s16(x,y,z)).getContent() == CONTENT_IGNORE);
			UASSERT(b.getNode(v3s16(x,y,z)).getLight(LIGHTBANK_DAY) == 0);
			UASSERT(b.getNode(v3s16(x,y,z)).getLight(LIGHTBANK_NIGHT) == 0);
		}

		{
			MapNode n(CONTENT_AIR);
			for(u16 z=0; z<MAP_BLOCKSIZE; z++)
			for(u16 y=0; y<MAP_BLOCKSIZE; y++)
			for(u16 x=0; x<MAP_BLOCKSIZE; x++)
			{
				b.setNode(v3s16(x,y,z), n);
			}
		}

		/*
			Parent fetch functions
		*/
		parent.position_valid = false;
		parent.node.setContent(5);

		MapNode n;

		// Positions in the block should still be valid
		UASSERT(b.isValidPositionParent(v3s16(0,0,0)) == true);
		UASSERT(b.isValidPositionParent(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1)) == true);
		n = b.getNodeParent(v3s16(0,MAP_BLOCKSIZE-1,0));
		UASSERT(n.getContent() == CONTENT_AIR);

		// ...but outside the block they should be invalid
		UASSERT(b.isValidPositionParent(v3s16(-121,2341,0)) == false);
		UASSERT(b.isValidPositionParent(v3s16(-1,0,0)) == false);
		UASSERT(b.isValidPositionParent(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE)) == false);

		{
			bool exception_thrown = false;
			try{
				// This should throw an exception
				MapNode n = b.getNodeParent(v3s16(0,0,-1));
			}
			catch(InvalidPositionException &e)
			{
				exception_thrown = true;
			}
			UASSERT(exception_thrown);
		}

		parent.position_valid = true;
		// Now the positions outside should be valid
		UASSERT(b.isValidPositionParent(v3s16(-121,2341,0)) == true);
		UASSERT(b.isValidPositionParent(v3s16(-1,0,0)) == true);
		UASSERT(b.isValidPositionParent(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE)) == true);
		n = b.getNodeParent(v3s16(0,0,MAP_BLOCKSIZE));
		UASSERT(n.getContent() == 5);

		/*
			Set a node
		*/
		v3s16 p(1,2,0);
		n.setContent(4);
		b.setNode(p, n);
		UASSERT(b.getNode(p).getContent() == 4);
		//TODO: Update to new system
		/*UASSERT(b.getNodeTile(p) == 4);
		UASSERT(b.getNodeTile(v3s16(-1,-1,0)) == 5);*/

		/*
			propagateSunlight()
		*/
		// Set lighting of all nodes to 0
		for(u16 z=0; z<MAP_BLOCKSIZE; z++){
			for(u16 y=0; y<MAP_BLOCKSIZE; y++){
				for(u16 x=0; x<MAP_BLOCKSIZE; x++){
					MapNode n = b.getNode(v3s16(x,y,z));
					n.setLight(LIGHTBANK_DAY, 0);
					n.setLight(LIGHTBANK_NIGHT, 0);
					b.setNode(v3s16(x,y,z), n);
				}
			}
		}
		{
			/*
				Check how the block handles being a lonely sky block
			*/
			parent.position_valid = true;
			b.setIsUnderground(false);
			parent.node.setContent(CONTENT_AIR);
			parent.node.setLight(LIGHTBANK_DAY, LIGHT_SUN);
			parent.node.setLight(LIGHTBANK_NIGHT, 0);
			std::map<v3s16, bool> light_sources;
			// The bottom block is invalid, because we have a shadowing node
			UASSERT(b.propagateSunlight(light_sources) == false);
			UASSERT(b.getNode(v3s16(1,4,0)).getLight(LIGHTBANK_DAY) == LIGHT_SUN);
			UASSERT(b.getNode(v3s16(1,3,0)).getLight(LIGHTBANK_DAY) == LIGHT_SUN);
			UASSERT(b.getNode(v3s16(1,2,0)).getLight(LIGHTBANK_DAY) == 0);
			UASSERT(b.getNode(v3s16(1,1,0)).getLight(LIGHTBANK_DAY) == 0);
			UASSERT(b.getNode(v3s16(1,0,0)).getLight(LIGHTBANK_DAY) == 0);
			UASSERT(b.getNode(v3s16(1,2,3)).getLight(LIGHTBANK_DAY) == LIGHT_SUN);
			UASSERT(b.getFaceLight2(1000, p, v3s16(0,1,0)) == LIGHT_SUN);
			UASSERT(b.getFaceLight2(1000, p, v3s16(0,-1,0)) == 0);
			UASSERT(b.getFaceLight2(0, p, v3s16(0,-1,0)) == 0);
			// According to MapBlock::getFaceLight,
			// The face on the z+ side should have double-diminished light
			//UASSERT(b.getFaceLight(p, v3s16(0,0,1)) == diminish_light(diminish_light(LIGHT_MAX)));
			// The face on the z+ side should have diminished light
			UASSERT(b.getFaceLight2(1000, p, v3s16(0,0,1)) == diminish_light(LIGHT_MAX));
		}
		/*
			Check how the block handles being in between blocks with some non-sunlight
			while being underground
		*/
		{
			// Make neighbors to exist and set some non-sunlight to them
			parent.position_valid = true;
			b.setIsUnderground(true);
			parent.node.setLight(LIGHTBANK_DAY, LIGHT_MAX/2);
			std::map<v3s16, bool> light_sources;
			// The block below should be valid because there shouldn't be
			// sunlight in there either
			UASSERT(b.propagateSunlight(light_sources, true) == true);
			// Should not touch nodes that are not affected (that is, all of them)
			//UASSERT(b.getNode(v3s16(1,2,3)).getLight() == LIGHT_SUN);
			// Should set light of non-sunlighted blocks to 0.
			UASSERT(b.getNode(v3s16(1,2,3)).getLight(LIGHTBANK_DAY) == 0);
		}
		/*
			Set up a situation where:
			- There is only air in this block
			- There is a valid non-sunlighted block at the bottom, and
			- Invalid blocks elsewhere.
			- the block is not underground.

			This should result in bottom block invalidity
		*/
		{
			b.setIsUnderground(false);
			// Clear block
			for(u16 z=0; z<MAP_BLOCKSIZE; z++){
				for(u16 y=0; y<MAP_BLOCKSIZE; y++){
					for(u16 x=0; x<MAP_BLOCKSIZE; x++){
						MapNode n;
						n.setContent(CONTENT_AIR);
						n.setLight(LIGHTBANK_DAY, 0);
						b.setNode(v3s16(x,y,z), n);
					}
				}
			}
			// Make neighbors invalid
			parent.position_valid = false;
			// Add exceptions to the top of the bottom block
			for(u16 x=0; x<MAP_BLOCKSIZE; x++)
			for(u16 z=0; z<MAP_BLOCKSIZE; z++)
			{
				parent.validity_exceptions.push_back(v3s16(MAP_BLOCKSIZE+x, MAP_BLOCKSIZE-1, MAP_BLOCKSIZE+z));
			}
			// Lighting value for the valid nodes
			parent.node.setLight(LIGHTBANK_DAY, LIGHT_MAX/2);
			std::map<v3s16, bool> light_sources;
			// Bottom block is not valid
			UASSERT(b.propagateSunlight(light_sources) == false);
		}
	}
};

struct TestMapSector: public TestBase
{
	class TC : public NodeContainer
	{
	public:

		MapNode node;
		bool position_valid;

		TC()
		{
			position_valid = true;
		}

		virtual bool isValidPosition(v3s16 p)
		{
			return position_valid;
		}

		virtual MapNode getNode(v3s16 p)
		{
			if(position_valid == false)
				throw InvalidPositionException();
			return node;
		}

		virtual void setNode(v3s16 p, MapNode & n)
		{
			if(position_valid == false)
				throw InvalidPositionException();
		};

		virtual u16 nodeContainerId() const
		{
			return 666;
		}
	};

	void Run()
	{
		TC parent;
		parent.position_valid = false;

		// Create one with no heightmaps
		ServerMapSector sector(&parent, v2s16(1,1));

		UASSERT(sector.getBlockNoCreateNoEx(0) == nullptr);
		UASSERT(sector.getBlockNoCreateNoEx(1) == nullptr);

		MapBlock * bref = sector.createBlankBlock(-2);

		UASSERT(sector.getBlockNoCreateNoEx(0) == nullptr);
		UASSERT(sector.getBlockNoCreateNoEx(-2) == bref);

		//TODO: Check for AlreadyExistsException

		/*bool exception_thrown = false;
		try{
			sector.getBlock(0);
		}
		catch(InvalidPositionException &e){
			exception_thrown = true;
		}
		UASSERT(exception_thrown);*/

	}
};
#endif
