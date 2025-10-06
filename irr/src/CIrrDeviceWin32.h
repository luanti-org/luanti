// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_

#include "CIrrDeviceStub.h"
#include "IrrlichtDevice.h"
#include "ITexture.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h> // For JOYCAPS
#include <windowsx.h>
#if !defined(GET_X_LPARAM)
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

struct SJoystickWin32Control;

class CIrrDeviceWin32 : public CIrrDeviceStub
{
	friend struct SJoystickWin32Control;

public:
	//! constructor
	CIrrDeviceWin32(const SIrrlichtCreationParameters &params);

	//! destructor
	virtual ~CIrrDeviceWin32();

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

	//! returns if window has focus
	bool isWindowFocused() const override;

	//! returns if window is minimized
	bool isWindowMinimized() const override;

	//! returns last state from maximizeWindow() and restoreWindow()
	bool isWindowMaximized() const override;

	//! notifies the device that it should close itself
	void closeDevice() override;

	//! Notifies the device, that it has been resized
	/** Must be publis as it is called from free function (event handler) */
	void OnResized();

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

	//! Get the position of the window on screen
	core::position2di getWindowPosition() override;

	//! Activate any joysticks, and generate events for them.
	bool activateJoysticks(core::array<SJoystickInfo> &joystickInfo) override;

	//! Remove all messages pending in the system message loop
	void clearSystemMessages() override;

	//! Get the device type
	E_DEVICE_TYPE getType() const override
	{
		return EIDT_WIN32;
	}

	//! Get the display density in dots per inch.
	float getDisplayDensity() const override;

	//! Compares to the last call of this function to return double and triple clicks.
	//! \return Returns only 1,2 or 3. A 4th click will start with 1 again.
	u32 checkSuccessiveClicks(s32 mouseX, s32 mouseY, EMOUSE_INPUT_EVENT inputEvent) override
	{
		// we just have to make it public
		return CIrrDeviceStub::checkSuccessiveClicks(mouseX, mouseY, inputEvent);
	}

	//! Switch to fullscreen
	bool switchToFullScreen();

	// convert an Irrlicht texture to a windows cursor
	HCURSOR TextureToCursor(HWND hwnd, video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot);

	//! Implementation of the win32 cursor control
	class CCursorControl : public gui::ICursorControl
	{
	public:
		CCursorControl(CIrrDeviceWin32 *device, const core::dimension2d<u32> &wsize, HWND hwnd, bool fullscreen);
		~CCursorControl();

		//! Changes the visible state of the mouse cursor.
		void setVisible(bool visible) override
		{
			CURSORINFO info;
			info.cbSize = sizeof(CURSORINFO);
			BOOL gotCursorInfo = GetCursorInfo(&info);
			while (gotCursorInfo) {
#ifdef CURSOR_SUPPRESSED
				// Since Windows 8 the cursor can be suppressed by a touch interface
				if (visible && info.flags == CURSOR_SUPPRESSED) {
					break;
				}
#endif
				if ((visible && info.flags == CURSOR_SHOWING) || // visible
						(!visible && info.flags == 0))           // hidden
				{
					break;
				}
				// this only increases an internal
				// display counter in windows, so it
				// might have to be called some more
				const int showResult = ShowCursor(visible);
				// if result has correct sign we can
				// stop here as well
				if ((!visible && showResult < 0) ||
						(visible && showResult >= 0))
					break;
				// yes, it really must be set each time
				info.cbSize = sizeof(CURSORINFO);
				gotCursorInfo = GetCursorInfo(&info);

#ifdef CURSOR_SUPPRESSED
				// Not sure if a cursor which we tried to hide still can be suppressed.
				// I have no touch-display for testing this and MSDN doesn't describe it.
				// But adding this check shouldn't hurt and might prevent an endless loop.
				if (!visible && info.flags == CURSOR_SUPPRESSED) {
					break;
				}
#endif
			}
			IsVisible = visible;
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
			if (!UseReferenceRect)
				setPosition(core::round32(x * WindowSize.Width), core::round32(y * WindowSize.Height));
			else
				setPosition(core::round32(x * ReferenceRect.getWidth()), core::round32(y * ReferenceRect.getHeight()));
		}

		//! Sets the new position of the cursor.
		void setPosition(const core::position2d<s32> &pos) override
		{
			setPosition(pos.X, pos.Y);
		}

