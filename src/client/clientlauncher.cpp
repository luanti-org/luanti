// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "gui/mainmenumanager.h"
#include "clouds.h"
#include "gui/touchcontrols.h"
#include "filesys.h"
#include "gui/guiMainMenu.h"
#include "game.h"
#include "player.h"
#include "chat.h"
#include "gettext.h"
#include "inputhandler.h"
#include "profiler.h"
#include "gui/guiEngine.h"
#include "fontengine.h"
#include "clientlauncher.h"
#include "version.h"
#include "renderingengine.h"
#include "settings.h"
#include "util/tracy_wrapper.h"
#include <IGUISpriteBank.h>
#include <ICameraSceneNode.h>
#include <unordered_map>

#if USE_SOUND
	#include "sound/sound_openal.h"
#endif

/* mainmenumanager.h
 */
gui::IGUIEnvironment *guienv = nullptr;
gui::IGUIStaticText *guiroot = nullptr;
MainMenuManager g_menumgr;

// Passed to menus to allow disconnecting and exiting
MainGameCallback *g_gamecallback = nullptr;

#if 0
// This can be helpful for the next code cleanup
static void dump_start_data(const GameStartData &data)
{
	std::cout <<
		"\ndedicated   " << (int)data.is_dedicated_server <<
		"\nport        " << data.socket_port <<
		"\nworld_path  " << data.world_spec.path <<
		"\nworld game  " << data.world_spec.gameid <<
		"\ngame path   " << data.game_spec.path <<
		"\nplayer name " << data.name <<
		"\naddress     " << data.address << std::endl;
}
#endif

ClientLauncher::~ClientLauncher()
{
	delete input;

	g_settings->deregisterAllChangedCallbacks(this);

	delete g_fontengine;
	g_fontengine = nullptr;
	delete g_gamecallback;
	g_gamecallback = nullptr;

	guiroot = nullptr;
	guienv = nullptr;
	assert(g_menumgr.menuCount() == 0);

	delete m_rendering_engine;

	// delete event receiver only after all Irrlicht stuff is gone
	delete receiver;

#if USE_SOUND
	g_sound_manager_singleton.reset();
#endif
}


