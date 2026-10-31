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
		  public ScriptApiSecurity,
		  public ScriptApiAsync
{
public:
	MainMenuScripting(GUIEngine* guiengine);

	// Is the main menu allowed write access to this path?
	static bool mayModifyPath(const std::string &path);

protected:
	// from ScriptApiSecurity:
	bool checkPathInternal(const std::string &abs_path, bool write_required,
		bool *write_allowed) override;
	// from ScriptApiAsync:
	void reportAsyncError(const std::string &msg) override;

private:
	void initializeModApi(lua_State *L, int top);
	static void registerLuaClasses(lua_State *L, int top);
};
