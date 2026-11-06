// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include <algorithm>
#include <array>

#include "irrRoundedRect.h"

namespace gui
{

//! Draw a rounded version of the default bevelled button pane.
//! \param colors Skin colors in dark shadow, shadow, face, high light order.
inline void drawRoundedButtonPane(video::IVideoDriver *driver,
		const core::rect<s32> &rect, s32 radius,
		const std::array<video::SColor, 4> &colors, bool pressed,
		const core::rect<s32> *clip = nullptr)
{
	if (rect.getWidth() <= 0 || rect.getHeight() <= 0)
		return;

	const s32 max_radius = std::min(rect.getWidth(), rect.getHeight()) / 2;
	radius = core::clamp(radius, 0, max_radius);

	std::array<video::SColor, 4> layer;
	auto draw_layer = [&] (const core::rect<s32> &r, s32 layer_radius,
			const video::SColor &color) {
		layer = {color, color, color, color};
		drawRoundedRect(driver, r, layer_radius, layer, clip);
	};

	core::rect<s32> r = rect;
	if (!pressed) {
		draw_layer(r, radius, colors[0]);
		r.LowerRightCorner.X -= 1;
		r.LowerRightCorner.Y -= 1;
		draw_layer(r, radius - 1, colors[3]);
		r.UpperLeftCorner.X += 1;
		r.UpperLeftCorner.Y += 1;
		draw_layer(r, radius - 1, colors[1]);
		r.LowerRightCorner.X -= 1;
		r.LowerRightCorner.Y -= 1;
		draw_layer(r, radius - 2, colors[2]);
	} else {
		draw_layer(r, radius, colors[3]);
		r.LowerRightCorner.X -= 1;
		r.LowerRightCorner.Y -= 1;
		draw_layer(r, radius - 1, colors[0]);
		r.UpperLeftCorner.X += 1;
		r.UpperLeftCorner.Y += 1;
		draw_layer(r, radius - 1, colors[1]);
		r.UpperLeftCorner.X += 1;
		r.UpperLeftCorner.Y += 1;
		draw_layer(r, radius - 2, colors[2]);
	}
}

} // namespace gui
