// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "lua_api/l_base.h"
#include "config.h"

struct HTTPFetchRequest;
struct HTTPFetchResult;

class ModApiHttp : public ModApiBase {
private:
#if USE_CURL
	// Helpers for HTTP fetch functions
	static void read_http_fetch_request(lua_State *L, HTTPFetchRequest &req);
	static void push_http_fetch_result(lua_State *L, HTTPFetchResult &res, bool completed = true);

	// http_fetch_sync({url=, timeout=, data=})
	static int l_http_fetch_sync(lua_State *L);

	// http_fetch_async({url=, timeout=, data=})
	static int l_http_fetch_async(lua_State *L);

	// http_fetch_async_get(handle)
	static int l_http_fetch_async_get(lua_State *L);

	// request_http_api()
	static int l_request_http_api(lua_State *L);

	// get_http_api()
	static int l_get_http_api(lua_State *L);
#endif

	// set_http_api_lua() [internal]
	static int l_set_http_api_lua(lua_State *L);


public:
	static void Initialize(lua_State *L, int top);
	static void InitializeAsync(lua_State *L, int top);
};
