// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "statusTextHelper.h"

#include <irrlicht_changes/static_text.h>

#include "client/renderingengine.h"

StatusTextHelper::StatusTextHelper(gui::IGUIEnvironment *guienv, gui::IGUIElement *parent)
{
	if (!guienv)
		return;

	gui::IGUIElement *root = parent ? parent : guienv->getRootGUIElement();

	m_guitext_status = grab(gui::StaticText::add(guienv, L"",
			core::recti(), false, false, root));
	m_guitext_status->setVisible(false);

	// Initialize text color from skin
	if (guienv->getSkin())
		m_text_color = guienv->getSkin()->getColor(gui::EGDC_BUTTON_TEXT);
	else
		m_text_color = video::SColor(255, 0, 0, 0);
}

StatusTextHelper::~StatusTextHelper()
{
	if (m_guitext_status) {
		m_guitext_status->remove();
		m_guitext_status.reset();
	}
}

void StatusTextHelper::showStatusText(const std::wstring &str)
{
	m_statustext = str;
	m_statustext_time = 0.0f;
	m_fade_progress = 0.0f;
}

void StatusTextHelper::clearStatusText()
{
	m_statustext.clear();
	m_statustext_time = 0.0f;
	m_fade_progress = 0.0f;
	if (m_guitext_status)
		m_guitext_status->setVisible(false);
}

void StatusTextHelper::setVisible(bool visible)
{
	if (m_guitext_status)
		m_guitext_status->setVisible(visible);
}

bool StatusTextHelper::isVisible() const
{
	return m_guitext_status && m_guitext_status->isVisible();
}

void StatusTextHelper::setGameStyle()
{
	m_display_duration = 1.5f;
	m_background_enabled = false;
	m_use_main_menu_position = false;
	// in-game: top-anchored vertically
	m_text_alignment_v = gui::EGUIA_UPPERLEFT;
}

void StatusTextHelper::setMainMenuStyle()
{
	m_display_duration = 3.0f;
	m_background_color = video::SColor(220, 0, 0, 0);
	m_background_enabled = true;
	m_use_main_menu_position = true;
	// main menu: centered vertically
	m_text_alignment_v = gui::EGUIA_CENTER;
}

void StatusTextHelper::update(float dtime)
{
	if (!m_guitext_status || m_statustext.empty())
		return;

	m_statustext_time += dtime;

	if (m_statustext_time >= m_display_duration) {
		clearStatusText();
		return;
	}

	m_fade_progress = m_statustext_time / m_display_duration;

	// update() is the sole owner of the GUI element's visual state.
	// showStatusText() only resets the timers; all setText / color /
	// position work happens here.
	m_guitext_status->setText(m_statustext.c_str());
	m_guitext_status->setVisible(true);
	m_guitext_status->setTextAlignment(gui::EGUIA_CENTER, m_text_alignment_v);

	updatePosition();

	// Quadratic fade feels a bit smoother than linear.
	const f32 alpha_factor = 1.0f - m_fade_progress * m_fade_progress;

	// Background (optional)
	if (m_background_enabled) {
		video::SColor bg_fade = m_background_color;
		bg_fade.setAlpha(static_cast<u32>(bg_fade.getAlpha() * alpha_factor));
		m_guitext_status->setBackgroundColor(bg_fade);
		m_guitext_status->setDrawBackground(true);
	} else {
		m_guitext_status->setDrawBackground(false);
	}

	// Text color fade
	video::SColor text_fade = m_text_color;
	text_fade.setAlpha(static_cast<u32>(text_fade.getAlpha() * alpha_factor));
	m_guitext_status->setOverrideColor(text_fade);
	m_guitext_status->enableOverrideColor(true);
}

void StatusTextHelper::updatePosition()
{
	if (!m_guitext_status)
		return;

	v2u32 screensize = RenderingEngine::getWindowSize();
	s32 text_width = m_guitext_status->getTextWidth();
	s32 text_height = m_guitext_status->getTextHeight();

	if (m_use_main_menu_position) {
		// Full-width bar at bottom (main menu style)
		const s32 bar_height = MAIN_MENU_BAR_HEIGHT;
		m_guitext_status->setRelativePosition(core::rect<s32>(
				0,
				(s32)screensize.Y - bar_height,
				(s32)screensize.X,
				(s32)screensize.Y));
	} else {
		// Centered above bottom (game style)
		const s32 status_y = (s32)screensize.Y - 150;
		const s32 status_x = ((s32)screensize.X - text_width) / 2;
		m_guitext_status->setRelativePosition(core::rect<s32>(
				status_x,
				status_y - text_height,
				status_x + text_width,
				status_y));
	}
}
