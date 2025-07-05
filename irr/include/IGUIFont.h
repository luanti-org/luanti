// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "SColor.h"
#include "rect.h"
#include "irrString.h"

namespace gui
{

//! An enum for the different types of GUI font.
enum EGUI_FONT_TYPE
{
	//! Bitmap fonts loaded from an XML file or a texture.
	EGFT_BITMAP = 0,

	//! Scalable vector fonts loaded from an XML file.
	/** These fonts reside in system memory and use no video memory
	until they are displayed. These are slower than bitmap fonts
	but can be easily scaled and rotated. */
	EGFT_VECTOR,

	//! A font which uses a the native API provided by the operating system.
	/** Currently not used. */
	EGFT_OS,

	//! An external font type provided by the user.
	EGFT_CUSTOM
};

//! Font interface.
class IGUIFont : public virtual IReferenceCounted
{
public:
	//! Draws some text and clips it to the specified rectangle if wanted.
	/** \param text: Text to draw
	\param position: Rectangle specifying position where to draw the text.
	\param color: Color of the text
	\param hcenter: Specifies if the text should be centered horizontally into the rectangle.
	\param vcenter: Specifies if the text should be centered vertically into the rectangle.
	\param clip: Optional pointer to a rectangle against which the text will be clipped.
	If the pointer is null, no clipping will be done. */
	virtual void draw(const core::stringw &text, const core::rect<s32> &position,
			video::SColor color, bool hcenter = false, bool vcenter = false,
			const core::rect<s32> *clip = 0) = 0;

	//! Calculates the width and height of a given string of text.
	/** \return Returns width and height of the area covered by the text if
	it would be drawn. */
	virtual core::dimension2du getDimension(const wchar_t *text) const = 0;

	//! Calculates the index of the character in the text which is on a specific position.
	/** \param text: Text string.
	\param pixel_x: X pixel position of which the index of the character will be returned.
	\return Returns zero based index of the character in the text, and -1 if no no character
	is on this position. (=the text is too short). */
	virtual s32 getCharacterFromPos(const wchar_t *text, s32 pixel_x) const = 0;

	//! Returns the type of this font
	virtual EGUI_FONT_TYPE getType() const { return EGFT_CUSTOM; }

	//! Sets global kerning width for the font.
	virtual void setKerningWidth(s32 kerning) = 0;

	//! Sets global kerning height for the font.
	virtual void setKerningHeight(s32 kerning) = 0;

	//! Gets kerning values (distance between letters) for the font. If no parameters are provided,
	/** the global kerning distance is returned.
	\param thisLetter: If this parameter is provided, the left side kerning
	for this letter is added to the global kerning value. For example, a
	space might only be one pixel wide, but it may be displayed as several
	pixels.
	\param previousLetter: If provided, kerning is calculated for both
	letters and added to the global kerning value. For example, in a font
	which supports kerning pairs a string such as 'Wo' may have the 'o'
	tucked neatly under the 'W'.
	*/
	virtual core::vector2di getKerning(const wchar_t thisLetter = 0, const wchar_t previousLetter = 0) const = 0;

	//! Define which characters should not be drawn by the font.
	/** For example " " would not draw any space which is usually blank in
	most fonts.
	\param s String of symbols which are not send down to the videodriver
	*/
	virtual void setInvisibleCharacters(const wchar_t *s) = 0;
};

} // end namespace gui
