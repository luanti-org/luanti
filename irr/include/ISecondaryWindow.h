// Luanti DS fork
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "dimension2d.h"
#include "IEventReceiver.h"
#include "SExposedVideoData.h"

//! A second native window/surface that shares the device's rendering context.
/** Intended for dual-screen setups: the device's main window shows the 3D
world, this one shows utility UI.

Rendering: pass getVideoData() to IVideoDriver::beginScene(). Everything drawn
until the matching endScene() goes to this window, getScreenSize() reports this
window's size, and endScene() presents this window and switches back to the
main window.

Input: pointer (mouse, touch) events that happen inside this window are sent
to the receiver set with setEventReceiver(), in this window's pixel
coordinates. They never reach the device's main receiver or GUI environment.
Keyboard and text events go to this receiver first while this window has
keyboard focus; if it does not absorb them, they continue down the normal
device pipeline, so game keys keep working. */
class ISecondaryWindow
{
public:
	virtual ~ISecondaryWindow() = default;

	//! Size of the drawable area in pixels
	virtual core::dimension2du getSize() const = 0;

	//! Whether this window currently has keyboard focus
	virtual bool isFocused() const = 0;

	//! Set the window title
	virtual void setCaption(const wchar_t *text) = 0;

	//! Receiver for input events that belong to this window. Not owned.
	virtual void setEventReceiver(IEventReceiver *receiver) = 0;
	virtual IEventReceiver *getEventReceiver() const = 0;

	//! Pass this to IVideoDriver::beginScene() to render into this window
	virtual const video::SExposedVideoData &getVideoData() const = 0;
};
