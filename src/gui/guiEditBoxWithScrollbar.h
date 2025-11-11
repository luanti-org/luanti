// Copyright (C) 2002-2012 Nikolaus Gebhardt, Modified by Mustapha Tachouct
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef GUIEDITBOXWITHSCROLLBAR_HEADER
#define GUIEDITBOXWITHSCROLLBAR_HEADER

#include "CGUIEditBox.h"

class ISimpleTextureSource;

class GUIEditBoxWithScrollBar : public gui::CGUIEditBox
{
public:

	//! constructor
	GUIEditBoxWithScrollBar(const wchar_t* text, bool border, gui::IGUIEnvironment* environment,
		IGUIElement* parent, s32 id, const core::rect<s32>& rectangle,
		ISimpleTextureSource *tsrc, bool writable = true, bool has_vscrollbar = true);

	//! destructor
	virtual ~GUIEditBoxWithScrollBar() {}

	//! draws the element and its children
	void draw() override;

	//! Change the background color
	void setBackgroundColor(const video::SColor &bg_color);

protected:
	//! create a Vertical ScrollBar
	void createVScrollBar();

	bool m_bg_color_used;
	video::SColor m_bg_color;

	ISimpleTextureSource *m_tsrc;
};


#endif // GUIEDITBOXWITHSCROLLBAR_HEADER

