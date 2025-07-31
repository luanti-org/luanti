// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "game_formspec.h"

#include "gettext.h"
#include "nodemetadata.h"
#include "renderingengine.h"
#include "client.h"
#include "scripting_client.h"
#include "cpp_api/s_client_common.h"
#include "clientmap.h"
#include "gui/guiFormSpecMenu.h"
#include "gui/mainmenumanager.h"
#include "gui/touchcontrols.h"
#include "gui/touchscreeneditor.h"
#include "gui/guiPasswordChange.h"
#include "gui/guiPasswordChange.h"
#include "gui/guiOpenURL.h"
#include "gui/guiVolumeChange.h"

/*
	Text input system
*/

struct TextDestNodeMetadata : public TextDest
{
	TextDestNodeMetadata(v3s16 p, Client *client)
	{
		m_p = p;
		m_client = client;
	}
	void gotText(const StringMap &fields)
	{
		m_client->sendNodemetaFields(m_p, "", fields);
	}

	v3s16 m_p;
	Client *m_client;
};

struct TextDestPlayerInventory : public TextDest
{
	TextDestPlayerInventory(Client *client)
	{
		m_client = client;
		m_formname.clear();
	}
	TextDestPlayerInventory(Client *client, const std::string &formname)
	{
		m_client = client;
		m_formname = formname;
	}
	void gotText(const StringMap &fields)
	{
		m_client->sendInventoryFields(m_formname, fields);
	}

	Client *m_client;
};

struct LocalScriptingFormspecHandler : public TextDest
{
	LocalScriptingFormspecHandler(const std::string &formname, ScriptApiClientCommon *script)
	{
		m_formname = formname;
		m_script = script;
	}

	void gotText(const StringMap &fields)
	{
		m_script->on_formspec_input(m_formname, fields);
	}

	ScriptApiClientCommon *m_script = nullptr;
};

struct HardcodedPauseFormspecHandler : public TextDest
{
	HardcodedPauseFormspecHandler()
	{
		m_formname = "MT_PAUSE_MENU";
	}

	void gotText(const StringMap &fields)
	{
		if (fields.find("btn_settings") != fields.end()) {
			g_gamecallback->openSettings();
			return;
		}

		if (fields.find("btn_sound") != fields.end()) {
			g_gamecallback->changeVolume();
			return;
		}

		if (fields.find("btn_exit_menu") != fields.end()) {
			g_gamecallback->disconnect();
			return;
		}

		if (fields.find("btn_exit_os") != fields.end()) {
			g_gamecallback->exitToOS();
#ifndef __ANDROID__
			RenderingEngine::get_raw_device()->closeDevice();
#endif
			return;
		}

		if (fields.find("btn_change_password") != fields.end()) {
			g_gamecallback->changePassword();
			return;
		}
	}
};

struct LegacyDeathFormspecHandler : public TextDest
{
	LegacyDeathFormspecHandler(Client *client)
	{
		m_formname = "MT_DEATH_SCREEN";
		m_client = client;
	}

	void gotText(const StringMap &fields)
	{
		if (fields.find("quit") != fields.end())
			m_client->sendRespawnLegacy();
	}

	Client *m_client = nullptr;
};

/* Form update callback */

class NodeMetadataFormSource: public IFormSource
{
public:
	NodeMetadataFormSource(ClientMap *map, v3s16 p):
		m_map(map),
		m_p(p)
	{
	}
	const std::string &getForm() const
	{
		static const std::string empty_string = "";
		NodeMetadata *meta = m_map->getNodeMetadata(m_p);

		if (!meta)
			return empty_string;

		return meta->getString("formspec");
	}

	virtual std::string resolveText(const std::string &str)
	{
		NodeMetadata *meta = m_map->getNodeMetadata(m_p);

		if (!meta)
			return str;

		return meta->resolveString(str);
	}

	ClientMap *m_map;
	v3s16 m_p;
};

class PlayerInventoryFormSource: public IFormSource
{
public:
	PlayerInventoryFormSource(Client *client):
		m_client(client)
	{
	}

	const std::string &getForm() const
	{
		LocalPlayer *player = m_client->getEnv().getLocalPlayer();

		if (!player->inventory_formspec_override.empty())
			return player->inventory_formspec_override;

		return player->inventory_formspec;
	}

	Client *m_client;
};


//// GameFormSpec

void GameFormSpec::init(Client *client, RenderingEngine *rendering_engine, InputHandler *input)
{
	m_client = client;
	m_rendering_engine = rendering_engine;
	m_input = input;
	m_pause_script = std::make_unique<PauseMenuScripting>(client);
	m_pause_script->loadBuiltin();

	// Make sure any remaining game callback requests are cleared out.
	*g_gamecallback = MainGameCallback();
}

