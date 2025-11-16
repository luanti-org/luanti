// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIElement.h"
#include "SColor.h"

namespace gui
{
class IGUIFont;

//! Multi or single line text label.
class IGUIStaticText : public IGUIElement
{
public:
	//! constructor
	IGUIStaticText(IGUIEnvironment *environment, IGUIElement *parent, s32 id, core::rect<s32> rectangle) :
			IGUIElement(EGUIET_STATIC_TEXT, environment, parent, id, rectangle) {}

	//! Sets another skin independent font.
	/** If this is set to zero, the button uses the font of the skin.
	\param font: New font to set. */
	virtual void setOverrideFont(IGUIFont *font = 0) = 0;

	//! Gets the override font (if any)
	/** \return The override font (may be 0) */
	virtual IGUIFont *getOverrideFont(void) const = 0;

	//! Get the font which is used right now for drawing
	/** Currently this is the override font when one is set and the
	font of the active skin otherwise */
	virtual IGUIFont *getActiveFont() const = 0;

	//! Sets another color for the text.
	/** If set, the static text does not use the EGDC_BUTTON_TEXT color defined
	in the skin, but the set color instead. You don't need to call
	IGUIStaticText::enableOverrrideColor(true) after this, this is done
	by this function.
	If you set a color, and you want the text displayed with the color
	of the skin again, call IGUIStaticText::enableOverrideColor(false);
	\param color: New color of the text. */
	virtual void setOverrideColor(video::SColor color) = 0;

	//! Gets the override color
	/** \return: The override color */
	virtual video::SColor getOverrideColor(void) const = 0;

	//! Gets the currently used text color
	/** Either a skin-color for the current state or the override color */
	virtual video::SColor getActiveColor() const = 0;

	//! Sets if the static text should use the override color or the color in the gui skin.
	/** \param enable: If set to true, the override color, which can be set
	with IGUIStaticText::setOverrideColor is used, otherwise the
	EGDC_BUTTON_TEXT color of the skin. */
	virtual void enableOverrideColor(bool enable) = 0;

	//! Checks if an override color is enabled
	/** \return true if the override color is enabled, false otherwise */
	virtual bool isOverrideColorEnabled(void) const = 0;

	//! Sets another color for the background.
	virtual void setBackgroundColor(video::SColor color) = 0;

	//! Sets whether to draw the background
	virtual void setDrawBackground(bool draw) = 0;

	//! Checks if background drawing is enabled
	/** \return true if background drawing is enabled, false otherwise */
	virtual bool isDrawBackgroundEnabled() const = 0;

	//! Gets the background color
	/** \return: The background color */
	virtual video::SColor getBackgroundColor() const = 0;

	//! Sets whether to draw the border
	virtual void setDrawBorder(bool draw) = 0;

	//! Checks if border drawing is enabled
	/** \return true if border drawing is enabled, false otherwise */
	virtual bool isDrawBorderEnabled() const = 0;

	//! Sets text justification mode
	/** \param horizontal: EGUIA_UPPERLEFT for left justified (default),
	EGUIA_LOWERRIGHT for right justified, or EGUIA_CENTER for centered text.
	\param vertical: EGUIA_UPPERLEFT to align with top edge (default),
	EGUIA_LOWERRIGHT for bottom edge, or EGUIA_CENTER for centered text. */
	virtual void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical) = 0;

	//! Enables or disables word wrap for using the static text as multiline text control.
	/** \param enable: If set to true, words going over one line are
	broken on to the next line. */
	virtual void setWordWrap(bool enable) = 0;

	//! Checks if word wrap is enabled
	/** \return true if word wrap is enabled, false otherwise */
	virtual bool isWordWrapEnabled(void) const = 0;

	//! Returns the height of the text in pixels when it is drawn.
	/** This is useful for adjusting the layout of gui elements based on the height
	of the multiline text in this element.
	\return Height of text in pixels. */
	virtual s32 getTextHeight() const = 0;

	//! Returns the width of the current text, in the current font
	/** If the text is broken, this returns the width of the widest line
	\return The width of the text, or the widest broken line. */
	virtual s32 getTextWidth(void) const = 0;

	//! Set whether the text in this label should be clipped if it goes outside bounds
	virtual void setTextRestrainedInside(bool restrainedInside) = 0;

	//! Checks if the text in this label should be clipped if it goes outside bounds
	virtual bool isTextRestrainedInside() const = 0;

	//! Set whether the string should be interpreted as right-to-left (RTL) text
	/** \note This component does not implement the Unicode bidi standard, the
	text of the component should be already RTL if you call this. The
	main difference when RTL is enabled is that the linebreaks for multiline
	elements are performed starting from the end.
	*/
	virtual void setRightToLeft(bool rtl) = 0;

	//! Checks whether the text in this element should be interpreted as right-to-left
	virtual bool isRightToLeft() const = 0;
};

} // end namespace gui
