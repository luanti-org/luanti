// Luanti DS fork
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "secondaryscreen.h"
#include "client/fontengine.h"
#include <algorithm>
#include "log.h"
#include "settings.h"
#include <IrrlichtDevice.h>
#include <ISecondaryWindow.h>
#include <IVideoDriver.h>
#include <IGUIFont.h>

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
	m_window->setEventReceiver(this);
}

SecondaryScreen::~SecondaryScreen()
{
	m_window->setEventReceiver(nullptr);
	m_device->destroySecondaryWindow();
}

bool SecondaryScreen::OnEvent(const SEvent &event)
{
	switch (event.EventType) {
	case EET_MOUSE_INPUT_EVENT:
		if (event.MouseInput.Event == EMIE_MOUSE_WHEEL)
			return true;
		m_has_pointer = true;
		m_pointer_is_touch = false;
		m_pointer_pos = v2s32(event.MouseInput.X, event.MouseInput.Y);
		m_pointer_down = event.MouseInput.isLeftPressed();
		return true;
	case EET_TOUCH_INPUT_EVENT:
		m_has_pointer = true;
		m_pointer_is_touch = true;
		m_pointer_pos = v2s32(event.TouchInput.X, event.TouchInput.Y);
		m_pointer_down = event.TouchInput.Event != ETIE_LEFT_UP;
		return true;
	default:
		// Keyboard input falls through to the game
		return false;
	}
}

void SecondaryScreen::draw(video::IVideoDriver *driver)
{
	driver->beginScene(true, true, BACKGROUND_COLOR, m_window->getVideoData());

	const v2u32 size = driver->getScreenSize();
	const video::SColor ink(255, 64, 64, 56);
	const video::SColor accent(255, 200, 80, 40);

	gui::IGUIFont *font = g_fontengine ? g_fontengine->getFont() : nullptr;

	if (font) {
		const s32 line_h = font->getDimension(L"Ay").Height;
		font->draw(L"Luanti DS - secondary screen",
				core::rect<s32>(0, line_h / 2, size.X, line_h * 2), ink, true, false);
		std::wstring info = std::to_wstring(size.X) + L" x " + std::to_wstring(size.Y) + L" px";
		font->draw(info.c_str(),
				core::rect<s32>(0, size.Y - line_h * 2, size.X, size.Y - line_h / 2),
				ink, true, false);
	}

	if (m_has_pointer) {
		// Crosshair through the last pointer position
		const video::SColor c = m_pointer_down ? accent : ink;
		const s32 x = m_pointer_pos.X, y = m_pointer_pos.Y;
		driver->draw2DRectangle(c, core::rect<s32>(x, 0, x + 1, size.Y));
		driver->draw2DRectangle(c, core::rect<s32>(0, y, size.X, y + 1));
		driver->draw2DRectangle(c, core::rect<s32>(x - 4, y - 4, x + 5, y + 5));

		if (font) {
			std::wstring label = std::wstring(m_pointer_is_touch ? L"touch " : L"mouse ") +
					std::to_wstring(x) + L", " + std::to_wstring(y);
			const s32 line_h = font->getDimension(L"Ay").Height;
			font->draw(label.c_str(), core::rect<s32>(x + 8, y + 4, x + 400, y + 4 + line_h),
					c, false, false);
		}
	}

	driver->endScene();
}
