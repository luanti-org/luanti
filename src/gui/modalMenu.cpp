// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 stujones11, Stuart Jones <stujones111@gmail.com>

#include <cstdlib>
#include <IEventReceiver.h>
#include <IGUIComboBox.h>
#include <IGUIEditBox.h>
#include "client/renderingengine.h"
#include "modalMenu.h"
#include "gettext.h"
#include "gui/guiInventoryList.h"
#include "porting.h"
#include "settings.h"
#include "touchcontrols.h"

PointerAction PointerAction::fromEvent(const SEvent &event) {
	switch (event.EventType) {
	case EET_MOUSE_INPUT_EVENT:
		return {v2s32(event.MouseInput.X, event.MouseInput.Y), porting::getTimeMs()};
	case EET_TOUCH_INPUT_EVENT:
		return {v2s32(event.TouchInput.X, event.TouchInput.Y), porting::getTimeMs()};
	default:
		FATAL_ERROR("SEvent given to PointerAction::fromEvent has wrong EventType");
	}
}

bool PointerAction::isRelated(PointerAction previous) {
	u64 time_delta = porting::getDeltaMs(previous.time, time);
	v2s32 pos_delta = pos - previous.pos;
	f32 distance_sq = (f32)pos_delta.X * pos_delta.X + (f32)pos_delta.Y * pos_delta.Y;

	return time_delta < 400 && distance_sq < (30.0f * 30.0f);
}

GUIModalMenu::GUIModalMenu(gui::IGUIEnvironment* env, gui::IGUIElement* parent,
	s32 id, IMenuManager *menumgr, bool remap_click_outside) :
		IGUIElement(gui::EGUIET_ELEMENT, env, parent, id,
				core::rect<s32>(0, 0, 100, 100)),
#ifdef __ANDROID__
		m_jni_field_name(""),
#endif
		m_menumgr(menumgr),
		m_remap_click_outside(remap_click_outside)
{
	m_gui_scale = g_settings->getFloat("gui_scaling", 0.5f, 20.0f) *
			RenderingEngine::getDisplayDensity();

	setVisible(true);
	m_menumgr->createdMenu(this);
}

GUIModalMenu::~GUIModalMenu()
{
	m_menumgr->deletingMenu(this);
}

void GUIModalMenu::allowFocusRemoval(bool allow)
{
	m_allow_focus_removal = allow;
}

bool GUIModalMenu::canTakeFocus(gui::IGUIElement *e)
{
	return (e && (e == this || isMyChild(e))) || m_allow_focus_removal;
}

void GUIModalMenu::draw()
{
	if (!IsVisible)
		return;

	video::IVideoDriver *driver = Environment->getVideoDriver();
	v2u32 screensize = driver->getScreenSize();
	if (screensize != m_screensize_old) {
		m_screensize_old = screensize;
		regenerateGui(screensize);
	}

	drawMenu();
}

/*
	This should be called when the menu wants to quit.

	WARNING: THIS DEALLOCATES THE MENU FROM MEMORY. Return
	immediately if you call this from the menu itself.

	(More precisely, this decrements the reference count.)
*/
void GUIModalMenu::quitMenu()
{
	allowFocusRemoval(true);
	// This removes Environment's grab on us
	Environment->removeFocus(this);
	m_menumgr->deletingMenu(this);
	this->remove();
	if (g_touchcontrols)
		g_touchcontrols->show();
}

static bool isChild(gui::IGUIElement *tocheck, gui::IGUIElement *parent)
{
	while (tocheck) {
		if (tocheck == parent) {
			return true;
		}
		tocheck = tocheck->getParent();
	}
	return false;
}

