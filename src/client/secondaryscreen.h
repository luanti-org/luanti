// Luanti DS fork
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"
#include "irr_v2d.h"
#include <IEventReceiver.h>
#include <SColor.h>

class IrrlichtDevice;
class ISecondaryWindow;
namespace video { class IVideoDriver; }

/*
The bottom ("secondary") screen of the dual-screen layout.

Owns the second native window, receives the pointer input that happens
inside it and draws it once per frame, after the main window.

For now it only shows the retro background plus a pointer probe that
marks the last click/tap in window pixels, which verifies that input
coordinates map 1:1 onto what is drawn.
*/
class SecondaryScreen : public IEventReceiver
{
public:
	static constexpr video::SColor BACKGROUND_COLOR{0xFFF0F0E8}; // #F0F0E8

	// Returns nullptr if the window could not be created
	static SecondaryScreen *create(IrrlichtDevice *device);
	~SecondaryScreen();

	void draw(video::IVideoDriver *driver);

	bool OnEvent(const SEvent &event) override;

	ISecondaryWindow *getWindow() const { return m_window; }

private:
	SecondaryScreen(IrrlichtDevice *device, ISecondaryWindow *window);

	IrrlichtDevice *m_device;
	ISecondaryWindow *m_window;

	// Pointer probe
	bool m_has_pointer = false;
	bool m_pointer_down = false;
	bool m_pointer_is_touch = false;
	v2s32 m_pointer_pos;
};
