// Luanti DS fork
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "secondaryscreen.h"
#include <algorithm>
#include <string>
#include "client/client.h"
#include "client/clientenvironment.h"
#include "client/fontengine.h"
#include "client/hud.h"
#include "client/localplayer.h"
#include "client/renderingengine.h"
#include "constants.h"
#include "gui/mainmenumanager.h"
#include "log.h"
#include "porting.h"
#include "settings.h"
#include "util/numeric.h"
#include <IrrlichtDevice.h>
#include <ISecondaryWindow.h>
#include <IVideoDriver.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>

static constexpr s32 TARGET_INVENTORY_BUTTON = -1;

SecondaryScreen *SecondaryScreen::create(IrrlichtDevice *device)
{
	core::dimension2du size(
		std::max<u16>(g_settings->getU16("dual_screen_w"), 1),
		std::max<u16>(g_settings->getU16("dual_screen_h"), 1));

	ISecondaryWindow *window = device->createSecondaryWindow(size, L"Luanti DS - Bottom screen");
	if (!window) {
		warningstream << "Dual screen: could not open the secondary window, "
				"continuing with a single screen" << std::endl;
		return nullptr;
	}
	infostream << "Dual screen: secondary window is " << window->getSize().Width
			<< "x" << window->getSize().Height << std::endl;
	return new SecondaryScreen(device, window);
}

SecondaryScreen::SecondaryScreen(IrrlichtDevice *device, ISecondaryWindow *window) :
	m_device(device), m_window(window)
{
	m_guienv = m_device->createGUIEnvironment();
	sanity_check(m_guienv);
	// Same look as the main window: share its skin (colors, font, sizes)
	m_guienv->setSkin(m_device->getGUIEnvironment()->getSkin());

	m_window->setEventReceiver(this);
	m_window->setGUIEnvironment(m_guienv);
}

SecondaryScreen::~SecondaryScreen()
{
	m_window->setEventReceiver(nullptr);
	m_window->setGUIEnvironment(nullptr);
	m_guienv->drop();
	m_device->destroySecondaryWindow();
}

void SecondaryScreen::setGame(Client *client, Hud *hud)
{
	m_client = client;
	m_hud = hud;
	if (m_hud)
		m_hud->setHotbarOnSecondary(true);

	m_pressed.reset();
	m_touch_id = -1;
	m_inventory_requested = false;
	m_hotbar_selection.reset();
	m_slot_rects.clear();
}

bool SecondaryScreen::isWindowFocused() const
{
	return m_window->isFocused();
}

bool SecondaryScreen::hostsTopMenu() const
{
	GUIModalMenu *top = g_menumgr.tryGetTopMenu();
	return top && top->getEnvironment() == m_guienv;
}

void SecondaryScreen::postToGUI(const SEvent &event)
{
	m_guienv->postEventFromUser(event);
}

bool SecondaryScreen::consumeInventoryRequest()
{
	bool ret = m_inventory_requested;
	m_inventory_requested = false;
	return ret;
}

std::optional<u16> SecondaryScreen::consumeHotbarSelection()
{
	auto ret = m_hotbar_selection;
	m_hotbar_selection.reset();
	return ret;
}

/*
	Input
*/

bool SecondaryScreen::OnEvent(const SEvent &event)
{
	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		if (!event.MouseInput.Simulated)
			RenderingEngine::setLastPointerType(PointerType::Mouse);
	} else if (event.EventType == EET_TOUCH_INPUT_EVENT) {
		RenderingEngine::setLastPointerType(PointerType::Touch);
	} else {
		// Keyboard input continues to the main receiver, which forwards it
		// here (postToGUI) if a menu on this screen is active
		return false;
	}

	if (hostsTopMenu()) {
		// Same order as for the main window: menu preprocessing (this turns
		// touches into mouse events), then the GUI environment
		if (!g_menumgr.runPreprocessEvent(event))
			m_guienv->postEventFromUser(event);
		return true;
	}

	if (isMenuActive()) {
		// A menu on the main window is modal: the dashboard waits
		m_pressed.reset();
		m_touch_id = -1;
		return true;
	}

	handlePointer(event);
	return true;
}

void SecondaryScreen::handlePointer(const SEvent &event)
{
	enum { NONE, DOWN, UP } phase = NONE;
	v2s32 pos;

	if (event.EventType == EET_MOUSE_INPUT_EVENT) {
		pos = v2s32(event.MouseInput.X, event.MouseInput.Y);
		if (event.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN)
			phase = DOWN;
		else if (event.MouseInput.Event == EMIE_LMOUSE_LEFT_UP)
			phase = UP;
	} else {
		pos = v2s32(event.TouchInput.X, event.TouchInput.Y);
		const s32 id = static_cast<s32>(event.TouchInput.ID);
		if (event.TouchInput.Event == ETIE_PRESSED_DOWN && m_touch_id < 0) {
			m_touch_id = id;
			phase = DOWN;
		} else if (event.TouchInput.Event == ETIE_LEFT_UP && id == m_touch_id) {
			m_touch_id = -1;
			phase = UP;
		}
	}

	if (phase == DOWN) {
		m_pressed = hitTest(pos);
	} else if (phase == UP) {
		// Activate on release over the same target, like a normal button
		if (m_pressed && hitTest(pos) == m_pressed)
			activate(*m_pressed);
		m_pressed.reset();
	}
}

