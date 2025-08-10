// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "cpp_api/s_mainmenu.h"
#include "cpp_api/s_security.h"
#include "cpp_api/s_async.h"

/*****************************************************************************/
/* Scripting <-> Main Menu Interface                                         */
/*****************************************************************************/

class MainMenuScripting
		: virtual public ScriptApiBase,
		  public ScriptApiMainMenu,
		  public ScriptApiSecurity
{
public:
	MainMenuScripting(GUIEngine* guiengine);

	// Global step handler to pass back async events
	void step();

	// Pass async events from engine to async threads
	u32 queueAsync(std::string &&serialized_func,
		std::string &&serialized_param);

	// Is the main menu allowed writeable access to this path?
	static bool mayModifyPath(const std::string &path);

	// (public implementation so it can be used from AsyncEngine)
	static bool checkPathAccess(const std::string &abs_path, bool write_required,
		bool *write_allowed);

protected:
	bool checkPathInternal(const std::string &abs_path, bool write_required,
		bool *write_allowed) override {
		return checkPathAccess(abs_path, write_required, write_allowed);
	}

private:
	void initializeModApi(lua_State *L, int top);
	static void registerLuaClasses(lua_State *L, int top);

	AsyncEngine asyncEngine;
};
