// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIFont.h"

namespace gui
{
class IGUISpriteBank;

//! Font interface.
class IGUIFontBitmap : public IGUIFont
{
public:
	//! Returns the type of this font
	EGUI_FONT_TYPE getType() const override { return EGFT_BITMAP; }

	//! returns the parsed Symbol Information
	virtual IGUISpriteBank *getSpriteBank() const = 0;

	//! returns the sprite number from a given character
	virtual u32 getSpriteNoFromChar(const wchar_t *c) const = 0;
};

} // end namespace gui
