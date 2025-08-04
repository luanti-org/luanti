// Copyright (C) 2005-2006 Etienne Petitjean
// Copyright (C) 2007-2012 Christian Stehno
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_OSX_DEVICE_

#include "CIrrDeviceStub.h"
#include "IrrlichtDevice.h"
#include "IGUIEnvironment.h"
#include "ICursorControl.h"

#import <AppKit/NSWindow.h>
#import <AppKit/NSBitmapImageRep.h>

#include <map>

class CIrrDeviceMacOSX;

@interface CIrrDelegateOSX : NSObject <NSApplicationDelegate, NSWindowDelegate>

- (id)initWithDevice:(CIrrDeviceMacOSX *)device;
- (void)terminate:(id)sender;
- (BOOL)isQuit;

@end

class CIrrDeviceMacOSX : public CIrrDeviceStub
{
public:
	//! constructor
	CIrrDeviceMacOSX(const SIrrlichtCreationParameters &params);

	//! destructor
	virtual ~CIrrDeviceMacOSX();

	//! runs the device. Returns false if device wants to be deleted
	bool run() override;

	//! Cause the device to temporarily pause execution and let other processes to run
	// This should bring down processor usage without major performance loss for Irrlicht
	void yield() override;

	//! Pause execution and let other processes to run for a specified amount of time.
	void sleep(u32 timeMs, bool pauseTimer) override;

	//! sets the caption of the window
	void setWindowCaption(const wchar_t *text) override;

	//! returns if window is active. if not, nothing need to be drawn
	bool isWindowActive() const override;

	//! Checks if the Irrlicht window has focus
	bool isWindowFocused() const override;

	//! Checks if the Irrlicht window is minimized
	bool isWindowMinimized() const override;

	//! notifies the device that it should close itself
	void closeDevice() override;

	//! Sets if the window should be resizable in windowed mode.
	void setResizable(bool resize) override;

	//! Returns true if the window is resizable, false if not
	virtual bool isResizable() const;

	//! Minimizes the window if possible
	void minimizeWindow() override;

	//! Maximizes the window if possible.
	void maximizeWindow() override;

	//! Restore the window to normal size if possible.
	void restoreWindow() override;

	//! Get the position of this window on screen
	core::position2di getWindowPosition() override;

	//! Activate any joysticks, and generate events for them.
	bool activateJoysticks(core::array<SJoystickInfo> &joystickInfo) override;

	//! Get the device type
	E_DEVICE_TYPE getType() const override
	{
		return EIDT_OSX;
	}

	void setMouseLocation(int x, int y);
	void setResize(int width, int height);
	void setCursorVisible(bool visible);
	void setWindow(NSWindow *window);

private:
	//! create the driver
	void createDriver();

	//! Implementation of the macos x cursor control
	class CCursorControl : public gui::ICursorControl
	{
	public:
		CCursorControl(const core::dimension2d<u32> &wsize, CIrrDeviceMacOSX *device) :
				WindowSize(wsize), InvWindowSize(0.0f, 0.0f), Device(device), IsVisible(true), UseReferenceRect(false)
		{
			CursorPos.X = CursorPos.Y = 0;
			if (WindowSize.Width != 0)
				InvWindowSize.Width = 1.0f / WindowSize.Width;
			if (WindowSize.Height != 0)
				InvWindowSize.Height = 1.0f / WindowSize.Height;
		}

		//! Changes the visible state of the mouse cursor.
		void setVisible(bool visible) override
		{
			IsVisible = visible;
			Device->setCursorVisible(visible);
		}

		//! Returns if the cursor is currently visible.
		bool isVisible() const override
		{
			return IsVisible;
		}

		//! Sets the new position of the cursor.
		void setPosition(const core::position2d<f32> &pos) override
		{
			setPosition(pos.X, pos.Y);
		}

		//! Sets the new position of the cursor.
		void setPosition(f32 x, f32 y) override
		{
			setPosition((s32)(x * WindowSize.Width), (s32)(y * WindowSize.Height));
		}

		//! Sets the new position of the cursor.
		void setPosition(const core::position2d<s32> &pos) override
		{
			if (CursorPos.X != pos.X || CursorPos.Y != pos.Y)
				setPosition(pos.X, pos.Y);
		}

		//! Sets the new position of the cursor.
		void setPosition(s32 x, s32 y) override
		{
			if (UseReferenceRect) {
				Device->setMouseLocation(ReferenceRect.UpperLeftCorner.X + x, ReferenceRect.UpperLeftCorner.Y + y);
			} else {
				Device->setMouseLocation(x, y);
			}
		}

		//! Returns the current position of the mouse cursor.
		const core::position2d<s32> &getPosition(bool updateCursor) override
		{
			return CursorPos;
		}

		//! Returns the current position of the mouse cursor.
		core::position2d<f32> getRelativePosition(bool updateCursor) override
		{
			if (!UseReferenceRect) {
				return core::position2d<f32>(CursorPos.X * InvWindowSize.Width,
						CursorPos.Y * InvWindowSize.Height);
			}

			return core::position2d<f32>(CursorPos.X / (f32)ReferenceRect.getWidth(),
					CursorPos.Y / (f32)ReferenceRect.getHeight());
		}

		//! Sets an absolute reference rect for calculating the cursor position.
		void setReferenceRect(core::rect<s32> *rect = 0) override
		{
			if (rect) {
				ReferenceRect = *rect;
				UseReferenceRect = true;

				// prevent division through zero and uneven sizes

				if (!ReferenceRect.getHeight() || ReferenceRect.getHeight() % 2)
					ReferenceRect.LowerRightCorner.Y += 1;

				if (!ReferenceRect.getWidth() || ReferenceRect.getWidth() % 2)
					ReferenceRect.LowerRightCorner.X += 1;
			} else
				UseReferenceRect = false;
		}

		//! Updates the internal cursor position
		void updateInternalCursorPosition(int x, int y)
		{
			CursorPos.X = x;
			CursorPos.Y = y;
		}

	private:
		core::position2d<s32> CursorPos;
		core::dimension2d<s32> WindowSize;
		core::dimension2d<float> InvWindowSize;
		core::rect<s32> ReferenceRect;
		CIrrDeviceMacOSX *Device;
		bool IsVisible;
		bool UseReferenceRect;
	};

	bool createWindow();
	void initKeycodes();
	void storeMouseLocation();
	void postMouseEvent(void *event, SEvent &ievent);
	void postKeyEvent(void *event, SEvent &ievent, bool pressed);
	void pollJoysticks();

	NSWindow *Window;
	CGDirectDisplayID Display;
	std::map<int, int> KeyCodes;
	int DeviceWidth;
	int DeviceHeight;
	int ScreenWidth;
	int ScreenHeight;
	u32 MouseButtonStates;
	bool IsFullscreen;
	bool IsActive;
	bool IsShiftDown;
	bool IsControlDown;
	bool IsResizable;
};

#endif // _IRR_COMPILE_WITH_OSX_DEVICE_
