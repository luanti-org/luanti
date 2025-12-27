// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiStatusMessage.h"
#include <irrlicht_changes/static_text.h>
#include <gettext.h>
#include "client/renderingengine.h"

GUIStatusMessage::GUIStatusMessage(gui::IGUIEnvironment *guienv, gui::IGUIElement *parent)
{
	if (!guienv)
		return;

	gui::IGUIElement *root = parent ? parent : guienv->getRootGUIElement();

	// Create the static text element for displaying status messages
	m_guitext_status = gui::StaticText::add(guienv, L"<Status>",
		core::recti(), false, false, root);
	m_guitext_status->setVisible(false);

	// Get initial color from the skin
	if (guienv->getSkin())
		m_initial_color = guienv->getSkin()->getColor(gui::EGDC_BUTTON_TEXT);
	else
		m_initial_color = video::SColor(255, 0, 0, 0);
}

GUIStatusMessage::~GUIStatusMessage()
{
	if (m_guitext_status) {
		m_guitext_status->remove();
		m_guitext_status = nullptr;
	}
}

void GUIStatusMessage::showStatusText(const std::wstring &str)
{
	m_statustext = str;
	m_statustext_time = 0.0f;
	m_fade_progress = 0.0f;
}

void GUIStatusMessage::showTranslatedStatusText(const char *str)
{
	showStatusText(wstrgettext(str));
}

void GUIStatusMessage::clearStatusText()
{
	m_statustext.clear();
	m_statustext_time = 0.0f;
	m_fade_progress = 0.0f;
	if (m_guitext_status)
		m_guitext_status->setVisible(false);
}

void GUIStatusMessage::setVisible(bool visible)
{
	if (m_guitext_status)
		m_guitext_status->setVisible(visible);
}

bool GUIStatusMessage::isVisible() const
{
	if (m_guitext_status)
		return m_guitext_status->isVisible();
	return false;
}

void GUIStatusMessage::update(float dtime)
{
	if (!m_guitext_status || m_statustext.empty())
		return;

	m_statustext_time += dtime;

	// Check if we should clear the message
	if (m_statustext_time >= m_display_duration) {
		clearStatusText();
		return;
	}

	// Update fade progress and position
	m_fade_progress = m_statustext_time / m_display_duration;

	// Set text content
	m_guitext_status->setText(m_statustext.c_str());
	m_guitext_status->setVisible(true);

	// Apply text alignment
	m_guitext_status->setTextAlignment(m_text_alignment, m_text_alignment);

	updatePosition();

	// Apply background if enabled
	if (m_background_enabled) {
		video::SColor bg_fade = m_background_color;
		f32 d = m_fade_progress;
		bg_fade.setAlpha(static_cast<u32>(
			bg_fade.getAlpha() * (1.0f - d * d)));
		m_guitext_status->setBackgroundColor(bg_fade);
		m_guitext_status->setDrawBackground(true);
	}

	// Apply fade-out effect to text color
	video::SColor fade_color = m_initial_color;
	f32 d = m_fade_progress;
	fade_color.setAlpha(static_cast<u32>(
		fade_color.getAlpha() * (1.0f - d * d)));
	m_guitext_status->setOverrideColor(fade_color);
	m_guitext_status->enableOverrideColor(true);
}

void GUIStatusMessage::updatePosition()
{
	if (!m_guitext_status)
		return;

	v2u32 screensize = RenderingEngine::getWindowSize();

	s32 status_width = m_guitext_status->getTextWidth();
	s32 status_height = m_guitext_status->getTextHeight();

	if (m_use_custom_position) {
		// Use custom position (for main menu style - full width bar at bottom)
		if (m_center_horizontally) {
			// Full-width bar at the very bottom of the screen
			s32 bar_height = m_bar_height > 0 ? m_bar_height : (status_height + 12);
			s32 status_y = screensize.Y; // Position at very bottom

			// Position the element as a full-width bar
			m_guitext_status->setRelativePosition(core::rect<s32>(
				0,
				status_y - bar_height,
				(s32)screensize.X,
				status_y));
		} else {
			m_guitext_status->setRelativePosition(core::rect<s32>(
				m_custom_pos_x,
				m_custom_pos_y - status_height,
				m_custom_pos_x + status_width,
				m_custom_pos_y));
		}
	} else {
		// Default position (for in-game style)
		s32 status_y = screensize.Y - 150;
		s32 status_x = (screensize.X - status_width) / 2;

		m_guitext_status->setRelativePosition(core::rect<s32>(
			status_x,
			status_y - status_height,
			status_x + status_width,
			status_y));
	}
}
