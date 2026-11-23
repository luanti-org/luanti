// Luanti DS fork
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include <IEventReceiver.h>
#include <SColor.h>
#include <rect.h>
#include <optional>
#include <vector>

class IrrlichtDevice;
class ISecondaryWindow;
class Client;
class Hud;
namespace video { class IVideoDriver; }
namespace gui { class IGUIEnvironment; }

/*
	The bottom ("secondary") screen of the dual-screen layout.

	Owns the second native window and a GUI environment of its own, so that
	menus (formspecs) can be shown there. Receives the pointer input that
	happens inside that window and draws the window once per frame, after
	the main window.

	When no menu is open on it, it shows a dashboard: FPS, player position,
	an Inventory button and the hotbar.
*/
class SecondaryScreen : public IEventReceiver
{
public:
	static constexpr video::SColor BACKGROUND_COLOR{0xFF808080}; // neutral mid gray

	// Returns nullptr if the window could not be created
	static SecondaryScreen *create(IrrlichtDevice *device);
	~SecondaryScreen();

	// Set while a game is running (both nullptr otherwise)
	void setGame(Client *client, Hud *hud);

	void draw(video::IVideoDriver *driver);

	// Input from the secondary window
	bool OnEvent(const SEvent &event) override;

	gui::IGUIEnvironment *getGUIEnvironment() const { return m_guienv; }

	// Whether the bottom window has keyboard focus
	bool isWindowFocused() const;

	// Whether the active (topmost) menu is shown on this screen
	bool hostsTopMenu() const;

	// Sends keyboard/text input to this screen's GUI (e.g. a formspec field)
	void postToGUI(const SEvent &event);

	// Requests from the dashboard, polled by the game
	bool consumeInventoryRequest();
	std::optional<u16> consumeHotbarSelection();

private:
	SecondaryScreen(IrrlichtDevice *device, ISecondaryWindow *window);

	void drawDashboard(video::IVideoDriver *driver, v2u32 size);
	void handlePointer(const SEvent &event);
	// -1: Inventory button, >= 0: hotbar slot, nullopt: nothing
	std::optional<s32> hitTest(v2s32 pos) const;
	void activate(s32 target);

	IrrlichtDevice *m_device;
	ISecondaryWindow *m_window;
	gui::IGUIEnvironment *m_guienv = nullptr;

	Client *m_client = nullptr;
	Hud *m_hud = nullptr;

	// Layout of the last drawn dashboard, for hit testing
	core::rect<s32> m_button_rect;
	std::vector<core::rect<s32>> m_slot_rects;

	// Pointer state
	std::optional<s32> m_pressed;   // target under the pointer when pressed
	s32 m_touch_id = -1;            // finger being tracked, -1 if none

	// Pending requests
	bool m_inventory_requested = false;
	std::optional<u16> m_hotbar_selection;

	// FPS counter
	u64 m_fps_time_start = 0;
	u32 m_fps_frames = 0;
	u32 m_fps = 0;
};
