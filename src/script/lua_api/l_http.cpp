// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "lua_api/l_internal.h"
#include "common/c_converter.h"
#include "common/c_content.h"
#include "lua_api/l_http.h"
#include "cpp_api/s_security.h"
#include "util/enum_string.h"
#include "httpfetch.h"
#include "settings.h"
#include "debug.h"
#include "log.h"

#include <iomanip>

#define HTTP_API(name) \
	lua_pushstring(L, #name); \
	lua_pushcfunction(L, l_http_##name); \
	lua_settable(L, -3);

const static EnumString es_HttpMethod[] = {
	{HTTP_GET, "GET"},
	{HTTP_HEAD, "HEAD"},
	{HTTP_POST, "POST"},
	{HTTP_PUT, "PUT"},
	{HTTP_PATCH, "PATCH"},
	{HTTP_DELETE, "DELETE"},
	{0, nullptr}
};

#if USE_CURL
void ModApiHttp::read_http_fetch_request(lua_State *L, HTTPFetchRequest &req)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	req.caller = httpfetch_caller_alloc_secure();
	getstringfield(L, 1, "url", req.url);
	getstringfield(L, 1, "user_agent", req.useragent);
	req.multipart = getboolfield_default(L, 1, "multipart", false);
	float timeout_sec = 0;
	if (getfloatfield(L, 1, "timeout", timeout_sec))
		req.timeout = timeout_sec * 1000;

	lua_getfield(L, 1, "method");
	if (lua_isstring(L, -1))
		string_to_enum(es_HttpMethod, req.method, lua_tostring(L, -1));
	lua_pop(L, 1);

	// post_data: if table, post form data, otherwise raw data DEPRECATED use data and method instead
	lua_getfield(L, 1, "post_data");
	if (lua_isnil(L, 2)) {
		lua_pop(L, 1);
		lua_getfield(L, 1, "data");
	} else {
		req.method = HTTP_POST;
	}

	if (lua_istable(L, 2)) {
		lua_pushnil(L);
		while (lua_next(L, 2) != 0) {
			req.fields[readParam<std::string>(L, -2)] = readParam<std::string>(L, -1);
			lua_pop(L, 1);
		}
	} else if (lua_isstring(L, 2)) {
		req.raw_data = readParam<std::string>(L, 2);
	}

	lua_pop(L, 1);

	lua_getfield(L, 1, "extra_headers");
	if (lua_istable(L, 2)) {
		lua_pushnil(L);
		while (lua_next(L, 2) != 0) {
			req.extra_headers.emplace_back(readParam<std::string>(L, -1));
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

void ModApiHttp::push_http_fetch_result(lua_State *L, HTTPFetchResult &res, bool completed)
{
	lua_newtable(L);
	setboolfield(L, -1, "succeeded", res.succeeded);
	setboolfield(L, -1, "timeout", res.timeout);
	setboolfield(L, -1, "completed", completed);
	setintfield(L, -1, "code", res.response_code);
	setstringfield(L, -1, "data", res.data);
}

// http_api.fetch_sync(HTTPRequest definition)
int ModApiHttp::l_http_fetch_sync(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	HTTPFetchRequest req;
	read_http_fetch_request(L, req);

	infostream << "Mod performs HTTP request with URL " << req.url << std::endl;

	HTTPFetchResult res;
	bool completed = httpfetch_sync_interruptible(req, res);

	push_http_fetch_result(L, res, completed);

	return 1;
}

// http_api.fetch_async(HTTPRequest definition)
int ModApiHttp::l_http_fetch_async(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	HTTPFetchRequest req;
	read_http_fetch_request(L, req);

	infostream << "Mod performs HTTP request with URL " << req.url << std::endl;
	httpfetch_async(req);

	// Convert handle to hex string since lua can't handle 64-bit integers
	std::stringstream handle_conversion_stream;
	handle_conversion_stream << std::hex << req.caller;
	std::string caller_handle(handle_conversion_stream.str());

	lua_pushstring(L, caller_handle.c_str());
	return 1;
}

// http_api.fetch_async_get(handle)
int ModApiHttp::l_http_fetch_async_get(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	std::string handle_str = luaL_checkstring(L, 1);

	// Convert hex string back to 64-bit handle
	u64 handle;
	std::stringstream handle_conversion_stream;
	handle_conversion_stream << std::hex << handle_str;
	handle_conversion_stream >> handle;

	HTTPFetchResult res;
	bool completed = httpfetch_async_get(handle, res);

	push_http_fetch_result(L, res, completed);

	return 1;
}

int ModApiHttp::l_request_http_api(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	if (!ScriptApiSecurity::checkWhitelisted(L, "secure.http_mods") &&
			!ScriptApiSecurity::checkWhitelisted(L, "secure.trusted_mods")) {
		lua_pushnil(L);
		return 1;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_HTTP_API_LUA);
	assert(lua_isfunction(L, -1));

	lua_newtable(L);
	HTTP_API(fetch_async);
	HTTP_API(fetch_async_get);

	// Stack now looks like this:
	// <function> <table with fetch_async, fetch_async_get>
	// Now call it to append .fetch(request, callback) to table
	lua_call(L, 1, 1);

	return 1;
}

int ModApiHttp::l_get_http_api(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

	lua_newtable(L);
	HTTP_API(fetch_async);
	HTTP_API(fetch_async_get);
	HTTP_API(fetch_sync);

	return 1;
}

#endif

int ModApiHttp::l_set_http_api_lua(lua_State *L)
{
	NO_MAP_LOCK_REQUIRED;

#if USE_CURL
	// This is called by builtin to give us a function that will later
	// populate the http_api table with additional method(s).
	// We need this because access to the HTTP api is security-relevant and
	// any mod could just mess with a global variable.
	luaL_checktype(L, 1, LUA_TFUNCTION);
	lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_HTTP_API_LUA);
#endif

	return 0;
}

void ModApiHttp::Initialize(lua_State *L, int top)
{
#if USE_CURL

	bool isMainmenu = false;
#if CHECK_CLIENT_BUILD()
	isMainmenu = ModApiBase::getGuiEngine(L) != nullptr;
#endif

	if (isMainmenu) {
		API_FCT(get_http_api);
	} else {
		API_FCT(request_http_api);
		API_FCT(set_http_api_lua);
	}

#else

	// Define this function anyway so builtin can call it without checking
	API_FCT(set_http_api_lua);

#endif
}

void ModApiHttp::InitializeAsync(lua_State *L, int top)
{
#if USE_CURL
	API_FCT(get_http_api);
#endif
}
