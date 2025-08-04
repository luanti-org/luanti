// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUIListBox.h"
#include "irrArray.h"

namespace gui
{

class IGUIFont;
class IGUIScrollBar;

class CGUIListBox : public IGUIListBox
{
public:
	//! constructor
	CGUIListBox(IGUIEnvironment *environment, IGUIElement *parent,
			s32 id, core::rect<s32> rectangle, bool clip = true,
			bool drawBack = false, bool moveOverSelect = false);

	//! destructor
	virtual ~CGUIListBox();

	//! returns amount of list items
	u32 getItemCount() const override;

	//! returns string of a list item. the id may be a value from 0 to itemCount-1
	const wchar_t *getListItem(u32 id) const override;

	//! adds an list item, returns id of item
	u32 addItem(const wchar_t *text) override;

	//! clears the list
	void clear() override;

	//! returns id of selected item. returns -1 if no item is selected.
	s32 getSelected() const override;

	//! sets the selected item. Set this to -1 if no item should be selected
	void setSelected(s32 id) override;

	//! sets the selected item. Set this to -1 if no item should be selected
	void setSelected(const wchar_t *item) override;

	//! called if an event happened.
	bool OnEvent(const SEvent &event) override;

	//! draws the element and its children
	void draw() override;

	//! adds an list item with an icon
	//! \param text Text of list entry
	//! \param icon Sprite index of the Icon within the current sprite bank. Set it to -1 if you want no icon
	//! \return
	//! returns the id of the new created item
	u32 addItem(const wchar_t *text, s32 icon) override;

	//! Returns the icon of an item
	s32 getIcon(u32 id) const override;

	//! removes an item from the list
	void removeItem(u32 id) override;

	//! get the the id of the item at the given absolute coordinates
	s32 getItemAt(s32 xpos, s32 ypos) const override;

	//! Sets the sprite bank which should be used to draw list icons. This font is set to the sprite bank of
	//! the built-in-font by default. A sprite can be displayed in front of every list item.
	//! An icon is an index within the icon sprite bank. Several default icons are available in the
	//! skin through getIcon
	void setSpriteBank(IGUISpriteBank *bank) override;

	//! set whether the listbox should scroll to newly selected items
	void setAutoScrollEnabled(bool scroll) override;

	//! returns true if automatic scrolling is enabled, false if not.
	bool isAutoScrollEnabled() const override;

	//! Update the position and size of the listbox, and update the scrollbar
	void updateAbsolutePosition() override;

	//! set all item colors at given index to color
	void setItemOverrideColor(u32 index, video::SColor color) override;

	//! set all item colors of specified type at given index to color
	void setItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType, video::SColor color) override;

	//! clear all item colors at index
	void clearItemOverrideColor(u32 index) override;

	//! clear item color at index for given colortype
	void clearItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) override;

	//! has the item at index its color overwritten?
	bool hasItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const override;

	//! return the overwrite color at given item index.
	video::SColor getItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const override;

	//! return the default color which is used for the given colorType
	video::SColor getItemDefaultColor(EGUI_LISTBOX_COLOR colorType) const override;

	//! set the item at the given index
	void setItem(u32 index, const wchar_t *text, s32 icon) override;

	//! Insert the item at the given index
	//! Return the index on success or -1 on failure.
	s32 insertItem(u32 index, const wchar_t *text, s32 icon) override;

	//! Swap the items at the given indices
	void swapItems(u32 index1, u32 index2) override;

	//! set global itemHeight
	void setItemHeight(s32 height) override;

	//! Sets whether to draw the background
	void setDrawBackground(bool draw) override;

	//! Access the vertical scrollbar
	IGUIScrollBar *getVerticalScrollBar() const override;

private:
	struct ListItem
	{
		core::stringw Text;
		s32 Icon = -1;

		// A multicolor extension
		struct ListItemOverrideColor
		{
			bool Use = false;
			video::SColor Color;
		};
		ListItemOverrideColor OverrideColors[EGUI_LBC_COUNT]{};
	};

	void recalculateItemHeight();
	void selectNew(s32 ypos, bool onlyHover = false);
	void recalculateScrollPos();
	void updateScrollBarSize(s32 size);

	// extracted that function to avoid copy&paste code
	void recalculateItemWidth(s32 icon);

	// get labels used for serialization
	bool getSerializationLabels(EGUI_LISTBOX_COLOR colorType, core::stringc &useColorLabel, core::stringc &colorLabel) const;

	core::array<ListItem> Items;
	s32 Selected;
	s32 ItemHeight;
	s32 ItemHeightOverride;
	s32 TotalItemHeight;
	s32 ItemsIconWidth;
	gui::IGUIFont *Font;
	gui::IGUISpriteBank *IconBank;
	gui::IGUIScrollBar *ScrollBar;
	u32 selectTime;
	u32 LastKeyTime;
	core::stringw KeyBuffer;
	bool Selecting;
	bool DrawBack;
	bool MoveOverSelect;
	bool AutoScroll;
	bool HighlightWhenNotFocused;
};

} // end namespace gui
