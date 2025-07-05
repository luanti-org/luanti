// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h
// This device code is based on the original SDL device implementation
// contributed by Shane Parker (sirshane).

#pragma once

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_

#include "IrrlichtDevice.h"
#include "CIrrDeviceStub.h"
#include "ICursorControl.h"

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
#include <emscripten/html5.h>
#endif

#include <SDL.h>
// DirectFB is removed in SDL3, thou distribution as Alpine currently ships SDL2
// with enabled DirectFB, but requiring another fix at a top of SDL2.
// We don't need DirectFB in Irrlicht/Minetest, so simply disable it here to prevent issues.
#undef SDL_VIDEO_DRIVER_DIRECTFB
#include <SDL_syswm.h>

#include <memory>
#include <unordered_map>

class CIrrDeviceSDL : public CIrrDeviceStub
{
public:
	//! constructor
	CIrrDeviceSDL(const SIrrlichtCreationParameters &param);

	//! destructor
	virtual ~CIrrDeviceSDL();

	//! runs the device. Returns false if device wants to be deleted
	bool run() override;

	//! pause execution temporarily
	void yield() override;

	//! pause execution for a specified time
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

	//! returns color format of the window.
	video::ECOLOR_FORMAT getColorFormat() const override;

	//! notifies the device that it should close itself
	void closeDevice() override;

	//! Sets if the window should be resizable in windowed mode.
	void setResizable(bool resize = false) override;

	//! Minimizes the window.
	void minimizeWindow() override;

	//! Maximizes the window.
	void maximizeWindow() override;

	//! Restores the window size.
	void restoreWindow() override;

	//! Checks if the window is maximized.
	bool isWindowMaximized() const override;

	//! Checks if the Irrlicht window is running in fullscreen mode
	/** \return True if window is fullscreen. */
	bool isFullscreen() const override;

	//! Enables or disables fullscreen mode.
	/** \return True on success. */
	bool setFullscreen(bool fullscreen) override;

	//! Checks if the window could possibly be visible.
	bool isWindowVisible() const override;

	//! Checks if the Irrlicht device supports touch events.
	bool supportsTouchEvents() const override;

	//! Get the position of this window on screen
	core::position2di getWindowPosition() override;

	//! Activate any joysticks, and generate events for them.
	bool activateJoysticks(core::array<SJoystickInfo> &joystickInfo) override;

	//! Get the device type
	E_DEVICE_TYPE getType() const override
	{
		return EIDT_SDL;
	}

	//! Get the SDL version
	std::string getVersionString() const override
	{
		SDL_version ver;
		SDL_GetVersion(&ver);
		return std::to_string(ver.major) + "." + std::to_string(ver.minor) + "." + std::to_string(ver.patch);
	}

	//! Get the display density in dots per inch.
	float getDisplayDensity() const override;

	void SwapWindow();

	//! Implementation of the linux cursor control
	class CCursorControl : public gui::ICursorControl
	{
	public:
		CCursorControl(CIrrDeviceSDL *dev) :
				Device(dev), IsVisible(true)
		{
			initCursors();
		}

		//! Changes the visible state of the mouse cursor.
		void setVisible(bool visible) override
		{
			IsVisible = visible;
			if (visible)
				SDL_ShowCursor(SDL_ENABLE);
			else {
				SDL_ShowCursor(SDL_DISABLE);
			}
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
#ifndef __ANDROID__
			// On Android, this somehow results in a camera jump when enabling
			// relative mouse mode and it isn't supported anyway.
			SDL_WarpMouseInWindow(Device->Window,
					static_cast<int>(x / Device->ScaleX),
					static_cast<int>(y / Device->ScaleY));
#endif

			if (SDL_GetRelativeMouseMode()) {
				// There won't be an event for this warp (details on libsdl-org/SDL/issues/6034)
				Device->MouseX = x;
				Device->MouseY = y;
			}
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
			return core::position2d<f32>(CursorPos.X / (f32)Device->Width,
					CursorPos.Y / (f32)Device->Height);
		}

		void setReferenceRect(core::rect<s32> *rect = 0) override
		{
		}

		virtual void setRelativeMode(bool relative) override
		{
			// Only change it when necessary, as it flushes mouse motion when enabled
			if (relative != static_cast<bool>(SDL_GetRelativeMouseMode())) {
				if (relative)
					SDL_SetRelativeMouseMode(SDL_TRUE);
				else
					SDL_SetRelativeMouseMode(SDL_FALSE);
			}
		}

