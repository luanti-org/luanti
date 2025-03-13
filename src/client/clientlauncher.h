// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>

class RenderingEngine;
class Settings;
class MyEventReceiver;
class InputHandler;
struct GameStartData;
struct MainMenuData;

class ClientLauncher
{
public:
	ClientLauncher() = default;

	~ClientLauncher();

	bool run(GameStartData &start_data, const Settings &cmd_args);

private:
	void init_args(GameStartData &start_data, const Settings &cmd_args);
	bool init_engine();
	void init_input();
	void init_joysticks();

	static void setting_changed_callback(const std::string &name, void *data);
	void config_guienv();

	bool launch_game(std::string &error_message, bool reconnect_requested,
		GameStartData &start_data, const Settings &cmd_args);

	void main_menu(MainMenuData *menudata);

	bool skip_main_menu = false;
	bool random_input = false;
	RenderingEngine *m_rendering_engine = nullptr;
	InputHandler *input = nullptr;
	MyEventReceiver *receiver = nullptr;
};
