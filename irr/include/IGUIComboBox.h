// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIElement.h"

namespace gui
{

//! Combobox widget
/** \par This element can create the following events of type EGUI_EVENT_TYPE:
\li EGET_COMBO_BOX_CHANGED
*/
class IGUIComboBox : public IGUIElement
{
public:
	//! constructor
	IGUIComboBox(IGUIEnvironment *environment, IGUIElement *parent, s32 id, core::rect<s32> rectangle) :
			IGUIElement(EGUIET_COMBO_BOX, environment, parent, id, rectangle) {}

	//! Returns amount of items in box
	virtual u32 getItemCount() const = 0;

	//! Returns string of an item. the idx may be a value from 0 to itemCount-1
	virtual const wchar_t *getItem(u32 idx) const = 0;

	//! Returns item data of an item. the idx may be a value from 0 to itemCount-1
	virtual u32 getItemData(u32 idx) const = 0;

	//! Returns index based on item data
	virtual s32 getIndexForItemData(u32 data) const = 0;

	//! Adds an item and returns the index of it
	virtual u32 addItem(const wchar_t *text, u32 data = 0) = 0;

	//! Removes an item from the combo box.
	/** Warning. This will change the index of all following items */
	virtual void removeItem(u32 idx) = 0;

	//! Deletes all items in the combo box
	virtual void clear() = 0;

	//! Returns id of selected item. returns -1 if no item is selected.
	virtual s32 getSelected() const = 0;

	//! Sets the selected item. Set this to -1 if no item should be selected
	virtual void setSelected(s32 idx) = 0;

	//! Sets the selected item and emits a change event.
	/** Set this to -1 if no item should be selected */
	virtual void setAndSendSelected(s32 idx) = 0;

	//! Sets text justification of the text area
	/** \param horizontal: EGUIA_UPPERLEFT for left justified (default),
	EGUIA_LOWERRIGHT for right justified, or EGUIA_CENTER for centered text.
	\param vertical: EGUIA_UPPERLEFT to align with top edge,
	EGUIA_LOWERRIGHT for bottom edge, or EGUIA_CENTER for centered text (default). */
	virtual void setTextAlignment(EGUI_ALIGNMENT horizontal, EGUI_ALIGNMENT vertical) = 0;

	//! Set the maximal number of rows for the selection listbox
	virtual void setMaxSelectionRows(u32 max) = 0;

	//! Get the maximal number of rows for the selection listbox
	virtual u32 getMaxSelectionRows() const = 0;
};

} // end namespace gui
