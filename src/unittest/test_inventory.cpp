// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <sstream>

#include "gamedef.h"
#include "inventory.h"

class TestInventory : public TestBase {
public:
	TestInventory() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestInventory"; }

	void runTests(IGameDef *gamedef);

	void testSerializeDeserialize(IItemDefManager *idef);

	static const char *serialized_inventory_in;
	static const char *serialized_inventory_out;
	static const char *serialized_inventory_inc;
};

static TestInventory g_test_instance;

void TestInventory::runTests(IGameDef *gamedef)
{
	TEST(testSerializeDeserialize, gamedef->getItemDefManager());
}

////////////////////////////////////////////////////////////////////////////////

void TestInventory::testSerializeDeserialize(IItemDefManager *idef)
{
	Inventory inv(idef);
	std::istringstream is(serialized_inventory_in, std::ios::binary);

	inv.deSerialize(is);
	UASSERT(inv.getList("0"));
	UASSERT(!inv.getList("main"));

	inv.getList("0")->setName("main");
	UASSERT(!inv.getList("0"));
	UASSERT(inv.getList("main"));
	UASSERTEQ(u32, inv.getList("main")->getWidth(), 3);

	inv.getList("main")->setWidth(5);
	std::ostringstream inv_os(std::ios::binary);
	inv.serialize(inv_os, false);
	UASSERTEQ(std::string, inv_os.str(), serialized_inventory_out);

	inv.setModified(false);
	inv_os.str("");
	inv_os.clear();
	inv.serialize(inv_os, true);
	UASSERTEQ(std::string, inv_os.str(), serialized_inventory_inc);

	ItemStack leftover = inv.getList("main")->takeItem(7, 99 - 12);
	ItemStack wanted = ItemStack("default:dirt", 99 - 12, 0, idef);
	UASSERT(leftover == wanted);
	leftover = inv.getList("main")->getItem(7);
	wanted.count = 12;
	UASSERT(leftover == wanted);
}

const char *TestInventory::serialized_inventory_in =
	"List 0 10\n"
	"Width 3\n"
	"Empty\n"
	"Empty\n"
	"Item default:cobble 61\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 71\n"
	"Empty\n"
	"Item default:dirt 99\n"
	"Item default:cobble 38\n"
	"Empty\n"
	"EndInventoryList\n"
	"List abc 1\n"
	"Item default:stick 3\n"
	"Width 0\n"
	"EndInventoryList\n"
	"EndInventory\n";

const char *TestInventory::serialized_inventory_out =
	"List main 10\n"
	"Width 5\n"
	"Empty\n"
	"Empty\n"
	"Item default:cobble 61\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 71\n"
	"Empty\n"
	"Item default:dirt 99\n"
	"Item default:cobble 38\n"
	"Empty\n"
	"EndInventoryList\n"
	"List abc 1\n"
	"Width 0\n"
	"Item default:stick 3\n"
	"EndInventoryList\n"
	"EndInventory\n";

const char *TestInventory::serialized_inventory_inc =
	"KeepList main\n"
	"KeepList abc\n"
	"EndInventory\n";