bool GUIModalMenu::remapClickOutside(const SEvent &event)
{
	if (!m_remap_click_outside || event.EventType != EET_MOUSE_INPUT_EVENT ||
			(event.MouseInput.Event != EMIE_LMOUSE_PRESSED_DOWN &&
					event.MouseInput.Event != EMIE_LMOUSE_LEFT_UP))
		return false;

	// The formspec must only be closed if both the EMIE_LMOUSE_PRESSED_DOWN and
	// the EMIE_LMOUSE_LEFT_UP event haven't been absorbed by something else.

	PointerAction last = m_last_click_outside;
	m_last_click_outside = {}; // always reset
	PointerAction current = PointerAction::fromEvent(event);

	gui::IGUIElement *hovered =
			Environment->getRootGUIElement()->getElementFromPoint(current.pos);
	if (isChild(hovered, this))
		return false;

	// Dropping items is also done by tapping outside the formspec. If an item
	// is selected, make sure it is dropped without closing the formspec.
	// We have to explicitly restrict this to GUIInventoryList because other
	// GUI elements like text fields like to absorb events for no reason.
	GUIInventoryList *focused = dynamic_cast<GUIInventoryList *>(Environment->getFocus());
	if (focused && focused->OnEvent(event))
		// Return true since the event was handled, even if it wasn't handled by us.
		return true;

	if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN) {
		m_last_click_outside = current;
		return true;
	}

	if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP &&
			current.isRelated(last)) {
		SEvent translated{};
		translated.EventType              = EET_KEY_INPUT_EVENT;
		translated.KeyInput.Key           = KEY_ESCAPE;
		translated.KeyInput.SystemKeyCode = EscapeKey.getScancode();
		translated.KeyInput.Control       = false;
		translated.KeyInput.Shift         = false;
		translated.KeyInput.PressedDown   = true;
		translated.KeyInput.Char          = 0;
		OnEvent(translated);
		return true;
	}

	return false;
}

bool GUIModalMenu::simulateMouseEvent(ETOUCH_INPUT_EVENT touch_event, bool second_try)
{
	IGUIElement *target;
	if (!second_try)
		target = Environment->getFocus();
	else
		target = m_touch_hovered.get();

	SEvent mouse_event{}; // value-initialized, not unitialized
	mouse_event.EventType = EET_MOUSE_INPUT_EVENT;
	mouse_event.MouseInput.X = m_pointer.X;
	mouse_event.MouseInput.Y = m_pointer.Y;
	mouse_event.MouseInput.Simulated = true;
	switch (touch_event) {
	case ETIE_PRESSED_DOWN:
		mouse_event.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;
		mouse_event.MouseInput.ButtonStates = EMBSM_LEFT;
		break;
	case ETIE_MOVED:
		mouse_event.MouseInput.Event = EMIE_MOUSE_MOVED;
		mouse_event.MouseInput.ButtonStates = EMBSM_LEFT;
		break;
	case ETIE_LEFT_UP:
		mouse_event.MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
		mouse_event.MouseInput.ButtonStates = 0;
		break;
	case ETIE_COUNT:
		// ETIE_COUNT is used for double-tap events.
		mouse_event.MouseInput.Event = EMIE_LMOUSE_DOUBLE_CLICK;
		mouse_event.MouseInput.ButtonStates = EMBSM_LEFT;
		break;
	default:
		return false;
	}

	bool retval;
	do {
		if (preprocessEvent(mouse_event)) {
			retval = true;
			break;
		}
		if (!target) {
			retval = false;
			break;
		}
		retval = target->OnEvent(mouse_event);
	} while (false);

	if (!retval && !second_try)
		return simulateMouseEvent(touch_event, true);

	return retval;
}

void GUIModalMenu::enter(gui::IGUIElement *hovered)
{
	if (!hovered)
		return;
	sanity_check(!m_touch_hovered);
	m_touch_hovered.grab(hovered);
	SEvent gui_event{};
	gui_event.EventType = EET_GUI_EVENT;
	gui_event.GUIEvent.Caller = m_touch_hovered.get();
	gui_event.GUIEvent.EventType = gui::EGET_ELEMENT_HOVERED;
	gui_event.GUIEvent.Element = gui_event.GUIEvent.Caller;
	m_touch_hovered->OnEvent(gui_event);
}

void GUIModalMenu::leave()
{
	if (!m_touch_hovered)
		return;
	SEvent gui_event{};
	gui_event.EventType = EET_GUI_EVENT;
	gui_event.GUIEvent.Caller = m_touch_hovered.get();
	gui_event.GUIEvent.EventType = gui::EGET_ELEMENT_LEFT;
	m_touch_hovered->OnEvent(gui_event);
	m_touch_hovered.reset();
}

