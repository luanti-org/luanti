// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_X11_DEVICE_

#include "CIrrDeviceStub.h"
#include "IrrlichtDevice.h"
#include "ICursorControl.h"
#include "os.h"
namespace video
{
	class ITexture;
}

#ifdef _IRR_COMPILE_WITH_X11_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#ifdef _IRR_LINUX_X11_XINPUT2_
#include <X11/extensions/XInput2.h>
#endif

#else
#define KeySym s32
#endif

class CIrrDeviceLinux : public CIrrDeviceStub
{
public:
	//! constructor
	CIrrDeviceLinux(const SIrrlichtCreationParameters &param);

	//! destructor
	virtual ~CIrrDeviceLinux();

	//! runs the device. Returns false if device wants to be deleted
	bool run() override;

	//! Cause the device to temporarily pause execution and let other processes to run
	// This should bring down processor usage without major performance loss for Irrlicht
	void yield() override;

	//! Pause execution and let other processes to run for a specified amount of time.
	void sleep(u32 timeMs, bool pauseTimer) override;

	//! sets the caption of the window
	void setWindowCaption(const wchar_t *text) override;

	//! Sets the window icon.
	bool setWindowIcon(const video::IImage *img) override;

	//! returns if window is active. if not, nothing need to be drawn
	bool isWindowActive() const override;

	//! returns if window has focus.
	bool isWindowFocused() const override;

	//! returns if window is minimized.
	bool isWindowMinimized() const override;

	//! returns last state from maximizeWindow() and restoreWindow()
	bool isWindowMaximized() const override;

	//! Checks if the Irrlicht device supports touch events.
	bool supportsTouchEvents() const override;

	//! returns color format of the window.
	video::ECOLOR_FORMAT getColorFormat() const override;

	//! notifies the device that it should close itself
	void closeDevice() override;

	//! Sets if the window should be resizable in windowed mode.
	void setResizable(bool resize = false) override;

	//! Resize the render window.
	void setWindowSize(const core::dimension2d<u32> &size) override;

	//! Minimizes the window.
	void minimizeWindow() override;

	//! Maximizes the window.
	void maximizeWindow() override;

	//! Restores the window size.
	void restoreWindow() override;

	//! Get the position of this window on screen
	core::position2di getWindowPosition() override;

	//! Activate any joysticks, and generate events for them.
	bool activateJoysticks(core::array<SJoystickInfo> &joystickInfo) override;

	//! gets text from the clipboard
	//! \return Returns 0 if no string is in there, otherwise utf-8 text.
	virtual const c8 *getTextFromClipboard() const;

	//! gets text from the primary selection
	//! \return Returns 0 if no string is in there, otherwise utf-8 text.
	virtual const c8 *getTextFromPrimarySelection() const;

	//! copies text to the clipboard
	//! This sets the clipboard selection and _not_ the primary selection.
	//! @param text The text in utf-8
	virtual void copyToClipboard(const c8 *text) const;

	//! copies text to the primary selection
	//! This sets the primary selection which you have on X on the middle mouse button.
	//! @param text The text in utf-8
	virtual void copyToPrimarySelection(const c8 *text) const;

	//! Remove all messages pending in the system message loop
	void clearSystemMessages() override;

	//! Get the device type
	E_DEVICE_TYPE getType() const override
	{
		return EIDT_X11;
	}

	//! Get the display density in dots per inch.
	float getDisplayDensity() const override;

#ifdef _IRR_COMPILE_WITH_X11_
	// convert an Irrlicht texture to a X11 cursor
	Cursor TextureToCursor(video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot);
	Cursor TextureToMonochromeCursor(video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot);
#ifdef _IRR_LINUX_XCURSOR_
	Cursor TextureToARGBCursor(video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot);
#endif
#endif

private:
	//! create the driver
	void createDriver();

	bool createWindow();

	void createKeyMap();

	void pollJoysticks();

	void initXAtoms();

	void initXInput2();

	bool switchToFullscreen();

	void setupTopLevelXorgWindow();

#ifdef _IRR_COMPILE_WITH_X11_
	bool createInputContext();
	void destroyInputContext();
	EKEY_CODE getKeyCode(XEvent &event);

