// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CIrrDeviceStub.h"
#include "ISceneManager.h"
#include "IEventReceiver.h"
#include "IFileSystem.h"
#include "IGUIElement.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "os.h"
#include "CTimer.h"
#include "CLogger.h"
#include "irrString.h"

namespace video
{
#ifndef _IRR_COMPILE_WITH_OPENGL_
IVideoDriver *createOpenGLDriver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager)
{
	os::Printer::log("No OpenGL support compiled in.", ELL_ERROR);
	return nullptr;
}
#endif

#ifndef ENABLE_OPENGL3
IVideoDriver *createOpenGL3Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager)
{
	os::Printer::log("No OpenGL 3 support compiled in.", ELL_ERROR);
	return nullptr;
}
#endif

#ifndef _IRR_COMPILE_WITH_OGLES2_
IVideoDriver *createOGLES2Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager)
{
	os::Printer::log("No OpenGL ES 2 support compiled in.", ELL_ERROR);
	return nullptr;
}
#endif

#ifndef _IRR_COMPILE_WITH_WEBGL1_
IVideoDriver *createWebGL1Driver(const SIrrlichtCreationParameters &params, io::IFileSystem *io, IContextManager *contextManager)
{
	os::Printer::log("No WebGL 1 support compiled in.", ELL_ERROR);
	return nullptr;
}
#endif
}


//! constructor
CIrrDeviceStub::CIrrDeviceStub(const SIrrlichtCreationParameters &params) :
		IrrlichtDevice(), VideoDriver(0), GUIEnvironment(0), SceneManager(0),
		Timer(0), CursorControl(0), UserReceiver(params.EventReceiver),
		Logger(0), Operator(0), FileSystem(0),
		InputReceivingSceneManager(0), ContextManager(0),
		CreationParams(params), Close(false)
{
	Timer = new CTimer();
	if (os::Printer::Logger) {
		os::Printer::Logger->grab();
		Logger = (CLogger *)os::Printer::Logger;
		Logger->setReceiver(UserReceiver);
	} else {
		Logger = new CLogger(UserReceiver);
		os::Printer::Logger = Logger;
	}
	Logger->setLogLevel(CreationParams.LoggingLevel);

	os::Printer::Logger = Logger;

	FileSystem = io::createFileSystem();
}

CIrrDeviceStub::~CIrrDeviceStub()
{
	if (GUIEnvironment)
		GUIEnvironment->drop();

	if (SceneManager)
		SceneManager->drop();

	if (VideoDriver)
		VideoDriver->drop();

	if (ContextManager)
		ContextManager->drop();

	if (FileSystem)
		FileSystem->drop();

	if (InputReceivingSceneManager)
		InputReceivingSceneManager->drop();

	if (CursorControl)
		CursorControl->drop();

	if (Operator)
		Operator->drop();

	CursorControl = 0;

	if (Timer)
		Timer->drop();

	if (Logger->drop())
		os::Printer::Logger = 0;
}

void CIrrDeviceStub::createGUIAndScene()
{
	// create gui environment
	GUIEnvironment = gui::createGUIEnvironment(FileSystem, VideoDriver, Operator);

	// create Scene manager
	SceneManager = scene::createSceneManager(VideoDriver, CursorControl);

	setEventReceiver(UserReceiver);
}

//! returns the video driver
video::IVideoDriver *CIrrDeviceStub::getVideoDriver()
{
	return VideoDriver;
}

//! return file system
io::IFileSystem *CIrrDeviceStub::getFileSystem()
{
	return FileSystem;
}

//! returns the gui environment
gui::IGUIEnvironment *CIrrDeviceStub::getGUIEnvironment()
{
	return GUIEnvironment;
}

//! returns the scene manager
scene::ISceneManager *CIrrDeviceStub::getSceneManager()
{
	return SceneManager;
}

//! \return Returns a pointer to the ITimer object. With it the
//! current Time can be received.
ITimer *CIrrDeviceStub::getTimer()
{
	return Timer;
}

//! Sets the window icon.
bool CIrrDeviceStub::setWindowIcon(const video::IImage *img)
{
	return false;
}

//! \return Returns a pointer to the mouse cursor control interface.
gui::ICursorControl *CIrrDeviceStub::getCursorControl()
{
	return CursorControl;
}

//! return the context manager
video::IContextManager *CIrrDeviceStub::getContextManager()
{
	return ContextManager;
}