		//! Sets the new position of the cursor.
		void setPosition(s32 x, s32 y) override
		{
			if (UseReferenceRect) {
				SetCursorPos(ReferenceRect.UpperLeftCorner.X + x,
						ReferenceRect.UpperLeftCorner.Y + y);
			} else {
				RECT rect;
				if (GetWindowRect(HWnd, &rect))
					SetCursorPos(x + rect.left + BorderX, y + rect.top + BorderY);
			}

			CursorPos.X = x;
			CursorPos.Y = y;
		}

		//! Returns the current position of the mouse cursor.
		const core::position2d<s32> &getPosition(bool updateCursor) override
		{
			if (updateCursor)
				updateInternalCursorPosition();
			return CursorPos;
		}

		//! Returns the current position of the mouse cursor.
		core::position2d<f32> getRelativePosition(bool updateCursor) override
		{
			if (updateCursor)
				updateInternalCursorPosition();

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

		/** Used to notify the cursor that the window was resized. */
		void OnResize(const core::dimension2d<u32> &size)
		{
			WindowSize = size;
			if (size.Width != 0)
				InvWindowSize.Width = 1.0f / size.Width;
			else
				InvWindowSize.Width = 0.f;

			if (size.Height != 0)
				InvWindowSize.Height = 1.0f / size.Height;
			else
				InvWindowSize.Height = 0.f;
		}

		/** Used to notify the cursor that the window resizable settings changed. */
		void updateBorderSize(bool fullscreen, bool resizable)
		{
			if (!fullscreen) {
				s32 paddingBorder = 0;
#ifdef SM_CXPADDEDBORDER
				paddingBorder = GetSystemMetrics(SM_CXPADDEDBORDER);
#endif

				if (resizable) {
					BorderX = GetSystemMetrics(SM_CXSIZEFRAME) + paddingBorder;
					BorderY = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYSIZEFRAME) + paddingBorder;
				} else {
					BorderX = GetSystemMetrics(SM_CXDLGFRAME) + paddingBorder;
					BorderY = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYDLGFRAME) + paddingBorder;
				}
			} else {
				BorderX = BorderY = 0;
			}
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

		void update();

	private:
		//! Updates the internal cursor position
		void updateInternalCursorPosition()
		{
			POINT p;
			if (!GetCursorPos(&p)) {
				DWORD xy = GetMessagePos();
				p.x = GET_X_LPARAM(xy);
				p.y = GET_Y_LPARAM(xy);
			}

			if (UseReferenceRect) {
				CursorPos.X = p.x - ReferenceRect.UpperLeftCorner.X;
				CursorPos.Y = p.y - ReferenceRect.UpperLeftCorner.Y;
			} else {
				RECT rect;
				if (GetWindowRect(HWnd, &rect)) {
					CursorPos.X = p.x - rect.left - BorderX;
					CursorPos.Y = p.y - rect.top - BorderY;
				} else {
					// window seems not to be existent, so set cursor to
					// a negative value
					CursorPos.X = -1;
					CursorPos.Y = -1;
				}
			}
		}

		CIrrDeviceWin32 *Device;
		core::position2d<s32> CursorPos;
		core::dimension2d<u32> WindowSize;
		core::dimension2d<f32> InvWindowSize;
		HWND HWnd;

		s32 BorderX, BorderY;
		core::rect<s32> ReferenceRect;
		bool UseReferenceRect;
		bool IsVisible;

		struct CursorFrameW32
		{
			CursorFrameW32() :
					IconHW(0) {}
			CursorFrameW32(HCURSOR icon) :
					IconHW(icon) {}

			HCURSOR IconHW; // hardware cursor
		};

		struct CursorW32
		{
			CursorW32() {}
			explicit CursorW32(HCURSOR iconHw, u32 frameTime = 0) :
					FrameTime(frameTime)
			{
				Frames.push_back(CursorFrameW32(iconHw));
			}
			core::array<CursorFrameW32> Frames;
			u32 FrameTime;
		};

		core::array<CursorW32> Cursors;
		gui::ECURSOR_ICON ActiveIcon;
		u32 ActiveIconStartTime;

		void initCursors();
	};

	//! returns the win32 cursor control
	CCursorControl *getWin32CursorControl();

private:
	//! create the driver
	void createDriver();

	//! Process system events
	void handleSystemMessages();

	void getWindowsVersion(core::stringc &version);

	void resizeIfNecessary();

	DWORD getWindowStyle(bool fullscreen, bool resizable) const;

	HWND HWnd;

	bool Resized;
	bool ExternalWindow;
	CCursorControl *Win32CursorControl;

	SJoystickWin32Control *JoyControl;

	bool WindowMaximized;
};

#endif // _IRR_COMPILE_WITH_WINDOWS_DEVICE_
