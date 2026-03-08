// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <IGUIEnvironment.h>
#include <IGUIStaticText.h>
#include <IVideoDriver.h>
#include <string>
#include "irrlichttypes.h"

/*
 * Reusable GUI element for displaying status texts with automatic fade-out.
 * Supports two main styles: in-game and main menu.
 */
class GUIStatusText
{
public:
	GUIStatusText(gui::IGUIEnvironment *guienv, gui::IGUIElement *parent = nullptr);
	~GUIStatusText();

	// Display a status text (will fade out after configured duration)
	void showStatusText(const std::wstring &str);

	// Update the element (call once per frame)
	void update(float dtime);

	// Style for in-game display (centered, no background)
	void setGameStyle();

	// Style for main menu display (full-width bar at bottom)
	void setMainMenuStyle();

	// Clear the current text immediately
	void clearStatusText();

	// Visibility control
	void setVisible(bool visible);
	bool isVisible() const;

	// Getters for testing
	const std::wstring &getStatusText() const { return m_statustext; }
	float getStatusTextTime() const { return m_statustext_time; }

private:
	gui::IGUIStaticText *m_guitext_status = nullptr;
	std::wstring m_statustext;
	float m_statustext_time = 0.0f;
	float m_display_duration = 1.5f;
	float m_fade_progress = 0.0f;
	video::SColor m_text_color;
	video::SColor m_background_color = video::SColor(0, 0, 0, 0);
	bool m_background_enabled = false;
	bool m_use_main_menu_position = false;
	// Height of the full-width bar at the bottom of the screen (main menu style)
	static constexpr s32 MAIN_MENU_BAR_HEIGHT = 40;
	gui::EGUI_ALIGNMENT m_text_alignment = gui::EGUIA_CENTER;

	// Internal helper to update position based on screen size and style
	void updatePosition();
};

