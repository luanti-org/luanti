// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014 sapier
// Copyright (C) 2018 srifqi, Muhammad Rifqi Priyo Susanto
// Copyright (C) 2024 grorp, Gregor Parzefall

#include "touchcontrols.h"

#include "gettime.h"
#include "irr_v2d.h"
#include "log.h"
#include "porting.h"
#include "settings.h"
#include "client/guiscalingfilter.h"
#include "client/keycode.h"
#include "client/renderingengine.h"
#include "util/numeric.h"
#include "gettext.h"
#include "IGUIStaticText.h"
#include "IGUIFont.h"
#include <IrrlichtDevice.h>
#include <ISceneCollisionManager.h>

#include <iostream>
#include <algorithm>

TouchControls *g_touchcontrols;

static const char *button_image_names[] = {
	"jump_btn.png",
	"down.png",
	"zoom.png",
	"aux1_btn.png",
	"overflow_btn.png",

	"fly_btn.png",
	"noclip_btn.png",
	"fast_btn.png",
	"debug_btn.png",
	"camera_btn.png",
	"rangeview_btn.png",
	"minimap_btn.png",
	"",

	"chat_btn.png",
	"inventory_btn.png",
	"drop_btn.png",
	"exit_btn.png",

	"joystick_off.png",
	"joystick_bg.png",
	"joystick_center.png",
};

// compare with GUIKeyChangeMenu::init_keys
static const char *button_titles[] = {
	N_("Jump"),
	N_("Sneak"),
	N_("Zoom"),
	N_("Aux1"),
	N_("Overflow menu"),

	N_("Toggle fly"),
	N_("Toggle noclip"),
	N_("Toggle fast"),
	N_("Toggle debug"),
	N_("Change camera"),
	N_("Range select"),
	N_("Toggle minimap"),
	N_("Toggle chat log"),

	N_("Chat"),
	N_("Inventory"),
	N_("Drop"),
	N_("Exit"),

	N_("Joystick"),
	N_("Joystick"),
	N_("Joystick"),
};

static void load_button_texture(IGUIImage *gui_button, const std::string &path,
		const recti &button_rect, ISimpleTextureSource *tsrc, video::IVideoDriver *driver)
{
	video::ITexture *texture = guiScalingImageButton(driver,
			tsrc->getTexture(path), button_rect.getWidth(),
			button_rect.getHeight());
	gui_button->setImage(texture);
	gui_button->setScaleImage(true);
}

void button_info::emitAction(bool action, video::IVideoDriver *driver,
		IEventReceiver *receiver, ISimpleTextureSource *tsrc)
{
	if (keycode == KEY_UNKNOWN)
		return;

	SEvent translated{};
	translated.EventType        = EET_KEY_INPUT_EVENT;
	translated.KeyInput.Key     = keycode;
	translated.KeyInput.Control = false;
	translated.KeyInput.Shift   = false;
	translated.KeyInput.Char    = 0;

	if (action) {
		translated.KeyInput.PressedDown = true;
		receiver->OnEvent(translated);

		if (toggleable == button_info::FIRST_TEXTURE) {
			toggleable = button_info::SECOND_TEXTURE;
			load_button_texture(gui_button.get(), toggle_textures[1],
					gui_button->getRelativePosition(),
					tsrc, driver);
		} else if (toggleable == button_info::SECOND_TEXTURE) {
			toggleable = button_info::FIRST_TEXTURE;
			load_button_texture(gui_button.get(), toggle_textures[0],
					gui_button->getRelativePosition(),
					tsrc, driver);
		}
	} else {
		translated.KeyInput.PressedDown = false;
		receiver->OnEvent(translated);
	}
}

