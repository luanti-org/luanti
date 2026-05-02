/*
Copyright (C) 2002-2013 Nikolaus Gebhardt
This file is part of the "Irrlicht Engine".
For conditions of distribution and use, see copyright notice in irrlicht.h
*/

#include "guiScrollBar.h"
#include "guiButton.h"

GUIScrollBar::GUIScrollBar(IGUIEnvironment *environment, IGUIElement *parent, s32 id,
		core::rect<s32> rectangle, bool horizontal, ISimpleTextureSource *tsrc) :
		CGUIScrollBar(environment, parent, id, rectangle, horizontal)
{
	// We use GUIButton instead of CGUIButton
	if (UpButton) {
		UpButton->remove();
		UpButton->drop();
	}
	UpButton = new GUIButton(Environment, this, -1, {}, tsrc, NoClip);
	UpButton->setSubElement(true);
	UpButton->setTabStop(false);
	if (DownButton) {
		DownButton->remove();
		DownButton->drop();
	}
	DownButton = new GUIButton(Environment, this, -1, {}, tsrc, NoClip);
	DownButton->setSubElement(true);
	DownButton->setTabStop(false);

	refreshControls();
}
