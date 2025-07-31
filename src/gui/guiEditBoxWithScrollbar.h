// Copyright (C) 2002-2012 Nikolaus Gebhardt, Modified by Mustapha Tachouct
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef GUIEDITBOXWITHSCROLLBAR_HEADER
#define GUIEDITBOXWITHSCROLLBAR_HEADER

#include "guiEditBox.h"

class ISimpleTextureSource;

class GUIEditBoxWithScrollBar : public GUIEditBox
{
public:

	//! constructor
	GUIEditBoxWithScrollBar(const wchar_t* text, bool border, IGUIEnvironment* environment,
		IGUIElement* parent, s32 id, const core::rect<s32>& rectangle,
		ISimpleTextureSource *tsrc, bool writable = true, bool has_vscrollbar = true);

	//! destructor
	virtual ~GUIEditBoxWithScrollBar() {}

	//! Sets whether to draw the background
	virtual void setDrawBackground(bool draw);

	//! draws the element and its children
	virtual void draw();

	//! Updates the absolute position, splits text if required
	virtual void updateAbsolutePosition();

	//! Change the background color
	virtual void setBackgroundColor(const video::SColor &bg_color);

	virtual bool isDrawBackgroundEnabled() const;
	virtual bool isDrawBorderEnabled() const;
	virtual void setCursorChar(const wchar_t cursorChar);
	virtual wchar_t getCursorChar() const;
	virtual void setCursorBlinkTime(u32 timeMs);
	virtual u32 getCursorBlinkTime() const;

protected:
	//! Breaks the single text line.
	virtual void breakText();
	//! sets the area of the given line
	virtual void setTextRect(s32 line);
	//! calculates the current scroll position
	void calculateScrollPos();
	//! calculated the FrameRect
	void calculateFrameRect();
	//! create a Vertical ScrollBar
	void createVScrollBar();

	s32 getCursorPos(s32 x, s32 y);

	bool m_background;

	bool m_bg_color_used;
	video::SColor m_bg_color;

	ISimpleTextureSource *m_tsrc;
};


#endif // GUIEDITBOXWITHSCROLLBAR_HEADER

