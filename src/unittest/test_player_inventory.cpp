// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "test.h"
#include "player.h"
#include "itemdef.h"
#include "inventory.h"
#include "gamedef.h"
#include <map>

class TestItemDefManager : public IItemDefManager
{
public:
	const ItemDefinition &get(const std::string &name) const override {
		if (m_item_defs.find(name) == m_item_defs.end()) {
			ItemDefinition def;
			def.name = name;
			m_item_defs[name] = def;
		}
		return m_item_defs.at(name);
	}
	const std::string &getAlias(const std::string &alias) const override {
		return alias;
	}
	void getAll(std::set<std::string> &result) const override {}
	bool isKnown(const std::string &name) const override { return true; }
	void serialize(std::ostream &os, u16 protocol_version) override {}
private:
	mutable std::map<std::string, ItemDefinition> m_item_defs;
};

class TestPlayer : public Player
{
public:
	TestPlayer(const std::string &name, IItemDefManager *idef) : Player(name, idef) {}
	~TestPlayer() override {}
};

class TestPlayerInventory : public TestBase
{
public:
	TestPlayerInventory() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestPlayerInventory"; }

	void runTests(IGameDef *gamedef)
	{
		TEST(testWieldFromCustomList, gamedef->getItemDefManager());
	}

	void testWieldFromCustomList(IItemDefManager *itemdef)
	{
		// Create a player
		TestPlayer player("testplayer", itemdef);

		// Create a custom inventory list
		const std::string custom_list_name = "custom_wield_list";
		player.inventory.addList(custom_list_name, 10);

		// Create an item and add it to the custom list
		ItemStack test_item("test_item", 1, 0, itemdef);
		player.inventory.getList(custom_list_name)->changeItem(0, test_item);

		// Set the player's wield list to the custom list
		player.setWieldListName(custom_list_name);
		player.setWieldIndex(0);

		// Check if the wielded item is the one from the custom list
		ItemStack wielded_item;
		ItemStack hand_item;
		player.getWieldedItem(&wielded_item, &hand_item);

		UASSERTEQ(std::string, wielded_item.name, "test_item");
	}
};

static TestPlayerInventory g_test_instance;
