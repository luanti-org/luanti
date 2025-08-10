// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "dimension2d.h"
#include "EDriverTypes.h"
#include "EDeviceTypes.h"
#include "IEventReceiver.h"
#include "ICursorControl.h"
#include "ITimer.h"
#include "IOSOperator.h"
#include "irrArray.h"
#include "position2d.h"
#include "SColor.h" // video::ECOLOR_FORMAT
#include <string>
#include <variant>

class ILogger;
class IEventReceiver;

namespace io
{
class IFileSystem;
} // end namespace io

namespace gui
{
class IGUIEnvironment;
} // end namespace gui

namespace scene
{
class ISceneManager;
} // end namespace scene

namespace video
{
class IContextManager;
class IImage;
class IVideoDriver;
extern "C" bool isDriverSupported(E_DRIVER_TYPE driver);
} // end namespace video

//! The Irrlicht device. You can create it with createDevice() or createDeviceEx().
/** This is the most important class of the Irrlicht Engine. You can
access everything in the engine if you have a pointer to an instance of
this class.  There should be only one instance of this class at any
time.
*/
class IrrlichtDevice : public virtual IReferenceCounted
{
public:
	//! Runs the device.
	/** Also increments the virtual timer by calling
	ITimer::tick();. You can prevent this
	by calling ITimer::stop(); before and ITimer::start() after
	calling IrrlichtDevice::run(). Returns false if device wants
	to be deleted. Use it in this way:
	\code
	while(device->run())
	{
		// draw everything here
	}
	\endcode
	If you want the device to do nothing if the window is inactive
	(recommended), use the slightly enhanced code shown at isWindowActive().

	Note if you are running Irrlicht inside an external, custom
	created window: Calling Device->run() will cause Irrlicht to
	dispatch windows messages internally.
	If you are running Irrlicht in your own custom window, you can
	also simply use your own message loop using GetMessage,
	DispatchMessage and whatever and simply don't use this method.
	But note that Irrlicht will not be able to fetch user input
	then. See SIrrlichtCreationParameters::WindowId for more
	information and example code.
	*/
	virtual bool run() = 0;

	//! Cause the device to temporarily pause execution and let other processes run.
	/** This should bring down processor usage without major performance loss for Irrlicht.
	But this is system dependent, so there's a chance your thread won't get control back quickly.
	*/
	virtual void yield() = 0;

	//! Pause execution and let other processes to run for a specified amount of time.
	/** It may not wait the full given time, as sleep may be interrupted and also may wait longer on some OS.
	\param timeMs: Time to sleep for in milliseconds. Note that the OS can round up this number.
				   On Windows you usually get at least 15ms sleep time minium for any value > 0.
				   So if you call this in your main loop you can't get more than 65 FPS anymore in your game.
				   On most Linux systems it's relatively exact, but also no guarantee.
	\param pauseTimer: If true, pauses the device timer while sleeping
	*/
	virtual void sleep(u32 timeMs, bool pauseTimer = false) = 0;

	//! Provides access to the video driver for drawing 3d and 2d geometry.
	/** \return Pointer the video driver. */
	virtual video::IVideoDriver *getVideoDriver() = 0;

	//! Provides access to the virtual file system.
	/** \return Pointer to the file system. */
	virtual io::IFileSystem *getFileSystem() = 0;

	//! Provides access to the 2d user interface environment.
	/** \return Pointer to the gui environment. */
	virtual gui::IGUIEnvironment *getGUIEnvironment() = 0;

	//! Provides access to the scene manager.
	/** \return Pointer to the scene manager. */
	virtual scene::ISceneManager *getSceneManager() = 0;

	//! Provides access to the cursor control.
	/** \return Pointer to the mouse cursor control interface. */
	virtual gui::ICursorControl *getCursorControl() = 0;

	//! Provides access to the message logger.
	/** \return Pointer to the logger. */
	virtual ILogger *getLogger() = 0;

	//! Get context manager
	virtual video::IContextManager *getContextManager() = 0;

	//! Provides access to the operation system operator object.
	/** The OS operator provides methods for
	getting system specific information and doing system
	specific operations, such as exchanging data with the clipboard
	or reading the operation system version.
	\return Pointer to the OS operator. */
	virtual IOSOperator *getOSOperator() = 0;

	//! Provides access to the engine's timer.
	/** The system time can be retrieved by it as
	well as the virtual time, which also can be manipulated.
	\return Pointer to the ITimer object. */
	virtual ITimer *getTimer() = 0;

