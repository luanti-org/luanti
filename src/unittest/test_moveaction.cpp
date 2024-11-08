// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#include "test.h"

#include "mock_inventorymanager.h"
#include "mock_server.h"
#include "mock_serveractiveobject.h"

class TestMoveAction : public TestBase
{
public:
	TestMoveAction() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestMoveAction"; }

	void runTests(IGameDef *gamedef);

	void testMove(ServerActiveObject *obj, IGameDef *gamedef);
	void testMoveFillStack(ServerActiveObject *obj, IGameDef *gamedef);
	void testMoveSomewhere(ServerActiveObject *obj, IGameDef *gamedef);
	void testMoveSomewherePartial(ServerActiveObject *obj, IGameDef *gamedef);
	void testMoveUnallowed(ServerActiveObject *obj, IGameDef *gamedef);
	void testMovePartial(ServerActiveObject *obj, IGameDef *gamedef);

	void testSwap(ServerActiveObject *obj, IGameDef *gamedef);
	void testSwapFromUnallowed(ServerActiveObject *obj, IGameDef *gamedef);
	void testSwapToUnallowed(ServerActiveObject *obj, IGameDef *gamedef);

	void testCallbacks(ServerActiveObject *obj, Server *server);
	void testCallbacksSwap(ServerActiveObject *obj, Server *server);
};

static TestMoveAction g_test_instance;

void TestMoveAction::runTests(IGameDef *gamedef)
{
	MockServer server(getTestTempDirectory());

	server.createScripting();
	try {
		std::string builtin = Server::getBuiltinLuaPath() + DIR_DELIM;
		auto script = server.getScriptIface();
		script->loadBuiltin();
		script->loadMod(builtin + "game" DIR_DELIM "tests" DIR_DELIM "test_moveaction.lua", BUILTIN_MOD_NAME);
	} catch (ModError &e) {
		rawstream << e.what() << std::endl;
		num_tests_failed = 1;
		return;
	}

	MetricsBackend mb;
	auto null_map = std::unique_ptr<ServerMap>();
	ServerEnvironment server_env(std::move(null_map), &server, &mb);
	MockServerActiveObject obj(&server_env);
	obj.setId(1);
	server.getScriptIface()->addObjectReference(&obj);

	TEST(testMove, &obj, gamedef);
	TEST(testMoveFillStack, &obj, gamedef);
	TEST(testMoveSomewhere, &obj, gamedef);
	TEST(testMoveSomewherePartial, &obj, gamedef);
	TEST(testMoveUnallowed, &obj, gamedef);
	TEST(testMovePartial, &obj, gamedef);

	TEST(testSwap, &obj, gamedef);
	TEST(testSwapFromUnallowed, &obj, gamedef);
	TEST(testSwapToUnallowed, &obj, gamedef);

	TEST(testCallbacks, &obj, &server);
	TEST(testCallbacksSwap, &obj, &server);

	server.getScriptIface()->removeObjectReference(&obj);
}

static ItemStack parse_itemstack(const char *s)
{
	ItemStack item;
	item.deSerialize(s, nullptr);
	return item;
}

static void apply_action(const char *s, InventoryManager *inv, ServerActiveObject *obj, IGameDef *gamedef)
{
	std::istringstream str(s);
	InventoryAction *action = InventoryAction::deSerialize(str);
	action->apply(inv, obj, gamedef);
	delete action;
}

void TestMoveAction::testMove(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:stone 50"));
	inv.p2.addList("main", 10);

	apply_action("Move 20 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:stone 30");
	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:stone 20");
}

void TestMoveAction::testMoveFillStack(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	auto list = inv.p1.addList("main", 10);
	list->changeItem(0, parse_itemstack("default:stone 209"));
	list->changeItem(1, parse_itemstack("default:stone 90")); // 9 free slots

	apply_action("Move 209 player:p1 main 0 player:p1 main 1", &inv, obj, gamedef);

	UASSERT(list->getItem(0).getItemString() == "default:stone 200");
	UASSERT(list->getItem(1).getItemString() == "default:stone 99");

	// Trigger stack swap
	apply_action("Move 200 player:p1 main 0 player:p1 main 1", &inv, obj, gamedef);

	UASSERT(list->getItem(0).getItemString() == "default:stone 99");
	UASSERT(list->getItem(1).getItemString() == "default:stone 200");
}

void TestMoveAction::testMoveSomewhere(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:stone 50"));
	InventoryList *list = inv.p2.addList("main", 10);
	list->addItem(0, parse_itemstack("default:brick 10"));
	list->addItem(2, parse_itemstack("default:stone 85"));

	apply_action("MoveSomewhere 50 player:p1 main 0 player:p2 main", &inv, obj, gamedef);

	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:brick 10");
	UASSERT(inv.p2.getList("main")->getItem(1).getItemString() == "default:stone 36");
	// Partially moved
	UASSERT(inv.p2.getList("main")->getItem(2).getItemString() == "default:stone 99");
}

