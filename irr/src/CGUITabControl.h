// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IGUITabControl.h"
#include "irrArray.h"
#include "IGUISkin.h"

namespace gui
{
class CGUITabControl;
class IGUIButton;

// A tab, onto which other gui elements could be added.
class CGUITab : public IGUITab
{
public:
	//! constructor
	CGUITab(IGUIEnvironment *environment,
			IGUIElement *parent, const core::rect<s32> &rectangle,
			s32 id);

	//! draws the element and its children
	void draw() override;

	//! sets if the tab should draw its background
	void setDrawBackground(bool draw = true) override;

	//! sets the color of the background, if it should be drawn.
	void setBackgroundColor(video::SColor c) override;

	//! sets the color of the text
	void setTextColor(video::SColor c) override;

	//! returns true if the tab is drawing its background, false if not
	bool isDrawingBackground() const override;

	//! returns the color of the background
	video::SColor getBackgroundColor() const override;

	video::SColor getTextColor() const override;

private:
	video::SColor BackColor;
	bool OverrideTextColorEnabled;
	video::SColor TextColor;
	bool DrawBackground;
};

//! A standard tab control
class CGUITabControl : public IGUITabControl
{
public:
	//! destructor
	CGUITabControl(IGUIEnvironment *environment,
			IGUIElement *parent, const core::rect<s32> &rectangle,
			bool fillbackground = true, bool border = true, s32 id = -1);

	//! destructor
	virtual ~CGUITabControl();

	//! Adds a tab
	IGUITab *addTab(const wchar_t *caption, s32 id = -1) override;

	//! Adds an existing tab
	s32 addTab(IGUITab *tab) override;

	//! Insert the tab at the given index
	IGUITab *insertTab(s32 idx, const wchar_t *caption, s32 id = -1) override;

	//! Insert an existing tab
	/** Note that it will also add the tab as a child of this TabControl.
	\return Index of added tab (should be same as the one passed) or -1 for failure*/
	s32 insertTab(s32 idx, IGUITab *tab, bool serializationMode) override;

	//! Removes a tab from the tabcontrol
	void removeTab(s32 idx) override;

	//! Clears the tabcontrol removing all tabs
	void clear() override;

	//! Returns amount of tabs in the tabcontrol
	s32 getTabCount() const override;

	//! Returns a tab based on zero based index
	IGUITab *getTab(s32 idx) const override;

	//! Brings a tab to front.
	bool setActiveTab(s32 idx) override;

	//! Brings a tab to front.
	bool setActiveTab(IGUITab *tab) override;

	//! For given given tab find it's zero-based index (or -1 for not found)
	s32 getTabIndex(const IGUIElement *tab) const override;

	//! Returns which tab is currently active
	s32 getActiveTab() const override;

	//! get the the id of the tab at the given absolute coordinates
	s32 getTabAt(s32 xpos, s32 ypos) const override;

	//! called if an event happened.
	bool OnEvent(const SEvent &event) override;

	//! draws the element and its children
	void draw() override;

	//! Removes a child.
	void removeChild(IGUIElement *child) override;

	//! Set the height of the tabs
	void setTabHeight(s32 height) override;

	//! Get the height of the tabs
	s32 getTabHeight() const override;

	//! set the maximal width of a tab. Per default width is 0 which means "no width restriction".
	void setTabMaxWidth(s32 width) override;

	//! get the maximal width of a tab
	s32 getTabMaxWidth() const override;

	//! Set the alignment of the tabs
	//! note: EGUIA_CENTER is not an option
	void setTabVerticalAlignment(gui::EGUI_ALIGNMENT alignment) override;

	//! Get the alignment of the tabs
	gui::EGUI_ALIGNMENT getTabVerticalAlignment() const override;

	//! Set the extra width added to tabs on each side of the text
	void setTabExtraWidth(s32 extraWidth) override;

	//! Get the extra width added to tabs on each side of the text
	s32 getTabExtraWidth() const override;

	//! Update the position of the element, decides scroll button status
	void updateAbsolutePosition() override;

private:
	void scrollLeft();
	void scrollRight();
	//! Indicates whether the tabs overflow in X direction
	bool needScrollControl(s32 startIndex = 0, bool withScrollControl = false, s32 *pos_rightmost = nullptr);
	//! Left index calculation based on the selected tab
	s32 calculateScrollIndexFromActive();
	s32 calcTabWidth(IGUIFont *font, const wchar_t *text) const;
	core::rect<s32> calcTabPos();
	void setVisibleTab(s32 idx);
	void removeTabButNotChild(s32 idx);

	void recalculateScrollButtonPlacement();
	void recalculateScrollBar();
	void refreshSprites();

	core::array<IGUITab *> Tabs;
	s32 ActiveTabIndex;
	bool Border;
	bool FillBackground;
	bool ScrollControl;
	s32 TabHeight;
	gui::EGUI_ALIGNMENT VerticalAlignment;
	IGUIButton *UpButton;
	IGUIButton *DownButton;
	s32 TabMaxWidth;
	s32 CurrentScrollTabIndex;
	s32 TabExtraWidth;
};

} // end namespace gui