		void setActiveIcon(gui::ECURSOR_ICON iconId) override
		{
			ActiveIcon = iconId;
			if (iconId > Cursors.size() || !Cursors[iconId]) {
				iconId = gui::ECI_NORMAL;
				if (iconId > Cursors.size() || !Cursors[iconId])
					return;
			}
			SDL_SetCursor(Cursors[iconId].get());
		}

		gui::ECURSOR_ICON getActiveIcon() const override
		{
			return ActiveIcon;
		}

	private:
		void updateCursorPos()
		{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
			EmscriptenPointerlockChangeEvent pointerlockStatus; // let's hope that test is not expensive ...
			if (emscripten_get_pointerlock_status(&pointerlockStatus) == EMSCRIPTEN_RESULT_SUCCESS) {
				if (pointerlockStatus.isActive) {
					CursorPos.X += Device->MouseXRel;
					CursorPos.Y += Device->MouseYRel;
					Device->MouseXRel = 0;
					Device->MouseYRel = 0;
				} else {
					CursorPos.X = Device->MouseX;
					CursorPos.Y = Device->MouseY;
				}
			}
#else
			CursorPos.X = Device->MouseX;
			CursorPos.Y = Device->MouseY;

			if (CursorPos.X < 0)
				CursorPos.X = 0;
			if (CursorPos.X > (s32)Device->Width)
				CursorPos.X = Device->Width;
			if (CursorPos.Y < 0)
				CursorPos.Y = 0;
			if (CursorPos.Y > (s32)Device->Height)
				CursorPos.Y = Device->Height;
#endif
		}

		void initCursors();

		CIrrDeviceSDL *Device;
		core::position2d<s32> CursorPos;
		bool IsVisible;

		struct CursorDeleter
		{
			void operator()(SDL_Cursor *ptr)
			{
				if (ptr)
					SDL_FreeCursor(ptr);
			}
		};
		std::vector<std::unique_ptr<SDL_Cursor, CursorDeleter>> Cursors;
		gui::ECURSOR_ICON ActiveIcon = gui::ECURSOR_ICON::ECI_NORMAL;
	};

private:
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	static EM_BOOL MouseUpDownCallback(int eventType, const EmscriptenMouseEvent *event, void *userData);
	static EM_BOOL MouseEnterCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);
	static EM_BOOL MouseLeaveCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);

#endif
	// Check if a key is a known special character with no side effects on text boxes.
	static bool keyIsKnownSpecial(EKEY_CODE irrlichtKey);

	// Return the Char that should be sent to Irrlicht for the given key (either the one passed in or 0).
	static int findCharToPassToIrrlicht(uint32_t sdlKey, EKEY_CODE irrlichtKey, bool numlock);

	std::variant<u32, EKEY_CODE> getScancodeFromKey(const Keycode &key) const override;
	Keycode getKeyFromScancode(const u32 scancode) const override;

	// Check if a text box is in focus. Enable or disable SDL_TEXTINPUT events only if in focus.
	void resetReceiveTextInputEvents();

	//! create the driver
	void createDriver();

	bool createWindow();
	bool createWindowWithContext();

	void createKeyMap();

	void logAttributes();
	SDL_GLContext Context;
	SDL_Window *Window;
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	core::array<SDL_Joystick *> Joysticks;
#endif

	s32 MouseX, MouseY;
	// these two only continue to exist for some Emscripten stuff idk about
	s32 MouseXRel, MouseYRel;
	u32 MouseButtonStates;

	u32 Width, Height;
	f32 ScaleX = 1.0f, ScaleY = 1.0f;
	void updateSizeAndScale();

	bool Resizable;

	static u32 getFullscreenFlag(bool fullscreen);

	core::rect<s32> lastElemPos;

	// TODO: This is only used for scancode/keycode conversion with EKEY_CODE (among other things, for Luanti
	// to display keys to users). Drop this along with EKEY_CODE.
	std::unordered_map<SDL_Keycode, EKEY_CODE> KeyMap;

	s32 CurrentTouchCount;
	bool IsInBackground;
};

#endif // _IRR_COMPILE_WITH_SDL_DEVICE_
