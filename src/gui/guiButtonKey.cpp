// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "guiButtonKey.h"
using namespace gui;

GUIButtonKey *GUIButtonKey::addButton(IGUIEnvironment *environment,
		const core::rect<s32> &rectangle, ISimpleTextureSource *tsrc,
		IGUIElement *parent, s32 id, const wchar_t *text,
		const wchar_t *tooltiptext)
{
	auto button = make_irr<GUIButtonKey>(environment,
			parent ? parent : environment->getRootGUIElement(), id, rectangle, tsrc);

	if (text)
		button->setText(text);

	if (tooltiptext)
		button->setToolTipText(tooltiptext);

	return button.get();
}

void GUIButtonKey::setKey(KeyPress kp)
{
	key_value = kp;
	keysym = utf8_to_wide(kp.sym());
	super::setText(wstrgettext(kp.name()).c_str());
}

void GUIButtonKey::sendKey()
{
	if (Parent) {
		SEvent e;
		e.EventType = EET_GUI_EVENT;
		e.GUIEvent.Caller = this;
		e.GUIEvent.Element = nullptr;
		e.GUIEvent.EventType = EGET_BUTTON_CLICKED;
		Parent->OnEvent(e);
	}
}

bool GUIButtonKey::OnEvent(const SEvent & event)
{
	switch(event.EventType)
	{
	case EET_KEY_INPUT_EVENT:
		if (!event.KeyInput.PressedDown) {
			bool wasPressed = isPressed();
			setPressed(false);
			if (capturing) {
				cancelCapture();
				if (event.KeyInput.Key != KEY_ESCAPE)
					sendKey();
				return true;
			} else if (wasPressed && (event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE)) {
				startCapture();
				return true;
			}
			break;
		} else if (capturing) {
			if (event.KeyInput.Key != KEY_ESCAPE) {
				setPressed(true);
				setKey(KeyPress(event.KeyInput));
			}
			return true;
		} else if (event.KeyInput.Key == KEY_RETURN || event.KeyInput.Key == KEY_SPACE) {
			setPressed(true);
			return true;
		}
		break;
	case EET_MOUSE_INPUT_EVENT: {
		auto in_rect = AbsoluteClippingRect.isPointInside(
				core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y));
		switch (event.MouseInput.Event)
		{
		case EMIE_LMOUSE_LEFT_UP:
			if (!capturing && in_rect) {
				setPressed(false);
				startCapture();
				return true;
			}
			[[fallthrough]];
		case EMIE_MMOUSE_LEFT_UP: [[fallthrough]];
		case EMIE_RMOUSE_LEFT_UP:
			setPressed(false);
			if (capturing) {
				cancelCapture();
				sendKey();
				return true;
			}
			break;
		case EMIE_LMOUSE_PRESSED_DOWN:
			if (capturing) {
				if (event.MouseInput.Simulated) {
					cancelCapture(true);
					if (in_rect)
						return true;
				} else {
					setPressed(true);
					setKey(LMBKey);
					return true;
				}
			} else if (in_rect) {
				Environment->setFocus(this);
				setPressed(true);
				return true;
			}
			break;
		case EMIE_MMOUSE_PRESSED_DOWN:
			if (capturing) {
				setPressed(true);
				setKey(MMBKey);
				return true;
			}
			break;
		case EMIE_RMOUSE_PRESSED_DOWN:
			if (capturing) {
				setPressed(true);
				setKey(RMBKey);
				return true;
			}
			break;
		default:
			break;
		}
		break;
	}
	case EET_GUI_EVENT:
		if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST) {
			if (capturing)
				return true;
			else
				nostart = false; // lift nostart restriction if "mouse" (finger) is released outside the button
		}
	default:
		break;
	}

	return Parent ? Parent->OnEvent(event) : false;
}
