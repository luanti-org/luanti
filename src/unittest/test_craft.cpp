// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 DS

#include "test.h"

#include "craftdef.h"

class TestCraft : public TestBase
{
public:
	TestCraft() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestCraft"; }

	void runTests(IGameDef *gamedef);

	static std::string getDumpedCraftResult(CraftInput input, IGameDef *gamedef);
	static void registerItemWithGroups(const std::string &itemname,
			const std::vector<std::string> &groups, IGameDef *gamedef);

	void testShapeless(IGameDef *gamedef);
};

static TestCraft g_test_instance;

void TestCraft::runTests(IGameDef *gamedef)
{
	TEST(testShapeless, gamedef);
}

std::string TestCraft::getDumpedCraftResult(CraftInput input, IGameDef *gamedef)
{
	// (input is passed by value, because getCraftResult needs a non-const ref
	// for decrementing input)

	IWritableCraftDefManager *cdef = (IWritableCraftDefManager *)gamedef->getCraftDefManager();

	CraftOutput output{};
	std::vector<ItemStack> output_replacements;

	cdef->getCraftResult(input, output, output_replacements, false, gamedef);

	return output.dump();
}

void TestCraft::registerItemWithGroups(const std::string &itemname,
		const std::vector<std::string> &groups, IGameDef *gamedef)
{
	IWritableItemDefManager *idef = (IWritableItemDefManager *)gamedef->getItemDefManager();

	if (idef->isKnown(itemname)) {
		// already registered. check that the groups match
		const ItemDefinition &itemdef = idef->get(itemname);

		SANITY_CHECK(itemdef.groups.size() == groups.size());
		for (const auto &g : groups) {
			auto it = itemdef.groups.find(g);
			SANITY_CHECK(it != itemdef.groups.end());
			SANITY_CHECK(it->second == 1);
		}

	} else {
		// register it
		ItemDefinition itemdef{};

		itemdef.type = ITEM_CRAFT;
		itemdef.name = itemname;
		itemdef.description = itemname;
		for (const auto &g : groups)
			itemdef.groups[g] = 1;
		idef->registerItem(itemdef);
	}
}

void TestCraft::testShapeless(IGameDef *gamedef)
{
	IWritableItemDefManager *idef = (IWritableItemDefManager *)gamedef->getItemDefManager();
	IWritableCraftDefManager *cdef = (IWritableCraftDefManager *)gamedef->getCraftDefManager();

	auto to_item = [&](const std::string &itemstring) -> ItemStack {
		ItemStack item;
		item.deSerialize(itemstring, idef);
		return item;
	};

	cdef->clear();

	idef->registerAlias("crafttest:a1", "crafttest:i1");
	registerItemWithGroups("crafttest:i1", {}, gamedef);
	registerItemWithGroups("crafttest:i2", {}, gamedef);
	registerItemWithGroups("crafttest:i3", {}, gamedef);
	registerItemWithGroups("crafttest:i4", {}, gamedef);
	registerItemWithGroups("crafttest:g1g2", {"crafttest_g1", "crafttest_g2"}, gamedef);

	cdef->registerCraft(new CraftDefinitionShapeless(
				"crafttest:i1",
				{
					"crafttest:i1",
					"crafttest:a1",
				},
				CraftReplacements{}
			), gamedef);

	cdef->registerCraft(new CraftDefinitionShapeless(
				"crafttest:i2",
				{
					"crafttest:i2",
					"crafttest:i1",
					"crafttest:i2",
					"crafttest:i1",
					"crafttest:i2",
					"crafttest:i1",
					"crafttest:i2",
					"crafttest:i1",
					"crafttest:i2",
					"crafttest:i1",
					"crafttest:i2",
					"crafttest:i1",
				},
				CraftReplacements{}
			), gamedef);

	cdef->registerCraft(new CraftDefinitionShapeless(
				"crafttest:i3",
				{
					"crafttest:i2",
					"crafttest:i1",
					"crafttest:i2",
					"group:crafttest_g1",
				},
				CraftReplacements{}
			), gamedef);

	cdef->registerCraft(new CraftDefinitionShapeless(
				"crafttest:i4",
				{
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g2",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
					"group:crafttest_g1",
				},
				CraftReplacements{}
			), gamedef);

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 3,
			{
				to_item("crafttest:i1"),
				to_item("crafttest:i1"),
			}), gamedef),
			"(item=\"crafttest:i1\", time=0)");

	cdef->initHashes(gamedef);

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 3,
			{
				to_item("crafttest:i1"),
				to_item("crafttest:i1"),
			}), gamedef),
			"(item=\"crafttest:i1\", time=0)");

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 3,
			{
				to_item("crafttest:i1"),
				to_item(""),
				to_item("crafttest:i1"),
			}), gamedef),
			"(item=\"crafttest:i1\", time=0)");

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 4,
			{
				to_item("crafttest:i1"),
				to_item("crafttest:i1"),
			}), gamedef),
			"(item=\"crafttest:i1\", time=0)");

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 3,
			{
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
			}), gamedef),
			"(item=\"crafttest:i2\", time=0)");

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 4,
			{
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
			}), gamedef),
			"(item=\"crafttest:i2\", time=0)");

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 3,
			{
				to_item("crafttest:i2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:g1g2"),
			}), gamedef),
			"(item=\"crafttest:i3\", time=0)");

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 3,
			{
				to_item("crafttest:g1g2"),
				to_item("crafttest:i1"),
				to_item("crafttest:i2"),
				to_item("crafttest:i2"),
			}), gamedef),
			"(item=\"crafttest:i3\", time=0)");

	UASSERTEQ(std::string, getDumpedCraftResult(CraftInput(CRAFT_METHOD_NORMAL, 3,
			{
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
				to_item("crafttest:g1g2"),
			}), gamedef),
			"(item=\"crafttest:i4\", time=0)");
}