//! Compares to the last call of this function to return double and triple clicks.
u32 CIrrDeviceStub::checkSuccessiveClicks(s32 mouseX, s32 mouseY, EMOUSE_INPUT_EVENT inputEvent)
{
	const s32 MAX_MOUSEMOVE = 3;

	u32 clickTime = getTimer()->getRealTime();

	if ((clickTime - MouseMultiClicks.LastClickTime) < MouseMultiClicks.DoubleClickTime && core::abs_(MouseMultiClicks.LastClick.X - mouseX) <= MAX_MOUSEMOVE && core::abs_(MouseMultiClicks.LastClick.Y - mouseY) <= MAX_MOUSEMOVE && MouseMultiClicks.CountSuccessiveClicks < 3 && MouseMultiClicks.LastMouseInputEvent == inputEvent) {
		++MouseMultiClicks.CountSuccessiveClicks;
	} else {
		MouseMultiClicks.CountSuccessiveClicks = 1;
	}

	MouseMultiClicks.LastMouseInputEvent = inputEvent;
	MouseMultiClicks.LastClickTime = clickTime;
	MouseMultiClicks.LastClick.X = mouseX;
	MouseMultiClicks.LastClick.Y = mouseY;

	return MouseMultiClicks.CountSuccessiveClicks;
}

//! send the event to the right receiver
bool CIrrDeviceStub::postEventFromUser(const SEvent &event)
{
	bool absorbed = false;

	if (UserReceiver)
		absorbed = UserReceiver->OnEvent(event);

	if (!absorbed && GUIEnvironment)
		absorbed = GUIEnvironment->postEventFromUser(event);

	scene::ISceneManager *inputReceiver = InputReceivingSceneManager;
	if (!inputReceiver)
		inputReceiver = SceneManager;

	if (!absorbed && inputReceiver)
		absorbed = inputReceiver->postEventFromUser(event);

	return absorbed;
}

//! Sets a new event receiver to receive events
void CIrrDeviceStub::setEventReceiver(IEventReceiver *receiver)
{
	UserReceiver = receiver;
	Logger->setReceiver(receiver);
	if (GUIEnvironment)
		GUIEnvironment->setUserEventReceiver(receiver);
}

//! Returns poinhter to the current event receiver. Returns 0 if there is none.
IEventReceiver *CIrrDeviceStub::getEventReceiver()
{
	return UserReceiver;
}

//! \return Returns a pointer to the logger.
ILogger *CIrrDeviceStub::getLogger()
{
	return Logger;
}

//! Returns the operation system opertator object.
IOSOperator *CIrrDeviceStub::getOSOperator()
{
	return Operator;
}

//! Sets the input receiving scene manager.
void CIrrDeviceStub::setInputReceivingSceneManager(scene::ISceneManager *sceneManager)
{
	if (sceneManager)
		sceneManager->grab();
	if (InputReceivingSceneManager)
		InputReceivingSceneManager->drop();

	InputReceivingSceneManager = sceneManager;
}

//! Checks if the window is maximized.
bool CIrrDeviceStub::isWindowMaximized() const
{
	return false;
}

//! Checks if the window is running in fullscreen mode
bool CIrrDeviceStub::isFullscreen() const
{
	return CreationParams.Fullscreen;
}

//! returns color format
video::ECOLOR_FORMAT CIrrDeviceStub::getColorFormat() const
{
	return video::ECF_R5G6B5;
}

//! No-op in this implementation
bool CIrrDeviceStub::activateJoysticks(core::array<SJoystickInfo> &joystickInfo)
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::activateAccelerometer(float updateInterval)
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::deactivateAccelerometer()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::isAccelerometerActive()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::isAccelerometerAvailable()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::activateGyroscope(float updateInterval)
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::deactivateGyroscope()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::isGyroscopeActive()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::isGyroscopeAvailable()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::activateDeviceMotion(float updateInterval)
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::deactivateDeviceMotion()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::isDeviceMotionActive()
{
	return false;
}

//! No-op in this implementation
bool CIrrDeviceStub::isDeviceMotionAvailable()
{
	return false;
}

//! Set the maximal elapsed time between 2 clicks to generate doubleclicks for the mouse. It also affects tripleclick behavior.
void CIrrDeviceStub::setDoubleClickTime(u32 timeMs)
{
	MouseMultiClicks.DoubleClickTime = timeMs;
}

//! Get the maximal elapsed time between 2 clicks to generate double- and tripleclicks for the mouse.
u32 CIrrDeviceStub::getDoubleClickTime() const
{
	return MouseMultiClicks.DoubleClickTime;
}

//! Remove all messages pending in the system message loop
void CIrrDeviceStub::clearSystemMessages()
{
}

//! Get the display density in dots per inch.
float CIrrDeviceStub::getDisplayDensity() const
{
	return 0.0f;
}

//! Checks whether the input device should take input from the IME
bool CIrrDeviceStub::acceptsIME()
{
	if (!GUIEnvironment)
		return false;
	gui::IGUIElement *elem = GUIEnvironment->getFocus();
	return elem && elem->acceptsIME();
}
