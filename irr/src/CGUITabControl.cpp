// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUITabControl.h"

#include "CGUIButton.h"
#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "IVideoDriver.h"
#include "rect.h"
#include "os.h"

namespace gui
{

// ------------------------------------------------------------------
// Tab
// ------------------------------------------------------------------

//! constructor
CGUITab::CGUITab(IGUIEnvironment *environment,
		IGUIElement *parent, const core::rect<s32> &rectangle,
		s32 id) :
		IGUITab(environment, parent, id, rectangle),
		BackColor(0, 0, 0, 0), OverrideTextColorEnabled(false), TextColor(255, 0, 0, 0),
		DrawBackground(false)
{
	const IGUISkin *const skin = environment->getSkin();
	if (skin)
		TextColor = skin->getColor(EGDC_BUTTON_TEXT);
}

//! draws the element and its children
void CGUITab::draw()
{
	if (!IsVisible)
		return;

	IGUISkin *skin = Environment->getSkin();

	if (skin && DrawBackground)
		skin->draw2DRectangle(this, BackColor, AbsoluteRect, &AbsoluteClippingRect);

	IGUIElement::draw();
}

//! sets if the tab should draw its background
void CGUITab::setDrawBackground(bool draw)
{
	DrawBackground = draw;
}

//! sets the color of the background, if it should be drawn.
void CGUITab::setBackgroundColor(video::SColor c)
{
	BackColor = c;
}

//! sets the color of the text
void CGUITab::setTextColor(video::SColor c)
{
	OverrideTextColorEnabled = true;
	TextColor = c;
}

video::SColor CGUITab::getTextColor() const
{
	if (OverrideTextColorEnabled)
		return TextColor;
	else
		return Environment->getSkin()->getColor(EGDC_BUTTON_TEXT);
}

//! returns true if the tab is drawing its background, false if not
bool CGUITab::isDrawingBackground() const
{
	return DrawBackground;
}

//! returns the color of the background
video::SColor CGUITab::getBackgroundColor() const
{
	return BackColor;
}

// ------------------------------------------------------------------
// Tabcontrol
// ------------------------------------------------------------------

//! constructor
CGUITabControl::CGUITabControl(IGUIEnvironment *environment,
		IGUIElement *parent, const core::rect<s32> &rectangle,
		bool fillbackground, bool border, s32 id) :
		IGUITabControl(environment, parent, id, rectangle),
		ActiveTabIndex(-1),
		Border(border), FillBackground(fillbackground), ScrollControl(false), TabHeight(0), VerticalAlignment(EGUIA_UPPERLEFT),
		UpButton(0), DownButton(0), TabMaxWidth(0), CurrentScrollTabIndex(0), TabExtraWidth(20)
{
	IGUISkin *skin = Environment->getSkin();
	IGUISpriteBank *sprites = 0;

	TabHeight = 32;

	if (skin) {
		sprites = skin->getSpriteBank();
		TabHeight = skin->getSize(gui::EGDS_BUTTON_HEIGHT) + 2;
	}

	UpButton = Environment->addButton(core::rect<s32>(0, 0, 10, 10), this);

	if (UpButton) {
		UpButton->setSpriteBank(sprites);
		UpButton->setVisible(false);
		UpButton->setSubElement(true);
		UpButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
		UpButton->setOverrideFont(Environment->getBuiltInFont());
		UpButton->grab();
	}

	DownButton = Environment->addButton(core::rect<s32>(0, 0, 10, 10), this);

	if (DownButton) {
		DownButton->setSpriteBank(sprites);
		DownButton->setVisible(false);
		DownButton->setSubElement(true);
		DownButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
		DownButton->setOverrideFont(Environment->getBuiltInFont());
		DownButton->grab();
	}

	setTabVerticalAlignment(EGUIA_UPPERLEFT);
	refreshSprites();
}

//! destructor
CGUITabControl::~CGUITabControl()
{
	for (u32 i = 0; i < Tabs.size(); ++i) {
		if (Tabs[i])
			Tabs[i]->drop();
	}

	if (UpButton)
		UpButton->drop();

	if (DownButton)
		DownButton->drop();
}

void CGUITabControl::refreshSprites()
{
	video::SColor color(255, 255, 255, 255);
	IGUISkin *skin = Environment->getSkin();
	if (skin) {
		color = skin->getColor(isEnabled() ? EGDC_WINDOW_SYMBOL : EGDC_GRAY_WINDOW_SYMBOL);

		if (UpButton) {
			UpButton->setSprite(EGBS_BUTTON_UP, skin->getIcon(EGDI_CURSOR_LEFT), color);
			UpButton->setSprite(EGBS_BUTTON_DOWN, skin->getIcon(EGDI_CURSOR_LEFT), color);
		}

		if (DownButton) {
			DownButton->setSprite(EGBS_BUTTON_UP, skin->getIcon(EGDI_CURSOR_RIGHT), color);
			DownButton->setSprite(EGBS_BUTTON_DOWN, skin->getIcon(EGDI_CURSOR_RIGHT), color);
		}
	}
}

//! Adds a tab
IGUITab *CGUITabControl::addTab(const wchar_t *caption, s32 id)
{
	CGUITab *tab = new CGUITab(Environment, this, calcTabPos(), id);

	tab->setText(caption);
	tab->setAlignment(EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
	tab->setVisible(false);
	Tabs.push_back(tab); // no grab as new already creates a reference

	if (ActiveTabIndex == -1) {
		ActiveTabIndex = Tabs.size() - 1;
		tab->setVisible(true);
	}

	recalculateScrollBar();

	return tab;
}

//! adds a tab which has been created elsewhere
s32 CGUITabControl::addTab(IGUITab *tab)
{
	return insertTab(Tabs.size(), tab, false);
}

//! Insert the tab at the given index
IGUITab *CGUITabControl::insertTab(s32 idx, const wchar_t *caption, s32 id)
{
	if (idx < 0 || idx > (s32)Tabs.size()) // idx == Tabs.size() is indeed OK here as core::array can handle that
		return NULL;

	CGUITab *tab = new CGUITab(Environment, this, calcTabPos(), id);

	tab->setText(caption);
	tab->setAlignment(EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
	tab->setVisible(false);
	Tabs.insert(tab, (u32)idx);

	if (ActiveTabIndex == -1) {
		ActiveTabIndex = (u32)idx;
		tab->setVisible(true);
	} else if (idx <= ActiveTabIndex) {
		++ActiveTabIndex;
		setVisibleTab(ActiveTabIndex);
	}

	recalculateScrollBar();

	return tab;
}

s32 CGUITabControl::insertTab(s32 idx, IGUITab *tab, bool serializationMode)
{
	if (!tab)
		return -1;
	if (idx > (s32)Tabs.size() && !serializationMode) // idx == Tabs.size() is indeed OK here as core::array can handle that
		return -1;
	// Not allowing to add same tab twice as it would make things complicated (serialization or setting active visible)
	if (getTabIndex(tab) >= 0)
		return -1;

	if (idx < 0)
		idx = (s32)Tabs.size();

	if (tab->getParent() != this)
		this->addChildToEnd(tab);

	tab->setVisible(false);

	tab->grab();

	if (serializationMode) {
		while (idx >= (s32)Tabs.size()) {
			Tabs.push_back(0);
		}
		Tabs[idx] = tab;

		if (idx == ActiveTabIndex) { // in serialization that can happen for any index
			setVisibleTab(ActiveTabIndex);
			tab->setVisible(true);
		}
	} else {
		Tabs.insert(tab, (u32)idx);

		if (ActiveTabIndex == -1) {
			ActiveTabIndex = idx;
			setVisibleTab(ActiveTabIndex);
		} else if (idx <= ActiveTabIndex) {
			++ActiveTabIndex;
			setVisibleTab(ActiveTabIndex);
		}
	}

	recalculateScrollBar();

	return idx;
}

//! Removes a child.
void CGUITabControl::removeChild(IGUIElement *child)
{
	s32 idx = getTabIndex(child);
	if (idx >= 0)
		removeTabButNotChild(idx);

	// remove real element
	IGUIElement::removeChild(child);

	recalculateScrollBar();
}

//! Removes a tab from the tabcontrol
void CGUITabControl::removeTab(s32 idx)
{
	if (idx < 0 || idx >= (s32)Tabs.size())
		return;

	removeChild(Tabs[(u32)idx]);
}

void CGUITabControl::removeTabButNotChild(s32 idx)
{
	if (idx < 0 || idx >= (s32)Tabs.size())
		return;

	Tabs[(u32)idx]->drop();
	Tabs.erase((u32)idx);

	if (idx < ActiveTabIndex) {
		--ActiveTabIndex;
		setVisibleTab(ActiveTabIndex);
	} else if (idx == ActiveTabIndex) {
		if ((u32)idx == Tabs.size())
			--ActiveTabIndex;
		setVisibleTab(ActiveTabIndex);
	}
}

//! Clears the tabcontrol removing all tabs
void CGUITabControl::clear()
{
	for (u32 i = 0; i < Tabs.size(); ++i) {
		if (Tabs[i]) {
			IGUIElement::removeChild(Tabs[i]);
			Tabs[i]->drop();
		}
	}
	Tabs.clear();

	recalculateScrollBar();
}

//! Returns amount of tabs in the tabcontrol
s32 CGUITabControl::getTabCount() const
{
	return Tabs.size();
}

//! Returns a tab based on zero based index
IGUITab *CGUITabControl::getTab(s32 idx) const
{
	if (idx < 0 || (u32)idx >= Tabs.size())
		return 0;

	return Tabs[idx];
}

//! called if an event happened.
bool CGUITabControl::OnEvent(const SEvent &event)
{
	if (isEnabled()) {
		switch (event.EventType) {
		case EET_GUI_EVENT:
			switch (event.GUIEvent.EventType) {
			case EGET_BUTTON_CLICKED:
				if (event.GUIEvent.Caller == UpButton) {
					scrollLeft();
					return true;
				} else if (event.GUIEvent.Caller == DownButton) {
					scrollRight();
					return true;
				}

				break;
			default:
				break;
			}
			break;
		case EET_MOUSE_INPUT_EVENT:
			switch (event.MouseInput.Event) {
			// case EMIE_LMOUSE_PRESSED_DOWN:
			//	// todo: dragging tabs around
			//	return true;
			case EMIE_LMOUSE_LEFT_UP: {
				s32 idx = getTabAt(event.MouseInput.X, event.MouseInput.Y);
				if (idx >= 0) {
					setActiveTab(idx);
					return true;
				}
				break;
			}
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	return IGUIElement::OnEvent(event);
}

void CGUITabControl::scrollLeft()
{
	if (CurrentScrollTabIndex > 0)
		--CurrentScrollTabIndex;
	recalculateScrollBar();
}

void CGUITabControl::scrollRight()
{
	if (CurrentScrollTabIndex < (s32)(Tabs.size()) - 1) {
		if (needScrollControl(CurrentScrollTabIndex, true))
			++CurrentScrollTabIndex;
	}
	recalculateScrollBar();
}

s32 CGUITabControl::calcTabWidth(IGUIFont *font, const wchar_t *text) const
{
	if (!font)
		return 0;

	s32 len = font->getDimension(text).Width + TabExtraWidth;
	if (TabMaxWidth > 0 && len > TabMaxWidth)
		len = TabMaxWidth;

	return len;
}

bool CGUITabControl::needScrollControl(s32 startIndex, bool withScrollControl, s32 *pos_rightmost)
{
	if (startIndex < 0)
		startIndex = 0;

	IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return false;

	IGUIFont *font = skin->getFont();

	if (Tabs.empty())
		return false;

	if (!font)
		return false;

	s32 pos = AbsoluteRect.UpperLeftCorner.X + 2;
	const s32 pos_right = withScrollControl ? UpButton->getAbsolutePosition().UpperLeftCorner.X - 2 : AbsoluteRect.LowerRightCorner.X;

	for (s32 i = startIndex; i < (s32)Tabs.size(); ++i) {
		// get Text
		const wchar_t *text = 0;
		if (Tabs[i]) {
			text = Tabs[i]->getText();

			// get text length
			s32 len = calcTabWidth(font, text); // always without withScrollControl here or len would be shortened
			pos += len;
		}

		if (pos > pos_right)
			return true;
	}

	if (pos_rightmost)
		*pos_rightmost = pos;
	return false;
}

s32 CGUITabControl::calculateScrollIndexFromActive()
{
	if (!ScrollControl || Tabs.empty())
		return 0;

	IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return false;

	IGUIFont *font = skin->getFont();
	if (!font)
		return false;

	const s32 pos_left = AbsoluteRect.UpperLeftCorner.X + 2;
	const s32 pos_right = UpButton->getAbsolutePosition().UpperLeftCorner.X - 2;

	// Move from center to the left border left until it is reached
	s32 pos_cl = (pos_left + pos_right) / 2;
	s32 i = ActiveTabIndex;
	for (; i > 0; --i) {
		if (!Tabs[i])
			continue;

		s32 len = calcTabWidth(font, Tabs[i]->getText());
		if (i == ActiveTabIndex)
			len /= 2;
		if (pos_cl - len < pos_left)
			break;

		pos_cl -= len;
	}
	if (i == 0)
		return i;

	// Is scrolling to right still possible?
	s32 pos_rr = 0;
	if (needScrollControl(i, true, &pos_rr))
		return i; // Yes? -> OK

	// No? -> Decrease "i" more. Append tabs until scrolling becomes necessary
	for (--i; i > 0; --i) {
		if (!Tabs[i])
			continue;

		pos_rr += calcTabWidth(font, Tabs[i]->getText());
		if (pos_rr > pos_right)
			break;
	}
	return i + 1;
}

core::rect<s32> CGUITabControl::calcTabPos()
{
	core::rect<s32> r;
	r.UpperLeftCorner.X = 0;
	r.LowerRightCorner.X = AbsoluteRect.getWidth();
	if (Border) {
		++r.UpperLeftCorner.X;
		--r.LowerRightCorner.X;
	}

	if (VerticalAlignment == EGUIA_UPPERLEFT) {
		r.UpperLeftCorner.Y = TabHeight + 2;
		r.LowerRightCorner.Y = AbsoluteRect.getHeight() - 1;
		if (Border) {
			--r.LowerRightCorner.Y;
		}
	} else {
		r.UpperLeftCorner.Y = 0;
		r.LowerRightCorner.Y = AbsoluteRect.getHeight() - (TabHeight + 2);
		if (Border) {
			++r.UpperLeftCorner.Y;
		}
	}

	return r;
}

//! draws the element and its children
void CGUITabControl::draw()
{
	if (!IsVisible)
		return;

	IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return;

	IGUIFont *font = skin->getFont();
	video::IVideoDriver *driver = Environment->getVideoDriver();

	core::rect<s32> frameRect(AbsoluteRect);

	// some empty background as placeholder when there are no tabs
	if (Tabs.empty())
		driver->draw2DRectangle(skin->getColor(EGDC_3D_HIGH_LIGHT), frameRect, &AbsoluteClippingRect);

	if (!font)
		return;

	// tab button bar can be above or below the tabs
	if (VerticalAlignment == EGUIA_UPPERLEFT) {
		frameRect.UpperLeftCorner.Y += 2;
		frameRect.LowerRightCorner.Y = frameRect.UpperLeftCorner.Y + TabHeight;
	} else {
		frameRect.UpperLeftCorner.Y = frameRect.LowerRightCorner.Y - TabHeight - 1;
		frameRect.LowerRightCorner.Y -= 2;
	}

	core::rect<s32> tr;
	s32 pos = frameRect.UpperLeftCorner.X + 2;

	bool needLeftScroll = CurrentScrollTabIndex > 0;
	bool needRightScroll = false;

	// left and right pos of the active tab
	s32 left = 0;
	s32 right = 0;

	// const wchar_t* activetext = 0;
	IGUITab *activeTab = 0;

	// Draw all tab-buttons except the active one
	for (u32 i = CurrentScrollTabIndex; i < Tabs.size() && !needRightScroll; ++i) {
		// get Text
		const wchar_t *text = 0;
		if (Tabs[i])
			text = Tabs[i]->getText();

		// get text length
		s32 len = calcTabWidth(font, text);
		if (ScrollControl) {
			s32 space = UpButton->getAbsolutePosition().UpperLeftCorner.X - 2 - pos;
			if (space < len) {
				needRightScroll = true;
				len = space;
			}
		}

		frameRect.LowerRightCorner.X += len;
		frameRect.UpperLeftCorner.X = pos;
		frameRect.LowerRightCorner.X = frameRect.UpperLeftCorner.X + len;

		pos += len;

		if ((s32)i == ActiveTabIndex) {
			// for active button just remember values
			left = frameRect.UpperLeftCorner.X;
			right = frameRect.LowerRightCorner.X;
			// activetext = text;
			activeTab = Tabs[i];
		} else {
			skin->draw3DTabButton(this, false, frameRect, &AbsoluteClippingRect, VerticalAlignment);

			// draw text
			core::rect<s32> textClipRect(frameRect); // TODO: exact size depends on borders in draw3DTabButton which we don't get with current interface
			textClipRect.clipAgainst(AbsoluteClippingRect);
			font->draw(text, frameRect, Tabs[i]->getTextColor(),
					true, true, &textClipRect);
		}
	}

	// Draw active tab button
	// Drawn later than other buttons because it draw over the buttons before/after it.
	if (left != 0 && right != 0 && activeTab != 0) {
		// draw upper highlight frame
		if (VerticalAlignment == EGUIA_UPPERLEFT) {
			frameRect.UpperLeftCorner.X = left - 2;
			frameRect.LowerRightCorner.X = right + 2;
			frameRect.UpperLeftCorner.Y -= 2;

			skin->draw3DTabButton(this, true, frameRect, &AbsoluteClippingRect, VerticalAlignment);

			// draw text
			core::rect<s32> textClipRect(frameRect); // TODO: exact size depends on borders in draw3DTabButton which we don't get with current interface
			textClipRect.clipAgainst(AbsoluteClippingRect);
			font->draw(activeTab->getText(), frameRect, activeTab->getTextColor(),
					true, true, &textClipRect);

			tr.UpperLeftCorner.X = AbsoluteRect.UpperLeftCorner.X;
			tr.LowerRightCorner.X = left - 1;
			tr.UpperLeftCorner.Y = frameRect.LowerRightCorner.Y - 1;
			tr.LowerRightCorner.Y = frameRect.LowerRightCorner.Y;
			driver->draw2DRectangle(skin->getColor(EGDC_3D_HIGH_LIGHT), tr, &AbsoluteClippingRect);

			tr.UpperLeftCorner.X = right;
			tr.LowerRightCorner.X = AbsoluteRect.LowerRightCorner.X;
			driver->draw2DRectangle(skin->getColor(EGDC_3D_HIGH_LIGHT), tr, &AbsoluteClippingRect);
		} else {
			frameRect.UpperLeftCorner.X = left - 2;
			frameRect.LowerRightCorner.X = right + 2;
			frameRect.LowerRightCorner.Y += 2;

			skin->draw3DTabButton(this, true, frameRect, &AbsoluteClippingRect, VerticalAlignment);

			// draw text
			font->draw(activeTab->getText(), frameRect, activeTab->getTextColor(),
					true, true, &frameRect);

			tr.UpperLeftCorner.X = AbsoluteRect.UpperLeftCorner.X;
			tr.LowerRightCorner.X = left - 1;
			tr.UpperLeftCorner.Y = frameRect.UpperLeftCorner.Y - 1;
			tr.LowerRightCorner.Y = frameRect.UpperLeftCorner.Y;
			driver->draw2DRectangle(skin->getColor(EGDC_3D_DARK_SHADOW), tr, &AbsoluteClippingRect);

			tr.UpperLeftCorner.X = right;
			tr.LowerRightCorner.X = AbsoluteRect.LowerRightCorner.X;
			driver->draw2DRectangle(skin->getColor(EGDC_3D_DARK_SHADOW), tr, &AbsoluteClippingRect);
		}
	} else {
		// No active tab
		// Draw a line separating button bar from tab area
		tr.UpperLeftCorner.X = AbsoluteRect.UpperLeftCorner.X;
		tr.LowerRightCorner.X = AbsoluteRect.LowerRightCorner.X;
		tr.UpperLeftCorner.Y = frameRect.LowerRightCorner.Y - 1;
		tr.LowerRightCorner.Y = frameRect.LowerRightCorner.Y;

		if (VerticalAlignment == EGUIA_UPPERLEFT) {
			driver->draw2DRectangle(skin->getColor(EGDC_3D_HIGH_LIGHT), tr, &AbsoluteClippingRect);
		} else {
			tr.UpperLeftCorner.Y = frameRect.UpperLeftCorner.Y - 1;
			tr.LowerRightCorner.Y = frameRect.UpperLeftCorner.Y;
			driver->draw2DRectangle(skin->getColor(EGDC_3D_DARK_SHADOW), tr, &AbsoluteClippingRect);
		}
	}

	// drawing some border and background for the tab-area.
	skin->draw3DTabBody(this, Border, FillBackground, AbsoluteRect, &AbsoluteClippingRect, TabHeight, VerticalAlignment);

	// enable scrollcontrols on need
	if (UpButton)
		UpButton->setEnabled(needLeftScroll);
	if (DownButton)
		DownButton->setEnabled(needRightScroll);
	refreshSprites();

	IGUIElement::draw();
}

//! Set the height of the tabs
void CGUITabControl::setTabHeight(s32 height)
{
	if (height < 0)
		height = 0;

	TabHeight = height;

	recalculateScrollButtonPlacement();
	recalculateScrollBar();
}

//! Get the height of the tabs
s32 CGUITabControl::getTabHeight() const
{
	return TabHeight;
}

//! set the maximal width of a tab. Per default width is 0 which means "no width restriction".
void CGUITabControl::setTabMaxWidth(s32 width)
{
	TabMaxWidth = width;
}

//! get the maximal width of a tab
s32 CGUITabControl::getTabMaxWidth() const
{
	return TabMaxWidth;
}

//! Set the extra width added to tabs on each side of the text
void CGUITabControl::setTabExtraWidth(s32 extraWidth)
{
	if (extraWidth < 0)
		extraWidth = 0;

	TabExtraWidth = extraWidth;

	recalculateScrollBar();
}

//! Get the extra width added to tabs on each side of the text
s32 CGUITabControl::getTabExtraWidth() const
{
	return TabExtraWidth;
}

void CGUITabControl::recalculateScrollBar()
{
	// Down: to right, Up: to left
	if (!UpButton || !DownButton)
		return;

	ScrollControl = needScrollControl() || CurrentScrollTabIndex > 0;

	if (ScrollControl) {
		UpButton->setVisible(true);
		DownButton->setVisible(true);
	} else {
		UpButton->setVisible(false);
		DownButton->setVisible(false);
	}

	bringToFront(UpButton);
	bringToFront(DownButton);
}

//! Set the alignment of the tabs
void CGUITabControl::setTabVerticalAlignment(EGUI_ALIGNMENT alignment)
{
	VerticalAlignment = alignment;

	recalculateScrollButtonPlacement();
	recalculateScrollBar();

	core::rect<s32> r(calcTabPos());
	for (u32 i = 0; i < Tabs.size(); ++i) {
		Tabs[i]->setRelativePosition(r);
	}
}

void CGUITabControl::recalculateScrollButtonPlacement()
{
	IGUISkin *skin = Environment->getSkin();
	s32 ButtonSize = 16;
	s32 ButtonHeight = TabHeight - 2;
	if (ButtonHeight < 0)
		ButtonHeight = TabHeight;
	if (skin) {
		ButtonSize = skin->getSize(EGDS_WINDOW_BUTTON_WIDTH);
		if (ButtonSize > TabHeight)
			ButtonSize = TabHeight;
	}

	s32 ButtonX = RelativeRect.getWidth() - (s32)(2.5f * (f32)ButtonSize) - 1;
	s32 ButtonY = 0;

	if (VerticalAlignment == EGUIA_UPPERLEFT) {
		ButtonY = 2 + (TabHeight / 2) - (ButtonHeight / 2);
		UpButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
		DownButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_UPPERLEFT);
	} else {
		ButtonY = RelativeRect.getHeight() - (TabHeight / 2) - (ButtonHeight / 2) - 2;
		UpButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT);
		DownButton->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT);
	}

	UpButton->setRelativePosition(core::rect<s32>(ButtonX, ButtonY, ButtonX + ButtonSize, ButtonY + ButtonHeight));
	ButtonX += ButtonSize + 1;
	DownButton->setRelativePosition(core::rect<s32>(ButtonX, ButtonY, ButtonX + ButtonSize, ButtonY + ButtonHeight));
}

//! Get the alignment of the tabs
EGUI_ALIGNMENT CGUITabControl::getTabVerticalAlignment() const
{
	return VerticalAlignment;
}

s32 CGUITabControl::getTabAt(s32 xpos, s32 ypos) const
{
	core::position2di p(xpos, ypos);
	IGUISkin *skin = Environment->getSkin();
	IGUIFont *font = skin->getFont();

	core::rect<s32> frameRect(AbsoluteRect);

	if (VerticalAlignment == EGUIA_UPPERLEFT) {
		frameRect.UpperLeftCorner.Y += 2;
		frameRect.LowerRightCorner.Y = frameRect.UpperLeftCorner.Y + TabHeight;
	} else {
		frameRect.UpperLeftCorner.Y = frameRect.LowerRightCorner.Y - TabHeight;
	}

	s32 pos = frameRect.UpperLeftCorner.X + 2;

	if (!frameRect.isPointInside(p))
		return -1;

	bool abort = false;
	for (s32 i = CurrentScrollTabIndex; i < (s32)Tabs.size() && !abort; ++i) {
		// get Text
		const wchar_t *text = 0;
		if (Tabs[i])
			text = Tabs[i]->getText();

		// get text length
		s32 len = calcTabWidth(font, text);
		if (ScrollControl) {
			// TODO: merge this with draw() ?
			s32 space = UpButton->getAbsolutePosition().UpperLeftCorner.X - 2 - pos;
			if (space < len) {
				abort = true;
				len = space;
			}
		}

		frameRect.UpperLeftCorner.X = pos;
		frameRect.LowerRightCorner.X = frameRect.UpperLeftCorner.X + len;

		pos += len;

		if (frameRect.isPointInside(p)) {
			return i;
		}
	}
	return -1;
}

//! Returns which tab is currently active
s32 CGUITabControl::getActiveTab() const
{
	return ActiveTabIndex;
}

//! Brings a tab to front.
bool CGUITabControl::setActiveTab(s32 idx)
{
	if ((u32)idx >= Tabs.size())
		return false;

	bool changed = (ActiveTabIndex != idx);

	ActiveTabIndex = idx;

	setVisibleTab(ActiveTabIndex);

	if (changed && Parent) {
		SEvent event;
		event.EventType = EET_GUI_EVENT;
		event.GUIEvent.Caller = this;
		event.GUIEvent.Element = 0;
		event.GUIEvent.EventType = EGET_TAB_CHANGED;
		Parent->OnEvent(event);
	}

	if (ScrollControl) {
		CurrentScrollTabIndex = calculateScrollIndexFromActive();
		recalculateScrollBar();
	}

	return true;
}

void CGUITabControl::setVisibleTab(s32 idx)
{
	for (u32 i = 0; i < Tabs.size(); ++i)
		if (Tabs[i])
			Tabs[i]->setVisible((s32)i == idx);
}

bool CGUITabControl::setActiveTab(IGUITab *tab)
{
	return setActiveTab(getTabIndex(tab));
}

s32 CGUITabControl::getTabIndex(const IGUIElement *tab) const
{
	for (u32 i = 0; i < Tabs.size(); ++i)
		if (Tabs[i] == tab)
			return (s32)i;

	return -1;
}

//! Update the position of the element, decides scroll button status
void CGUITabControl::updateAbsolutePosition()
{
	IGUIElement::updateAbsolutePosition();
	recalculateScrollBar();
}

} // end namespace gui
