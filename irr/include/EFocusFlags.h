// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

namespace gui
{

//! Bitflags for defining the the focus behavior of the gui
// (all names start with SET as we might add REMOVE flags later to control that behavior as well)
enum EFOCUS_FLAG
{
	//! When set the focus changes when the left mouse-button got clicked while over an element
	EFF_SET_ON_LMOUSE_DOWN = 0x1,

	//! When set the focus changes when the right mouse-button got clicked while over an element
	//! Note that elements usually don't care about right-click and that won't change with this flag
	//! This is mostly to allow taking away focus from elements with right-mouse additionally.
	EFF_SET_ON_RMOUSE_DOWN = 0x2,

	//! When set the focus changes when the mouse-cursor is over an element
	EFF_SET_ON_MOUSE_OVER = 0x4,

	//! When set the focus can be changed with TAB-key combinations.
	EFF_SET_ON_TAB = 0x8,

	//! When set it's possible to set the focus to disabled elements.
	EFF_CAN_FOCUS_DISABLED = 0x16
};

} // namespace gui
