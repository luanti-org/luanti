// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

/*
	All kinds of stuff that needs to be exposed from main.cpp
*/
#include "modalMenu.h"
#include <cassert>
#include <list>

#include "IGUIEnvironment.h"

namespace gui {
	class IGUIStaticText;
}

class IGameCallback
{
public:
	virtual void exitToOS() = 0;
	virtual void openSettings() = 0;
	virtual void disconnect() = 0;
	virtual void changePassword() = 0;
	virtual void changeVolume() = 0;
	virtual void showOpenURLDialog(const std::string &url) = 0;
	virtual void touchscreenLayout() = 0;
};

extern gui::IGUIEnvironment *guienv;
extern gui::IGUIStaticText *guiroot;

// Handler for the modal menus

class MainMenuManager : public IMenuManager
{
public:
	virtual void createdMenu(gui::IGUIElement *menu)
	{
		for (gui::IGUIElement *e : m_stack) {
			if (e == menu)
				return;
		}

		if(!m_stack.empty())
			m_stack.back()->setVisible(false);

		m_stack.push_back(menu);
		guienv->setFocus(m_stack.back());
	}

	virtual void deletingMenu(gui::IGUIElement *menu)
	{
		// Remove all entries if there are duplicates
		m_stack.remove(menu);

		if(!m_stack.empty()) {
			m_stack.back()->setVisible(true);
			guienv->setFocus(m_stack.back());
		} else {
			guienv->removeFocus(menu);
		}
	}

	// Returns true to prevent further processing
	virtual bool preprocessEvent(const SEvent& event)
	{
		if (m_stack.empty())
			return false;
		GUIModalMenu *mm = dynamic_cast<GUIModalMenu*>(m_stack.back());
		return mm && mm->preprocessEvent(event);
	}

	size_t menuCount() const
	{
		return m_stack.size();
	}

	GUIModalMenu *tryGetTopMenu() const
	{
		if (m_stack.empty())
			return nullptr;
		return dynamic_cast<GUIModalMenu *>(m_stack.back());
	}

	void deleteFront()
	{
		m_stack.front()->setVisible(false);
		deletingMenu(m_stack.front());
	}

	bool pausesGame()
	{
		for (gui::IGUIElement *i : m_stack) {
			GUIModalMenu *mm = dynamic_cast<GUIModalMenu*>(i);
			if (mm && mm->pausesGame())
				return true;
		}
		return false;
	}

private:
	std::list<gui::IGUIElement*> m_stack;
};

extern MainMenuManager g_menumgr;

static inline bool isMenuActive()
{
	return g_menumgr.menuCount() != 0;
}

class MainGameCallback : public IGameCallback
{
public:
	MainGameCallback() = default;
	virtual ~MainGameCallback() = default;

	void exitToOS() override
	{
		shutdown_requested = true;
	}

	void openSettings() override
	{
		settings_requested = true;
	}

	void disconnect() override
	{
		disconnect_requested = true;
	}

	void changePassword() override
	{
		changepassword_requested = true;
	}

	void changeVolume() override
	{
		changevolume_requested = true;
	}

	void touchscreenLayout() override
	{
		touchscreenlayout_requested = true;
	}

	void showOpenURLDialog(const std::string &url) override
	{
		show_open_url_dialog = url;
	}

	bool disconnect_requested = false;
	bool settings_requested = false;
	bool changepassword_requested = false;
	bool changevolume_requested = false;
	bool touchscreenlayout_requested = false;
	bool shutdown_requested = false;
	std::string show_open_url_dialog = "";
};

extern MainGameCallback *g_gamecallback;
