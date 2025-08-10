// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIListBox.h"

#include "CGUIListBox.h"
#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "IGUIFont.h"
#include "IGUISpriteBank.h"
#include "CGUIScrollBar.h"
#include "os.h"

namespace gui
{

//! constructor
CGUIListBox::CGUIListBox(IGUIEnvironment *environment, IGUIElement *parent,
		s32 id, core::rect<s32> rectangle, bool clip,
		bool drawBack, bool moveOverSelect) :
		IGUIListBox(environment, parent, id, rectangle),
		Selected(-1),
		ItemHeight(0), ItemHeightOverride(0),
		TotalItemHeight(0), ItemsIconWidth(0), Font(0), IconBank(0),
		ScrollBar(0), selectTime(0), LastKeyTime(0), Selecting(false), DrawBack(drawBack),
		MoveOverSelect(moveOverSelect), AutoScroll(true), HighlightWhenNotFocused(true)
{
	IGUISkin *skin = Environment->getSkin();

	ScrollBar = new CGUIScrollBar(false, Environment, this, -1,
			core::recti(0, 0, 1, 1),
			!clip);
	ScrollBar->setSubElement(true);
	ScrollBar->setTabStop(false);
	ScrollBar->setAlignment(EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT, EGUIA_UPPERLEFT, EGUIA_LOWERRIGHT);
	ScrollBar->setVisible(false);
	ScrollBar->setPos(0);

	updateScrollBarSize(skin->getSize(EGDS_SCROLLBAR_SIZE));

	setNotClipped(!clip);

	// this element can be tabbed to
	setTabStop(true);
	setTabOrder(-1);

	updateAbsolutePosition();
}

//! destructor
CGUIListBox::~CGUIListBox()
{
	if (ScrollBar)
		ScrollBar->drop();

	if (Font)
		Font->drop();

	if (IconBank)
		IconBank->drop();
}

//! returns amount of list items
u32 CGUIListBox::getItemCount() const
{
	return Items.size();
}

//! returns string of a list item. the may be a value from 0 to itemCount-1
const wchar_t *CGUIListBox::getListItem(u32 id) const
{
	if (id >= Items.size())
		return 0;

	return Items[id].Text.c_str();
}

//! Returns the icon of an item
s32 CGUIListBox::getIcon(u32 id) const
{
	if (id >= Items.size())
		return -1;

	return Items[id].Icon;
}

//! adds a list item, returns id of item
u32 CGUIListBox::addItem(const wchar_t *text)
{
	return addItem(text, -1);
}

//! adds a list item, returns id of item
void CGUIListBox::removeItem(u32 id)
{
	if (id >= Items.size())
		return;

	if ((u32)Selected == id) {
		Selected = -1;
	} else if ((u32)Selected > id) {
		Selected -= 1;
		selectTime = os::Timer::getTime();
	}

	Items.erase(id);

	recalculateItemHeight();
}

s32 CGUIListBox::getItemAt(s32 xpos, s32 ypos) const
{
	if (xpos < AbsoluteRect.UpperLeftCorner.X || xpos >= AbsoluteRect.LowerRightCorner.X || ypos < AbsoluteRect.UpperLeftCorner.Y || ypos >= AbsoluteRect.LowerRightCorner.Y)
		return -1;

	if (ItemHeight == 0)
		return -1;

	s32 item = ((ypos - AbsoluteRect.UpperLeftCorner.Y - 1) + ScrollBar->getPos()) / ItemHeight;
	if (item < 0 || item >= (s32)Items.size())
		return -1;

	return item;
}

//! clears the list
void CGUIListBox::clear()
{
	Items.clear();
	ItemsIconWidth = 0;
	Selected = -1;

	ScrollBar->setPos(0);

	recalculateItemHeight();
}

void CGUIListBox::recalculateItemHeight()
{
	IGUISkin *skin = Environment->getSkin();

	if (Font != skin->getFont()) {
		if (Font)
			Font->drop();

		Font = skin->getFont();
		if (0 == ItemHeightOverride)
			ItemHeight = 0;

		if (Font) {
			if (0 == ItemHeightOverride)
				ItemHeight = Font->getDimension(L"A").Height + 4;

			Font->grab();
		}
	}

	TotalItemHeight = ItemHeight * Items.size();
	ScrollBar->setMax(core::max_(0, TotalItemHeight - AbsoluteRect.getHeight()));
	s32 minItemHeight = ItemHeight > 0 ? ItemHeight : 1;
	ScrollBar->setSmallStep(minItemHeight);
	ScrollBar->setLargeStep(2 * minItemHeight);

	if (TotalItemHeight <= AbsoluteRect.getHeight())
		ScrollBar->setVisible(false);
	else
		ScrollBar->setVisible(true);
}

//! returns id of selected item. returns -1 if no item is selected.
s32 CGUIListBox::getSelected() const
{
	return Selected;
}

//! sets the selected item. Set this to -1 if no item should be selected
void CGUIListBox::setSelected(s32 id)
{
	if ((u32)id >= Items.size())
		Selected = -1;
	else
		Selected = id;

	selectTime = os::Timer::getTime();

	recalculateScrollPos();
}

//! sets the selected item. Set this to -1 if no item should be selected
void CGUIListBox::setSelected(const wchar_t *item)
{
	s32 index = -1;

	if (item) {
		for (index = 0; index < (s32)Items.size(); ++index) {
			if (Items[index].Text == item)
				break;
		}
	}
	setSelected(index);
}

//! called if an event happened.
bool CGUIListBox::OnEvent(const SEvent &event)
{
	if (isEnabled()) {
		switch (event.EventType) {
		case EET_KEY_INPUT_EVENT:
			if (event.KeyInput.PressedDown &&
					(event.KeyInput.Key == KEY_DOWN ||
							event.KeyInput.Key == KEY_UP ||
							event.KeyInput.Key == KEY_HOME ||
							event.KeyInput.Key == KEY_END ||
							event.KeyInput.Key == KEY_NEXT ||
							event.KeyInput.Key == KEY_PRIOR)) {
				s32 oldSelected = Selected;
				switch (event.KeyInput.Key) {
				case KEY_DOWN:
					Selected += 1;
					break;
				case KEY_UP:
					Selected -= 1;
					break;
				case KEY_HOME:
					Selected = 0;
					break;
				case KEY_END:
					Selected = (s32)Items.size() - 1;
					break;
				case KEY_NEXT:
					Selected += AbsoluteRect.getHeight() / ItemHeight;
					break;
				case KEY_PRIOR:
					Selected -= AbsoluteRect.getHeight() / ItemHeight;
					break;
				default:
					break;
				}
				if (Selected < 0)
					Selected = 0;
				if (Selected >= (s32)Items.size())
					Selected = Items.size() - 1; // will set Selected to -1 for empty listboxes which is correct

				recalculateScrollPos();

				// post the news

				if (oldSelected != Selected && Parent && !Selecting && !MoveOverSelect) {
					SEvent e;
					e.EventType = EET_GUI_EVENT;
					e.GUIEvent.Caller = this;
					e.GUIEvent.Element = 0;
					e.GUIEvent.EventType = EGET_LISTBOX_CHANGED;
					Parent->OnEvent(e);
				}

				return true;
			} else if (!event.KeyInput.PressedDown && (event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE)) {
				if (Parent) {
					SEvent e;
					e.EventType = EET_GUI_EVENT;
					e.GUIEvent.Caller = this;
					e.GUIEvent.Element = 0;
					e.GUIEvent.EventType = EGET_LISTBOX_SELECTED_AGAIN;
					Parent->OnEvent(e);
				}
				return true;
			} else if (event.KeyInput.Key == KEY_TAB) {
				return false;
			} else if (event.KeyInput.PressedDown && event.KeyInput.Char) {
				// change selection based on text as it is typed.
				u32 now = os::Timer::getTime();

				if (now - LastKeyTime < 500) {
					// add to key buffer if it isn't a key repeat
					if (!(KeyBuffer.size() == 1 && KeyBuffer[0] == event.KeyInput.Char)) {
						KeyBuffer += L" ";
						KeyBuffer[KeyBuffer.size() - 1] = event.KeyInput.Char;
					}
				} else {
					KeyBuffer = L" ";
					KeyBuffer[0] = event.KeyInput.Char;
				}
				LastKeyTime = now;

				// find the selected item, starting at the current selection
				s32 start = Selected;
				// dont change selection if the key buffer matches the current item
				if (Selected > -1 && KeyBuffer.size() > 1) {
					if (Items[Selected].Text.size() >= KeyBuffer.size() &&
							KeyBuffer.equals_ignore_case(Items[Selected].Text.subString(0, KeyBuffer.size())))
						return true;
				}

				s32 current;
				for (current = start + 1; current < (s32)Items.size(); ++current) {
					if (Items[current].Text.size() >= KeyBuffer.size()) {
						if (KeyBuffer.equals_ignore_case(Items[current].Text.subString(0, KeyBuffer.size()))) {
							if (Parent && Selected != current && !Selecting && !MoveOverSelect) {
								SEvent e;
								e.EventType = EET_GUI_EVENT;
								e.GUIEvent.Caller = this;
								e.GUIEvent.Element = 0;
								e.GUIEvent.EventType = EGET_LISTBOX_CHANGED;
								Parent->OnEvent(e);
							}
							setSelected(current);
							return true;
						}
					}
				}
				for (current = 0; current <= start; ++current) {
					if (Items[current].Text.size() >= KeyBuffer.size()) {
						if (KeyBuffer.equals_ignore_case(Items[current].Text.subString(0, KeyBuffer.size()))) {
							if (Parent && Selected != current && !Selecting && !MoveOverSelect) {
								Selected = current;
								SEvent e;
								e.EventType = EET_GUI_EVENT;
								e.GUIEvent.Caller = this;
								e.GUIEvent.Element = 0;
								e.GUIEvent.EventType = EGET_LISTBOX_CHANGED;
								Parent->OnEvent(e);
							}
							setSelected(current);
							return true;
						}
					}
				}

				return true;
			}
			break;

		case EET_GUI_EVENT:
			switch (event.GUIEvent.EventType) {
			case gui::EGET_SCROLL_BAR_CHANGED:
				if (event.GUIEvent.Caller == ScrollBar)
					return true;
				break;
			case gui::EGET_ELEMENT_FOCUS_LOST: {
				if (event.GUIEvent.Caller == this)
					Selecting = false;
			}
			default:
				break;
			}
			break;

		case EET_MOUSE_INPUT_EVENT: {
			core::position2d<s32> p(event.MouseInput.X, event.MouseInput.Y);

			switch (event.MouseInput.Event) {
			case EMIE_MOUSE_WHEEL:
				ScrollBar->setPos(ScrollBar->getPos() + (event.MouseInput.Wheel < 0 ? -1 : 1) * -ItemHeight / 2);
				return true;

			case EMIE_LMOUSE_PRESSED_DOWN: {
				Selecting = true;
				return true;
			}

			case EMIE_LMOUSE_LEFT_UP: {
				Selecting = false;

				if (isPointInside(p))
					selectNew(event.MouseInput.Y);

				return true;
			}

			case EMIE_MOUSE_MOVED:
				if (Selecting || MoveOverSelect) {
					if (isPointInside(p)) {
						selectNew(event.MouseInput.Y, true);
						return true;
					}
				}
			default:
				break;
			}
		} break;
		default:
			break;
		}
	}

	return IGUIElement::OnEvent(event);
}

void CGUIListBox::selectNew(s32 ypos, bool onlyHover)
{
	u32 now = os::Timer::getTime();
	s32 oldSelected = Selected;

	Selected = getItemAt(AbsoluteRect.UpperLeftCorner.X, ypos);
	if (Selected < 0 && !Items.empty())
		Selected = 0;

	recalculateScrollPos();

	gui::EGUI_EVENT_TYPE eventType = (Selected == oldSelected && now < selectTime + 500) ? EGET_LISTBOX_SELECTED_AGAIN : EGET_LISTBOX_CHANGED;
	selectTime = now;
	// post the news
	if (Parent && !onlyHover) {
		SEvent event;
		event.EventType = EET_GUI_EVENT;
		event.GUIEvent.Caller = this;
		event.GUIEvent.Element = 0;
		event.GUIEvent.EventType = eventType;
		Parent->OnEvent(event);
	}
}

//! Update the position and size of the listbox, and update the scrollbar
void CGUIListBox::updateAbsolutePosition()
{
	IGUIElement::updateAbsolutePosition();

	recalculateItemHeight();
}

//! draws the element and its children
void CGUIListBox::draw()
{
	if (!IsVisible)
		return;

	recalculateItemHeight(); // if the font changed

	IGUISkin *skin = Environment->getSkin();
	updateScrollBarSize(skin->getSize(EGDS_SCROLLBAR_SIZE));

	core::rect<s32> *clipRect = 0;

	// draw background
	core::rect<s32> frameRect(AbsoluteRect);

	// draw items

	core::rect<s32> clientClip(AbsoluteRect);
	clientClip.UpperLeftCorner.Y += 1;
	clientClip.UpperLeftCorner.X += 1;
	if (ScrollBar->isVisible())
		clientClip.LowerRightCorner.X -= ScrollBar->getRelativePosition().getWidth();
	clientClip.LowerRightCorner.Y -= 1;
	clientClip.clipAgainst(AbsoluteClippingRect);

	skin->draw3DSunkenPane(this, skin->getColor(EGDC_3D_HIGH_LIGHT), true,
			DrawBack, frameRect, &AbsoluteClippingRect);

	if (clipRect)
		clientClip.clipAgainst(*clipRect);

	frameRect = AbsoluteRect;
	frameRect.UpperLeftCorner.X += 1;
	if (ScrollBar->isVisible())
		frameRect.LowerRightCorner.X -= ScrollBar->getRelativePosition().getWidth();

	frameRect.LowerRightCorner.Y = AbsoluteRect.UpperLeftCorner.Y + ItemHeight;

	frameRect.UpperLeftCorner.Y -= ScrollBar->getPos();
	frameRect.LowerRightCorner.Y -= ScrollBar->getPos();

	bool hl = (HighlightWhenNotFocused || Environment->hasFocus(this) || Environment->hasFocus(ScrollBar));

	for (s32 i = 0; i < (s32)Items.size(); ++i) {
		if (frameRect.LowerRightCorner.Y >= AbsoluteRect.UpperLeftCorner.Y &&
				frameRect.UpperLeftCorner.Y <= AbsoluteRect.LowerRightCorner.Y) {
			if (i == Selected && hl)
				skin->draw2DRectangle(this, skin->getColor(EGDC_HIGH_LIGHT), frameRect, &clientClip);

			core::rect<s32> textRect = frameRect;
			textRect.UpperLeftCorner.X += 3;

			if (Font) {
				if (IconBank && (Items[i].Icon > -1)) {
					core::position2di iconPos = textRect.UpperLeftCorner;
					iconPos.Y += textRect.getHeight() / 2;
					iconPos.X += ItemsIconWidth / 2;

					if (i == Selected && hl) {
						IconBank->draw2DSprite((u32)Items[i].Icon, iconPos, &clientClip,
								hasItemOverrideColor(i, EGUI_LBC_ICON_HIGHLIGHT) ? getItemOverrideColor(i, EGUI_LBC_ICON_HIGHLIGHT) : getItemDefaultColor(EGUI_LBC_ICON_HIGHLIGHT),
								selectTime, os::Timer::getTime(), false, true);
					} else {
						IconBank->draw2DSprite((u32)Items[i].Icon, iconPos, &clientClip,
								hasItemOverrideColor(i, EGUI_LBC_ICON) ? getItemOverrideColor(i, EGUI_LBC_ICON) : getItemDefaultColor(EGUI_LBC_ICON),
								0, (i == Selected) ? os::Timer::getTime() : 0, false, true);
					}
				}

				textRect.UpperLeftCorner.X += ItemsIconWidth + 3;

				if (i == Selected && hl) {
					Font->draw(Items[i].Text.c_str(), textRect,
							hasItemOverrideColor(i, EGUI_LBC_TEXT_HIGHLIGHT) ? getItemOverrideColor(i, EGUI_LBC_TEXT_HIGHLIGHT) : getItemDefaultColor(EGUI_LBC_TEXT_HIGHLIGHT),
							false, true, &clientClip);
				} else {
					Font->draw(Items[i].Text.c_str(), textRect,
							hasItemOverrideColor(i, EGUI_LBC_TEXT) ? getItemOverrideColor(i, EGUI_LBC_TEXT) : getItemDefaultColor(EGUI_LBC_TEXT),
							false, true, &clientClip);
				}

				textRect.UpperLeftCorner.X -= ItemsIconWidth + 3;
			}
		}

		frameRect.UpperLeftCorner.Y += ItemHeight;
		frameRect.LowerRightCorner.Y += ItemHeight;
	}

	IGUIElement::draw();
}

//! adds an list item with an icon
u32 CGUIListBox::addItem(const wchar_t *text, s32 icon)
{
	ListItem i;
	i.Text = text;
	i.Icon = icon;

	Items.push_back(i);
	recalculateItemHeight();
	recalculateItemWidth(icon);

	return Items.size() - 1;
}

void CGUIListBox::setSpriteBank(IGUISpriteBank *bank)
{
	if (bank == IconBank)
		return;
	if (IconBank)
		IconBank->drop();

	IconBank = bank;
	if (IconBank)
		IconBank->grab();
}

void CGUIListBox::recalculateScrollPos()
{
	if (!AutoScroll)
		return;

	const s32 selPos = (getSelected() == -1 ? TotalItemHeight : getSelected() * ItemHeight) - ScrollBar->getPos();

	if (selPos < 0) {
		ScrollBar->setPos(ScrollBar->getPos() + selPos);
	} else if (selPos > AbsoluteRect.getHeight() - ItemHeight) {
		ScrollBar->setPos(ScrollBar->getPos() + selPos - AbsoluteRect.getHeight() + ItemHeight);
	}
}

void CGUIListBox::updateScrollBarSize(s32 size)
{
	if (size != ScrollBar->getRelativePosition().getWidth()) {
		core::recti r(RelativeRect.getWidth() - size, 0, RelativeRect.getWidth(), RelativeRect.getHeight());
		ScrollBar->setRelativePosition(r);
	}
}

void CGUIListBox::setAutoScrollEnabled(bool scroll)
{
	AutoScroll = scroll;
}

bool CGUIListBox::isAutoScrollEnabled() const
{
	return AutoScroll;
}

bool CGUIListBox::getSerializationLabels(EGUI_LISTBOX_COLOR colorType, core::stringc &useColorLabel, core::stringc &colorLabel) const
{
	switch (colorType) {
	case EGUI_LBC_TEXT:
		useColorLabel = "UseColText";
		colorLabel = "ColText";
		break;
	case EGUI_LBC_TEXT_HIGHLIGHT:
		useColorLabel = "UseColTextHl";
		colorLabel = "ColTextHl";
		break;
	case EGUI_LBC_ICON:
		useColorLabel = "UseColIcon";
		colorLabel = "ColIcon";
		break;
	case EGUI_LBC_ICON_HIGHLIGHT:
		useColorLabel = "UseColIconHl";
		colorLabel = "ColIconHl";
		break;
	default:
		return false;
	}
	return true;
}

void CGUIListBox::recalculateItemWidth(s32 icon)
{
	if (IconBank && icon > -1 &&
			IconBank->getSprites().size() > (u32)icon &&
			IconBank->getSprites()[(u32)icon].Frames.size()) {
		u32 rno = IconBank->getSprites()[(u32)icon].Frames[0].rectNumber;
		if (IconBank->getPositions().size() > rno) {
			const s32 w = IconBank->getPositions()[rno].getWidth();
			if (w > ItemsIconWidth)
				ItemsIconWidth = w;
		}
	}
}

void CGUIListBox::setItem(u32 index, const wchar_t *text, s32 icon)
{
	if (index >= Items.size())
		return;

	Items[index].Text = text;
	Items[index].Icon = icon;

	recalculateItemHeight();
	recalculateItemWidth(icon);
}

//! Insert the item at the given index
//! Return the index on success or -1 on failure.
s32 CGUIListBox::insertItem(u32 index, const wchar_t *text, s32 icon)
{
	ListItem i;
	i.Text = text;
	i.Icon = icon;

	Items.insert(i, index);
	recalculateItemHeight();
	recalculateItemWidth(icon);

	return index;
}

void CGUIListBox::swapItems(u32 index1, u32 index2)
{
	if (index1 >= Items.size() || index2 >= Items.size())
		return;

	ListItem dummmy = Items[index1];
	Items[index1] = Items[index2];
	Items[index2] = dummmy;
}

void CGUIListBox::setItemOverrideColor(u32 index, video::SColor color)
{
	for (u32 c = 0; c < EGUI_LBC_COUNT; ++c) {
		Items[index].OverrideColors[c].Use = true;
		Items[index].OverrideColors[c].Color = color;
	}
}

void CGUIListBox::setItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType, video::SColor color)
{
	if (index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT)
		return;

	Items[index].OverrideColors[colorType].Use = true;
	Items[index].OverrideColors[colorType].Color = color;
}

