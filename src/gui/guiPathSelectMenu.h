
// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 sapier

#pragma once

#include <string>

#include "modalMenu.h"
#include "IGUIFileOpenDialog.h"

struct TextDest;

class GUIFileSelectMenu : public GUIModalMenu
{
public:
	GUIFileSelectMenu(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
			IMenuManager *menumgr, const std::string &title,
			const std::string &formid, bool is_file_select);
	~GUIFileSelectMenu();

	/*
	 Remove and re-add (or reposition) stuff
	 */
	void regenerateGui(v2u32 screensize);

	void drawMenu();

	bool OnEvent(const SEvent &event);

	void setTextDest(TextDest *dest) { m_text_dst = dest; }

protected:
	std::wstring getLabelByID(s32 id) { return L""; }
	std::string getNameByID(s32 id) { return ""; }

private:
	void acceptInput();

	std::wstring m_title;
	bool m_accepted = false;

	gui::IGUIFileOpenDialog *m_fileOpenDialog = nullptr;

	TextDest *m_text_dst = nullptr;

	std::string m_formname;
	bool m_file_select_dialog;
};
