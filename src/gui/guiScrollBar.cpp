/*
Copyright (C) 2002-2013 Nikolaus Gebhardt
This file is part of the "Irrlicht Engine".
For conditions of distribution and use, see copyright notice in irrlicht.h
*/

#include "guiScrollBar.h"

GUIScrollBar::GUIScrollBar(IGUIEnvironment *environment, IGUIElement *parent, s32 id,
		core::rect<s32> rectangle, bool horizontal, ISimpleTextureSource *tsrc) :
		CGUIScrollBar(environment, parent, id, rectangle, horizontal)
{
	(void)tsrc; // Yet unused.
}