void GameFormSpec::deleteFormspec()
{
	if (m_formspec) {
		m_formspec->drop();
		m_formspec = nullptr;
	}
}

void GameFormSpec::reset()
{
	if (m_formspec)
		m_formspec->quitMenu();
	deleteFormspec();
}

bool GameFormSpec::handleEmptyFormspec(const std::string &formspec, const std::string &formname)
{
	if (formspec.empty()) {
		GUIModalMenu *menu = g_menumgr.tryGetTopMenu();
		if (menu && (formname.empty() || formname == menu->getName())) {
			// `m_formspec` will be fixed up in `GameFormSpec::update()`
			menu->quitMenu();
		}
		return true;
	}
	return false;
}

void GameFormSpec::showFormSpec(const std::string &formspec, const std::string &formname)
{
	if (handleEmptyFormspec(formspec, formname))
		return;

	FormspecFormSource *fs_src =
		new FormspecFormSource(formspec);
	TextDestPlayerInventory *txt_dst =
		new TextDestPlayerInventory(m_client, formname);

	// Replace the currently open formspec
	GUIFormSpecMenu::create(m_formspec, m_client, m_rendering_engine->get_gui_env(),
		&m_input->joystick, fs_src, txt_dst, m_client->getFormspecPrepend(),
		m_client->getSoundManager());
	m_formspec->setName(formname);
}

void GameFormSpec::showCSMFormSpec(const std::string &formspec, const std::string &formname)
{
	if (handleEmptyFormspec(formspec, formname))
		return;

	FormspecFormSource *fs_src = new FormspecFormSource(formspec);
	LocalScriptingFormspecHandler *txt_dst =
		new LocalScriptingFormspecHandler(formname, m_client->getScript());

	GUIFormSpecMenu::create(m_formspec, m_client, m_rendering_engine->get_gui_env(),
			&m_input->joystick, fs_src, txt_dst, m_client->getFormspecPrepend(),
			m_client->getSoundManager());
	m_formspec->setName(formname);
}

void GameFormSpec::showPauseMenuFormSpec(const std::string &formspec, const std::string &formname)
{
	// The pause menu env is a trusted context like the mainmenu env and provides
	// the in-game settings formspec.
	// Neither CSM nor the server must be allowed to mess with it.

	// If we send updated formspec contents, we can either (1) recycle the old
	// GUIFormSpecMenu or (2) close the old and open a new one. This is option 2.
	(void)handleEmptyFormspec("", formname);
	if (formspec.empty())
		return;

	FormspecFormSource *fs_src = new FormspecFormSource(formspec);
	LocalScriptingFormspecHandler *txt_dst =
		new LocalScriptingFormspecHandler(formname, m_pause_script.get());

	GUIFormSpecMenu *fs = nullptr;
	GUIFormSpecMenu::create(fs, m_client, m_rendering_engine->get_gui_env(),
			// Ignore formspec prepend.
			&m_input->joystick, fs_src, txt_dst, "",
			m_client->getSoundManager());

	fs->setName(formname);
	fs->doPause = true;
	fs->drop(); // 1 reference held by `g_menumgr`
}

void GameFormSpec::showNodeFormspec(const std::string &formspec, const v3s16 &nodepos)
{
	infostream << "Launching custom inventory view" << std::endl;

	InventoryLocation inventoryloc;
	inventoryloc.setNodeMeta(nodepos);

	NodeMetadataFormSource *fs_src = new NodeMetadataFormSource(
		&m_client->getEnv().getClientMap(), nodepos);
	TextDest *txt_dst = new TextDestNodeMetadata(nodepos, m_client);

	GUIFormSpecMenu::create(m_formspec, m_client, m_rendering_engine->get_gui_env(),
		&m_input->joystick, fs_src, txt_dst, m_client->getFormspecPrepend(),
		m_client->getSoundManager());

	m_formspec->setFormSpec(formspec, inventoryloc);
}