void TestMoveAction::testMoveSomewherePartial(ServerActiveObject *obj, IGameDef *gamedef)
{
	// "Fail" because the destination list is full.
	MockInventoryManager inv(gamedef);

	InventoryList *src = inv.p1.addList("main", 3);
	src->addItem(0, parse_itemstack("default:brick 10"));
	src->changeItem(1, parse_itemstack("default:stone 111")); // oversized

	InventoryList *dst = inv.p2.addList("main", 1);
	dst->addItem(0, parse_itemstack("default:stone 98"));

	// No free slots to fit
	apply_action("MoveSomewhere 10 player:p1 main 0 player:p2 main", &inv, obj, gamedef);
	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:brick 10");

	// Only 1 item fits
	apply_action("MoveSomewhere 111 player:p1 main 1 player:p2 main", &inv, obj, gamedef);
	UASSERT(inv.p1.getList("main")->getItem(1).getItemString() == "default:stone 110");
}

void TestMoveAction::testMoveUnallowed(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:takeput_deny 50"));
	inv.p2.addList("main", 10);

	apply_action("Move 20 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:takeput_deny 50");
	UASSERT(inv.p2.getList("main")->getItem(0).empty())
}

void TestMoveAction::testMovePartial(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:takeput_max_5 50"));
	inv.p2.addList("main", 10);

	// Lua: limited to 5 per transaction
	apply_action("Move 20 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:takeput_max_5 45");
	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:takeput_max_5 5");
}

void TestMoveAction::testSwap(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:stone 50"));
	inv.p2.addList("main", 10)->addItem(0, parse_itemstack("default:brick 60"));

	apply_action("Move 50 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:brick 60");
	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:stone 50");
}

void TestMoveAction::testSwapFromUnallowed(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:takeput_deny 50"));
	inv.p2.addList("main", 10)->addItem(0, parse_itemstack("default:brick 60"));

	apply_action("Move 50 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:takeput_deny 50");
	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:brick 60");
}

void TestMoveAction::testSwapToUnallowed(ServerActiveObject *obj, IGameDef *gamedef)
{
	MockInventoryManager inv(gamedef);

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:stone 50"));
	inv.p2.addList("main", 10)->addItem(0, parse_itemstack("default:takeput_deny 60"));

	apply_action("Move 50 player:p1 main 0 player:p2 main 0", &inv, obj, gamedef);

	UASSERT(inv.p1.getList("main")->getItem(0).getItemString() == "default:stone 50");
	UASSERT(inv.p2.getList("main")->getItem(0).getItemString() == "default:takeput_deny 60");
}

static bool check_function(lua_State *L, bool expect_swap)
{
	bool ok = false;
	int error_handler = PUSH_ERROR_HANDLER(L);

	lua_getglobal(L, "core");
	lua_getfield(L, -1, "__helper_check_callbacks");
	lua_pushboolean(L, expect_swap);
	int result = lua_pcall(L, 1, 1, error_handler);
	if (result == 0)
		ok = lua_toboolean(L, -1);
	else
		errorstream << lua_tostring(L, -1) << std::endl; // Error msg

	lua_settop(L, 0);
	return ok;
}

void TestMoveAction::testCallbacks(ServerActiveObject *obj, Server *server)
{
	server->m_inventory_mgr = std::make_unique<MockInventoryManager>(server);
	MockInventoryManager &inv = *(MockInventoryManager *)server->getInventoryMgr();

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:takeput_cb_1 10"));
	inv.p2.addList("main", 10);

	apply_action("Move 10 player:p1 main 0 player:p2 main 1", &inv, obj, server);

	// Expecting no swap. 4 callback executions in total. See Lua file for details.
	UASSERT(check_function(server->getScriptIface()->getStack(), false));

	server->m_inventory_mgr.reset();
}

void TestMoveAction::testCallbacksSwap(ServerActiveObject *obj, Server *server)
{
	server->m_inventory_mgr = std::make_unique<MockInventoryManager>(server);
	MockInventoryManager &inv = *(MockInventoryManager *)server->getInventoryMgr();

	inv.p1.addList("main", 10)->addItem(0, parse_itemstack("default:takeput_cb_2 50"));
	inv.p2.addList("main", 10)->addItem(1, parse_itemstack("default:takeput_cb_1 10"));

	apply_action("Move 10 player:p1 main 0 player:p2 main 1", &inv, obj, server);

	// Expecting swap. 8 callback executions in total. See Lua file for details.
	UASSERT(check_function(server->getScriptIface()->getStack(), true));

	server->m_inventory_mgr.reset();
}
