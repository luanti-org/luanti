// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 sfan5

#include "test.h"

#include "mock_server.h"
#include "server/luaentity_sao.h"
#include "emerge.h"

/*
 * Tests how SAOs behave in the server environment.
 * See also test_serveractiveobjectmgr.cpp and test_activeobject.cpp for other tests.
 */

class TestSAO : public TestBase
{
public:
	TestSAO() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestSAO"; }

	void runTests(IGameDef *gamedef);

	void testStaticSave(ServerEnvironment *env);
	void testNotSaved(ServerEnvironment *env);
	void testActivate(ServerEnvironment *env);
	void testStaticToFalse(ServerEnvironment *env);
	void testStaticToTrue(ServerEnvironment *env);

private:
	// enough for both removeRemovedObjects and deactivateFarObjects to be called
	// use as param to ServerEnvironment::step()
	float m_step_interval;
};

static TestSAO g_test_instance;

const static char *helper_lua_src = R"(
core.register_entity(":test:static", {})
core.register_entity(":test:non_static", {
	initial_properties = {
		static_save = false,
	}
})
)";

void TestSAO::runTests(IGameDef *gamedef)
{
	MockServer server(getTestTempDirectory());

	const auto helper_lua = getTestTempFile();
	{
		std::ofstream ofs(helper_lua, std::ios::out | std::ios::binary);
		ofs << helper_lua_src;
		std::ofstream ofs2(server.getWorldPath() + DIR_DELIM "world.mt",
			std::ios::out | std::ios::binary);
		ofs2 << "backend = dummy\n";
	}

	server.createScripting();
	try {
		auto script = server.getScriptIface();
		script->loadBuiltin();
		script->loadMod(helper_lua, BUILTIN_MOD_NAME);
	} catch (ModError &e) {
		rawstream << e.what() << std::endl;
		num_tests_failed = 1;
		return;
	}

	// unfortunately we have to create a lot since a DummyMap doesn't cut it.
	// TODO: best to factor entity mgmt out of ServerEnvironment, also
	//       EmergeManager should become mockable
	MetricsBackend mb;
	EmergeManager emerge(&server, &mb);
	auto map = std::make_unique<ServerMap>(server.getWorldPath(), gamedef, &emerge, &mb);
	ServerEnvironment env(std::move(map), &server, &mb);
	env.loadMeta();

	m_step_interval = std::max(
		g_settings->getFloat("active_block_mgmt_interval"), 0.5f) + 0.1f;

	TEST(testStaticSave, &env);
	TEST(testNotSaved, &env);
	TEST(testActivate, &env);
	TEST(testStaticToFalse, &env);
	TEST(testStaticToTrue, &env);

	env.deactivateBlocksAndObjects();
}

static LuaEntitySAO *add_entity(ServerEnvironment *env, v3f pos, const char *name)
{
	auto obj_u = std::make_unique<LuaEntitySAO>(env, pos, name, "");
	auto obj = obj_u.get();
	u16 id = env->addActiveObject(std::move(obj_u));
	if (!id || obj->isGone())
		return nullptr;
	return obj;
}

static u16 assert_active_in_block(MapBlock *block, u16 obj_id = 0)
{
	const auto &so = block->m_static_objects;
	UASSERTEQ(size_t, so.getStoredSize(), 0);
	UASSERTEQ(size_t, so.getActiveSize(), 1);
	if (obj_id) {
		UASSERT(so.getAllActives().count(obj_id) == 1);
	} else {
		obj_id = so.getAllActives().begin()->first;
		UASSERT(obj_id != 0);
	}
	return obj_id;
}

void TestSAO::testStaticSave(ServerEnvironment *env)
{
	Map &map = env->getMap();

	const v3f testpos(0, -66 * BS, 0);
	const v3s16 testblockpos = getNodeBlockPos(floatToInt(testpos, BS));

	auto obj = add_entity(env, testpos, "test:static");
	UASSERT(obj);
	const u16 obj_id = obj->getId();

	// static data should have been created
	UASSERT(obj->accessObjectProperties()->static_save);
	UASSERT(obj->m_static_exists);
	UASSERTEQ(auto, obj->m_static_block, testblockpos);
	// check that it's really in there
	auto *block = map.getBlockNoCreateNoEx(testblockpos);
	UASSERT(block);
	assert_active_in_block(block, obj_id);

	// Since we have no active blocks, stepping will deactivate it.
	env->step(m_step_interval);

	obj = nullptr; // dangling reference
	UASSERT(!env->getActiveObject(obj_id));
	UASSERTEQ(size_t, block->m_static_objects.getStoredSize(), 1);
	UASSERTEQ(size_t, block->m_static_objects.getActiveSize(), 0);
}