bool GUIModalMenu::preprocessEvent(const SEvent &event)
{
#ifdef __ANDROID__
	// display software keyboard when clicking edit boxes
	if (event.EventType == EET_MOUSE_INPUT_EVENT &&
			((event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN &&
			!porting::hasPhysicalKeyboardAndroid()) ||
			event.MouseInput.Event == EMIE_LMOUSE_DOUBLE_CLICK)) {
		gui::IGUIElement *hovered =
			Environment->getRootGUIElement()->getElementFromPoint(
				core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y));
		if ((hovered) && (hovered->getType() == gui::EGUIET_EDIT_BOX)) {
			bool retval = hovered->OnEvent(event);
			if (retval)
				Environment->setFocus(hovered);

			std::string field_name = getNameByID(hovered->getID());
			// read-only field
			if (field_name.empty())
				return retval;

			m_jni_field_name = field_name;

			// single line text input
			int type = 2;

			// multi line text input
			if (((gui::IGUIEditBox *)hovered)->isMultiLineEnabled())
				type = 1;

			// passwords are always single line
			if (((gui::IGUIEditBox *)hovered)->isPasswordBox())
				type = 3;

			porting::showTextInputDialog("",
					wide_to_utf8(((gui::IGUIEditBox *) hovered)->getText()), type);
			// Since we have opened the dialog, we have to return true to mark
			// the event as handled (avoids double-opening).
			return true;
		}
	}

	if (event.EventType == EET_GUI_EVENT) {
		if (event.GUIEvent.EventType == gui::EGET_LISTBOX_OPENED) {
			gui::IGUIComboBox *dropdown = (gui::IGUIComboBox *) event.GUIEvent.Caller;

			std::string field_name = getNameByID(dropdown->getID());
			if (field_name.empty())
				return false;

			m_jni_field_name = field_name;

			s32 selected_idx = dropdown->getSelected();
			s32 option_size = dropdown->getItemCount();
			std::vector<std::string> list_of_options;

			for (s32 i = 0; i < option_size; i++) {
				list_of_options.push_back(wide_to_utf8(dropdown->getItem(i)));
			}

			porting::showComboBoxDialog(list_of_options.data(), option_size, selected_idx);
			return true; // Prevent the Irrlicht dropdown from opening.
		}
	}
#endif

	// If the second touch arrives here again, that means nobody handled it.
	// Abort to avoid infinite recursion.
	if (m_second_touch)
		return true;

	// Convert touch events into mouse events.
	if (event.EventType == EET_TOUCH_INPUT_EVENT) {
		irr_ptr<GUIModalMenu> holder;
		holder.grab(this); // keep this alive until return (it might be dropped downstream [?])

		if (event.TouchInput.touchedCount == 1) {
			m_pointer = v2s32(event.TouchInput.X, event.TouchInput.Y);

			gui::IGUIElement *hovered = Environment->getRootGUIElement()->getElementFromPoint(core::position2d<s32>(m_pointer));
			if (event.TouchInput.Event == ETIE_PRESSED_DOWN)
				Environment->setFocus(hovered);
			if (m_touch_hovered != hovered) {
				leave();
				enter(hovered);
			}
			bool ret = simulateMouseEvent(event.TouchInput.Event);
			if (event.TouchInput.Event == ETIE_LEFT_UP)
				leave();

			// Detect double-taps and convert them into double-click events.
			if (event.TouchInput.Event == ETIE_PRESSED_DOWN) {
				PointerAction current = PointerAction::fromEvent(event);
				if (current.isRelated(m_last_touch)) {
					// ETIE_COUNT is used for double-tap events.
					simulateMouseEvent(ETIE_COUNT);
				}
				m_last_touch = current;
			}

			return ret;
		} else if (event.TouchInput.touchedCount == 2) {
			if (event.TouchInput.Event != ETIE_LEFT_UP)
				return true; // ignore
			auto focused = Environment->getFocus();
			if (!focused)
				return true;
			// The second-touch event is propagated as is (not converted).
			m_second_touch = true;
			focused->OnEvent(event);
			m_second_touch = false;
			return true;
		} else {
			// Any other touch after the second touch is ignored.
			return true;
		}
	}

	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		if (!event.MouseInput.Simulated) {
			// Only process if this is a real mouse event.
			m_pointer = v2s32(event.MouseInput.X, event.MouseInput.Y);
			m_touch_hovered.reset();
		}

		if (remapClickOutside(event))
			return true;
	}

	return false;
}

#ifdef __ANDROID__
porting::AndroidDialogState GUIModalMenu::getAndroidUIInputState()
{
	// No dialog is shown
	if (m_jni_field_name.empty())
		return porting::DIALOG_CANCELED;

	return porting::getInputDialogState();
}
#endif

GUIModalMenu::ScalingInfo GUIModalMenu::getScalingInfo(v2u32 screensize, v2u32 base_size)
{
	f32 scale = m_gui_scale;
	scale = std::min(scale, (f32)screensize.X / (f32)base_size.X);
	scale = std::min(scale, (f32)screensize.Y / (f32)base_size.Y);
	s32 w = base_size.X * scale, h = base_size.Y * scale;
	return {scale, core::rect<s32>(
		screensize.X / 2 - w / 2,
		screensize.Y / 2 - h / 2,
		screensize.X / 2 + w / 2,
		screensize.Y / 2 + h / 2
	)};
}