void GameFormSpec::showPlayerInventory(const std::string *fs_override)
{
	/*
	 * Don't permit to open inventory is CAO or player doesn't exists.
	 * This prevent showing an empty inventory at player load
	 */

	LocalPlayer *player = m_client->getEnv().getLocalPlayer();
	if (!player || !player->getCAO())
		return;

	infostream << "Game: Launching inventory" << std::endl;

	auto fs_src = std::make_unique<PlayerInventoryFormSource>(m_client);

	InventoryLocation inventoryloc;
	inventoryloc.setCurrentPlayer();

	if (fs_override) {
		// Temporary overwrite for this specific formspec.
		player->inventory_formspec_override = *fs_override;
	} else {
		// Show the regular inventory formspec
		player->inventory_formspec_override.clear();
	}

	// If prevented by Client-Side Mods
	if (m_client->modsLoaded() && m_client->getScript()->on_inventory_open(m_client->getInventory(inventoryloc)))
		return;

	// Empty formspec -> do not show.
	if (fs_src->getForm().empty())
		return;

	TextDest *txt_dst = new TextDestPlayerInventory(m_client);

	GUIFormSpecMenu::create(m_formspec, m_client, m_rendering_engine->get_gui_env(),
		&m_input->joystick, fs_src.get(), txt_dst, m_client->getFormspecPrepend(),
		m_client->getSoundManager());

	m_formspec->setFormSpec(fs_src->getForm(), inventoryloc);
	fs_src.release(); // owned by GUIFormSpecMenu
}

#define SIZE_TAG "size[11,5.5,true]" // Fixed size (ignored in touchscreen mode)

void GameFormSpec::showPauseMenu()
{
	std::string control_text;

	if (g_touchcontrols) {
		control_text = strgettext("Controls:\n"
			"No menu open:\n"
			"- slide finger: look around\n"
			"- tap: place/punch/use (default)\n"
			"- long tap: dig/use (default)\n"
			"Menu/inventory open:\n"
			"- double tap (outside):\n"
			" --> close\n"
			"- touch stack, touch slot:\n"
			" --> move stack\n"
			"- touch&drag, tap 2nd finger\n"
			" --> place single item to slot\n"
			);
	}

	auto simple_singleplayer_mode = m_client->m_simple_singleplayer_mode;

	float ypos = simple_singleplayer_mode ? 0.7f : 0.1f;
	std::ostringstream os;

	os << "formspec_version[1]" << SIZE_TAG
		<< "button_exit[4," << (ypos++) << ";3,0.5;btn_continue;"
		<< strgettext("Continue") << "]";

	if (!simple_singleplayer_mode) {
		os << "button[4," << (ypos++) << ";3,0.5;btn_change_password;"
			<< strgettext("Change Password") << "]";
	} else {
		os << "field[4.95,0;5,1.5;;" << strgettext("Game paused") << ";]";
	}

	os	<< "button[4," << (ypos++) << ";3,0.5;btn_settings;"
		<< strgettext("Settings") << "]";

#ifndef __ANDROID__
#if USE_SOUND
	os << "button[4," << (ypos++) << ";3,0.5;btn_sound;"
		<< strgettext("Sound Volume") << "]";
#endif
#endif

	os		<< "button_exit[4," << (ypos++) << ";3,0.5;btn_exit_menu;"
		<< strgettext("Exit to Menu") << "]";
	os		<< "button_exit[4," << (ypos++) << ";3,0.5;btn_exit_os;"
		<< strgettext("Exit to OS")   << "]";
	if (!control_text.empty()) {
	os		<< "textarea[7.5,0.25;3.9,6.25;;" << control_text << ";]";
	}
	os		<< "textarea[0.4,0.25;3.9,6.25;;" << PROJECT_NAME_C " " VERSION_STRING "\n"
		<< "\n"
		<<  strgettext("Game info:") << "\n";
	const std::string &address = m_client->getAddressName();
	os << strgettext("- Mode: ");
	if (!simple_singleplayer_mode) {
		if (address.empty())
			os << strgettext("Hosting server");
		else
			os << strgettext("Remote server");
	} else {
		os << strgettext("Singleplayer");
	}
	os << "\n";
	if (simple_singleplayer_mode || address.empty()) {
		static const std::string on = strgettext("On");
		static const std::string off = strgettext("Off");
		// Note: Status of enable_damage and creative_mode settings is intentionally
		// NOT shown here because the game might roll its own damage system and/or do
		// a per-player Creative Mode, in which case writing it here would mislead.
		bool damage = g_settings->getBool("enable_damage");
		const std::string &announced = g_settings->getBool("server_announce") ? on : off;
		if (!simple_singleplayer_mode) {
			if (damage) {
				const std::string &pvp = g_settings->getBool("enable_pvp") ? on : off;
				//~ PvP = Player versus Player
				os << strgettext("- PvP: ") << pvp << "\n";
			}
			os << strgettext("- Public: ") << announced << "\n";
			std::string server_name = g_settings->get("server_name");
			str_formspec_escape(server_name);
			if (announced == on && !server_name.empty())
				os << strgettext("- Server Name: ") << server_name;

		}
	}
	os << ";]";

	/* Create menu */
	/* Note: FormspecFormSource and LocalFormspecHandler  *
	 * are deleted by guiFormSpecMenu                     */
	FormspecFormSource *fs_src = new FormspecFormSource(os.str());
	HardcodedPauseFormspecHandler *txt_dst = new HardcodedPauseFormspecHandler();

	GUIFormSpecMenu::create(m_formspec, m_client, m_rendering_engine->get_gui_env(),
			&m_input->joystick, fs_src, txt_dst, m_client->getFormspecPrepend(),
			m_client->getSoundManager());
	m_formspec->setFocus("btn_continue");
	// game will be paused in next step, if in singleplayer (see Game::m_is_paused)
	m_formspec->doPause = true;
}

