// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "IVideoDriver.h"
#include "SColor.h"
#include "rect.h"

namespace gui
{

//! Interpolate a color from a box's four corner colors.
//! Order: top-left, top-right, bottom-right, bottom-left.
inline video::SColor roundedRectColorAt(const std::array<video::SColor, 4> &colors,
		f32 fx, f32 fy)
{
	const video::SColor top = colors[0].getInterpolated(colors[1], fx);
	const video::SColor bottom = colors[3].getInterpolated(colors[2], fx);
	return top.getInterpolated(bottom, fy);
}

//! Horizontal inset of a rounded rectangle's edge for one pixel row.
//! Returns the number of pixels cut off on each side at row y.
inline s32 roundedRectRowInset(s32 radius, s32 y, s32 top, s32 bottom)
{
	if (radius <= 0)
		return 0;

	f32 dy;
	if (y < top + radius)
		dy = (f32)(top + radius) - ((f32)y + 0.5f);
	else if (y >= bottom - radius)
		dy = ((f32)y + 0.5f) - (f32)(bottom - radius);
	else
		return 0;

	const f32 rr = (f32)radius * (f32)radius;
	const f32 inset = (f32)radius - std::sqrt(std::max(0.0f, rr - dy * dy));
	return (s32)std::ceil(inset);
}

//! Draw a rectangle with rounded corners.
//! \param colors Corner colors in top-left, top-right, bottom-right,
//! bottom-left order. A uniform color may be supplied for all four entries.
//! \param round_top Whether the top corners are rounded.
//! \param round_bottom Whether the bottom corners are rounded.
inline void drawRoundedRect(video::IVideoDriver *driver, const core::rect<s32> &rect,
		s32 radius, const std::array<video::SColor, 4> &colors,
		const core::rect<s32> *clip = nullptr,
		bool round_top = true, bool round_bottom = true)
{
	if (rect.getWidth() <= 0 || rect.getHeight() <= 0)
		return;

	const s32 max_radius = std::min(rect.getWidth(), rect.getHeight()) / 2;
	radius = core::clamp(radius, 0, max_radius);

	if (radius <= 0 || (!round_top && !round_bottom)) {
		driver->draw2DRectangle(rect, colors[0], colors[1], colors[3], colors[2], clip);
		return;
	}

	const s32 x0 = rect.UpperLeftCorner.X;
	const s32 y0 = rect.UpperLeftCorner.Y;
	const s32 x1 = rect.LowerRightCorner.X;
	const s32 y1 = rect.LowerRightCorner.Y;
	const f32 inv_height = 1.0f / (f32)std::max(1, y1 - y0);

	auto draw_row = [&] (s32 y, s32 left, s32 right) {
		if (right <= left)
			return;
		const f32 fy = ((f32)y + 0.5f - (f32)y0) * inv_height;
		const video::SColor lc = roundedRectColorAt(colors, 0.0f, fy);
		const video::SColor rc = roundedRectColorAt(colors, 1.0f, fy);
		driver->draw2DRectangle(core::rect<s32>(left, y, right, y + 1),
				lc, rc, lc, rc, clip);
	};

	// Top rounded band
	if (round_top) {
		for (s32 y = y0; y < y0 + radius; ++y) {
			const s32 inset = roundedRectRowInset(radius, y, y0, y1);
			draw_row(y, x0 + inset, x1 - inset);
		}
	}

	// Central band (no rounding), drawn as one gradient rectangle
	const s32 mid_top = round_top ? y0 + radius : y0;
	const s32 mid_bottom = round_bottom ? y1 - radius : y1;
	if (mid_bottom > mid_top) {
		const f32 fy0 = ((f32)mid_top - (f32)y0) * inv_height;
		const f32 fy1 = ((f32)mid_bottom - (f32)y0) * inv_height;
		driver->draw2DRectangle(core::rect<s32>(x0, mid_top, x1, mid_bottom),
				roundedRectColorAt(colors, 0.0f, fy0),
				roundedRectColorAt(colors, 1.0f, fy0),
				roundedRectColorAt(colors, 0.0f, fy1),
				roundedRectColorAt(colors, 1.0f, fy1), clip);
	}

	// Bottom rounded band
	if (round_bottom) {
		for (s32 y = y1 - radius; y < y1; ++y) {
			const s32 inset = roundedRectRowInset(radius, y, y0, y1);
			draw_row(y, x0 + inset, x1 - inset);
		}
	}
}

//! Draw a rounded border ring between an outer and an inner rectangle.
//! \param colors Border colors in top, right, bottom, left order.
inline void drawRoundedRectBorder(video::IVideoDriver *driver,
		const core::rect<s32> &outer, const core::rect<s32> &inner,
		s32 radius, s32 inner_radius,
		const std::array<video::SColor, 4> &colors,
		const core::rect<s32> *clip = nullptr)
{
	if (outer.getWidth() <= 0 || outer.getHeight() <= 0)
		return;

	radius = core::clamp(radius, 0,
			std::min(outer.getWidth(), outer.getHeight()) / 2);
	inner_radius = core::clamp(inner_radius, 0,
			std::min(inner.getWidth(), inner.getHeight()) / 2);

	for (s32 y = outer.UpperLeftCorner.Y; y < outer.LowerRightCorner.Y; ++y) {
		const s32 outer_inset = roundedRectRowInset(radius, y,
				outer.UpperLeftCorner.Y, outer.LowerRightCorner.Y);
		const s32 ox0 = outer.UpperLeftCorner.X + outer_inset;
		const s32 ox1 = outer.LowerRightCorner.X - outer_inset;

		if (y < inner.UpperLeftCorner.Y || y >= inner.LowerRightCorner.Y) {
			// Above or below the inner rectangle: a solid top/bottom border.
			const video::SColor &color = y < inner.UpperLeftCorner.Y ?
					colors[0] : colors[2];
			if (ox1 > ox0)
				driver->draw2DRectangle(color,
						core::rect<s32>(ox0, y, ox1, y + 1), clip);
			continue;
		}

		const s32 inner_inset = roundedRectRowInset(inner_radius, y,
				inner.UpperLeftCorner.Y, inner.LowerRightCorner.Y);
		const s32 ix0 = inner.UpperLeftCorner.X + inner_inset;
		const s32 ix1 = inner.LowerRightCorner.X - inner_inset;

		if (ix0 > ox0)
			driver->draw2DRectangle(colors[3],
					core::rect<s32>(ox0, y, ix0, y + 1), clip);
		if (ox1 > ix1)
			driver->draw2DRectangle(colors[1],
					core::rect<s32>(ix1, y, ox1, y + 1), clip);
	}
}

} // namespace gui