bool ClientLauncher::run(GameStartData &start_data, const Settings &cmd_args)
{
	/* This function is called when a client must be started.
	 * Covered cases:
	 *   - Singleplayer (address but map provided)
	 *   - Join server (no map but address provided)
	 *   - Local server (for main menu only)
	*/

	init_args(start_data, cmd_args);

#if USE_SOUND
	g_sound_manager_singleton = createSoundManagerSingleton();
#endif

	if (!init_engine())
		return false;

	if (!m_rendering_engine->get_video_driver()) {
		errorstream << "Could not initialize video driver." << std::endl;
		return false;
	}

	m_rendering_engine->setupTopLevelWindow();

	// Create game callback for menus
	g_gamecallback = new MainGameCallback();

	m_rendering_engine->setResizable(true);

	init_input();

	guienv = m_rendering_engine->get_gui_env();
	config_guienv();
	g_settings->registerChangedCallback("dpi_change_notifier", setting_changed_callback, this);
	g_settings->registerChangedCallback("display_density_factor", setting_changed_callback, this);
	g_settings->registerChangedCallback("gui_scaling", setting_changed_callback, this);

	g_fontengine = new FontEngine(guienv);

	// Create the menu clouds
	// This is only global so it can be used by RenderingEngine::draw_load_screen().
	assert(!g_menucloudsmgr && !g_menuclouds);
	std::unique_ptr<IWritableShaderSource> ssrc(createShaderSource());
	ssrc->addShaderUniformSetterFactory(std::make_unique<FogShaderUniformSetterFactory>());
	g_menucloudsmgr = m_rendering_engine->get_scene_manager()->createNewSceneManager();
	g_menuclouds = new Clouds(g_menucloudsmgr, ssrc.get(), -1, rand());
	g_menuclouds->setHeight(100.0f);
	g_menuclouds->update(v3f(0, 0, 0), video::SColor(255, 240, 240, 255));
	scene::ICameraSceneNode* camera;
	camera = g_menucloudsmgr->addCameraSceneNode(NULL, v3f(0, 0, 0), v3f(0, 60, 100));
	camera->setFarValue(10000);

	/*
		GUI stuff
	*/

	ChatBackend chat_backend;

	// If an error occurs, this is set to something by menu().
	// It is then displayed before the menu shows on the next call to menu()
	std::string error_message;
	bool reconnect_requested = false;

	bool first_loop = true;

	/*
		Menu-game loop
	*/
	bool retval         = true;
	volatile auto *kill = porting::signal_handler_killstatus();

	while (m_rendering_engine->run() && !*kill &&
		!g_gamecallback->shutdown_requested) {
		// Set the window caption
		auto driver_name = m_rendering_engine->getVideoDriver()->getName();
		std::string caption = std::string(PROJECT_NAME_C) +
			" " + g_version_hash +
			" [" + gettext("Main Menu") + "]" +
			" [" + driver_name + "]";

		m_rendering_engine->get_raw_device()->
			setWindowCaption(utf8_to_wide(caption).c_str());

#ifdef NDEBUG
		try {
#endif
			m_rendering_engine->get_gui_env()->clear();

			/*
				We need some kind of a root node to be able to add
				custom gui elements directly on the screen.
				Otherwise they won't be automatically drawn.
			*/
			guiroot = m_rendering_engine->get_gui_env()->addStaticText(L"",
				core::rect<s32>(0, 0, 10000, 10000));

			bool should_run_game = launch_game(error_message, reconnect_requested,
				start_data, cmd_args);

			// Reset the reconnect_requested flag
			reconnect_requested = false;

			// If skip_main_menu, we only want to startup once
			if (skip_main_menu && !first_loop)
				break;
			first_loop = false;

			if (!should_run_game) {
				if (skip_main_menu)
					break;
				continue;
			}

			// Break out of menu-game loop to shut down cleanly
			if (!m_rendering_engine->run() || *kill)
				break;

			the_game(
				kill,
				input,
				m_rendering_engine,
				start_data,
				error_message,
				chat_backend,
				&reconnect_requested
			);
#ifdef NDEBUG
		} catch (std::exception &e) {
			error_message = "Some exception: ";
			error_message.append(debug_describe_exc(e));
			errorstream << error_message << std::endl;
		}
#endif

		m_rendering_engine->get_scene_manager()->clear();

		if (g_touchcontrols) {
			delete g_touchcontrols;
			g_touchcontrols = NULL;
		}

		/* Save the settings when leaving the game.
		 * This makes sure that setting changes made in-game are persisted even
		 * in case of a later unclean exit from the mainmenu.
		 * This is especially useful on Android because closing the app from the
		 * "Recents screen" results in an unclean exit.
		 * Caveat: This means that the settings are saved twice when exiting Minetest.
		 */
		if (!g_settings_path.empty())
			g_settings->updateConfigFile(g_settings_path.c_str());

		// If no main menu, show error and exit
		if (skip_main_menu) {
			if (!error_message.empty())
				retval = false;
			break;
		}
	} // Menu-game loop

	// If profiler was enabled print it one last time
	if (g_settings->getFloat("profiler_print_interval") > 0) {
		infostream << "Profiler:" << std::endl;
		g_profiler->print(infostream);
		g_profiler->clear();
	}

	assert(g_menucloudsmgr->getReferenceCount() == 1);
	g_menucloudsmgr->drop();
	g_menucloudsmgr = nullptr;
	assert(g_menuclouds->getReferenceCount() == 1);
	g_menuclouds->drop();
	g_menuclouds = nullptr;

	return retval;
}