	//! Sets the caption of the window.
	/** \param text: New text of the window caption. */
	virtual void setWindowCaption(const wchar_t *text) = 0;

	//! Sets the window icon.
	/** \param img The icon texture.
	\return False if no icon was set. */
	virtual bool setWindowIcon(const video::IImage *img) = 0;

	//! Returns if the window is active.
	/** If the window is inactive,
	nothing needs to be drawn. So if you don't want to draw anything
	when the window is inactive, create your drawing loop this way:
	\code
	while(device->run())
	{
		if (device->isWindowActive())
		{
			// draw everything here
		}
		else
			device->yield();
	}
	\endcode
	\return True if window is active. */
	virtual bool isWindowActive() const = 0;

	//! Checks if the Irrlicht window has the input focus
	/** \return True if window has focus. */
	virtual bool isWindowFocused() const = 0;

	//! Checks if the Irrlicht window is minimized
	/** \return True if window is minimized. */
	virtual bool isWindowMinimized() const = 0;

	//! Checks if the Irrlicht window is maximized
	//! Only fully works on SDL. Returns false, or the last value set via
	//! maximizeWindow() and restoreWindow(), on other backends.
	/** \return True if window is maximized. */
	virtual bool isWindowMaximized() const = 0;

	//! Checks if the Irrlicht window is running in fullscreen mode
	/** \return True if window is fullscreen. */
	virtual bool isFullscreen() const = 0;

	//! Enables or disables fullscreen mode.
	/** Only works on SDL.
	\return True on success. */
	virtual bool setFullscreen(bool fullscreen) { return false; }

	//! Checks if the window could possibly be visible.
	/** If this returns false, you should not do any rendering. */
	virtual bool isWindowVisible() const { return true; };

	//! Checks if the Irrlicht device supports touch events.
	/** Intentionally doesn't check whether a touch input device is available
	or similar. */
	virtual bool supportsTouchEvents() const { return false; }

	//! Get the current color format of the window
	/** \return Color format of the window. */
	virtual video::ECOLOR_FORMAT getColorFormat() const = 0;

	//! Notifies the device that it should close itself.
	/** IrrlichtDevice::run() will always return false after closeDevice() was called. */
	virtual void closeDevice() = 0;

	//! Sets a new user event receiver which will receive events from the engine.
	/** Return true in IEventReceiver::OnEvent to prevent the event from continuing along
	the chain of event receivers. The path that an event takes through the system depends
	on its type. See EEVENT_TYPE for details.
	\param receiver New receiver to be used. */
	virtual void setEventReceiver(IEventReceiver *receiver) = 0;

	//! Provides access to the current event receiver.
	/** \return Pointer to the current event receiver. Returns 0 if there is none. */
	virtual IEventReceiver *getEventReceiver() = 0;

	//! Sends a user created event to the engine.
	/** Is is usually not necessary to use this. However, if you
	are using an own input library for example for doing joystick
	input, you can use this to post key or mouse input events to
	the engine. Internally, this method only delegates the events
	further to the scene manager and the GUI environment. */
	virtual bool postEventFromUser(const SEvent &event) = 0;

	//! Sets the input receiving scene manager.
	/** If set to null, the main scene manager (returned by
	GetSceneManager()) will receive the input
	\param sceneManager New scene manager to be used. */
	virtual void setInputReceivingSceneManager(scene::ISceneManager *sceneManager) = 0;

	//! Sets if the window should be resizable in windowed mode.
	/** The default is false. This method only works in windowed
	mode.
	\param resize Flag whether the window should be resizable. */
	virtual void setResizable(bool resize = false) = 0;

	//! Resize the render window.
	/**	This will only work in windowed mode and is not yet supported on all systems.
	It does set the drawing/clientDC size of the window, the window decorations are added to that.
	You get the current window size with IVideoDriver::getScreenSize() (might be unified in future)
	*/
	virtual void setWindowSize(const core::dimension2d<u32> &size) = 0;

	//! Minimizes the window if possible.
	virtual void minimizeWindow() = 0;

	//! Maximizes the window if possible.
	virtual void maximizeWindow() = 0;

	//! Restore the window to normal size if possible.
	virtual void restoreWindow() = 0;

	//! Get the position of the frame on-screen
	virtual core::position2di getWindowPosition() = 0;

