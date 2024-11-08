// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "cpp_api/s_mainmenu.h"
#include "cpp_api/s_async.h"

/*****************************************************************************/
/* Scripting <-> Main Menu Interface                                         */
/*****************************************************************************/

class MainMenuScripting
		: virtual public ScriptApiBase,
		  public ScriptApiMainMenu
{
public:
	MainMenuScripting(GUIEngine* guiengine);

	// Global step handler to pass back async events
	void step();

	// Calls core.on_before_close()
	void beforeClose();

	// Pass async events from engine to async threads
	u32 queueAsync(std::string &&serialized_func,
		std::string &&serialized_param);

private:
	void initializeModApi(lua_State *L, int top);
	static void registerLuaClasses(lua_State *L, int top);

	AsyncEngine asyncEngine;
};
