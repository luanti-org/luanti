// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024 cx384

#pragma once

#include <string>
#include "irr_v3d.h"

class Client;
class RenderingEngine;
class InputHandler;
class ISoundManager;
class GUIFormSpecMenu;

/*
This object intend to contain the core fromspec functionality.
It includes:
  - methods to show specific formspec menus
  - storing the opened fromspec
  - handling fromspec related callbacks
 */
struct GameFormSpec
{
	void init(Client *client, RenderingEngine *rendering_engine, InputHandler *input)
	{
		m_client = client;
		m_rendering_engine = rendering_engine;
		m_input = input;
	}

	~GameFormSpec();

	void showFormSpec(const std::string &formspec, const std::string &formname);
	void showLocalFormSpec(const std::string &formspec, const std::string &formname);
	void showNodeFormspec(const std::string &formspec, const v3s16 &nodepos);
	void showPlayerInventory();
	void showDeathFormspecLegacy();
	void showPauseMenu();

	void update();
	void disableDebugView();

	bool handleCallbacks();

#ifdef __ANDROID__
	// Returns false if no formspec open
	bool handleAndroidUIInput();
#endif

private:
	Client *m_client;
	RenderingEngine *m_rendering_engine;
	InputHandler *m_input;

	// Default: "". If other than "": Empty show_formspec packets will only
	// close the formspec when the formname matches
	std::string m_formname;
	GUIFormSpecMenu *m_formspec = nullptr;

	void deleteFormspec();
};
