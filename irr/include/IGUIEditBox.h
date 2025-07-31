// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIElement.h"
#include "SColor.h"

namespace gui
{
class IGUIFont;

//! Single line edit box for editing simple text.
/** \par This element can create the following events of type EGUI_EVENT_TYPE:
\li EGET_EDITBOX_ENTER
\li EGET_EDITBOX_CHANGED
\li EGET_EDITBOX_MARKING_CHANGED
*/
class IGUIEditBox : public IGUIElement
{
public:
	//! constructor
	IGUIEditBox(IGUIEnvironment *environment, IGUIElement *parent, s32 id, core::rect<s32> rectangle) :
			IGUIElement(EGUIET_EDIT_BOX, environment, parent, id, rectangle) {}

	//! Sets another skin independent font.
	/** If this is set to zero, the button uses the font of the skin.
	\param font: New font to set. */
	virtual void setOverrideFont(IGUIFont *font = 0) = 0;

	//! Gets the override font (if any)
	/** \return The override font (may be 0) */
	virtual IGUIFont *getOverrideFont() const = 0;

	//! Get the font which is used right now for drawing
	/** Currently this is the override font when one is set and the
	font of the active skin otherwise */
	virtual IGUIFont *getActiveFont() const = 0;

	//! Sets another color for the text.
	/** If set, the edit box does not use the EGDC_BUTTON_TEXT color defined
	in the skin, but the set color instead. You don't need to call
	IGUIEditBox::enableOverrrideColor(true) after this, this is done
	by this function.
	If you set a color, and you want the text displayed with the color
	of the skin again, call IGUIEditBox::enableOverrideColor(false);
	\param color: New color of the text. */
	virtual void setOverrideColor(video::SColor color) = 0;

	//! Gets the override color
	virtual video::SColor getOverrideColor() const = 0;

	//! Sets if the text should use the override color or the color in the gui skin.
	/** \param enable: If set to true, the override color, which can be set
	with IGUIEditBox::setOverrideColor is used, otherwise the
	EGDC_BUTTON_TEXT color of the skin. */
	virtual void enableOverrideColor(bool enable) = 0;

	//! Checks if an override color is enabled
	/** \return true if the override color is enabled, false otherwise */
	virtual bool isOverrideColorEnabled(void) const = 0;

	//! Sets whether to draw the background
	virtual void setDrawBackground(bool draw) = 0;

	//! Checks if background drawing is enabled
	/** \return true if background drawing is enabled, false otherwise */
	virtual bool isDrawBackgroundEnabled() const = 0;

	//! Turns the border on or off
	/** \param border: true if you want the border to be drawn, false if not */
	virtual void setDrawBorder(bool border) = 0;

	//! Checks if border drawing is enabled
	/** \return true if border drawing is enabled, false otherwise */
	virtual bool isDrawBorderEnabled() const = 0;

	//! Sets text justification mode
	/** \param horizontal: EGUIA_UPPERLEFT for left justified (default),
	EGUIA_LOWERRIGHT for right justified, or EGUIA_CENTER for centered text.
	\param vertical: EGUIA_UPPERLEFT to align with top edge,
	EGUIA_LOWERRIGHT for bottom edge, or EGUIA_CENTER for centered text (default). */
	virtual void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical) = 0;

	//! Enables or disables word wrap.
	/** \param enable: If set to true, words going over one line are
	broken to the next line. */
	virtual void setWordWrap(bool enable) = 0;

	//! Checks if word wrap is enabled
	/** \return true if word wrap is enabled, false otherwise */
	virtual bool isWordWrapEnabled() const = 0;

	//! Enables or disables newlines.
	/** \param enable: If set to true, the EGET_EDITBOX_ENTER event will not be fired,
	instead a newline character will be inserted. */
	virtual void setMultiLine(bool enable) = 0;

	//! Checks if multi line editing is enabled
	/** \return true if multi-line is enabled, false otherwise */
	virtual bool isMultiLineEnabled() const = 0;

	//! Enables or disables automatic scrolling with cursor position
	/** \param enable: If set to true, the text will move around with the cursor position */
	virtual void setAutoScroll(bool enable) = 0;

	//! Checks to see if automatic scrolling is enabled
	/** \return true if automatic scrolling is enabled, false if not */
	virtual bool isAutoScrollEnabled() const = 0;

	//! Sets whether the edit box is a password box. Setting this to true will
	/** disable MultiLine, WordWrap and the ability to copy with ctrl+c or ctrl+x
	\param passwordBox: true to enable password, false to disable
	\param passwordChar: the character that is displayed instead of letters */
	virtual void setPasswordBox(bool passwordBox, wchar_t passwordChar = L'*') = 0;

	//! Returns true if the edit box is currently a password box.
	virtual bool isPasswordBox() const = 0;

	//! Gets the size area of the text in the edit box
	/** \return The size in pixels of the text */
	virtual core::dimension2du getTextDimension() = 0;

	//! Sets the maximum amount of characters which may be entered in the box.
	/** \param max: Maximum amount of characters. If 0, the character amount is
	infinity. */
	virtual void setMax(u32 max) = 0;

	//! Returns maximum amount of characters, previously set by setMax();
	virtual u32 getMax() const = 0;

	//! Set the character used for the cursor.
	/** By default it's "_" */
	virtual void setCursorChar(const wchar_t cursorChar) = 0;

	//! Get the character used for the cursor.
	virtual wchar_t getCursorChar() const = 0;

	//! Set the blinktime for the cursor. 2x blinktime is one full cycle.
	//** \param timeMs Blinktime in milliseconds. When set to 0 the cursor is constantly on without blinking */
	virtual void setCursorBlinkTime(u32 timeMs) = 0;

	//! Get the cursor blinktime
	virtual u32 getCursorBlinkTime() const = 0;
};

} // end namespace gui