static bool buttons_handlePress(std::vector<button_info> &buttons, size_t pointer_id, IGUIElement *element,
		video::IVideoDriver *driver, IEventReceiver *receiver, ISimpleTextureSource *tsrc)
{
	if (!element)
		return false;

	for (button_info &btn : buttons) {
		if (btn.gui_button.get() == element) {
			assert(std::find(btn.pointer_ids.begin(), btn.pointer_ids.end(), pointer_id) == btn.pointer_ids.end());
			btn.pointer_ids.push_back(pointer_id);

			if (btn.pointer_ids.size() > 1)
				return true;

			btn.emitAction(true, driver, receiver, tsrc);
			btn.repeat_counter = -BUTTON_REPEAT_DELAY;
			return true;
		}
	}

	return false;
}


static bool buttons_handleRelease(std::vector<button_info> &buttons, size_t pointer_id,
		video::IVideoDriver *driver, IEventReceiver *receiver, ISimpleTextureSource *tsrc)
{
	for (button_info &btn : buttons) {
		auto it = std::find(btn.pointer_ids.begin(), btn.pointer_ids.end(), pointer_id);
		if (it != btn.pointer_ids.end()) {
			btn.pointer_ids.erase(it);

			if (!btn.pointer_ids.empty())
				return true;

			btn.emitAction(false, driver, receiver, tsrc);
			return true;
		}
	}

	return false;
}

static bool buttons_step(std::vector<button_info> &buttons, float dtime,
		video::IVideoDriver *driver, IEventReceiver *receiver, ISimpleTextureSource *tsrc)
{
	bool has_pointers = false;

	for (button_info &btn : buttons) {
		if (btn.pointer_ids.empty())
			continue;
		has_pointers = true;

		btn.repeat_counter += dtime;
		if (btn.repeat_counter < BUTTON_REPEAT_INTERVAL)
			continue;

		btn.emitAction(false, driver, receiver, tsrc);
		btn.emitAction(true, driver, receiver, tsrc);
		btn.repeat_counter = 0.0f;
	}

	return has_pointers;
}

static EKEY_CODE id_to_keycode(touch_gui_button_id id)
{
	EKEY_CODE code;
	// ESC isn't part of the keymap.
	if (id == exit_id)
		return KEY_ESCAPE;

	std::string key = "";
	switch (id) {
		case jump_id:
			key = "jump";
			break;
		case sneak_id:
			key = "sneak";
			break;
		case zoom_id:
			key = "zoom";
			break;
		case aux1_id:
			key = "aux1";
			break;
		case fly_id:
			key = "freemove";
			break;
		case noclip_id:
			key = "noclip";
			break;
		case fast_id:
			key = "fastmove";
			break;
		case debug_id:
			key = "toggle_debug";
			break;
		case camera_id:
			key = "camera_mode";
			break;
		case range_id:
			key = "rangeselect";
			break;
		case minimap_id:
			key = "minimap";
			break;
		case toggle_chat_id:
			key = "toggle_chat";
			break;
		case chat_id:
			key = "chat";
			break;
		case inventory_id:
			key = "inventory";
			break;
		case drop_id:
			key = "drop";
			break;
		default:
			break;
	}
	assert(!key.empty());
	std::string resolved = g_settings->get("keymap_" + key);
	try {
		code = keyname_to_keycode(resolved.c_str());
	} catch (UnknownKeycode &e) {
		code = KEY_UNKNOWN;
		warningstream << "TouchControls: Unknown key '" << resolved
			      << "' for '" << key << "', hiding button." << std::endl;
	}
	return code;
}


