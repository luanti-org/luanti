// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"

namespace gui
{

//! List of all basic Irrlicht GUI elements.
/** An IGUIElement returns this when calling IGUIElement::getType(); */
enum EGUI_ELEMENT_TYPE
{
	//! A button (IGUIButton)
	EGUIET_BUTTON = 0,

	//! A check box (IGUICheckBox)
	EGUIET_CHECK_BOX,

	//! A combo box (IGUIComboBox)
	EGUIET_COMBO_BOX,

	//! A menu (IGUIMenu)
	EGUIET_MENU,

	//! An edit box (IGUIEditBox)
	EGUIET_EDIT_BOX,

	//! A file open dialog (IGUIFileOpenDialog)
	EGUIET_FILE_OPEN_DIALOG,

	//! An image (IGUIImage)
	EGUIET_IMAGE,

	//! A list box (IGUIListBox)
	EGUIET_LIST_BOX,

	//! A scroll bar (IGUIScrollBar)
	EGUIET_SCROLL_BAR,

	//! A static text (IGUIStaticText)
	EGUIET_STATIC_TEXT,

	//! A table (GUITable)
	EGUIET_TABLE,

	//! A tab (IGUITab)
	EGUIET_TAB,

	//! A tab control
	EGUIET_TAB_CONTROL,

	//! A tool bar (IGUIToolBar)
	EGUIET_TOOL_BAR,

	//! Unknown type.
	EGUIET_ELEMENT,

	//! The root of the GUI
	EGUIET_ROOT,

	//! Not an element, amount of elements in there
	EGUIET_COUNT,

	//! This enum is never used, it only forces the compiler to compile this enumeration to 32 bit.
	EGUIET_FORCE_32_BIT = 0x7fffffff

};

} // end namespace gui
