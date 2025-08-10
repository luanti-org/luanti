// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "cpp_api/s_base.h"
#include "util/string.h"
#include "gui/guiMainMenu.h"

class ScriptApiMainMenu : virtual public ScriptApiBase {
public:
	/**
	 * Hand over MainMenuDataForScript to lua to inform lua of the content
	 * @param data the data
	 */
	void setMainMenuData(const MainMenuDataForScript *data);

	/**
	 * process events received from formspec
	 * @param text events in textual form
	 */
	void handleMainMenuEvent(const std::string &text);

	/**
	 * process field data received from formspec
	 * @param fields data in field format
	 */
	void handleMainMenuButtons(const StringMap &fields);

	/**
	 * Called before the menu is closed, either to exit or to join a game
	 */
	void beforeClose();
};
