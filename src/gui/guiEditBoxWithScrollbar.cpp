// Copyright (C) 2002-2012 Nikolaus Gebhardt
// Modified by Mustapha T.
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "guiEditBoxWithScrollbar.h"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "rect.h"

#include "guiScrollBar.h"
#include "porting.h"
#include "Keycodes.h"

using namespace gui;

//! constructor
GUIEditBoxWithScrollBar::GUIEditBoxWithScrollBar(const wchar_t* text, bool border,
	IGUIEnvironment* environment, IGUIElement* parent, s32 id,
	const core::rect<s32>& rectangle, ISimpleTextureSource *tsrc,
	bool writable, bool has_vscrollbar)
	: CGUIEditBox(text, border, environment, parent, id, rectangle),
	m_bg_color_used(false), m_tsrc(tsrc)
{
	if (has_vscrollbar) {
		createVScrollBar();

		calculateFrameRect();
		breakText();

		calculateScrollPos();
	}
	setWritable(writable);
}

//! draws the element and its children
void GUIEditBoxWithScrollBar::draw()
{
	if (!IsVisible)
		return;

	IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return;

	video::SColor default_bg_color;

	if (m_bg_color_used) {
		OverrideBgColor = m_bg_color;
	} else if (IsWritable) {
		OverrideBgColor = skin->getColor(EGDC_WINDOW);
	} else {
		// Transparent
		OverrideBgColor = 0x00000001;
	}

	CGUIEditBox::draw();
}

//! create a vertical scroll bar
void GUIEditBoxWithScrollBar::createVScrollBar()
{
	IGUISkin *skin = 0;
	if (Environment)
		skin = Environment->getSkin();
	if (!skin || VScrollBar)
		return;

	s32 fontHeight = 1;

	if (OverrideFont) {
		fontHeight = OverrideFont->getDimension(L"Ay").Height;
	} else {
		IGUIFont *font;
		if ((font = skin->getFont())) {
			fontHeight = font->getDimension(L"Ay").Height;
		}
	}

	VScrollBarWidth = skin->getSize(gui::EGDS_SCROLLBAR_SIZE);

	core::rect<s32> scrollbarrect = RelativeRect;
	scrollbarrect.UpperLeftCorner.X += RelativeRect.getWidth() - VScrollBarWidth;
	VScrollBar = new GUIScrollBar(Environment, getParent(), -1,
			scrollbarrect, false, true, m_tsrc);

	VScrollBar->setVisible(false);
	VScrollBar->setSmallStep(3 * fontHeight);
	VScrollBar->setLargeStep(10 * fontHeight);
}

//! Change the background color
void GUIEditBoxWithScrollBar::setBackgroundColor(const video::SColor &bg_color)
{
	m_bg_color = bg_color;
	m_bg_color_used = true;
}
