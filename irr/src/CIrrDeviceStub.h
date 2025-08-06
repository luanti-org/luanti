// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IrrlichtDevice.h"
#include "SIrrCreationParameters.h"
#include "IContextManager.h"

// lots of prototypes:
class ILogger;
class CLogger;

namespace gui
{
class IGUIEnvironment;
IGUIEnvironment *createGUIEnvironment(io::IFileSystem *fs,
		video::IVideoDriver *Driver, IOSOperator *op);
}

namespace scene
{
ISceneManager *createSceneManager(video::IVideoDriver *driver, gui::ICursorControl *cc);
}

namespace io
{
IFileSystem *createFileSystem();
}

namespace video
{
	IVideoDriver *createNullDriver(io::IFileSystem *io, const core::dimension2d<u32> &screenSize);

	IVideoDriver *createOpenGLDriver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager);

	IVideoDriver *createOpenGL3Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager);

	IVideoDriver *createOGLES2Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager);

	IVideoDriver *createWebGL1Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager);
}

//! Stub for an Irrlicht Device implementation
class CIrrDeviceStub : public IrrlichtDevice
{
public:
	//! constructor
	CIrrDeviceStub(const SIrrlichtCreationParameters &param);

	//! destructor
	virtual ~CIrrDeviceStub();

	//! returns the video driver
	video::IVideoDriver *getVideoDriver() override;

	//! return file system
	io::IFileSystem *getFileSystem() override;

	//! returns the gui environment
	gui::IGUIEnvironment *getGUIEnvironment() override;

	//! returns the scene manager
	scene::ISceneManager *getSceneManager() override;

	//! \return Returns a pointer to the mouse cursor control interface.
	gui::ICursorControl *getCursorControl() override;

	//! return the context manager
	video::IContextManager *getContextManager() override;

	//! Returns a pointer to the ITimer object. With it the current Time can be received.
	ITimer *getTimer() override;

	//! Sets the window icon.
	bool setWindowIcon(const video::IImage *img) override;

	//! send the event to the right receiver
	bool postEventFromUser(const SEvent &event) override;

	//! Sets a new event receiver to receive events
	void setEventReceiver(IEventReceiver *receiver) override;

	//! Returns pointer to the current event receiver. Returns 0 if there is none.
	IEventReceiver *getEventReceiver() override;

	//! Sets the input receiving scene manager.
	/** If set to null, the main scene manager (returned by GetSceneManager()) will receive the input */
	void setInputReceivingSceneManager(scene::ISceneManager *sceneManager) override;

	//! Returns a pointer to the logger.
	ILogger *getLogger() override;

	//! Returns the operation system opertator object.
	IOSOperator *getOSOperator() override;

	//! Checks if the window is maximized.
	bool isWindowMaximized() const override;

	//! Checks if the window is running in fullscreen mode.
	bool isFullscreen() const override;

	//! get color format of the current window
	video::ECOLOR_FORMAT getColorFormat() const override;

	//! Activate any joysticks, and generate events for them.
	bool activateJoysticks(core::array<SJoystickInfo> &joystickInfo) override;

	//! Activate accelerometer.
	bool activateAccelerometer(float updateInterval = 0.016666f) override;

	//! Deactivate accelerometer.
	bool deactivateAccelerometer() override;

	//! Is accelerometer active.
	bool isAccelerometerActive() override;

	//! Is accelerometer available.
	bool isAccelerometerAvailable() override;

	//! Activate gyroscope.
	bool activateGyroscope(float updateInterval = 0.016666f) override;

	//! Deactivate gyroscope.
	bool deactivateGyroscope() override;

	//! Is gyroscope active.
	bool isGyroscopeActive() override;

	//! Is gyroscope available.
	bool isGyroscopeAvailable() override;

	//! Activate device motion.
	bool activateDeviceMotion(float updateInterval = 0.016666f) override;

	//! Deactivate device motion.
	bool deactivateDeviceMotion() override;

	//! Is device motion active.
	bool isDeviceMotionActive() override;

	//! Is device motion available.
	bool isDeviceMotionAvailable() override;

	//! Set the maximal elapsed time between 2 clicks to generate doubleclicks for the mouse. It also affects tripleclick behavior.
	//! When set to 0 no double- and tripleclicks will be generated.
	void setDoubleClickTime(u32 timeMs) override;

	//! Get the maximal elapsed time between 2 clicks to generate double- and tripleclicks for the mouse.
	u32 getDoubleClickTime() const override;

	//! Remove all messages pending in the system message loop
	void clearSystemMessages() override;

	//! Get the display density in dots per inch.
	float getDisplayDensity() const override;

	//! Resize the render window.
	void setWindowSize(const core::dimension2d<u32> &size) override {}

protected:
	void createGUIAndScene();

	//! Compares to the last call of this function to return double and triple clicks.
	/** Needed for win32 device event handling
	\return Returns only 1,2 or 3. A 4th click will start with 1 again.
	*/
	virtual u32 checkSuccessiveClicks(s32 mouseX, s32 mouseY, EMOUSE_INPUT_EVENT inputEvent);

	//! Checks whether the input device should take input from the IME
	bool acceptsIME();

	video::IVideoDriver *VideoDriver;
	gui::IGUIEnvironment *GUIEnvironment;
	scene::ISceneManager *SceneManager;
	ITimer *Timer;
	gui::ICursorControl *CursorControl;
	IEventReceiver *UserReceiver;
	CLogger *Logger;
	IOSOperator *Operator;
	io::IFileSystem *FileSystem;
	scene::ISceneManager *InputReceivingSceneManager;

	struct SMouseMultiClicks
	{
		SMouseMultiClicks() :
				DoubleClickTime(500), CountSuccessiveClicks(0), LastClickTime(0), LastMouseInputEvent(EMIE_COUNT)
		{
		}

		u32 DoubleClickTime;
		u32 CountSuccessiveClicks;
		u32 LastClickTime;
		core::position2di LastClick;
		EMOUSE_INPUT_EVENT LastMouseInputEvent;
	};
	SMouseMultiClicks MouseMultiClicks;
	video::IContextManager *ContextManager;
	SIrrlichtCreationParameters CreationParams;
	bool Close;
};