TouchControls::TouchControls(IrrlichtDevice *device, ISimpleTextureSource *tsrc):
		m_device(device),
		m_guienv(device->getGUIEnvironment()),
		m_receiver(device->getEventReceiver()),
		m_texturesource(tsrc)
{
	m_touchscreen_threshold = g_settings->getU16("touchscreen_threshold");
	m_long_tap_delay = g_settings->getU16("touch_long_tap_delay");
	m_fixed_joystick = g_settings->getBool("fixed_virtual_joystick");
	m_joystick_triggers_aux1 = g_settings->getBool("virtual_joystick_triggers_aux1");
	m_screensize = m_device->getVideoDriver()->getScreenSize();
	m_button_size = MYMIN(m_screensize.Y / 4.5f,
			RenderingEngine::getDisplayDensity() * 65.0f *
					g_settings->getFloat("hud_scaling"));

	// Initialize joystick display "button".
	// Joystick is placed on the bottom left of screen.
	if (m_fixed_joystick) {
		m_joystick_btn_off = grab_gui_element<IGUIImage>(makeButtonDirect(joystick_off_id,
				recti(m_button_size,
						m_screensize.Y - m_button_size * 4,
						m_button_size * 4,
						m_screensize.Y - m_button_size), true));
	} else {
		m_joystick_btn_off = grab_gui_element<IGUIImage>(makeButtonDirect(joystick_off_id,
				recti(m_button_size,
						m_screensize.Y - m_button_size * 3,
						m_button_size * 3,
						m_screensize.Y - m_button_size), true));
	}

	m_joystick_btn_bg = grab_gui_element<IGUIImage>(makeButtonDirect(joystick_bg_id,
			recti(m_button_size,
					m_screensize.Y - m_button_size * 4,
					m_button_size * 4,
					m_screensize.Y - m_button_size), false));

	m_joystick_btn_center = grab_gui_element<IGUIImage>(makeButtonDirect(joystick_center_id,
			recti(0, 0, m_button_size, m_button_size), false));

	// init jump button
	addButton(m_buttons, jump_id, button_image_names[jump_id],
			recti(m_screensize.X - 1.75f * m_button_size,
					m_screensize.Y - m_button_size,
					m_screensize.X - 0.25f * m_button_size,
					m_screensize.Y));

	// init sneak button
	addButton(m_buttons, sneak_id, button_image_names[sneak_id],
			recti(m_screensize.X - 3.25f * m_button_size,
					m_screensize.Y - m_button_size,
					m_screensize.X - 1.75f * m_button_size,
					m_screensize.Y));

	// init zoom button
	addButton(m_buttons, zoom_id, button_image_names[zoom_id],
			recti(m_screensize.X - 1.25f * m_button_size,
					m_screensize.Y - 4 * m_button_size,
					m_screensize.X - 0.25f * m_button_size,
					m_screensize.Y - 3 * m_button_size));

	// init aux1 button
	if (!m_joystick_triggers_aux1)
		addButton(m_buttons, aux1_id, button_image_names[aux1_id],
				recti(m_screensize.X - 1.25f * m_button_size,
						m_screensize.Y - 2.5f * m_button_size,
						m_screensize.X - 0.25f * m_button_size,
						m_screensize.Y - 1.5f * m_button_size));

	// init overflow button
	m_overflow_btn = grab_gui_element<IGUIImage>(makeButtonDirect(overflow_id,
				recti(m_screensize.X - 1.25f * m_button_size,
						m_screensize.Y - 5.5f * m_button_size,
						m_screensize.X - 0.25f * m_button_size,
						m_screensize.Y - 4.5f * m_button_size), true));

	const static touch_gui_button_id overflow_buttons[] {
		chat_id, inventory_id, drop_id, exit_id,
		fly_id, noclip_id, fast_id, debug_id, camera_id, range_id, minimap_id,
		toggle_chat_id,
	};

	IGUIStaticText *background = m_guienv->addStaticText(L"",
			recti(v2s32(0, 0), dimension2du(m_screensize)));
	background->setBackgroundColor(video::SColor(140, 0, 0, 0));
	background->setVisible(false);
	m_overflow_bg = grab_gui_element<IGUIStaticText>(background);

	s32 cols = 4;
	s32 rows = 3;
	f32 screen_aspect = (f32)m_screensize.X / (f32)m_screensize.Y;
	while ((s32)ARRLEN(overflow_buttons) > cols * rows) {
		f32 aspect = (f32)cols / (f32)rows;
		if (aspect > screen_aspect)
			rows++;
		else
			cols++;
	}

	v2s32 size(m_button_size, m_button_size);
	v2s32 spacing(m_screensize.X / (cols + 1), m_screensize.Y / (rows + 1));
	v2s32 pos(spacing);

	for (auto id : overflow_buttons) {
		if (id_to_keycode(id) == KEY_UNKNOWN)
			continue;

		recti rect(pos - size / 2, dimension2du(size.X, size.Y));
		if (rect.LowerRightCorner.X > (s32)m_screensize.X) {
			pos.X = spacing.X;
			pos.Y += spacing.Y;
			rect = recti(pos - size / 2, dimension2du(size.X, size.Y));
		}

		if (id == toggle_chat_id)
			// Chat is shown by default, so chat_hide_btn.png is shown first.
			addToggleButton(m_overflow_buttons, id, "chat_hide_btn.png",
					"chat_show_btn.png", rect, false);
		else
			addButton(m_overflow_buttons, id, button_image_names[id], rect, false);

		std::wstring str = wstrgettext(button_titles[id]);
		IGUIStaticText *text = m_guienv->addStaticText(str.c_str(), recti());
		IGUIFont *font = text->getActiveFont();
		dimension2du dim = font->getDimension(str.c_str());
		dim = dimension2du(dim.Width * 1.25f, dim.Height * 1.25f); // avoid clipping
		text->setRelativePosition(recti(pos.X - dim.Width / 2, pos.Y + size.Y / 2,
				pos.X + dim.Width / 2, pos.Y + size.Y / 2 + dim.Height));
		text->setTextAlignment(EGUIA_CENTER, EGUIA_UPPERLEFT);
		text->setVisible(false);
		m_overflow_button_titles.push_back(grab_gui_element<IGUIStaticText>(text));

		rect.addInternalPoint(text->getRelativePosition().UpperLeftCorner);
		rect.addInternalPoint(text->getRelativePosition().LowerRightCorner);
		m_overflow_button_rects.push_back(rect);

		pos.X += spacing.X;
	}

	m_status_text = grab_gui_element<IGUIStaticText>(
			m_guienv->addStaticText(L"", recti(), false, false));
	m_status_text->setVisible(false);
}

