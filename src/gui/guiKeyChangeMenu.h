// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013 Ciaran Gultnieks <ciaran@ciarang.com>
// Copyright (C) 2013 teddydestodes <derkomtur@schattengang.net>

#pragma once

#include "irrlichttypes_extrabloated.h"
#include "modalMenu.h"
#include "gettext.h"
#include "client/keycode.h"
#include <string>
#include <vector>

class ISimpleTextureSource;

struct key_setting
{
	int id;
	std::wstring button_name;
	KeyPress key;
	std::string setting_name;
	gui::IGUIButton *button;
};

class GUIKeyChangeMenu : public GUIModalMenu
{
public:
	GUIKeyChangeMenu(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
			IMenuManager *menumgr, ISimpleTextureSource *tsrc);
	~GUIKeyChangeMenu();

	/*
	 Remove and re-add (or reposition) stuff
	 */
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	bool acceptInput();

	bool OnEvent(const SEvent &event);

	bool pausesGame() { return true; }

protected:
	std::wstring getLabelByID(s32 id) { return L""; }
	std::string getNameByID(s32 id) { return ""; }

private:
	void init_keys();

	bool resetMenu();

	void add_key(int id, std::wstring button_name, const std::string &setting_name);

	bool shift_down = false;

	key_setting *active_key = nullptr;
	gui::IGUIStaticText *key_used_text = nullptr;
	std::vector<key_setting *> key_settings;
	ISimpleTextureSource *m_tsrc;
};
