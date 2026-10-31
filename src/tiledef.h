// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <string>
#include "tileanimation.h"

enum AlignStyle : u8 {
	ALIGN_STYLE_NODE,
	ALIGN_STYLE_WORLD,
	ALIGN_STYLE_USER_DEFINED,
	AlignStyle_END // Dummy for validity check
};

enum AlphaMode : u8 {
	ALPHAMODE_BLEND,
	ALPHAMODE_CLIP,
	ALPHAMODE_OPAQUE,
	ALPHAMODE_LEGACY_COMPAT, /* only sent by old servers, equals OPAQUE */
	AlphaMode_END // Dummy for validity check
};

/*
	Stand-alone definition of a TileSpec (basically a server-side TileSpec)
*/

struct TileDef
{
	std::string name = "";
	bool backface_culling = true; // Takes effect only in special cases
	bool tileable_horizontal = true;
	bool tileable_vertical = true;
	//! If true, the tile has its own color.
	bool has_color = false;
	//! The color of the tile.
	video::SColor color = video::SColor(0xFFFFFFFF);
	AlignStyle align_style = ALIGN_STYLE_NODE;
	u8 scale = 0;

	struct TileAnimationParams animation;

	TileDef()
	{
		animation.type = TAT_NONE;
	}

	void serialize(std::ostream &os, u16 protocol_version) const;
	void deSerialize(std::istream &is, u16 protocol_version);
};

