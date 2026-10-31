// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <atomic>
#include "cpp_api/s_base.h"
#include "cpp_api/s_security.h"
#include "cpp_api/s_async.h"

class CLIScripting
		: virtual public ScriptApiBase,
		  public ScriptApiSecurity,
		  public ScriptApiAsync
{
public:
	CLIScripting();

	void loadBuiltin();

	int run(const char *code);

protected:
	// from ScriptApiSecurity:
	bool checkPathInternal(const std::string &abs_path, bool write_required,
		bool *write_allowed) override {
		if (write_allowed)
			*write_allowed = true;
		return true;
	}
	// from ScriptApiAsync:
	void reportAsyncError(const std::string &msg) override;

private:
	void initializeModApi(lua_State *L, int top);
	static void registerLuaClasses(lua_State *L, int top);

	std::atomic<bool> m_error_caught;
};