void GameFormSpec::showDeathFormspecLegacy()
{
	static std::string formspec_str =
		std::string("formspec_version[1]") +
		SIZE_TAG
		"bgcolor[#320000b4;true]"
		"label[4.85,1.35;" + gettext("You died") + "]"
		"button_exit[4,3;3,0.5;btn_respawn;" + gettext("Respawn") + "]"
		;

	/* Create menu */
	/* Note: FormspecFormSource and LocalFormspecHandler  *
	 * are deleted by guiFormSpecMenu                     */
	FormspecFormSource *fs_src = new FormspecFormSource(formspec_str);
	LegacyDeathFormspecHandler *txt_dst = new LegacyDeathFormspecHandler(m_client);

	GUIFormSpecMenu::create(m_formspec, m_client, m_rendering_engine->get_gui_env(),
		&m_input->joystick, fs_src, txt_dst, m_client->getFormspecPrepend(),
		m_client->getSoundManager());
	m_formspec->setFocus("btn_respawn");
}

void GameFormSpec::update()
{
	/*
	   make sure menu is on top
	   1. Delete formspec menu reference if menu was removed
	   2. Else, make sure formspec menu is on top
	*/
	if (!m_formspec)
		return;

	if (m_formspec->getReferenceCount() == 1) {
		// See GUIFormSpecMenu::create what refcnt = 1 means
		this->deleteFormspec();
		return;
	}

	auto &loc = m_formspec->getFormspecLocation();
	if (loc.type == InventoryLocation::NODEMETA) {
		NodeMetadata *meta = m_client->getEnv().getClientMap().getNodeMetadata(loc.p);
		if (!meta || meta->getString("formspec").empty()) {
			m_formspec->quitMenu();
			return;
		}
	}

	if (isMenuActive())
		guiroot->bringToFront(m_formspec);
}

void GameFormSpec::disableDebugView()
{
	if (m_formspec) {
		m_formspec->setDebugView(false);
	}
}

/* returns false if game should exit, otherwise true
 */
bool GameFormSpec::handleCallbacks()
{
	auto texture_src = m_client->getTextureSource();

	if (g_gamecallback->disconnect_requested) {
		g_gamecallback->disconnect_requested = false;
		return false;
	}

	if (g_gamecallback->settings_requested) {
		m_pause_script->open_settings();
		g_gamecallback->settings_requested = false;
	}

	if (g_gamecallback->changepassword_requested) {
		(void)make_irr<GUIPasswordChange>(guienv, guiroot, -1,
				       &g_menumgr, m_client, texture_src);
		g_gamecallback->changepassword_requested = false;
	}

	if (g_gamecallback->changevolume_requested) {
		(void)make_irr<GUIVolumeChange>(guienv, guiroot, -1,
				     &g_menumgr, texture_src);
		g_gamecallback->changevolume_requested = false;
	}

	if (g_gamecallback->touchscreenlayout_requested) {
		(new GUITouchscreenLayout(guienv, guiroot, -1,
				     &g_menumgr, texture_src))->drop();
		g_gamecallback->touchscreenlayout_requested = false;
	}

	if (!g_gamecallback->show_open_url_dialog.empty()) {
		(void)make_irr<GUIOpenURLMenu>(guienv, guiroot, -1,
				 &g_menumgr, texture_src, g_gamecallback->show_open_url_dialog);
		g_gamecallback->show_open_url_dialog.clear();
	}

	return true;
}

#ifdef __ANDROID__
bool GameFormSpec::handleAndroidUIInput()
{
	// FIXME: m_formspec and this value are not in sync at all times.
	GUIModalMenu *menu = g_menumgr.tryGetTopMenu();
	if (menu) {
		menu->getAndroidUIInput();
		return true;
	}
	return false;
}
#endif