	const c8 *getTextFromSelection(Atom selection, core::stringc &text_buffer) const;
	bool becomeSelectionOwner(Atom selection) const;
#endif

	//! Implementation of the linux cursor control
	class CCursorControl : public gui::ICursorControl
	{
	public:
		CCursorControl(CIrrDeviceLinux *dev, bool null);

		~CCursorControl();

		//! Changes the visible state of the mouse cursor.
		void setVisible(bool visible) override
		{
			if (visible == IsVisible)
				return;
			IsVisible = visible;
#ifdef _IRR_COMPILE_WITH_X11_
			if (!Null) {
				if (!IsVisible)
					XDefineCursor(Device->XDisplay, Device->XWindow, InvisCursor);
				else
					XUndefineCursor(Device->XDisplay, Device->XWindow);
			}
#endif
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
			setPosition((s32)(x * Device->Width), (s32)(y * Device->Height));
		}

		//! Sets the new position of the cursor.
		void setPosition(const core::position2d<s32> &pos) override
		{
			setPosition(pos.X, pos.Y);
		}

		//! Sets the new position of the cursor.
		void setPosition(s32 x, s32 y) override
		{
#ifdef _IRR_COMPILE_WITH_X11_

			if (!Null) {
				if (UseReferenceRect) {
// NOTE: XIWarpPointer works when X11 has set a coordinate transformation matrix for the mouse unlike XWarpPointer
// which runs into a bug mentioned here: https://gitlab.freedesktop.org/xorg/xserver/-/issues/600
// So also workaround for Irrlicht bug #450
#ifdef _IRR_LINUX_X11_XINPUT2_
					if (DeviceId != 0) {
						XIWarpPointer(Device->XDisplay,
								DeviceId,
								None,
								Device->XWindow, 0, 0,
								Device->Width,
								Device->Height,
								ReferenceRect.UpperLeftCorner.X + x,
								ReferenceRect.UpperLeftCorner.Y + y);
					} else
#endif
					{
						XWarpPointer(Device->XDisplay,
								None,
								Device->XWindow, 0, 0,
								Device->Width,
								Device->Height,
								ReferenceRect.UpperLeftCorner.X + x,
								ReferenceRect.UpperLeftCorner.Y + y);
					}
				} else {
#ifdef _IRR_LINUX_X11_XINPUT2_
					if (DeviceId != 0) {
						XIWarpPointer(Device->XDisplay,
								DeviceId,
								None,
								Device->XWindow, 0, 0,
								Device->Width,
								Device->Height, x, y);
					} else
#endif
					{
						XWarpPointer(Device->XDisplay,
								None,
								Device->XWindow, 0, 0,
								Device->Width,
								Device->Height, x, y);
					}
				}
				XFlush(Device->XDisplay);
			}
#endif
			CursorPos.X = x;
			CursorPos.Y = y;
		}

		//! Returns the current position of the mouse cursor.
		const core::position2d<s32> &getPosition(bool updateCursor) override
		{
			if (updateCursor)
				updateCursorPos();
			return CursorPos;
		}

		//! Returns the current position of the mouse cursor.
		core::position2d<f32> getRelativePosition(bool updateCursor) override
		{
			if (updateCursor)
				updateCursorPos();

			if (!UseReferenceRect) {
				return core::position2d<f32>(CursorPos.X / (f32)Device->Width,
						CursorPos.Y / (f32)Device->Height);
			}

			return core::position2d<f32>(CursorPos.X / (f32)ReferenceRect.getWidth(),
					CursorPos.Y / (f32)ReferenceRect.getHeight());
		}

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

		//! Sets the active cursor icon
		void setActiveIcon(gui::ECURSOR_ICON iconId) override;

		//! Gets the currently active icon
		gui::ECURSOR_ICON getActiveIcon() const override
		{
			return ActiveIcon;
		}

		//! Add a custom sprite as cursor icon.
		gui::ECURSOR_ICON addIcon(const gui::SCursorSprite &icon) override;

		//! replace the given cursor icon.
		void changeIcon(gui::ECURSOR_ICON iconId, const gui::SCursorSprite &icon) override;

