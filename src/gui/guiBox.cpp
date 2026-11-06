// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiBox.h"
#include <algorithm>
#include <cstdlib>
#include <IVideoDriver.h>
#include "irr_v2d.h"
#include "guiRoundedRect.h"

GUIBox::GUIBox(gui::IGUIEnvironment *env, gui::IGUIElement *parent, s32 id,
	const core::rect<s32> &rectangle,
	const std::array<video::SColor, 4> &colors,
	const std::array<video::SColor, 4> &bordercolors,
	const std::array<s32, 4> &borderwidths,
	s32 borderradius) :
	gui::IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, rectangle),
	m_colors(colors),
	m_bordercolors(bordercolors),
	m_borderwidths(borderwidths),
	m_borderradius(borderradius)
{
}

void GUIBox::draw()
{
	if (!IsVisible)
		return;

	std::array<s32, 4> negative_borders = {0, 0, 0, 0};
	std::array<s32, 4> positive_borders = {0, 0, 0, 0};

	for (size_t i = 0; i <= 3; i++) {
		if (m_borderwidths[i] > 0)
			positive_borders[i] = m_borderwidths[i];
		else
			negative_borders[i] = m_borderwidths[i];
	}

	v2s32 upperleft = AbsoluteRect.UpperLeftCorner;
	v2s32 lowerright = AbsoluteRect.LowerRightCorner;

	v2s32 topleft_border = {
		upperleft.X - positive_borders[3],
		upperleft.Y - positive_borders[0]
	};
	v2s32 topleft_rect = {
		upperleft.X - negative_borders[3],
		upperleft.Y - negative_borders[0]
	};

	v2s32 lowerright_border = {
		lowerright.X + positive_borders[1],
		lowerright.Y + positive_borders[2]
	};
	v2s32 lowerright_rect = {
		lowerright.X + negative_borders[1],
		lowerright.Y + negative_borders[2]
	};

	core::rect<s32> main_rect(
		topleft_rect.X,
		topleft_rect.Y,
		lowerright_rect.X,
		lowerright_rect.Y
	);

	std::array<core::rect<s32>, 4> border_rects;

	border_rects[0] = core::rect<s32>(
		topleft_border.X,
		topleft_border.Y,
		lowerright_border.X,
		topleft_rect.Y
	);

	border_rects[1] = core::rect<s32>(
		lowerright_rect.X,
		topleft_rect.Y,
		lowerright_border.X,
		lowerright_rect.Y
	);

	border_rects[2] = core::rect<s32>(
		topleft_border.X,
		lowerright_rect.Y,
		lowerright_border.X,
		lowerright_border.Y
	);

	border_rects[3] = core::rect<s32>(
		topleft_border.X,
		topleft_rect.Y,
		topleft_rect.X,
		lowerright_rect.Y
	);

	video::IVideoDriver *driver = Environment->getVideoDriver();

	// The border rectangle can be larger than 'AbsoluteClippingRect',
	// hence clip against the (generally larger) parent.
	core::rect<s32> border_rect = core::rect<s32>(
		topleft_border.X,
		topleft_border.Y,
		lowerright_border.X,
		lowerright_border.Y
	);
	if (!isNotClipped()) {
		border_rect.clipAgainst(Parent->getAbsoluteClippingRect());
	}

	if (m_borderradius > 0) {
		const core::rect<s32> inner_rect = main_rect;
		const s32 max_radius = std::min(main_rect.getWidth(), main_rect.getHeight()) / 2;
		const s32 radius = core::clamp(m_borderradius, 0, max_radius);

		s32 max_border = 0;
		bool has_border = false;
		for (size_t i = 0; i <= 3; i++) {
			if (m_borderwidths[i] != 0)
				has_border = true;
			max_border = std::max(max_border, std::abs(m_borderwidths[i]));
		}

		if (has_border && border_rect.getArea() > 0) {
			const s32 inner_radius = std::max(0, radius - max_border);
			gui::drawRoundedRectBorder(driver, border_rect, inner_rect,
					radius, inner_radius, m_bordercolors, &border_rect);
		}

		gui::drawRoundedRect(driver, inner_rect, core::clamp(radius - max_border,
				0, std::min(inner_rect.getWidth(), inner_rect.getHeight()) / 2),
				m_colors, &AbsoluteClippingRect);
	} else {
		driver->draw2DRectangle(main_rect, m_colors[0], m_colors[1], m_colors[3],
			m_colors[2], &AbsoluteClippingRect);

		for (size_t i = 0; i <= 3; i++)
			driver->draw2DRectangle(m_bordercolors[i], border_rects[i],
					&border_rect);
	}

	IGUIElement::draw();
}