TouchControls::~TouchControls()
{
	releaseAll();
}

void TouchControls::addButton(std::vector<button_info> &buttons, touch_gui_button_id id,
		const std::string &image, const recti &rect, bool visible)
{
	IGUIImage *btn_gui_button = m_guienv->addImage(rect, nullptr, id);
	btn_gui_button->setVisible(visible);
	load_button_texture(btn_gui_button, image, rect,
			m_texturesource, m_device->getVideoDriver());

	button_info &btn = buttons.emplace_back();
	btn.keycode = id_to_keycode(id);
	btn.gui_button = grab_gui_element<IGUIImage>(btn_gui_button);
}

void TouchControls::addToggleButton(std::vector<button_info> &buttons, touch_gui_button_id id,
		const std::string &image_1, const std::string &image_2, const recti &rect, bool visible)
{
	addButton(buttons, id, image_1, rect, visible);
	button_info &btn = buttons.back();
	btn.toggleable = button_info::FIRST_TEXTURE;
	btn.toggle_textures[0] = image_1;
	btn.toggle_textures[1] = image_2;
}

IGUIImage *TouchControls::makeButtonDirect(touch_gui_button_id id,
		const recti &rect, bool visible)
{
	IGUIImage *btn_gui_button = m_guienv->addImage(rect, nullptr, id);
	btn_gui_button->setVisible(visible);
	load_button_texture(btn_gui_button, button_image_names[id], rect,
			m_texturesource, m_device->getVideoDriver());

	return btn_gui_button;
}

bool TouchControls::isHotbarButton(const SEvent &event)
{
	const v2s32 touch_pos = v2s32(event.TouchInput.X, event.TouchInput.Y);
	// check if hotbar item is pressed
	for (auto &[index, rect] : m_hotbar_rects) {
		if (rect.isPointInside(touch_pos)) {
			// We can't just emit a keypress event because the number keys
			// range from 1 to 9, but there may be more hotbar items.
			m_hotbar_selection = index;
			return true;
		}
	}
	return false;
}

std::optional<u16> TouchControls::getHotbarSelection()
{
	auto selection = m_hotbar_selection;
	m_hotbar_selection = std::nullopt;
	return selection;
}