void CGUIListBox::clearItemOverrideColor(u32 index)
{
	for (u32 c = 0; c < (u32)EGUI_LBC_COUNT; ++c) {
		Items[index].OverrideColors[c].Use = false;
	}
}

void CGUIListBox::clearItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType)
{
	if (index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT)
		return;

	Items[index].OverrideColors[colorType].Use = false;
}

bool CGUIListBox::hasItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const
{
	if (index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT)
		return false;

	return Items[index].OverrideColors[colorType].Use;
}

video::SColor CGUIListBox::getItemOverrideColor(u32 index, EGUI_LISTBOX_COLOR colorType) const
{
	if ((u32)index >= Items.size() || colorType < 0 || colorType >= EGUI_LBC_COUNT)
		return video::SColor();

	return Items[index].OverrideColors[colorType].Color;
}

video::SColor CGUIListBox::getItemDefaultColor(EGUI_LISTBOX_COLOR colorType) const
{
	IGUISkin *skin = Environment->getSkin();
	if (!skin)
		return video::SColor();

	switch (colorType) {
	case EGUI_LBC_TEXT:
		return skin->getColor(EGDC_BUTTON_TEXT);
	case EGUI_LBC_TEXT_HIGHLIGHT:
		return skin->getColor(EGDC_HIGH_LIGHT_TEXT);
	case EGUI_LBC_ICON:
		return skin->getColor(EGDC_ICON);
	case EGUI_LBC_ICON_HIGHLIGHT:
		return skin->getColor(EGDC_ICON_HIGH_LIGHT);
	default:
		return video::SColor();
	}
}

//! set global itemHeight
void CGUIListBox::setItemHeight(s32 height)
{
	ItemHeight = height;
	ItemHeightOverride = 1;
}

//! Sets whether to draw the background
void CGUIListBox::setDrawBackground(bool draw)
{
	DrawBack = draw;
}

//! Access the vertical scrollbar
IGUIScrollBar *CGUIListBox::getVerticalScrollBar() const
{
	return ScrollBar;
}

} // end namespace gui