void ClientLauncher::init_args(GameStartData &start_data, const Settings &cmd_args)
{
	skip_main_menu = cmd_args.getFlag("go");

	start_data.address = g_settings->get("address");
	if (cmd_args.exists("address")) {
		// Join a remote server
		start_data.address = cmd_args.get("address");
		start_data.world_path.clear();
		start_data.name = g_settings->get("name");
	}
	if (!start_data.world_path.empty()) {
		// Start a singleplayer instance
		start_data.address = "";
	}

	if (cmd_args.exists("name"))
		start_data.name = cmd_args.get("name");

	random_input = g_settings->getBool("random_input")
			|| cmd_args.getFlag("random-input");
}

bool ClientLauncher::init_engine()
{
	receiver = new MyEventReceiver();
	try {
		m_rendering_engine = new RenderingEngine(receiver);
	} catch (std::exception &e) {
		errorstream << e.what() << std::endl;
	}
	return !!m_rendering_engine;
}

void ClientLauncher::init_input()
{
	if (random_input)
		input = new RandomInputHandler();
	else
		input = new RealInputHandler(receiver);

	if (g_settings->getBool("enable_joysticks"))
		init_joysticks();
}

void ClientLauncher::init_joysticks()
{
	core::array<SJoystickInfo> infos;
	std::vector<SJoystickInfo> joystick_infos;

	// Make sure this is called maximum once per
	// irrlicht device, otherwise it will give you
	// multiple events for the same joystick.
	if (!m_rendering_engine->get_raw_device()->activateJoysticks(infos)) {
		errorstream << "Could not activate joystick support." << std::endl;
		return;
	}

	infostream << "Joystick support enabled" << std::endl;
	joystick_infos.reserve(infos.size());
	for (u32 i = 0; i < infos.size(); i++) {
		joystick_infos.push_back(infos[i]);
	}
	input->joystick.onJoystickConnect(joystick_infos);
}

void ClientLauncher::setting_changed_callback(const std::string &name, void *data)
{
	static_cast<ClientLauncher*>(data)->config_guienv();
}

static video::ITexture *loadTexture(video::IVideoDriver *driver, const char *path)
{
	// FIXME?: it would be cleaner to do this through a ITextureSource, but we don't have one
	video::ITexture *texture = nullptr;
	verbosestream << "Loading texture " << path << std::endl;
	if (auto *image = driver->createImageFromFile(path); image) {
		texture = driver->addTexture(fs::GetFilenameFromPath(path), image);
		image->drop();
	}
	return texture;
}

void ClientLauncher::config_guienv()
{
	gui::IGUISkin *skin = guienv->getSkin();

	skin->setColor(gui::EGDC_WINDOW_SYMBOL, video::SColor(255, 255, 255, 255));
	skin->setColor(gui::EGDC_BUTTON_TEXT, video::SColor(255, 255, 255, 255));
	skin->setColor(gui::EGDC_3D_LIGHT, video::SColor(0, 0, 0, 0));
	skin->setColor(gui::EGDC_3D_HIGH_LIGHT, video::SColor(255, 30, 30, 30));
	skin->setColor(gui::EGDC_3D_SHADOW, video::SColor(255, 0, 0, 0));
	skin->setColor(gui::EGDC_HIGH_LIGHT, video::SColor(255, 70, 120, 50));
	skin->setColor(gui::EGDC_HIGH_LIGHT_TEXT, video::SColor(255, 255, 255, 255));
	skin->setColor(gui::EGDC_EDITABLE, video::SColor(255, 128, 128, 128));
	skin->setColor(gui::EGDC_FOCUSED_EDITABLE, video::SColor(255, 96, 134, 49));

	float density = rangelim(g_settings->getFloat("gui_scaling"), 0.5f, 20) *
		RenderingEngine::getDisplayDensity();
	skin->setScale(density);
	skin->setSize(gui::EGDS_CHECK_BOX_WIDTH, (s32)(17.0f * density));
	skin->setSize(gui::EGDS_SCROLLBAR_SIZE, (s32)(21.0f * density));
	skin->setSize(gui::EGDS_WINDOW_BUTTON_WIDTH, (s32)(15.0f * density));

	static u32 orig_sprite_id = skin->getIcon(gui::EGDI_CHECK_BOX_CHECKED);
	static std::unordered_map<std::string, u32> sprite_ids;

	if (density > 1.5f) {
		// Texture dimensions should be a power of 2
		std::string path = porting::path_share + "/textures/base/pack/";
		if (density > 3.5f)
			path.append("checkbox_64.png");
		else if (density > 2.0f)
			path.append("checkbox_32.png");
		else
			path.append("checkbox_16.png");

		auto cached_id = sprite_ids.find(path);
		if (cached_id != sprite_ids.end()) {
			skin->setIcon(gui::EGDI_CHECK_BOX_CHECKED, cached_id->second);
		} else {
			auto *driver = m_rendering_engine->get_video_driver();
			auto *texture = loadTexture(driver, path.c_str());
			s32 id = skin->getSpriteBank()->addTextureAsSprite(texture);
			if (id != -1) {
				skin->setIcon(gui::EGDI_CHECK_BOX_CHECKED, id);
				sprite_ids.emplace(path, id);
			}
		}
	} else {
		skin->setIcon(gui::EGDI_CHECK_BOX_CHECKED, orig_sprite_id);
	}
}

