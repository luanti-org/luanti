// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Krock/SmallJoker <mk939@ymail.com>

#include "guiSkin.h"
#include "client/guiscalingfilter.h"


GUISkin::GUISkin(video::IVideoDriver *driver) : gui::CGUISkin(driver)
{
}


GUISkin::~GUISkin()
{
}

void GUISkin::drawColored3DButtonPanePressed(gui::IGUIElement *element,
		const core::rect<s32> &rect,
		const core::rect<s32> *clip,
		const video::SColor *colors)
{
	if (!Driver)
		return;

	if (tryDrawPane("_skin_button", StyleSpec::STATE_PRESSED, rect, clip))
		return;

	gui::CGUISkin::drawColored3DButtonPanePressed(element, rect, clip, colors);
}

void GUISkin::drawColored3DButtonPaneStandard(gui::IGUIElement *element,
		const core::rect<s32> &rect,
		const core::rect<s32> *clip,
		const video::SColor *colors)
{
	if (!Driver)
		return;

	if (tryDrawPane("_skin_button", StyleSpec::STATE_DEFAULT, rect, clip))
		return;

	gui::CGUISkin::drawColored3DButtonPaneStandard(element, rect, clip, colors);
}

bool GUISkin::tryDrawPane(const char *type, StyleSpec::State state,
		const core::rect<s32> &rect,
		const core::rect<s32> *clip)
{
	auto it = m_theme.find(type);
	if (it == m_theme.end())
		return false;

	video::SColor c = 0xFFFFFFFF;
	video::SColor image_colors[] = { c, c, c, c };

	// Similar to GUIFormSpecMenu::getStyleForElement
	StyleSpec::StateMap states;
	for (const StyleSpec &spec : it->second)
		states[(u32)spec.getState()] |= spec;

	StyleSpec style = StyleSpec::getStyleFromStatePropagation(states, state);
	video::ITexture *texture = style.getTexture(StyleSpec::BGIMG, m_texture_source);
	core::recti source_rect = core::rect<s32>(core::position2di(0,0), texture->getOriginalSize());

	core::recti bg_middle = style.getRect(StyleSpec::BGIMG_MIDDLE, core::recti());
	if (bg_middle.getArea() == 0) {
		Driver->draw2DImage(texture, rect, source_rect, clip, image_colors, true);
	} else {
		draw2DImage9Slice(Driver, texture, rect, source_rect, bg_middle, clip, image_colors);
	}

	return true;
}