void TouchControls::handleReleaseEvent(size_t pointer_id)
{
	// By the way: Android reuses pointer IDs, so m_pointer_pos[pointer_id]
	// will be overwritten soon anyway.
	m_pointer_downpos.erase(pointer_id);
	m_pointer_pos.erase(pointer_id);

	// handle buttons
	if (buttons_handleRelease(m_buttons, pointer_id, m_device->getVideoDriver(),
			m_receiver, m_texturesource))
		return;
	if (buttons_handleRelease(m_overflow_buttons, pointer_id, m_device->getVideoDriver(),
			m_receiver, m_texturesource))
		return;

	if (m_has_move_id && pointer_id == m_move_id) {
		// handle the point used for moving view
		m_has_move_id = false;

		// If m_tap_state is already set to TapState::ShortTap, we must keep
		// that value. Otherwise, many short taps will be ignored if you tap
		// very fast.
		if (!m_move_has_really_moved && !m_move_prevent_short_tap &&
				m_tap_state != TapState::LongTap) {
			m_tap_state = TapState::ShortTap;
		} else {
			m_tap_state = TapState::None;
		}
	}

	// handle joystick
	else if (m_has_joystick_id && pointer_id == m_joystick_id) {
		m_has_joystick_id = false;

		// reset joystick
		m_joystick_direction = 0.0f;
		m_joystick_speed = 0.0f;
		m_joystick_status_aux1 = false;
		applyJoystickStatus();

		updateVisibility();
	} else {
		infostream << "TouchControls::translateEvent released unknown button: "
				<< pointer_id << std::endl;
	}
}

