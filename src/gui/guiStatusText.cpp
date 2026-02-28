// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiStatusText.h"
#include <irrlicht_changes/static_text.h>
#include "client/renderingengine.h"

GUIStatusText::GUIStatusText(gui::IGUIEnvironment *guienv, gui::IGUIElement *parent)
{
	if (!guienv)
		return;

	gui::IGUIElement *root = parent ? parent : guienv->getRootGUIElement();

	m_guitext_status = gui::StaticText::add(guienv, L"",
		core::recti(), false, false, root);
	m_guitext_status->setVisible(false);

	// Initialize text color from skin
	if (guienv->getSkin())
		m_text_color = guienv->getSkin()->getColor(gui::EGDC_BUTTON_TEXT);
	else
		m_text_color = video::SColor(255, 0, 0, 0);
}

GUIStatusText::~GUIStatusText()
{
	if (m_guitext_status) {
		m_guitext_status->remove();
		m_guitext_status = nullptr;
	}
}

void GUIStatusText::showStatusText(const std::wstring &str)
{
	m_statustext = str;
	m_statustext_time = 0.0f;
	m_fade_progress = 0.0f;
}

void GUIStatusText::clearStatusText()
{
	m_statustext.clear();
	m_statustext_time = 0.0f;
	m_fade_progress = 0.0f;
	if (m_guitext_status)
		m_guitext_status->setVisible(false);
}

void GUIStatusText::setVisible(bool visible)
{
	if (m_guitext_status)
		m_guitext_status->setVisible(visible);
}

bool GUIStatusText::isVisible() const
{
	return m_guitext_status && m_guitext_status->isVisible();
}

void GUIStatusText::setGameStyle()
{
	m_display_duration = 1.5f;
	m_text_color = video::SColor(255, 255, 255, 255);
	m_background_enabled = false;
	m_use_custom_position = false;
	m_text_alignment = gui::EGUIA_CENTER;
}

void GUIStatusText::setMainMenuStyle()
{
	m_display_duration = 3.0f;
	m_text_color = video::SColor(255, 255, 255, 255);
	m_background_color = video::SColor(220, 0, 0, 0);
	m_background_enabled = true;
	m_use_custom_position = true;
	m_custom_pos_x = 0;
	m_custom_pos_y = 0;
	m_center_horizontally = true;
	m_bar_height = 40;
	m_text_alignment = gui::EGUIA_CENTER;
}

void GUIStatusText::update(float dtime)
{
	if (!m_guitext_status || m_statustext.empty())
		return;

	m_statustext_time += dtime;

	if (m_statustext_time >= m_display_duration) {
		clearStatusText();
		return;
	}

	m_fade_progress = m_statustext_time / m_display_duration;

	m_guitext_status->setText(m_statustext.c_str());
	m_guitext_status->setVisible(true);
	m_guitext_status->setTextAlignment(m_text_alignment, m_text_alignment);

	updatePosition();

	// Apply background with fade
	if (m_background_enabled) {
		video::SColor bg_fade = m_background_color;
		f32 alpha_factor = 1.0f - m_fade_progress * m_fade_progress;
		bg_fade.setAlpha(static_cast<u32>(bg_fade.getAlpha() * alpha_factor));
		m_guitext_status->setBackgroundColor(bg_fade);
		m_guitext_status->setDrawBackground(true);
	}

	// Apply text color with fade
	video::SColor text_fade = m_text_color;
	f32 alpha_factor = 1.0f - m_fade_progress * m_fade_progress;
	text_fade.setAlpha(static_cast<u32>(text_fade.getAlpha() * alpha_factor));
	m_guitext_status->setOverrideColor(text_fade);
	m_guitext_status->enableOverrideColor(true);
}

void GUIStatusText::updatePosition()
{
	if (!m_guitext_status)
		return;

	v2u32 screensize = RenderingEngine::getWindowSize();
	s32 text_width = m_guitext_status->getTextWidth();
	s32 text_height = m_guitext_status->getTextHeight();

	if (m_use_custom_position && m_center_horizontally) {
		// Full-width bar at bottom (main menu style)
		s32 bar_height = m_bar_height > 0 ? m_bar_height : (text_height + 12);
		m_guitext_status->setRelativePosition(core::rect<s32>(
			0,
			screensize.Y - bar_height,
			(s32)screensize.X,
			screensize.Y));
	} else {
		// Centered above bottom (game style)
		s32 status_y = screensize.Y - 150;
		s32 status_x = (screensize.X - text_width) / 2;
		m_guitext_status->setRelativePosition(core::rect<s32>(
			status_x,
			status_y - text_height,
			status_x + text_width,
			status_y));
	}
}

