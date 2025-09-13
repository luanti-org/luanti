// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#include "test.h"

#include <cmath>
#include "script/cpp_api/s_base.h"
#include "script/lua_api/l_util.h"
#include "script/lua_api/l_settings.h"
#include "script/common/c_converter.h"
#include "irrlicht_changes/printing.h"
#include "server.h"

namespace {
	class MyScriptApi : virtual public ScriptApiBase {
	public:
		MyScriptApi() : ScriptApiBase(ScriptingType::Async) {};
		void init();
		using ScriptApiBase::getStack; // make public
	};
}

class TestScriptApi : public TestBase
{
public:
	TestScriptApi() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestScriptApi"; }

	void runTests(IGameDef *gamedef);

	void testVectorMetatable(MyScriptApi *script);
	void testVectorRead(MyScriptApi *script);
	void testVectorReadErr(MyScriptApi *script);
	void testVectorReadMix(MyScriptApi *script);
};

static TestScriptApi g_test_instance;

void MyScriptApi::init()
{
	lua_State *L = getStack();

	lua_getglobal(L, "core");
	int top = lua_gettop(L);

	// By creating an environment of 'async' type we have the fewest amount
	// of external classes needed.
	lua_pushstring(L, "async");
	lua_setglobal(L, "INIT");

	LuaSettings::Register(L);
	ModApiUtil::InitializeAsync(L, top);

	lua_pop(L, 1);

	loadMod(Server::getBuiltinLuaPath() + DIR_DELIM + "init.lua", BUILTIN_MOD_NAME);
	checkSetByBuiltin();
}

void TestScriptApi::runTests(IGameDef *gamedef)
{
	MyScriptApi script;
	try {
		script.init();
	} catch (ModError &e) {
		rawstream << e.what() << std::endl;
		num_tests_failed = 1;
		return;
	}

	TEST(testVectorMetatable, &script);
	TEST(testVectorRead, &script);
	TEST(testVectorReadErr, &script);
	TEST(testVectorReadMix, &script);
}

// Runs Lua code and leaves `nresults` return values on the stack
static void run(lua_State *L, const char *code, int nresults)
{
	if (luaL_loadstring(L, code) != 0) {
		rawstream << lua_tostring(L, -1) << std::endl;
		UASSERT(false);
	}
	if (lua_pcall(L, 0, nresults, 0) != 0) {
		throw LuaError(lua_tostring(L, -1));
	}
}

void TestScriptApi::testVectorMetatable(MyScriptApi *script)
{
	lua_State *L = script->getStack();
	StackUnroller unroller(L);

	const auto &call_vector_check = [&] () -> bool {
		lua_setglobal(L, "tmp");
		run(L, "return vector.check(tmp)", 1);
		return lua_toboolean(L, -1);
	};

	push_v3s16(L, {1, 2, 3});
	UASSERT(call_vector_check());

	push_v3f(L, {1, 2, 3});
	UASSERT(call_vector_check());

	// 2-component vectors must not have this metatable
	push_v2s32(L, {0, 0});
	UASSERT(!call_vector_check());

	push_v2f(L, {0, 0});
	UASSERT(!call_vector_check());
}

void TestScriptApi::testVectorRead(MyScriptApi *script)
{
	lua_State *L = script->getStack();
	StackUnroller unroller(L);

	// both methods should parse these
	const std::pair<const char*, v3s16> pairs1[] = {
		{"return {x=1, y=-2, z=3}",    {1, -2, 3}},
		{"return {x=1.1, y=0, z=0}",   {1, 0, 0}},
		{"return {x=1.5, y=0, z=0}",   {2, 0, 0}},
		{"return {x=-1.1, y=0, z=0}",  {-1, 0, 0}},
		{"return {x=-1.5, y=0, z=0}",  {-2, 0, 0}},
		{"return vector.new(5, 6, 7)", {5, 6, 7}},
		{"return vector.new(32767, 0, -32768)", {S16_MAX, 0, S16_MIN}},
	};
	for (auto &it : pairs1) {
		run(L, it.first, 1);
		v3s16 v = read_v3s16(L, -1);
		UASSERTEQ(auto, v, it.second);
		v = check_v3s16(L, -1);
		UASSERTEQ(auto, v, it.second);
		lua_pop(L, 1);
	}
}

void TestScriptApi::testVectorReadErr(MyScriptApi *script)
{
	lua_State *L = script->getStack();
	StackUnroller unroller(L);

	// both methods should reject these
	const char *errs1[] = {
		"return 'bamboo'",
		"return function() end",
		"return nil",
	};
	for (auto &it : errs1) {
		infostream << it << std::endl;
		run(L, it, 1);
		EXCEPTION_CHECK(LuaError, read_v3s16(L, -1));
		EXCEPTION_CHECK(LuaError, check_v3s16(L, -1));
	}
}

void TestScriptApi::testVectorReadMix(MyScriptApi *script)
{
	lua_State *L = script->getStack();
	StackUnroller unroller(L);

	// read_v3s16 should allow these, but check_v3s16 should not
	const std::pair<const char*, v3s16> pairs2[] = {
		{"return {}",                         {0, 0, 0}},
		{"return {y=1, z=3}",                 {0, 1, 3}},
		{"return {x=1, z=3}",                 {1, 0, 3}},
		{"return {x=1, y=3}",                 {1, 3, 0}},
		{"return {x='3', y='2.9', z=3}",      {3, 3, 3}},
		{"return {x=false, y=0, z=0}",        {0, 0, 0}},
		{"return {x='?', y=0, z=0}",          {0, 0, 0}},
		{"return {x={'baguette'}, y=0, z=0}", {0, 0, 0}},
	};
	for (auto &it : pairs2) {
		infostream << it.first << std::endl;
		run(L, it.first, 1);
		v3s16 v = read_v3s16(L, -1);
		UASSERTEQ(auto, v, it.second);
		EXCEPTION_CHECK(LuaError, check_v3s16(L, -1));
		lua_pop(L, 1);
	}

	// same but even the result is undefined
	const char *errs2[] = {
		"return {x=0, y=0, z=0/0}", // nan
		"return {x=0, y=0, z=math.huge}", // inf
	};
	for (auto &it : errs2) {
		infostream << it << std::endl;
		run(L, it, 1);
		(void)read_v3s16(L, -1);
		EXCEPTION_CHECK(LuaError, check_v3s16(L, -1));
		lua_pop(L, 1);
	}
}