bool ClientLauncher::launch_game(std::string &error_message,
		bool reconnect_requested, GameStartData &start_data,
		const Settings &cmd_args)
{
	// Prepare and check the start data to launch a game
	std::string error_message_lua = error_message;
	error_message.clear();

	if (cmd_args.exists("password"))
		start_data.password = cmd_args.get("password");

	if (cmd_args.exists("password-file")) {
		std::ifstream passfile(cmd_args.get("password-file"));
		if (passfile.good()) {
			std::getline(passfile, start_data.password);
		} else {
			error_message = gettext("Provided password file "
					"failed to open: ")
					+ cmd_args.get("password-file");
			errorstream << error_message << std::endl;
			return false;
		}
	}

	// If a world was commanded, append and select it
	// This is provieded by "get_world_from_cmdline()", main.cpp
	if (!start_data.world_path.empty()) {
		auto &spec = start_data.world_spec;

		spec.path = start_data.world_path;
		spec.gameid = getWorldGameId(spec.path, true);
		spec.name = _("[--world parameter]");
	}

	/* Show the GUI menu
	 */
	std::string server_name, server_description;
	if (!skip_main_menu) {
		// Initialize menu data
		// TODO: Re-use existing structs (GameStartData)
		MainMenuData menudata;
		menudata.address                         = start_data.address;
		menudata.name                            = start_data.name;
		menudata.password                        = start_data.password;
		menudata.port                            = itos(start_data.socket_port);
		menudata.script_data.errormessage        = std::move(error_message_lua);
		menudata.script_data.reconnect_requested = reconnect_requested;

		main_menu(&menudata);

		// Skip further loading if there was an exit signal.
		if (*porting::signal_handler_killstatus())
			return false;

		if (!menudata.script_data.errormessage.empty()) {
			/* The calling function will pass this back into this function upon the
			 * next iteration (if any) causing it to be displayed by the GUI
			 */
			error_message = menudata.script_data.errormessage;
			return false;
		}

		int newport = stoi(menudata.port);
		if (newport != 0)
			start_data.socket_port = newport;

		// Update world information using main menu data
		std::vector<WorldSpec> worldspecs = getAvailableWorlds();

		int world_index = menudata.selected_world;
		if (world_index >= 0 && world_index < (int)worldspecs.size()) {
			start_data.world_spec = worldspecs[world_index];
		}

		start_data.name = menudata.name;
		start_data.password = menudata.password;
		start_data.address = std::move(menudata.address);
		start_data.allow_login_or_register = menudata.allow_login_or_register;
		server_name = menudata.servername;
		server_description = menudata.serverdescription;

		start_data.local_server = !menudata.simple_singleplayer_mode &&
			start_data.address.empty();
	} else {
		start_data.local_server = !start_data.world_path.empty() &&
			start_data.address.empty() && !start_data.name.empty();
	}

	if (!m_rendering_engine->run())
		return false;

	if (!start_data.isSinglePlayer() && start_data.name.empty()) {
		error_message = gettext("Please choose a name!");
		errorstream << error_message << std::endl;
		return false;
	}

	// If using simple singleplayer mode, override
	if (start_data.isSinglePlayer()) {
		start_data.name = "singleplayer";
		start_data.password = "";
		start_data.socket_port = myrand_range(49152, 65535);
	} else {
		g_settings->set("name", start_data.name);
	}

	if (start_data.name.length() > PLAYERNAME_SIZE - 1) {
		error_message = gettext("Player name too long.");
		start_data.name.resize(PLAYERNAME_SIZE);
		g_settings->set("name", start_data.name);
		return false;
	}

	auto &worldspec = start_data.world_spec;
	infostream << "Selected world: " << worldspec.name
	           << " [" << worldspec.path << "]" << std::endl;

	if (start_data.address.empty()) {
		// For singleplayer and local server
		if (worldspec.path.empty()) {
			error_message = gettext("No world selected and no address "
					"provided. Nothing to do.");
			errorstream << error_message << std::endl;
			return false;
		}

		if (!fs::PathExists(worldspec.path)) {
			error_message = gettext("Provided world path doesn't exist: ")
					+ worldspec.path;
			errorstream << error_message << std::endl;
			return false;
		}

		// Load gamespec for required game
		start_data.game_spec = findWorldSubgame(worldspec.path);
		if (!start_data.game_spec.isValid()) {
			error_message = gettext("Could not find or load game: ")
					+ worldspec.gameid;
			errorstream << error_message << std::endl;
			return false;
		}

		return true;
	}

	start_data.world_path = start_data.world_spec.path;
	return true;
}

