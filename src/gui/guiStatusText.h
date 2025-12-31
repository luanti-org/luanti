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
 * Can be used in-game (for fly/fast mode, volume changes, etc.) or in menus
 * (for screenshot notifications, etc.)
 *
 * The text automatically fades out and disappears after a configurable duration.
 */
class GUIStatusText
{
public:
	GUIStatusText(gui::IGUIEnvironment *guienv, gui::IGUIElement *parent = nullptr);
	~GUIStatusText();

	// Show a status text (will fade out after default duration)
	void showStatusText(const std::wstring &str);

	// Update the element (call once per frame)
	// dtime: time elapsed since last frame
	void update(float dtime);

	// Set the duration for which the text is shown before fading out
	void setDisplayDuration(float seconds) { m_display_duration = seconds; }
	float getDisplayDuration() const { return m_display_duration; }

	// Set background color (enables background if alpha > 0)
	void setBackgroundColor(const video::SColor &color) { m_background_color = color; }

	// Enable/disable background
	void setBackgroundEnabled(bool enabled) { m_background_enabled = enabled; }
	bool isBackgroundEnabled() const { return m_background_enabled; }

	// Set text color
	void setTextColor(const video::SColor &color) { m_initial_color = color; }

	// Set custom position (screen coordinates)
	void setPosition(s32 x, s32 y, bool center_horizontally = false)
	{
		m_custom_pos_x = x;
		m_custom_pos_y = y;
		m_use_custom_position = true;
		m_center_horizontally = center_horizontally;
	}

	// Set bar height for full-width display mode
	void setBarHeight(s32 height) { m_bar_height = height; }
	s32 getBarHeight() const { return m_bar_height; }

	// Set text alignment (for centered text in full-width bar)
	void setTextAlignment(gui::EGUI_ALIGNMENT align) { m_text_alignment = align; }

	// Get the underlying Irrlicht element for advanced customization if needed
	gui::IGUIStaticText *getElement() { return m_guitext_status; }
	const gui::IGUIStaticText *getElement() const { return m_guitext_status; }

	// Clear the current text immediately
	void clearStatusText();

	// Manual visibility control (normally managed automatically)
	void setVisible(bool visible);
	bool isVisible() const;

	// Get current text
	const std::wstring &getStatusText() const { return m_statustext; }

	// Get current time
	float getStatusTextTime() const { return m_statustext_time; }

	// Get the current fade progress (0.0 = fully visible, 1.0 = fully faded)
	float getFadeProgress() const { return m_fade_progress; }

private:
	gui::IGUIStaticText *m_guitext_status = nullptr;
	std::wstring m_statustext;
	float m_statustext_time = 0.0f;
	float m_display_duration = 1.5f; // Default: 1.5 seconds
	float m_fade_progress = 0.0f;
	video::SColor m_initial_color;
	video::SColor m_background_color = video::SColor(0, 0, 0, 0); // Transparent by default
	bool m_background_enabled = false;
	bool m_use_custom_position = false;
	s32 m_custom_pos_x = 0;
	s32 m_custom_pos_y = 0;
	bool m_center_horizontally = false;
	s32 m_bar_height = 0; // Height of full-width bar (0 = auto)
	gui::EGUI_ALIGNMENT m_text_alignment = gui::EGUIA_CENTER;

	// Internal helper to update position based on screen size
	void updatePosition();
};