void TestSAO::testNotSaved(ServerEnvironment *env)
{
	Map &map = env->getMap();

	const v3f testpos(0, 4 * BS, 0);
	const v3s16 testblockpos = getNodeBlockPos(floatToInt(testpos, BS));

	// make sure matching block exists (see below)
	auto *block = map.emergeBlock(testblockpos, true);
	UASSERT(block);

	auto obj = add_entity(env, testpos, "test:non_static");
	UASSERT(obj);
	const u16 obj_id = obj->getId();

	UASSERT(!obj->accessObjectProperties()->static_save);
	UASSERT(!obj->m_static_exists);
	UASSERTEQ(size_t, block->m_static_objects.size(), 0);

	// Non-static objects are only unloaded together with their block, so doing
	// this will *not* delete it.
	env->step(m_step_interval);

	UASSERT(env->getActiveObject(obj_id) == obj);

	// Here we force the block to be unloaded, this has the expected effect.
	map.timerUpdate(10.0f, 5.0f, -1);
	env->step(m_step_interval);

	block = nullptr;
	obj = nullptr;
	UASSERT(!env->getActiveObject(obj_id));
	block = map.emergeBlock(testblockpos, false);
	if (block)
		UASSERTEQ(size_t, block->m_static_objects.size(), 0);
}

void TestSAO::testActivate(ServerEnvironment *env)
{
	Map &map = env->getMap();

	const v3f testpos(0, 0, 100 * BS);
	const v3s16 testblockpos = getNodeBlockPos(floatToInt(testpos, BS));

	// this time insert the static object manually
	StaticObject s_obj;
	{
		auto obj = std::make_unique<LuaEntitySAO>(env, testpos, "test:static", "");
		s_obj = StaticObject(obj.get(), obj->getBasePosition());
	}

	auto *block = map.emergeBlock(testblockpos, true);
	UASSERT(block);
	block->m_static_objects.insert(0, s_obj);

	// this will convert it to an active object
	env->forceActivateBlock(block);

	const u16 obj_id = assert_active_in_block(block);
	auto *obj = env->getActiveObject(obj_id);
	UASSERT(obj);

	// same conditions as testStaticSave
	UASSERT(obj->m_static_exists);
	UASSERTEQ(auto, obj->m_static_block, testblockpos);

	// let the object be deactivated, it causes a warning when we unload
	// MapBlocks in further tests otherwise
	// FIXME: figure out if this is a bug that needs to be fixed
	env->step(m_step_interval);
}

void TestSAO::testStaticToFalse(ServerEnvironment *env)
{
	Map &map = env->getMap();

	const v3f testpos(0, 0, -22 * BS);
	const v3s16 testblockpos = getNodeBlockPos(floatToInt(testpos, BS));

	/*
	 * This test represents an edge case where static_save changed from true
	 * to false while the server was off.
	 */

	StaticObject s_obj;
	{
		auto obj = std::make_unique<LuaEntitySAO>(env, testpos, "test:non_static", "");
		// since the object is not added, it doesn't read info from Lua and retains
		// the default value. This is what allows us to do this.
		UASSERT(obj->isStaticAllowed());
		s_obj = StaticObject(obj.get(), obj->getBasePosition());
	}

	auto *block = map.emergeBlock(testblockpos, true);
	UASSERT(block);
	block->m_static_objects.insert(0, s_obj);

	env->forceActivateBlock(block);

	const u16 obj_id = assert_active_in_block(block);
	auto *obj = env->getActiveObject(obj_id);
	UASSERT(obj);

	// environment must remember place of static data
	UASSERT(obj->m_static_exists);
	UASSERTEQ(auto, obj->m_static_block, testblockpos);

#if 0
	/*
	 * FIXME: needs investigation
	 * When using this code path the test fails and there's also a warning.
	 * The problem is that the block is unloaded while the object is still in the
	 * active list, removeFarObjects() has no chance at doing something.
	 * This seems to be a general issue that in practice only applies with static_save=false.
	 */
	map.timerUpdate(10.0f, 5.0f, -1);
	env->step(m_step_interval);
#else
	obj->markForRemoval();
	env->step(m_step_interval);
#endif

	block = nullptr;
	obj = nullptr;
	UASSERT(!env->getActiveObject(obj_id));
	// static data must have been deleted
	block = map.emergeBlock(testblockpos, false);
	if (block)
		UASSERTEQ(size_t, block->m_static_objects.size(), 0);
}

void TestSAO::testStaticToTrue(ServerEnvironment *env)
{
	Map &map = env->getMap();

	const v3f testpos(123 * BS, 5 * BS, 0);
	const v3s16 testblockpos = getNodeBlockPos(floatToInt(testpos, BS));

	auto obj = add_entity(env, testpos, "test:non_static");
	UASSERT(obj);
	const u16 obj_id = obj->getId();

	UASSERT(!obj->m_static_exists);
	obj->accessObjectProperties()->static_save = true;

	env->step(m_step_interval);

	obj = nullptr;
	UASSERT(!env->getActiveObject(obj_id));

	// data must have been newly saved
	auto *block = map.getBlockNoCreateNoEx(testblockpos);
	UASSERT(block);
	UASSERTEQ(size_t, block->m_static_objects.getStoredSize(), 1);
	UASSERTEQ(size_t, block->m_static_objects.getActiveSize(), 0);
}