	//! Activate any joysticks, and generate events for them.
	/** Irrlicht contains support for joysticks, but does not generate joystick events by default,
	as this would consume joystick info that 3rd party libraries might rely on. Call this method to
	activate joystick support in Irrlicht and to receive SJoystickEvent events.
	\param joystickInfo On return, this will contain an array of each joystick that was found and activated.
	\return true if joysticks are supported on this device, false if joysticks are not
				 supported or support is compiled out.
	*/
	virtual bool activateJoysticks(core::array<SJoystickInfo> &joystickInfo) = 0;

	//! Activate accelerometer.
	virtual bool activateAccelerometer(float updateInterval = 0.016666f) = 0;

	//! Deactivate accelerometer.
	virtual bool deactivateAccelerometer() = 0;

	//! Is accelerometer active.
	virtual bool isAccelerometerActive() = 0;

	//! Is accelerometer available.
	virtual bool isAccelerometerAvailable() = 0;

	//! Activate gyroscope.
	virtual bool activateGyroscope(float updateInterval = 0.016666f) = 0;

	//! Deactivate gyroscope.
	virtual bool deactivateGyroscope() = 0;

	//! Is gyroscope active.
	virtual bool isGyroscopeActive() = 0;

	//! Is gyroscope available.
	virtual bool isGyroscopeAvailable() = 0;

	//! Activate device motion.
	virtual bool activateDeviceMotion(float updateInterval = 0.016666f) = 0;

	//! Deactivate device motion.
	virtual bool deactivateDeviceMotion() = 0;

	//! Is device motion active.
	virtual bool isDeviceMotionActive() = 0;

	//! Is device motion available.
	virtual bool isDeviceMotionAvailable() = 0;

	//! Set the maximal elapsed time between 2 clicks to generate doubleclicks for the mouse. It also affects tripleclick behavior.
	/** When set to 0 no double- and tripleclicks will be generated.
	\param timeMs maximal time in milliseconds for two consecutive clicks to be recognized as double click
	*/
	virtual void setDoubleClickTime(u32 timeMs) = 0;

	//! Get the maximal elapsed time between 2 clicks to generate double- and tripleclicks for the mouse.
	/** When return value is 0 no double- and tripleclicks will be generated.
	\return maximal time in milliseconds for two consecutive clicks to be recognized as double click
	*/
	virtual u32 getDoubleClickTime() const = 0;

	//! Remove messages pending in the system message loop
	/** This function is usually used after messages have been buffered for a longer time, for example
	when loading a large scene. Clearing the message loop prevents that mouse- or buttonclicks which users
	have pressed in the meantime will now trigger unexpected actions in the gui. <br>
	So far the following messages are cleared:<br>
	Win32: All keyboard and mouse messages<br>
	Linux: All keyboard and mouse messages<br>
	All other devices are not yet supported here.<br>
	The function is still somewhat experimental, as the kind of messages we clear is based on just a few use-cases.
	If you think further messages should be cleared, or some messages should not be cleared here, then please tell us. */
	virtual void clearSystemMessages() = 0;

	//! Get the type of the device.
	/** This allows the user to check which windowing system is currently being
	used. */
	virtual E_DEVICE_TYPE getType() const = 0;

	//! Get the version string of the underlying system (e.g. SDL)
	virtual std::string getVersionString() const
	{
		return "";
	}

	//! Get the display density in dots per inch.
	//! Returns 0.0f on failure.
	virtual float getDisplayDensity() const = 0;

	//! Check if a driver type is supported by the engine.
	/** Even if true is returned the driver may not be available
	for a configuration requested when creating the device. */
	static bool isDriverSupported(video::E_DRIVER_TYPE driver)
	{
		return video::isDriverSupported(driver);
	}

	//! Get the corresponding scancode for the keycode.
	/**
	\param key The keycode to convert.
	\return The implementation-dependent scancode for the key (represented by the u32 component) or, if a scancode is not
	available, the corresponding Irrlicht keycode (represented by the EKEY_CODE component).
	*/
	virtual std::variant<u32, EKEY_CODE> getScancodeFromKey(const Keycode &key) const {
		if (auto pv = std::get_if<EKEY_CODE>(&key))
			return *pv;
		return (u32)std::get<wchar_t>(key);
	}

	//! Get the corresponding keycode for the scancode.
	/**
	\param scancode The implementation-dependent scancode for the key.
	\return The corresponding keycode.
	*/
	virtual Keycode getKeyFromScancode(const u32 scancode) const {
		return Keycode(KEY_UNKNOWN, (wchar_t)scancode);
	}
};