void TouchControls::translateEvent(const SEvent &event)
{
	if (!m_visible) {
		infostream << "TouchControls::translateEvent got event but is not visible!"
				<< std::endl;
		return;
	}

	if (event.EventType != EET_TOUCH_INPUT_EVENT)
		return;

	const s32 half_button_size = m_button_size / 2.0f;
	const s32 fixed_joystick_range_sq = half_button_size * half_button_size * 3 * 3;
	const s32 X = event.TouchInput.X;
	const s32 Y = event.TouchInput.Y;
	const v2s32 touch_pos = v2s32(X, Y);
	const v2s32 fixed_joystick_center = v2s32(half_button_size * 5,
			m_screensize.Y - half_button_size * 5);
	const v2s32 dir_fixed = touch_pos - fixed_joystick_center;

	if (event.TouchInput.Event == ETIE_PRESSED_DOWN) {
		size_t pointer_id = event.TouchInput.ID;
		m_pointer_downpos[pointer_id] = touch_pos;
		m_pointer_pos[pointer_id] = touch_pos;

		bool prevent_short_tap = false;

		IGUIElement *element = m_guienv->getRootGUIElement()->getElementFromPoint(touch_pos);

		// handle overflow menu
		if (!m_overflow_open) {
			if (element == m_overflow_btn.get())  {
				toggleOverflowMenu();
				return;
			}
		} else {
			for (size_t i = 0; i < m_overflow_buttons.size(); i++) {
				if (m_overflow_button_rects[i].isPointInside(touch_pos)) {
					element = m_overflow_buttons[i].gui_button.get();
					break;
				}
			}

			if (buttons_handlePress(m_overflow_buttons, pointer_id, element,
					m_device->getVideoDriver(), m_receiver, m_texturesource))
				return;

			toggleOverflowMenu();
			// refresh since visibility of buttons has changed
		 	element = m_guienv->getRootGUIElement()->getElementFromPoint(touch_pos);
			// continue processing, but avoid accidentally placing a node
			// when closing the overflow menu
			prevent_short_tap = true;
		}

		// handle buttons
		if (buttons_handlePress(m_buttons, pointer_id, element,
				m_device->getVideoDriver(), m_receiver, m_texturesource))
			return;

		// handle hotbar
		if (isHotbarButton(event))
			// already handled in isHotbarButton()
			return;

		// Select joystick when joystick tapped (fixed joystick position) or
		// when left 1/3 of screen dragged (free joystick position)
		if ((m_fixed_joystick && dir_fixed.getLengthSQ() <= fixed_joystick_range_sq) ||
				(!m_fixed_joystick && X < m_screensize.X / 3.0f)) {
			// If we don't already have a starting point for joystick, make this the one.
			if (!m_has_joystick_id) {
				m_has_joystick_id           = true;
				m_joystick_id               = pointer_id;
				m_joystick_has_really_moved = false;

				updateVisibility();

				// If it's a fixed joystick, don't move the joystick "button".
				if (!m_fixed_joystick)
					m_joystick_btn_bg->setRelativePosition(
							touch_pos - half_button_size * 3);

				m_joystick_btn_center->setRelativePosition(
						touch_pos - half_button_size);
			}
		} else {
			// If we don't already have a moving point, make this the moving one.
			if (!m_has_move_id) {
				m_has_move_id              = true;
				m_move_id                  = pointer_id;
				m_move_has_really_moved    = false;
				m_move_downtime            = porting::getTimeMs();
				m_move_pos                 = touch_pos;
				// DON'T reset m_tap_state here, otherwise many short taps
				// will be ignored if you tap very fast.
				m_had_move_id              = true;
				m_move_prevent_short_tap   = prevent_short_tap;
			}
		}
	}
	else if (event.TouchInput.Event == ETIE_LEFT_UP) {
		verbosestream << "Up event for pointerid: " << event.TouchInput.ID << std::endl;
		handleReleaseEvent(event.TouchInput.ID);
	} else {
		assert(event.TouchInput.Event == ETIE_MOVED);

		if (!(m_has_joystick_id && m_fixed_joystick) &&
				m_pointer_pos[event.TouchInput.ID] == touch_pos)
			return;

		const v2s32 dir_free_original = touch_pos - m_pointer_downpos[event.TouchInput.ID];
		const v2s32 free_joystick_center = m_pointer_pos[event.TouchInput.ID];
		const v2s32 dir_free = touch_pos - free_joystick_center;

		const double touch_threshold_sq = m_touchscreen_threshold * m_touchscreen_threshold;

		if (m_has_move_id && event.TouchInput.ID == m_move_id) {
			m_move_pos = touch_pos;
			m_pointer_pos[event.TouchInput.ID] = touch_pos;

			// update camera_yaw and camera_pitch
			const double d = g_settings->getFloat("touchscreen_sensitivity", 0.001f, 10.0f)
					* 6.0f / RenderingEngine::getDisplayDensity();
			m_camera_yaw_change -= dir_free.X * d;
			m_camera_pitch_change += dir_free.Y * d;

			if (dir_free_original.getLengthSQ() > touch_threshold_sq)
				m_move_has_really_moved = true;
		}

		if (m_has_joystick_id && event.TouchInput.ID == m_joystick_id) {
			v2s32 dir = dir_free;
			if (m_fixed_joystick)
				dir = dir_fixed;

			const bool inside_joystick = dir_fixed.getLengthSQ() <= fixed_joystick_range_sq;
			const double distance_sq = dir.getLengthSQ();

			if (m_joystick_has_really_moved || inside_joystick ||
					(!m_fixed_joystick && distance_sq > touch_threshold_sq)) {
				m_joystick_has_really_moved = true;

				m_joystick_direction = atan2(dir.X, -dir.Y);

				const double distance = sqrt(distance_sq);
				if (distance <= m_touchscreen_threshold) {
					m_joystick_speed = 0.0f;
				} else {
					m_joystick_speed = distance / m_button_size;
					if (m_joystick_speed > 1.0f)
						m_joystick_speed = 1.0f;
				}

				m_joystick_status_aux1 = distance > (half_button_size * 3);

				if (distance > m_button_size) {
					// move joystick "button"
					v2s32 new_offset = dir * m_button_size / distance - half_button_size;
					if (m_fixed_joystick)
						m_joystick_btn_center->setRelativePosition(
								fixed_joystick_center + new_offset);
					else
						m_joystick_btn_center->setRelativePosition(
								free_joystick_center + new_offset);
				} else {
					m_joystick_btn_center->setRelativePosition(
							touch_pos - half_button_size);
				}
			}
		}
	}
}

