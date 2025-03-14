// SPDX-FileCopyrightText: 2024 Minetest
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irr_v2d.h"
#include "irr_v3d.h"
#include <SColor.h>
#include <vector>

#ifndef SERVER

namespace irr::video { class IVideoDriver; }
#endif // !def(SERVER)

/**
 * A polygon, stored as closed polyonal chain (ordered vertex list).
 * The functions assume it's convex.
 */
struct ConvexPolygon
{
	std::vector<v2f> vertices;

#ifndef SERVER

	/** Draws the polygon over the whole screen.
	*
	* X coordinates go from 0 (left) to 1 (right), Y from 0 (up) to 1 (down).
	*/
	void draw(video::IVideoDriver *driver, const video::SColor &color) const;

#endif // !def(SERVER)

	/** Calculates the area.
	 *
	* @return The area.
	*/
	[[nodiscard]]
	f32 area() const;

	/** Clips away the parts of the polygon that are on the wrong side of the
	* given clip line.
	* @param clip_line The homogeneous coordinates of an oriented 2D line. Clips
	*                  away all points p for which dot(clip_line, p) < 0 holds.
	* @return The clipped polygon.
	*/
	[[nodiscard]]
	ConvexPolygon clip(v3f clip_line) const;
};