		//! Return a system-specific size which is supported for cursors. Larger icons will fail, smaller icons might work.
		core::dimension2di getSupportedIconSize() const override;

#ifdef _IRR_COMPILE_WITH_X11_
		//! Set platform specific behavior flags.
		void setPlatformBehavior(gui::ECURSOR_PLATFORM_BEHAVIOR behavior) override { PlatformBehavior = behavior; }

		//! Return platform specific behavior.
		gui::ECURSOR_PLATFORM_BEHAVIOR getPlatformBehavior() const override { return PlatformBehavior; }

		void update();
		void clearCursors();
#endif
	private:
		void updateCursorPos()
		{
#ifdef _IRR_COMPILE_WITH_X11_
			if (Null)
				return;

			if (PlatformBehavior & gui::ECPB_X11_CACHE_UPDATES && !os::Timer::isStopped()) {
				u32 now = os::Timer::getTime();
				if (now <= LastQuery)
					return;
				LastQuery = now;
			}

			Window tmp;
			int itmp1, itmp2;
			unsigned int maskreturn;
			XQueryPointer(Device->XDisplay, Device->XWindow,
					&tmp, &tmp,
					&itmp1, &itmp2,
					&CursorPos.X, &CursorPos.Y, &maskreturn);
#endif
		}

		CIrrDeviceLinux *Device;
		core::position2d<s32> CursorPos;
		core::rect<s32> ReferenceRect;
#ifdef _IRR_COMPILE_WITH_X11_
		gui::ECURSOR_PLATFORM_BEHAVIOR PlatformBehavior;
		u32 LastQuery;
		Cursor InvisCursor;

#ifdef _IRR_LINUX_X11_XINPUT2_
		int DeviceId;
#endif

		struct CursorFrameX11
		{
			CursorFrameX11() :
					IconHW(0) {}
			CursorFrameX11(Cursor icon) :
					IconHW(icon) {}

			Cursor IconHW; // hardware cursor
		};

		struct CursorX11
		{
			CursorX11() {}
			explicit CursorX11(Cursor iconHw, u32 frameTime = 0) :
					FrameTime(frameTime)
			{
				Frames.push_back(CursorFrameX11(iconHw));
			}
			core::array<CursorFrameX11> Frames;
			u32 FrameTime;
		};

		core::array<CursorX11> Cursors;

		void initCursors();
#endif
		bool IsVisible;
		bool Null;
		bool UseReferenceRect;
		gui::ECURSOR_ICON ActiveIcon;
		u32 ActiveIconStartTime;
	};

	friend class CCursorControl;

#ifdef _IRR_COMPILE_WITH_X11_
	friend class COpenGLDriver;

	Display *XDisplay;
	XVisualInfo *VisualInfo;
	int Screennr;
	Window XWindow;
	XSetWindowAttributes WndAttributes;
	XSizeHints *StdHints;
	XIM XInputMethod;
	XIC XInputContext;
	bool HasNetWM;
	// text is utf-8
	mutable core::stringc Clipboard;
	mutable core::stringc PrimarySelection;
#endif
#if defined(_IRR_LINUX_X11_XINPUT2_)
	int currentTouchedCount;
#endif
	u32 Width, Height;
	bool WindowHasFocus;
	bool WindowMinimized;
	bool WindowMaximized;
	bool ExternalWindow;
	int AutorepeatSupport;

	struct SKeyMap
	{
		SKeyMap() {}
		SKeyMap(s32 x11, s32 win32) :
				X11Key(x11), Win32Key(win32)
		{
		}

		KeySym X11Key;
		s32 Win32Key;

		bool operator<(const SKeyMap &o) const
		{
			return X11Key < o.X11Key;
		}
	};

	core::array<SKeyMap> KeyMap;

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	struct JoystickInfo
	{
		int fd;
		int axes;
		int buttons;

		SEvent persistentData;

		JoystickInfo() :
				fd(-1), axes(0), buttons(0) {}
	};
	core::array<JoystickInfo> ActiveJoysticks;
#endif
};

#endif // _IRR_COMPILE_WITH_X11_DEVICE_
