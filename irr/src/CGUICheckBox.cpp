// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUICheckBox.h"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "IGUIFont.h"
#include "os.h"

namespace gui
{

//! constructor
CGUICheckBox::CGUICheckBox(bool checked, IGUIEnvironment *environment, IGUIElement *parent, s32 id, core::rect<s32> rectangle) :
		IGUICheckBox(environment, parent, id, rectangle), CheckTime(0), Pressed(false), Checked(checked), Border(false), Background(false)
{
	// this element can be tabbed into
	setTabStop(true);
	setTabOrder(-1);
}

//! called if an event happened.
bool CGUICheckBox::OnEvent(const SEvent &event)
{
	if (isEnabled()) {
		switch (event.EventType) {
		case EET_KEY_INPUT_EVENT:
			if (event.KeyInput.PressedDown &&
					(event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE)) {
				Pressed = true;
				return true;
			} else if (Pressed && event.KeyInput.PressedDown && event.KeyInput.Key == KEY_ESCAPE) {
				Pressed = false;
				return true;
			} else if (!event.KeyInput.PressedDown && Pressed &&
					   (event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE)) {
				Pressed = false;
				if (Parent) {
					SEvent newEvent;
					newEvent.EventType = EET_GUI_EVENT;
					newEvent.GUIEvent.Caller = this;
					newEvent.GUIEvent.Element = 0;
					Checked = !Checked;
					newEvent.GUIEvent.EventType = EGET_CHECKBOX_CHANGED;
					Parent->OnEvent(newEvent);
				}
				return true;
			}
			break;
		case EET_GUI_EVENT:
			if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST) {
				if (event.GUIEvent.Caller == this)
					Pressed = false;
			}
			break;
		case EET_MOUSE_INPUT_EVENT:
			if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
				Pressed = true;
				CheckTime = os::Timer::getTime();
				return true;
			} else if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP) {
				bool wasPressed = Pressed;
				Pressed = false;

				if (wasPressed && Parent) {
					if (!AbsoluteClippingRect.isPointInside(core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
						Pressed = false;
						return true;
					}

					SEvent newEvent;
					newEvent.EventType = EET_GUI_EVENT;
					newEvent.GUIEvent.Caller = this;
					newEvent.GUIEvent.Element = 0;
					Checked = !Checked;
					newEvent.GUIEvent.EventType = EGET_CHECKBOX_CHANGED;
					Parent->OnEvent(newEvent);
				}

				return true;
			}
			break;
		default:
			break;
		}
	}

	return IGUIElement::OnEvent(event);
}

//! draws the element and its children
void CGUICheckBox::draw()
{
	if (!IsVisible)
		return;

	IGUISkin *skin = Environment->getSkin();
	if (skin) {
		video::IVideoDriver *driver = Environment->getVideoDriver();
		core::rect<s32> frameRect(AbsoluteRect);

		// draw background
		if (Background) {
			video::SColor bgColor = skin->getColor(gui::EGDC_3D_FACE);
			driver->draw2DRectangle(bgColor, frameRect, &AbsoluteClippingRect);
		}

		// draw the border
		if (Border) {
			skin->draw3DSunkenPane(this, 0, true, false, frameRect, &AbsoluteClippingRect);
			frameRect.UpperLeftCorner.X += skin->getSize(EGDS_TEXT_DISTANCE_X);
			frameRect.LowerRightCorner.X -= skin->getSize(EGDS_TEXT_DISTANCE_X);
		}

		const s32 height = skin->getSize(EGDS_CHECK_BOX_WIDTH);

		// the rectangle around the "checked" area.
		core::rect<s32> checkRect(frameRect.UpperLeftCorner.X,
				((frameRect.getHeight() - height) / 2) + frameRect.UpperLeftCorner.Y,
				0, 0);

		checkRect.LowerRightCorner.X = checkRect.UpperLeftCorner.X + height;
		checkRect.LowerRightCorner.Y = checkRect.UpperLeftCorner.Y + height;

		EGUI_DEFAULT_COLOR col = EGDC_GRAY_EDITABLE;
		if (isEnabled())
			col = Pressed ? EGDC_FOCUSED_EDITABLE : EGDC_EDITABLE;
		skin->draw3DSunkenPane(this, skin->getColor(col),
				false, true, checkRect, &AbsoluteClippingRect);

		// the checked icon
		if (Checked) {
			skin->drawIcon(this, EGDI_CHECK_BOX_CHECKED, checkRect.getCenter(),
					CheckTime, os::Timer::getTime(), false, &AbsoluteClippingRect);
		}

		// associated text
		if (Text.size()) {
			checkRect = frameRect;
			checkRect.UpperLeftCorner.X += height + 5;

			IGUIFont *font = skin->getFont();
			if (font) {
				font->draw(Text.c_str(), checkRect,
						skin->getColor(isEnabled() ? EGDC_BUTTON_TEXT : EGDC_GRAY_TEXT), false, true, &AbsoluteClippingRect);
			}
		}
	}
	IGUIElement::draw();
}

//! set if box is checked
void CGUICheckBox::setChecked(bool checked)
{
	Checked = checked;
}

//! returns if box is checked
bool CGUICheckBox::isChecked() const
{
	return Checked;
}

//! Sets whether to draw the background
void CGUICheckBox::setDrawBackground(bool draw)
{
	Background = draw;
}

//! Checks if background drawing is enabled
bool CGUICheckBox::isDrawBackgroundEnabled() const
{
	return Background;
}

//! Sets whether to draw the border
void CGUICheckBox::setDrawBorder(bool draw)
{
	Border = draw;
}

//! Checks if border drawing is enabled
bool CGUICheckBox::isDrawBorderEnabled() const
{
	return Border;
}

} // end namespace gui