void ClientLauncher::main_menu(MainMenuData *menudata)
{
	volatile auto       *kill   = porting::signal_handler_killstatus();
	video::IVideoDriver *driver = m_rendering_engine->get_video_driver();
	auto                *device = m_rendering_engine->get_raw_device();

	// Wait until app is in foreground because of #15883
	infostream << "Waiting for app to be in foreground" << std::endl;
	while (m_rendering_engine->run() && !*kill) {
		if (device->isWindowVisible())
			break;
		sleep_ms(25);
	}
	infostream << "Waited for app to be in foreground" << std::endl;

	infostream << "Waiting for other menus" << std::endl;
	auto framemarker = FrameMarker("ClientLauncher::main_menu()-wait-frame").started();
	while (m_rendering_engine->run() && !*kill) {
		if (!isMenuActive())
			break;
		driver->beginScene(true, true, video::SColor(255, 128, 128, 128));
		m_rendering_engine->get_gui_env()->drawAll();
		driver->endScene();
		framemarker.end();
		// On some computers framerate doesn't seem to be automatically limited
		sleep_ms(25);
		framemarker.start();
	}
	framemarker.end();
	infostream << "Waited for other menus" << std::endl;

	auto *cur_control = device->getCursorControl();
	if (cur_control) {
		// Cursor can be non-visible when coming from the game
		cur_control->setVisible(true);
		// Set absolute mouse mode
		cur_control->setRelativeMode(false);
	}

	/* show main menu */
	GUIEngine mymenu(&input->joystick, guiroot, m_rendering_engine, &g_menumgr, menudata, *kill);

	/* leave scene manager in a clean state */
	m_rendering_engine->get_scene_manager()->clear();

	/* Save the settings when leaving the mainmenu.
	 * This makes sure that setting changes made in the mainmenu are persisted
	 * even in case of a later unclean exit from the game.
	 * This is especially useful on Android because closing the app from the
	 * "Recents screen" results in an unclean exit.
	 * Caveat: This means that the settings are saved twice when exiting Minetest.
	 */
	if (!g_settings_path.empty())
		g_settings->updateConfigFile(g_settings_path.c_str());
}