void TouchControls::applyJoystickStatus()
{
	if (m_joystick_triggers_aux1) {
		SEvent translated{};
		translated.EventType            = EET_KEY_INPUT_EVENT;
		translated.KeyInput.Key         = id_to_keycode(aux1_id);
		translated.KeyInput.PressedDown = false;
		m_receiver->OnEvent(translated);

		if (m_joystick_status_aux1) {
			translated.KeyInput.PressedDown = true;
			m_receiver->OnEvent(translated);
		}
	}
}

void TouchControls::step(float dtime)
{
	// simulate keyboard repeats
	buttons_step(m_buttons, dtime, m_device->getVideoDriver(), m_receiver, m_texturesource);
	buttons_step(m_overflow_buttons, dtime, m_device->getVideoDriver(), m_receiver, m_texturesource);

	// joystick
	applyJoystickStatus();

	// if a new placed pointer isn't moved for some time start digging
	if (m_has_move_id && !m_move_has_really_moved && m_tap_state == TapState::None) {
		u64 delta = porting::getDeltaMs(m_move_downtime, porting::getTimeMs());

		if (delta > m_long_tap_delay) {
			m_tap_state = TapState::LongTap;
		}
	}

	// Update the shootline.
	// Since not only the pointer position, but also the player position and
	// thus the camera position can change, it doesn't suffice to update the
	// shootline when a touch event occurs.
	// Note that the shootline isn't used if touch_use_crosshair is enabled.
	// Only updating when m_has_move_id means that the shootline will stay at
	// it's last in-world position when the player doesn't need it.
	if (!m_draw_crosshair && (m_has_move_id || m_had_move_id)) {
		v2s32 pointer_pos = getPointerPos();
		m_shootline = m_device
				->getSceneManager()
				->getSceneCollisionManager()
				->getRayFromScreenCoordinates(pointer_pos);
	}
	m_had_move_id = false;
}

void TouchControls::resetHotbarRects()
{
	m_hotbar_rects.clear();
}

void TouchControls::registerHotbarRect(u16 index, const recti &rect)
{
	m_hotbar_rects[index] = rect;
}

void TouchControls::setVisible(bool visible)
{
	if (m_visible == visible)
		return;

	m_visible = visible;
	if (!visible) {
		releaseAll();
		m_overflow_open = false;
	}
	updateVisibility();
}

void TouchControls::toggleOverflowMenu()
{
	// no releaseAll here so that you can e.g. continue holding the joystick
	// while the overflow menu is open
	m_overflow_open = !m_overflow_open;
	updateVisibility();
}

void TouchControls::updateVisibility()
{
	bool regular_visible = m_visible && !m_overflow_open;
	for (auto &button : m_buttons)
		button.gui_button->setVisible(regular_visible);
	m_overflow_btn->setVisible(regular_visible);

	m_joystick_btn_off->setVisible(regular_visible && !m_has_joystick_id);
	m_joystick_btn_bg->setVisible(regular_visible && m_has_joystick_id);
	m_joystick_btn_center->setVisible(regular_visible && m_has_joystick_id);

	bool overflow_visible = m_visible && m_overflow_open;
	m_overflow_bg->setVisible(overflow_visible);
	for (auto &button : m_overflow_buttons)
		button.gui_button->setVisible(overflow_visible);
	for (auto &text : m_overflow_button_titles)
		text->setVisible(overflow_visible);
}

void TouchControls::releaseAll()
{
	while (!m_pointer_pos.empty())
		handleReleaseEvent(m_pointer_pos.begin()->first);

	// Release those manually too since the change initiated by
	// handleReleaseEvent will only be applied later by applyContextControls.
	if (m_dig_pressed) {
		emitMouseEvent(EMIE_LMOUSE_LEFT_UP);
		m_dig_pressed = false;
	}
	if (m_place_pressed) {
		emitMouseEvent(EMIE_RMOUSE_LEFT_UP);
		m_place_pressed = false;
	}
}

