// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIStaticText.h"
#include "irrArray.h"

namespace gui
{
class CGUIStaticText : public IGUIStaticText
{
public:
	//! constructor
	CGUIStaticText(const wchar_t *text, bool border, IGUIEnvironment *environment,
			IGUIElement *parent, s32 id, const core::rect<s32> &rectangle,
			bool background = false);

	//! destructor
	virtual ~CGUIStaticText();

	//! draws the element and its children
	void draw() override;

	//! Sets another skin independent font.
	void setOverrideFont(IGUIFont *font = 0) override;

	//! Gets the override font (if any)
	IGUIFont *getOverrideFont() const override;

	//! Get the font which is used right now for drawing
	IGUIFont *getActiveFont() const override;

	//! Sets another color for the text.
	void setOverrideColor(video::SColor color) override;

	//! Sets another color for the background.
	void setBackgroundColor(video::SColor color) override;

	//! Sets whether to draw the background
	void setDrawBackground(bool draw) override;

	//! Gets the background color
	video::SColor getBackgroundColor() const override;

	//! Checks if background drawing is enabled
	bool isDrawBackgroundEnabled() const override;

	//! Sets whether to draw the border
	void setDrawBorder(bool draw) override;

	//! Checks if border drawing is enabled
	bool isDrawBorderEnabled() const override;

	//! Sets alignment mode for text
	void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical) override;

	//! Gets the override color
	video::SColor getOverrideColor() const override;

	//! Gets the currently used text color
	video::SColor getActiveColor() const override;

	//! Sets if the static text should use the override color or the
	//! color in the gui skin.
	void enableOverrideColor(bool enable) override;

	//! Checks if an override color is enabled
	bool isOverrideColorEnabled() const override;

	//! Set whether the text in this label should be clipped if it goes outside bounds
	void setTextRestrainedInside(bool restrainedInside) override;

	//! Checks if the text in this label should be clipped if it goes outside bounds
	bool isTextRestrainedInside() const override;

	//! Enables or disables word wrap for using the static text as
	//! multiline text control.
	void setWordWrap(bool enable) override;

	//! Checks if word wrap is enabled
	bool isWordWrapEnabled() const override;

	//! Sets the new caption of this element.
	void setText(const wchar_t *text) override;

	//! Returns the height of the text in pixels when it is drawn.
	s32 getTextHeight() const override;

	//! Returns the width of the current text, in the current font
	s32 getTextWidth() const override;

	//! Updates the absolute position, splits text if word wrap is enabled
	void updateAbsolutePosition() override;

	//! Set whether the string should be interpreted as right-to-left (RTL) text
	/** \note This component does not implement the Unicode bidi standard, the
	text of the component should be already RTL if you call this. The
	main difference when RTL is enabled is that the linebreaks for multiline
	elements are performed starting from the end.
	*/
	void setRightToLeft(bool rtl) override;

	//! Checks if the text should be interpreted as right-to-left text
	bool isRightToLeft() const override;

private:
	//! Breaks the single text line.
	void breakText();

	EGUI_ALIGNMENT HAlign, VAlign;
	bool Border;
	bool OverrideColorEnabled;
	bool OverrideBGColorEnabled;
	bool WordWrap;
	bool Background;
	bool RestrainTextInside;
	bool RightToLeft;

	video::SColor OverrideColor, BGColor;
	gui::IGUIFont *OverrideFont;
	gui::IGUIFont *LastBreakFont; // stored because: if skin changes, line break must be recalculated.

	core::array<core::stringw> BrokenText;
};

} // end namespace gui