std::optional<s32> SecondaryScreen::hitTest(v2s32 pos) const
{
	if (!m_client)
		return std::nullopt;
	if (m_button_rect.isPointInside(pos))
		return TARGET_INVENTORY_BUTTON;
	for (size_t i = 0; i < m_slot_rects.size(); i++) {
		if (m_slot_rects[i].isPointInside(pos))
			return static_cast<s32>(i);
	}
	return std::nullopt;
}

void SecondaryScreen::activate(s32 target)
{
	if (target == TARGET_INVENTORY_BUTTON)
		m_inventory_requested = true;
	else if (target >= 0)
		m_hotbar_selection = static_cast<u16>(target);
}

/*
	Drawing
*/

void SecondaryScreen::draw(video::IVideoDriver *driver)
{
	// FPS of the whole program, updated twice a second
	const u64 now = porting::getTimeMs();
	m_fps_frames++;
	if (now - m_fps_time_start >= 500) {
		if (m_fps_time_start != 0)
			m_fps = m_fps_frames * 1000 / (now - m_fps_time_start);
		m_fps_time_start = now;
		m_fps_frames = 0;
	}

	driver->beginScene(true, true, BACKGROUND_COLOR, m_window->getVideoData());

	const v2u32 size = driver->getScreenSize();
	if (hostsTopMenu()) {
		// The menu gets the whole screen
		m_button_rect = core::rect<s32>();
		m_slot_rects.clear();
	} else {
		drawDashboard(driver, size);
	}

	m_guienv->drawAll();

	driver->endScene();
}

void SecondaryScreen::drawDashboard(video::IVideoDriver *driver, v2u32 size)
{
	m_button_rect = core::rect<s32>();
	m_slot_rects.clear();

	if (!g_fontengine)
		return;

	const f32 scale = rangelim(g_settings->getFloat("gui_scaling"), 0.5f, 20.0f) *
			RenderingEngine::getDisplayDensity();
	const s32 margin = 12 * scale;
	const s32 w = size.X, h = size.Y;

	const video::SColor text_color(255, 245, 245, 245);
	const video::SColor dim_color(255, 215, 215, 215);

	gui::IGUIFont *font = g_fontengine->getFont();
	gui::IGUIFont *big_font = g_fontengine->getFont(g_fontengine->getDefaultFontSize() * 1.6f);
	if (!font)
		return;
	if (!big_font)
		big_font = font;
	const s32 line_h = font->getDimension(L"Ay").Height;

	// Top line: FPS on the left, position on the right
	std::wstring fps_text = L"FPS " + std::to_wstring(m_fps);
	font->draw(fps_text.c_str(),
			core::rect<s32>(margin, margin, w / 2, margin + line_h), text_color);

	LocalPlayer *player = m_client ? m_client->getEnv().getLocalPlayer() : nullptr;

	if (!player) {
		// Main menu or loading
		big_font->draw(L"Luanti DS", core::rect<s32>(0, 0, w, h), text_color, true, true);
		font->draw(L"Start a world to use this screen",
				core::rect<s32>(0, h / 2 + line_h * 2, w, h / 2 + line_h * 3),
				dim_color, true, false);
		return;
	}

	v3s16 p = floatToInt(player->getPosition(), BS);
	std::wstring pos_text = L"X " + std::to_wstring(p.X) + L"   Y " +
			std::to_wstring(p.Y) + L"   Z " + std::to_wstring(p.Z);
	s32 pos_w = font->getDimension(pos_text.c_str()).Width;
	font->draw(pos_text.c_str(),
			core::rect<s32>(w - margin - pos_w, margin, w - margin, margin + line_h),
			text_color);

	// Bottom: hotbar
	s32 hotbar_top = h - margin;
	if (m_hud && m_hud->isHotbarVisible()) {
		s32 hotbar_h = std::min<s32>((w - 2 * margin) / 8, h / 4);
		core::rect<s32> area(margin, h - margin - hotbar_h, w - margin, h - margin);
		m_hud->drawHotbarInRect(area, m_slot_rects);
		hotbar_top = area.UpperLeftCorner.Y;
	}

	// Middle: Inventory button, large enough to hit with a thumb
	const s32 top = margin * 2 + line_h;
	const s32 avail_h = hotbar_top - margin - top;
	const s32 btn_w = std::min<s32>(w - 4 * margin, 320 * scale);
	const s32 btn_h = std::min<s32>(avail_h, 80 * scale);
	if (btn_w <= 0 || btn_h <= 0)
		return;
	const s32 btn_x = (w - btn_w) / 2;
	const s32 btn_y = top + (avail_h - btn_h) / 2;
	m_button_rect = core::rect<s32>(btn_x, btn_y, btn_x + btn_w, btn_y + btn_h);

	const bool pressed = m_pressed && *m_pressed == TARGET_INVENTORY_BUTTON;
	const s32 border = std::max<s32>(2 * scale, 1);
	driver->draw2DRectangle(video::SColor(255, 200, 200, 200), m_button_rect);
	core::rect<s32> inner(m_button_rect.UpperLeftCorner + v2s32(border, border),
			m_button_rect.LowerRightCorner - v2s32(border, border));
	driver->draw2DRectangle(pressed ? video::SColor(255, 64, 64, 64) :
			video::SColor(255, 96, 96, 96), inner);
	big_font->draw(L"Inventory", m_button_rect, text_color, true, true);
}