void TouchControls::hide()
{
	setVisible(false);
}

void TouchControls::show()
{
	setVisible(true);
}

v2s32 TouchControls::getPointerPos()
{
	if (m_draw_crosshair)
		return v2s32(m_screensize.X / 2, m_screensize.Y / 2);
	// We can't just use m_pointer_pos[m_move_id] because applyContextControls
	// may emit release events after m_pointer_pos[m_move_id] is erased.
	return m_move_pos;
}

void TouchControls::emitMouseEvent(EMOUSE_INPUT_EVENT type)
{
	v2s32 pointer_pos = getPointerPos();

	SEvent event{};
	event.EventType               = EET_MOUSE_INPUT_EVENT;
	event.MouseInput.X            = pointer_pos.X;
	event.MouseInput.Y            = pointer_pos.Y;
	event.MouseInput.Shift        = false;
	event.MouseInput.Control      = false;
	event.MouseInput.ButtonStates = 0;
	event.MouseInput.Event        = type;
	event.MouseInput.Simulated    = true;
	m_receiver->OnEvent(event);
}

void TouchControls::applyContextControls(const TouchInteractionMode &mode)
{
	// Since the pointed thing has already been determined when this function
	// is called, we cannot use this function to update the shootline.

	sanity_check(mode != TouchInteractionMode_USER);
	u64 now = porting::getTimeMs();
	bool target_dig_pressed = false;
	bool target_place_pressed = false;

	// If the meanings of short and long taps have been swapped, abort any ongoing
	// short taps because they would do something else than the player expected.
	// Long taps don't need this, they're adjusted to the swapped meanings instead.
	if (mode != m_last_mode) {
		m_dig_pressed_until = 0;
		m_place_pressed_until = 0;
	}
	m_last_mode = mode;

	switch (m_tap_state) {
	case TapState::ShortTap:
		if (mode == SHORT_DIG_LONG_PLACE) {
			if (!m_dig_pressed) {
				// The button isn't currently pressed, we can press it.
				m_dig_pressed_until = now + SIMULATED_CLICK_DURATION_MS;
				// We're done with this short tap.
				m_tap_state = TapState::None;
			}  else {
				// The button is already pressed, perhaps due to another short tap.
				// Release it now, press it again during the next client step.
				// We can't release and press during the same client step because
				// the digging code simply ignores that.
				m_dig_pressed_until = 0;
			}
		} else {
			if (!m_place_pressed) {
				// The button isn't currently pressed, we can press it.
				m_place_pressed_until = now + SIMULATED_CLICK_DURATION_MS;
				// We're done with this short tap.
				m_tap_state = TapState::None;
			}  else {
				// The button is already pressed, perhaps due to another short tap.
				// Release it now, press it again during the next client step.
				// We can't release and press during the same client step because
				// the digging code simply ignores that.
				m_place_pressed_until = 0;
			}
		}
		break;

	case TapState::LongTap:
		if (mode == SHORT_DIG_LONG_PLACE)
			target_place_pressed = true;
		else
			target_dig_pressed = true;
		break;

	case TapState::None:
		break;
	}

	// Apply short taps.
	target_dig_pressed |= now < m_dig_pressed_until;
	target_place_pressed |= now < m_place_pressed_until;

	if (target_dig_pressed && !m_dig_pressed) {
		emitMouseEvent(EMIE_LMOUSE_PRESSED_DOWN);
		m_dig_pressed = true;

	} else if (!target_dig_pressed && m_dig_pressed) {
		emitMouseEvent(EMIE_LMOUSE_LEFT_UP);
		m_dig_pressed = false;
	}

	if (target_place_pressed && !m_place_pressed) {
		emitMouseEvent(EMIE_RMOUSE_PRESSED_DOWN);
		m_place_pressed = true;

	} else if (!target_place_pressed && m_place_pressed) {
		emitMouseEvent(EMIE_RMOUSE_LEFT_UP);
		m_place_pressed = false;
	}
}
