// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CIrrDeviceLinux.h"

#ifdef _IRR_COMPILE_WITH_X11_DEVICE_

#include <cstdio>
#include <cstdlib>
#include <sys/utsname.h>
#include <ctime>
#include <clocale>
#include "IEventReceiver.h"
#include "ISceneManager.h"
#include "IGUIElement.h"
#include "IGUIEnvironment.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include "COSOperator.h"
#include "CColorConverter.h"
#include "SIrrCreationParameters.h"
#include "SExposedVideoData.h"
#include "IGUISpriteBank.h"
#include "IImageLoader.h"
#include "IFileSystem.h"
#include "IVideoDriver.h"
#include <X11/XKBlib.h>
#include <X11/Xatom.h>

#if defined(_IRR_LINUX_X11_XINPUT2_)
#include <X11/extensions/XInput2.h>
#endif

#if defined(_IRR_COMPILE_WITH_OGLES2_)
#include "CEGLManager.h"
#endif

#if defined(_IRR_COMPILE_WITH_GLX_MANAGER_)
#include "CGLXManager.h"
#endif

#ifdef _IRR_LINUX_XCURSOR_
#include <X11/Xcursor/Xcursor.h>
#endif

#if defined(_IRR_COMPILE_WITH_X11_) || defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
#include <unistd.h>
#endif

#if defined _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
#include <fcntl.h>

#ifdef __FreeBSD__
#include <sys/joystick.h>
#else

// linux/joystick.h includes linux/input.h, which #defines values for various KEY_FOO keys.
// These override the KEY_FOO equivalents, which stops key handling from working.
// As a workaround, defining _INPUT_H stops linux/input.h from being included; it
// doesn't actually seem to be necessary except to pull in sys/ioctl.h.
#define _INPUT_H
#include <sys/ioctl.h> // Would normally be included in linux/input.h
#include <linux/joystick.h>
#undef _INPUT_H
#endif

#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

namespace
{
Atom X_ATOM_CLIPBOARD;
Atom X_ATOM_TARGETS;
Atom X_ATOM_UTF8_STRING;
Atom X_ATOM_UTF8_MIME_TYPE;
Atom X_ATOM_TEXT;
Atom X_ATOM_NETWM_MAXIMIZE_VERT;
Atom X_ATOM_NETWM_MAXIMIZE_HORZ;
Atom X_ATOM_NETWM_STATE;
Atom X_ATOM_NETWM_STATE_FULLSCREEN;

Atom X_ATOM_WM_DELETE_WINDOW;

#if defined(_IRR_LINUX_X11_XINPUT2_)
int XI_EXTENSIONS_OPCODE;
#endif
}

//! constructor
CIrrDeviceLinux::CIrrDeviceLinux(const SIrrlichtCreationParameters &param) :
		CIrrDeviceStub(param),
#ifdef _IRR_COMPILE_WITH_X11_
		XDisplay(0), VisualInfo(0), Screennr(0), XWindow(0), StdHints(0),
		XInputMethod(0), XInputContext(0),
		HasNetWM(false),
#endif
#if defined(_IRR_LINUX_X11_XINPUT2_)
		currentTouchedCount(0),
#endif
		Width(param.WindowSize.Width), Height(param.WindowSize.Height),
		WindowHasFocus(false), WindowMinimized(false), WindowMaximized(param.WindowMaximized),
		ExternalWindow(false), AutorepeatSupport(0)
{
	// print version, distribution etc.
	// thx to LynxLuna for pointing me to the uname function
	core::stringc linuxversion;
	struct utsname LinuxInfo;
	uname(&LinuxInfo);

	linuxversion += LinuxInfo.sysname;
	linuxversion += " ";
	linuxversion += LinuxInfo.release;
	linuxversion += " ";
	linuxversion += LinuxInfo.version;
	linuxversion += " ";
	linuxversion += LinuxInfo.machine;

	Operator = new COSOperator(linuxversion, this);
	os::Printer::log(linuxversion.c_str(), ELL_INFORMATION);

	// create keymap
	createKeyMap();

	// initialize X11 thread safety
	// libX11 1.8+ has this on by default
	// without it, multi-threaded GL drivers may crash
	XInitThreads();

	// create window
	if (CreationParams.DriverType != video::EDT_NULL) {
		// create the window, only if we do not use the null device
		if (!createWindow())
			return;
		if (param.WindowResizable < 2)
			setResizable(param.WindowResizable == 1 ? true : false);
#ifdef _IRR_COMPILE_WITH_X11_
		createInputContext();
#endif
	}

	// create cursor control
	CursorControl = new CCursorControl(this, CreationParams.DriverType == video::EDT_NULL);

	// create driver
	createDriver();

	if (!VideoDriver)
		return;

	createGUIAndScene();

	if (param.WindowMaximized)
		maximizeWindow();

	setupTopLevelXorgWindow();
}

//! destructor
CIrrDeviceLinux::~CIrrDeviceLinux()
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (StdHints)
		XFree(StdHints);
	// Disable cursor (it is drop'ed in stub)
	if (CursorControl) {
		CursorControl->setVisible(false);
		static_cast<CCursorControl *>(CursorControl)->clearCursors();
	}

	// Must free OpenGL textures etc before destroying context, so can't wait for stub destructor
	if (GUIEnvironment) {
		GUIEnvironment->drop();
		GUIEnvironment = NULL;
	}
	if (SceneManager) {
		SceneManager->drop();
		SceneManager = NULL;
	}
	if (VideoDriver) {
		VideoDriver->drop();
		VideoDriver = NULL;
	}

	destroyInputContext();

	if (XDisplay) {
		if (ContextManager) {
			ContextManager->destroyContext();
			ContextManager->destroySurface();
		}

		if (!ExternalWindow) {
			XDestroyWindow(XDisplay, XWindow);
			XCloseDisplay(XDisplay);
		}
	}
	if (VisualInfo)
		XFree(VisualInfo);

#endif // #ifdef _IRR_COMPILE_WITH_X11_

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	for (u32 joystick = 0; joystick < ActiveJoysticks.size(); ++joystick) {
		if (ActiveJoysticks[joystick].fd >= 0) {
			close(ActiveJoysticks[joystick].fd);
		}
	}
#endif
}

#if defined(_IRR_COMPILE_WITH_X11_) && defined(_DEBUG)
int IrrPrintXError(Display *display, XErrorEvent *event)
{
	char msg[256];
	char msg2[256];

	snprintf_irr(msg, 256, "%d", event->request_code);
	XGetErrorDatabaseText(display, "XRequest", msg, "unknown", msg2, 256);
	XGetErrorText(display, event->error_code, msg, 256);
	os::Printer::log("X Error", msg, ELL_WARNING);
	os::Printer::log("From call ", msg2, ELL_WARNING);
	return 0;
}
#endif

bool CIrrDeviceLinux::switchToFullscreen()
{
	if (!CreationParams.Fullscreen)
		return true;

	if (!HasNetWM) {
		os::Printer::log("NetWM support is required to allow Irrlicht to switch "
						 "to fullscreen mode. Running in windowed mode instead.",
				ELL_WARNING);
		CreationParams.Fullscreen = false;
		return false;
	}

	XEvent ev = {0};

	ev.type = ClientMessage;
	ev.xclient.window = XWindow;
	ev.xclient.message_type = X_ATOM_NETWM_STATE;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
	ev.xclient.data.l[1] = X_ATOM_NETWM_STATE_FULLSCREEN;

	XSendEvent(XDisplay, DefaultRootWindow(XDisplay), false,
			SubstructureNotifyMask | SubstructureRedirectMask, &ev);

	return true;
}

void CIrrDeviceLinux::setupTopLevelXorgWindow()
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (CreationParams.DriverType == video::EDT_NULL)
		return; // no display and window

	os::Printer::log("Configuring X11-specific top level window properties", ELL_DEBUG);

	// Set application name and class hints. For now name and class are the same.
	// Note: SDL uses the executable name here (i.e. "luanti").
	XClassHint *classhint = XAllocClassHint();
	classhint->res_name = const_cast<char *>("Luanti");
	classhint->res_class = const_cast<char *>("Luanti");

	XSetClassHint(XDisplay, XWindow, classhint);
	XFree(classhint);

	// FIXME: In the future WMNormalHints should be set ... e.g see the
	// gtk/gdk code (gdk/x11/gdksurface-x11.c) for the setup_top_level
	// method. But for now (as it would require some significant changes)
	// leave the code as is.

	// The following is borrowed from the above gdk source for setting top
	// level windows. The source indicates and the Xlib docs suggest that
	// this will set the WM_CLIENT_MACHINE and WM_LOCAL_NAME. This will not
	// set the WM_CLIENT_MACHINE to a Fully Qualified Domain Name (FQDN) which is
	// required by the Extended Window Manager Hints (EWMH) spec when setting
	// the _NET_WM_PID (see further down) but running Minetest in an env
	// where the window manager is on another machine from Minetest (therefore
	// making the PID useless) is not expected to be a problem. Further
	// more, using gtk/gdk as the model it would seem that not using a FQDN is
	// not an issue for modern Xorg window managers.

	os::Printer::log("Setting Xorg window manager Properties", ELL_DEBUG);

	XSetWMProperties(XDisplay, XWindow, NULL, NULL, NULL, 0, NULL, NULL, NULL);

	// Set the _NET_WM_PID window property according to the EWMH spec. _NET_WM_PID
	// (in conjunction with WM_CLIENT_MACHINE) can be used by window managers to
	// force a shutdown of an application if it doesn't respond to the destroy
	// window message.

	os::Printer::log("Setting Xorg _NET_WM_PID extended window manager property", ELL_DEBUG);

	Atom NET_WM_PID = XInternAtom(XDisplay, "_NET_WM_PID", false);

	long pid = static_cast<long>(getpid());

	XChangeProperty(XDisplay, XWindow, NET_WM_PID,
			XA_CARDINAL, 32, PropModeReplace,
			reinterpret_cast<unsigned char *>(&pid), 1);

	// Set the WM_CLIENT_LEADER window property here. Minetest has only one
	// window and that window will always be the leader.

	os::Printer::log("Setting Xorg WM_CLIENT_LEADER property", ELL_DEBUG);

	Atom WM_CLIENT_LEADER = XInternAtom(XDisplay, "WM_CLIENT_LEADER", false);

	XChangeProperty(XDisplay, XWindow, WM_CLIENT_LEADER,
			XA_WINDOW, 32, PropModeReplace,
			reinterpret_cast<unsigned char *>(&XWindow), 1);
#endif
}

#if defined(_IRR_COMPILE_WITH_X11_)
void IrrPrintXGrabError(int grabResult, const c8 *grabCommand)
{
	if (grabResult == GrabSuccess) {
		//		os::Printer::log(grabCommand, "GrabSuccess", ELL_INFORMATION);
		return;
	}

	switch (grabResult) {
	case AlreadyGrabbed:
		os::Printer::log(grabCommand, "AlreadyGrabbed", ELL_WARNING);
		break;
	case GrabNotViewable:
		os::Printer::log(grabCommand, "GrabNotViewable", ELL_WARNING);
		break;
	case GrabFrozen:
		os::Printer::log(grabCommand, "GrabFrozen", ELL_WARNING);
		break;
	case GrabInvalidTime:
		os::Printer::log(grabCommand, "GrabInvalidTime", ELL_WARNING);
		break;
	default:
		os::Printer::log(grabCommand, "grab failed with unknown problem", ELL_WARNING);
		break;
	}
}
#endif

bool CIrrDeviceLinux::createWindow()
{
#ifdef _IRR_COMPILE_WITH_X11_
#ifdef _DEBUG
	os::Printer::log("Creating X window...", ELL_INFORMATION);
	XSetErrorHandler(IrrPrintXError);
#endif

	XDisplay = XOpenDisplay(0);
	if (!XDisplay) {
		os::Printer::log("Error: Need running XServer to start Irrlicht Engine.", ELL_ERROR);
		if (XDisplayName(0)[0])
			os::Printer::log("Could not open display", XDisplayName(0), ELL_ERROR);
		else
			os::Printer::log("Could not open display, set DISPLAY variable", ELL_ERROR);
		return false;
	}

	Screennr = DefaultScreen(XDisplay);

	initXAtoms();

	// check netwm support
	Atom WMCheck = XInternAtom(XDisplay, "_NET_SUPPORTING_WM_CHECK", True);
	if (WMCheck != None)
		HasNetWM = true;

#if defined(_IRR_COMPILE_WITH_GLX_MANAGER_)
	// don't use the XVisual with OpenGL, because it ignores all requested
	// properties of the CreationParams
	if (CreationParams.DriverType == video::EDT_OPENGL
			|| CreationParams.DriverType == video::EDT_OPENGL3) {
		video::SExposedVideoData data;
		data.OpenGLLinux.X11Display = XDisplay;
		ContextManager = new video::CGLXManager(CreationParams, data, Screennr);
		VisualInfo = ((video::CGLXManager *)ContextManager)->getVisual();
	}
#endif

	if (!VisualInfo) {
		// create visual with standard X methods
		os::Printer::log("Using plain X visual");
		XVisualInfo visTempl; // Template to hold requested values
		int visNumber;        // Return value of available visuals

		visTempl.screen = Screennr;
		// ARGB visuals should be avoided for usual applications
		visTempl.depth = CreationParams.WithAlphaChannel ? 32 : 24;
		while ((!VisualInfo) && (visTempl.depth >= 16)) {
			VisualInfo = XGetVisualInfo(XDisplay, VisualScreenMask | VisualDepthMask,
					&visTempl, &visNumber);
			visTempl.depth -= 8;
		}
	}

	if (!VisualInfo) {
		os::Printer::log("Fatal error, could not get visual.", ELL_ERROR);
		XCloseDisplay(XDisplay);
		XDisplay = 0;
		return false;
	}
#ifdef _DEBUG
	else
		os::Printer::log("Visual chosen", core::stringc(static_cast<u32>(VisualInfo->visualid)).c_str(), ELL_DEBUG);
#endif

	// create color map
	Colormap colormap;
	colormap = XCreateColormap(XDisplay,
			RootWindow(XDisplay, VisualInfo->screen),
			VisualInfo->visual, AllocNone);

	WndAttributes.colormap = colormap;
	WndAttributes.border_pixel = 0;
	WndAttributes.event_mask = StructureNotifyMask | FocusChangeMask | ExposureMask;
	WndAttributes.event_mask |= PointerMotionMask |
								ButtonPressMask | KeyPressMask |
								ButtonReleaseMask | KeyReleaseMask;

	if (!CreationParams.WindowId) {
		int x = 0;
		int y = 0;

		if (!CreationParams.Fullscreen) {
			if (CreationParams.WindowPosition.X > 0)
				x = CreationParams.WindowPosition.X;
			if (CreationParams.WindowPosition.Y > 0)
				y = CreationParams.WindowPosition.Y;
		}

		// create new Window
		// Remove window manager decoration in fullscreen
		XWindow = XCreateWindow(XDisplay,
				RootWindow(XDisplay, VisualInfo->screen),
				x, y, Width, Height, 0, VisualInfo->depth,
				InputOutput, VisualInfo->visual,
				CWBorderPixel | CWColormap | CWEventMask,
				&WndAttributes);

		XMapRaised(XDisplay, XWindow);
		CreationParams.WindowId = (void *)XWindow;
		X_ATOM_WM_DELETE_WINDOW = XInternAtom(XDisplay, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(XDisplay, XWindow, &X_ATOM_WM_DELETE_WINDOW, 1);

		if (CreationParams.Fullscreen) {
			// Don't try to set window position
		} else if (CreationParams.WindowPosition.X >= 0 || CreationParams.WindowPosition.Y >= 0) { // default is -1, -1
			// Window managers are free to ignore positions above, so give it another shot
			XMoveWindow(XDisplay, XWindow, x, y);
		}
	} else {
		// attach external window
		XWindow = (Window)CreationParams.WindowId;
		{
			// Note: This might be further improved by using a InputOnly window instead of InputOutput.
			// I think then it should be possible to render into the given parent window instead of
			// creating a child-window.
			// Also... this does possibly leak.
			Window child_window = XCreateWindow(XDisplay,
					XWindow,
					0, 0, Width, Height, 0, VisualInfo->depth,
					InputOutput, VisualInfo->visual,
					CWBorderPixel | CWColormap | CWEventMask,
					&WndAttributes);

			// do not forget to map new window
			XMapWindow(XDisplay, child_window);

			// overwrite device window id
			XWindow = child_window;
		}
		XWindowAttributes wa;
		XGetWindowAttributes(XDisplay, XWindow, &wa);
		CreationParams.WindowSize.Width = wa.width;
		CreationParams.WindowSize.Height = wa.height;
		CreationParams.Fullscreen = false;
		ExternalWindow = true;
	}

	switchToFullscreen();

	WindowMinimized = false;
	XkbSetDetectableAutoRepeat(XDisplay, True, &AutorepeatSupport);

	Window tmp;
	u32 borderWidth;
	int x, y;
	unsigned int bits;

	XGetGeometry(XDisplay, XWindow, &tmp, &x, &y, &Width, &Height, &borderWidth, &bits);
	CreationParams.Bits = bits;
	CreationParams.WindowSize.Width = Width;
	CreationParams.WindowSize.Height = Height;

	StdHints = XAllocSizeHints();
	long num;
	XGetWMNormalHints(XDisplay, XWindow, StdHints, &num);

	initXInput2();

#endif // #ifdef _IRR_COMPILE_WITH_X11_
	return true;
}

//! create the driver
void CIrrDeviceLinux::createDriver()
{
	switch (CreationParams.DriverType) {
#ifdef _IRR_COMPILE_WITH_X11_
	case video::EDT_OPENGL:
	{
#ifdef _IRR_COMPILE_WITH_OPENGL_
		video::SExposedVideoData data;
		data.OpenGLLinux.X11Window = XWindow;
		data.OpenGLLinux.X11Display = XDisplay;

		ContextManager->initialize(CreationParams, data);
#endif
		VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, ContextManager);
	}
	break;
	case video::EDT_OPENGL3:
	{
#ifdef ENABLE_OPENGL3
		video::SExposedVideoData data;
		data.OpenGLLinux.X11Window = XWindow;
		data.OpenGLLinux.X11Display = XDisplay;

		ContextManager->initialize(CreationParams, data);
#endif
		VideoDriver = video::createOpenGL3Driver(CreationParams, FileSystem, ContextManager);
	}
	break;
	case video::EDT_OGLES2:
	{
#ifdef _IRR_COMPILE_WITH_OGLES2_
		video::SExposedVideoData data;
		data.OpenGLLinux.X11Window = XWindow;
		data.OpenGLLinux.X11Display = XDisplay;

		ContextManager = new video::CEGLManager();
		ContextManager->initialize(CreationParams, data);
#endif
		VideoDriver = video::createOGLES2Driver(CreationParams, FileSystem, ContextManager);
	}
	break;
	case video::EDT_WEBGL1:
	{
#ifdef _IRR_COMPILE_WITH_WEBGL1_
		video::SExposedVideoData data;
		data.OpenGLLinux.X11Window = XWindow;
		data.OpenGLLinux.X11Display = XDisplay;

		ContextManager = new video::CEGLManager();
		ContextManager->initialize(CreationParams, data);
#endif
		VideoDriver = video::createWebGL1Driver(CreationParams, FileSystem, ContextManager);
	}
	break;
	case video::EDT_NULL:
		VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
		break;
	default:
		os::Printer::log("Unable to create video driver of unknown type.", ELL_ERROR);
		break;
#else // no X11
	case video::EDT_NULL:
		VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
		break;
	default:
		os::Printer::log("No X11 support compiled in. Only Null driver available.", ELL_ERROR);
		break;
#endif
	}
}

#ifdef _IRR_COMPILE_WITH_X11_
bool CIrrDeviceLinux::createInputContext()
{
	// One one side it would be nicer to let users do that - on the other hand
	// not setting the environment locale will not work when using i18n X11 functions.
	// So users would have to call it always or their input is broken badly.
	// We can restore immediately - so won't mess with anything in users apps.
	core::stringc oldLocale(setlocale(LC_CTYPE, NULL));
	setlocale(LC_CTYPE, ""); // use environment locale

	if (!XSupportsLocale()) {
		os::Printer::log("Locale not supported. Falling back to non-i18n input.", ELL_WARNING);
		setlocale(LC_CTYPE, oldLocale.c_str());
		return false;
	}

	// Load XMODIFIERS (e.g. for IMEs)
	if (!XSetLocaleModifiers("")) {
		setlocale(LC_CTYPE, oldLocale.c_str());
		os::Printer::log("XSetLocaleModifiers failed. Falling back to non-i18n input.", ELL_WARNING);
		return false;
	}

	XInputMethod = XOpenIM(XDisplay, NULL, NULL, NULL);
	if (!XInputMethod) {
		setlocale(LC_CTYPE, oldLocale.c_str());
		os::Printer::log("XOpenIM failed to create an input method. Falling back to non-i18n input.", ELL_WARNING);
		return false;
	}

	XIMStyles *im_supported_styles;
	XGetIMValues(XInputMethod, XNQueryInputStyle, &im_supported_styles, (char *)NULL);
	XIMStyle bestStyle = 0;
	XIMStyle supportedStyle = XIMPreeditNothing | XIMStatusNothing;
	for (int i = 0; i < im_supported_styles->count_styles; ++i) {
		XIMStyle style = im_supported_styles->supported_styles[i];
		if ((style & supportedStyle) == style) /* if we can handle it */
		{
			bestStyle = style;
			break;
		}
	}
	XFree(im_supported_styles);

	if (!bestStyle) {
		XDestroyIC(XInputContext);
		XInputContext = 0;

		os::Printer::log("XInputMethod has no input style we can use. Falling back to non-i18n input.", ELL_WARNING);
		setlocale(LC_CTYPE, oldLocale.c_str());
		return false;
	}

	XInputContext = XCreateIC(XInputMethod,
			XNInputStyle, bestStyle,
			XNClientWindow, XWindow,
			(char *)NULL);
	if (!XInputContext) {
		os::Printer::log("XInputContext failed to create an input context. Falling back to non-i18n input.", ELL_WARNING);
		setlocale(LC_CTYPE, oldLocale.c_str());
		return false;
	}
	XSetICFocus(XInputContext);
	setlocale(LC_CTYPE, oldLocale.c_str());
	return true;
}

void CIrrDeviceLinux::destroyInputContext()
{
	if (XInputContext) {
		XUnsetICFocus(XInputContext);
		XDestroyIC(XInputContext);
		XInputContext = 0;
	}
	if (XInputMethod) {
		XCloseIM(XInputMethod);
		XInputMethod = 0;
	}
}

EKEY_CODE CIrrDeviceLinux::getKeyCode(XEvent &event)
{
	EKEY_CODE keyCode = (EKEY_CODE)0;

	SKeyMap mp;
	mp.X11Key = XkbKeycodeToKeysym(XDisplay, event.xkey.keycode, 0, 0);
	const s32 idx = KeyMap.binary_search(mp);
	if (idx != -1) {
		keyCode = (EKEY_CODE)KeyMap[idx].Win32Key;
	}
	if (keyCode == 0) {
		keyCode = KEY_UNKNOWN;
		if (!mp.X11Key) {
			os::Printer::log("No such X11Key, event keycode", core::stringc(event.xkey.keycode).c_str(), ELL_INFORMATION);
		} else if (idx == -1) {
			os::Printer::log("EKEY_CODE not found, X11 keycode", core::stringc(mp.X11Key).c_str(), ELL_INFORMATION);
		} else {
			os::Printer::log("EKEY_CODE is 0, X11 keycode", core::stringc(mp.X11Key).c_str(), ELL_INFORMATION);
		}
	}
	return keyCode;
}
#endif

//! runs the device. Returns false if device wants to be deleted
bool CIrrDeviceLinux::run()
{
	os::Timer::tick();

#ifdef _IRR_COMPILE_WITH_X11_

	if (CursorControl)
		static_cast<CCursorControl *>(CursorControl)->update();

	if ((CreationParams.DriverType != video::EDT_NULL) && XDisplay) {
		SEvent irrevent;
		irrevent.MouseInput.ButtonStates = 0xffffffff;

		while (XPending(XDisplay) > 0 && !Close) {
			XEvent event;
			XNextEvent(XDisplay, &event);
			if (XFilterEvent(&event, None))
				continue;

			switch (event.type) {
			case ConfigureNotify:
				// check for changed window size
				if ((event.xconfigure.width != (int)Width) ||
						(event.xconfigure.height != (int)Height)) {
					Width = event.xconfigure.width;
					Height = event.xconfigure.height;

					if (VideoDriver)
						VideoDriver->OnResize(core::dimension2d<u32>(Width, Height));
				}
				break;

			case MapNotify:
				WindowMinimized = false;
				break;

			case UnmapNotify:
				WindowMinimized = true;
				break;

			case FocusIn:
				WindowHasFocus = true;
				break;

			case FocusOut:
				WindowHasFocus = false;
				break;

			case MotionNotify:
				irrevent.EventType = EET_MOUSE_INPUT_EVENT;
				irrevent.MouseInput.Event = EMIE_MOUSE_MOVED;
				irrevent.MouseInput.X = event.xbutton.x;
				irrevent.MouseInput.Y = event.xbutton.y;
				irrevent.MouseInput.Control = (event.xkey.state & ControlMask) != 0;
				irrevent.MouseInput.Shift = (event.xkey.state & ShiftMask) != 0;

				// mouse button states
				irrevent.MouseInput.ButtonStates = (event.xbutton.state & Button1Mask) ? EMBSM_LEFT : 0;
				irrevent.MouseInput.ButtonStates |= (event.xbutton.state & Button3Mask) ? EMBSM_RIGHT : 0;
				irrevent.MouseInput.ButtonStates |= (event.xbutton.state & Button2Mask) ? EMBSM_MIDDLE : 0;

				postEventFromUser(irrevent);
				break;

			case ButtonPress:
			case ButtonRelease:

				irrevent.EventType = EET_MOUSE_INPUT_EVENT;
				irrevent.MouseInput.X = event.xbutton.x;
				irrevent.MouseInput.Y = event.xbutton.y;
				irrevent.MouseInput.Control = (event.xkey.state & ControlMask) != 0;
				irrevent.MouseInput.Shift = (event.xkey.state & ShiftMask) != 0;

				// mouse button states
				// This sets the state which the buttons had _prior_ to the event.
				// So unlike on Windows the button which just got changed has still the old state here.
				// We handle that below by flipping the corresponding bit later.
				irrevent.MouseInput.ButtonStates = (event.xbutton.state & Button1Mask) ? EMBSM_LEFT : 0;
				irrevent.MouseInput.ButtonStates |= (event.xbutton.state & Button3Mask) ? EMBSM_RIGHT : 0;
				irrevent.MouseInput.ButtonStates |= (event.xbutton.state & Button2Mask) ? EMBSM_MIDDLE : 0;

				irrevent.MouseInput.Event = EMIE_COUNT;

				switch (event.xbutton.button) {
				case Button1:
					irrevent.MouseInput.Event =
							(event.type == ButtonPress) ? EMIE_LMOUSE_PRESSED_DOWN : EMIE_LMOUSE_LEFT_UP;
					irrevent.MouseInput.ButtonStates ^= EMBSM_LEFT;
					break;

				case Button3:
					irrevent.MouseInput.Event =
							(event.type == ButtonPress) ? EMIE_RMOUSE_PRESSED_DOWN : EMIE_RMOUSE_LEFT_UP;
					irrevent.MouseInput.ButtonStates ^= EMBSM_RIGHT;
					break;

				case Button2:
					irrevent.MouseInput.Event =
							(event.type == ButtonPress) ? EMIE_MMOUSE_PRESSED_DOWN : EMIE_MMOUSE_LEFT_UP;
					irrevent.MouseInput.ButtonStates ^= EMBSM_MIDDLE;
					break;

				case Button4:
					if (event.type == ButtonPress) {
						irrevent.MouseInput.Event = EMIE_MOUSE_WHEEL;
						irrevent.MouseInput.Wheel = 1.0f;
					}
					break;

				case Button5:
					if (event.type == ButtonPress) {
						irrevent.MouseInput.Event = EMIE_MOUSE_WHEEL;
						irrevent.MouseInput.Wheel = -1.0f;
					}
					break;
				}

				if (irrevent.MouseInput.Event != EMIE_COUNT) {
					postEventFromUser(irrevent);

					if (irrevent.MouseInput.Event >= EMIE_LMOUSE_PRESSED_DOWN && irrevent.MouseInput.Event <= EMIE_MMOUSE_PRESSED_DOWN) {
						u32 clicks = checkSuccessiveClicks(irrevent.MouseInput.X, irrevent.MouseInput.Y, irrevent.MouseInput.Event);
						if (clicks == 2) {
							irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_DOUBLE_CLICK + irrevent.MouseInput.Event - EMIE_LMOUSE_PRESSED_DOWN);
							postEventFromUser(irrevent);
						} else if (clicks == 3) {
							irrevent.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_TRIPLE_CLICK + irrevent.MouseInput.Event - EMIE_LMOUSE_PRESSED_DOWN);
							postEventFromUser(irrevent);
						}
					}
				}
				break;

			case MappingNotify:
				XRefreshKeyboardMapping(&event.xmapping);
				break;

			case KeyRelease:
				if (0 == AutorepeatSupport && (XPending(XDisplay) > 0)) {
					// check for Autorepeat manually
					// We'll do the same as Windows does: Only send KeyPressed
					// So every KeyRelease is a real release
					XEvent next_event;
					XPeekEvent(event.xkey.display, &next_event);
					if ((next_event.type == KeyPress) &&
							(next_event.xkey.keycode == event.xkey.keycode) &&
							(next_event.xkey.time - event.xkey.time) < 2) // usually same time, but on some systems a difference of 1 is possible
					{
						// Ignore the key release event
						break;
					}
				}

				irrevent.EventType = EET_KEY_INPUT_EVENT;
				irrevent.KeyInput.PressedDown = false;
				irrevent.KeyInput.Char = 0; // on release that's undefined
				irrevent.KeyInput.Control = (event.xkey.state & ControlMask) != 0;
				irrevent.KeyInput.Shift = (event.xkey.state & ShiftMask) != 0;
				irrevent.KeyInput.Key = getKeyCode(event);

				postEventFromUser(irrevent);
				break;

			case KeyPress: {
				SKeyMap mp;
				if (XInputContext) {
					wchar_t buf[64] = {0};
					Status status;
					int strLen = XwcLookupString(XInputContext, &event.xkey, buf, sizeof(buf) / sizeof(wchar_t) - 1, &mp.X11Key, &status);
					if (status == XBufferOverflow) {
						os::Printer::log("XwcLookupString needs a larger buffer", ELL_INFORMATION);
					}
					if (strLen > 0 && (status == XLookupChars || status == XLookupBoth)) {
						if (strLen > 1) {
							// Multiple characters: send string event
							irrevent.EventType = EET_STRING_INPUT_EVENT;
							irrevent.StringInput.Str = new core::stringw(buf);
							postEventFromUser(irrevent);
							delete irrevent.StringInput.Str;
							irrevent.StringInput.Str = NULL;
							break;
						} else {
							irrevent.KeyInput.Char = buf[0];
						}
					} else {
#if 0 // Most of those are fine - but useful to have the info when debugging Irrlicht itself.
							if ( status == XLookupNone )
								os::Printer::log("XLookupNone", ELL_INFORMATION);
							else if ( status ==  XLookupKeySym )
								// Getting this also when user did not set setlocale(LC_ALL, ""); and using an unknown locale
								// XSupportsLocale doesn't seem to catch that unfortunately - any other ideas to catch it are welcome.
								os::Printer::log("XLookupKeySym", ELL_INFORMATION);
							else if ( status ==  XBufferOverflow )
								os::Printer::log("XBufferOverflow", ELL_INFORMATION);
							else if ( strLen == 0 )
								os::Printer::log("no string", ELL_INFORMATION);
#endif
						irrevent.KeyInput.Char = 0;
					}
				} else // Old version without InputContext. Does not support i18n, but good to have as fallback.
				{
					union
					{
						char buf[8];
						wchar_t wbuf[2];
					} tmp = {{0}};
					XLookupString(&event.xkey, tmp.buf, sizeof(tmp.buf), &mp.X11Key, NULL);
					irrevent.KeyInput.Char = tmp.wbuf[0];
				}

				irrevent.EventType = EET_KEY_INPUT_EVENT;
				irrevent.KeyInput.PressedDown = true;
				irrevent.KeyInput.Control = (event.xkey.state & ControlMask) != 0;
				irrevent.KeyInput.Shift = (event.xkey.state & ShiftMask) != 0;
				irrevent.KeyInput.Key = getKeyCode(event);
				postEventFromUser(irrevent);
			} break;

			case ClientMessage: {
				if (static_cast<Atom>(event.xclient.data.l[0]) == X_ATOM_WM_DELETE_WINDOW && X_ATOM_WM_DELETE_WINDOW != None) {
					os::Printer::log("Quit message received.", ELL_INFORMATION);
					Close = true;
				} else {
					// we assume it's a user message
					irrevent.EventType = EET_USER_EVENT;
					irrevent.UserEvent.UserData1 = static_cast<size_t>(event.xclient.data.l[0]);
					irrevent.UserEvent.UserData2 = static_cast<size_t>(event.xclient.data.l[1]);
					postEventFromUser(irrevent);
				}
			} break;

			case SelectionRequest: {
				XSelectionRequestEvent *req = &(event.xselectionrequest);

				auto send_response = [this, req](Atom property) {
					XEvent response;
					response.xselection.type = SelectionNotify;
					response.xselection.display = req->display;
					response.xselection.requestor = req->requestor;
					response.xselection.selection = req->selection;
					response.xselection.target = req->target;
					response.xselection.property = property;
					response.xselection.time = req->time;
					XSendEvent(XDisplay, req->requestor, 0, 0, &response);
					XFlush(XDisplay);
				};
				auto send_response_refuse = [&send_response] {
					send_response(None);
				};

				// sets the required property to data of type type and
				// sends the according response
				auto set_property_and_notify = [this, req, &send_response](Atom type, int format, const void *data, u32 data_size) {
					XChangeProperty(XDisplay,
							req->requestor,
							req->property,
							type,
							format,
							PropModeReplace,
							(const unsigned char *)data,
							data_size);
					send_response(req->property);
				};

				if ((req->selection != X_ATOM_CLIPBOARD &&
							req->selection != XA_PRIMARY) ||
						req->owner != XWindow) {
					// we are not the owner, refuse request
					send_response_refuse();
					break;
				}
				const core::stringc &text_buffer = req->selection == X_ATOM_CLIPBOARD ? Clipboard : PrimarySelection;

				// for debugging:
				//~ {
				//~ char *target_name = XGetAtomName(XDisplay, req->target);
				//~ fprintf(stderr, "CIrrDeviceLinux::run: target: %s (=%ld)\n",
				//~ target_name, req->target);
				//~ XFree(target_name);
				//~ }

				if (req->property == None) {
					// req is from obsolete client, use target as property name
					// and X_ATOM_UTF8_STRING as type
					// Note: this was not tested and might be incorrect
					os::Printer::log("CIrrDeviceLinux::run: SelectionRequest from obsolete client",
							ELL_WARNING);
					XChangeProperty(XDisplay,
							req->requestor,
							req->target, X_ATOM_UTF8_STRING,
							8, // format = 8-bit
							PropModeReplace,
							(unsigned char *)text_buffer.c_str(),
							text_buffer.size());
					send_response(req->target);
					break;
				}

				if (req->target == X_ATOM_TARGETS) {
					Atom data[] = {
							X_ATOM_TARGETS,
							X_ATOM_TEXT,
							X_ATOM_UTF8_STRING,
							X_ATOM_UTF8_MIME_TYPE};
					set_property_and_notify(
							XA_ATOM,
							32, // Atom is long, we need to set 32 for longs
							&data,
							sizeof(data) / sizeof(*data));

				} else if (req->target == X_ATOM_TEXT ||
						   req->target == X_ATOM_UTF8_STRING ||
						   req->target == X_ATOM_UTF8_MIME_TYPE) {
					set_property_and_notify(
							X_ATOM_UTF8_STRING,
							8,
							text_buffer.c_str(),
							text_buffer.size());

				} else {
					// refuse the request
					send_response_refuse();
				}
			} break;

#if defined(_IRR_LINUX_X11_XINPUT2_)
			case GenericEvent: {
				XGenericEventCookie *cookie = &event.xcookie;
				if (XGetEventData(XDisplay, cookie) && cookie->extension == XI_EXTENSIONS_OPCODE && XI_EXTENSIONS_OPCODE && (cookie->evtype == XI_TouchUpdate || cookie->evtype == XI_TouchBegin || cookie->evtype == XI_TouchEnd)) {
					XIDeviceEvent *de = (XIDeviceEvent *)cookie->data;

					irrevent.EventType = EET_TOUCH_INPUT_EVENT;

					irrevent.TouchInput.Event = cookie->evtype == XI_TouchUpdate ? ETIE_MOVED : (cookie->evtype == XI_TouchBegin ? ETIE_PRESSED_DOWN : ETIE_LEFT_UP);

					irrevent.TouchInput.ID = de->detail;
					irrevent.TouchInput.X = de->event_x;
					irrevent.TouchInput.Y = de->event_y;

					if (irrevent.TouchInput.Event == ETIE_PRESSED_DOWN) {
						currentTouchedCount++;
					}
					irrevent.TouchInput.touchedCount = currentTouchedCount;
					if (currentTouchedCount > 0 && irrevent.TouchInput.Event == ETIE_LEFT_UP) {
						currentTouchedCount--;
					}

					postEventFromUser(irrevent);
				}
			} break;
#endif

			default:
				break;
			} // end switch

			// Update IME information
			if (XInputContext && GUIEnvironment) {
				gui::IGUIElement *elem = GUIEnvironment->getFocus();
				if (elem && elem->acceptsIME()) {
					core::rect<s32> r = elem->getAbsolutePosition();
					XPoint p;
					p.x = (short)r.UpperLeftCorner.X;
					p.y = (short)r.LowerRightCorner.Y;
					XSetICFocus(XInputContext);
					XVaNestedList l = XVaCreateNestedList(0, XNSpotLocation, &p, NULL);
					XSetICValues(XInputContext, XNPreeditAttributes, l, NULL);
					XFree(l);
				} else {
					XUnsetICFocus(XInputContext);
				}
			}

		} // end while
	}
#endif //_IRR_COMPILE_WITH_X11_

	if (!Close)
		pollJoysticks();

	return !Close;
}

//! Pause the current process for the minimum time allowed only to allow other processes to execute
void CIrrDeviceLinux::yield()
{
	struct timespec ts = {0, 1};
	nanosleep(&ts, NULL);
}

//! Pause execution and let other processes to run for a specified amount of time.
void CIrrDeviceLinux::sleep(u32 timeMs, bool pauseTimer = false)
{
	const bool wasStopped = Timer ? Timer->isStopped() : true;

	struct timespec ts;
	ts.tv_sec = (time_t)(timeMs / 1000);
	ts.tv_nsec = (long)(timeMs % 1000) * 1000000;

	if (pauseTimer && !wasStopped)
		Timer->stop();

	nanosleep(&ts, NULL);

	if (pauseTimer && !wasStopped)
		Timer->start();
}

//! sets the caption of the window
void CIrrDeviceLinux::setWindowCaption(const wchar_t *text)
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (CreationParams.DriverType == video::EDT_NULL)
		return;

	XTextProperty txt;
	if (Success == XwcTextListToTextProperty(XDisplay, const_cast<wchar_t **>(&text),
						   1, XStdICCTextStyle, &txt)) {
		XSetWMName(XDisplay, XWindow, &txt);
		XSetWMIconName(XDisplay, XWindow, &txt);
		XFree(txt.value);
	}
#endif
}

//! Sets the window icon.
bool CIrrDeviceLinux::setWindowIcon(const video::IImage *img)
{
	if (CreationParams.DriverType == video::EDT_NULL)
		return false; // no display and window

	u32 height = img->getDimension().Height;
	u32 width = img->getDimension().Width;

	size_t icon_buffer_len = 2 + height * width;
	long *icon_buffer = new long[icon_buffer_len];

	icon_buffer[0] = width;
	icon_buffer[1] = height;

	for (u32 x = 0; x < width; x++) {
		for (u32 y = 0; y < height; y++) {
			video::SColor col = img->getPixel(x, y);
			long pixel_val = 0;
			pixel_val |= (u8)col.getAlpha() << 24;
			pixel_val |= (u8)col.getRed() << 16;
			pixel_val |= (u8)col.getGreen() << 8;
			pixel_val |= (u8)col.getBlue();
			icon_buffer[2 + x + y * width] = pixel_val;
		}
	}

	if (XDisplay == NULL) {
		os::Printer::log("Could not find x11 display for setting its icon.", ELL_ERROR);
		delete[] icon_buffer;
		return false;
	}

	Atom net_wm_icon = XInternAtom(XDisplay, "_NET_WM_ICON", False);
	Atom cardinal = XInternAtom(XDisplay, "CARDINAL", False);
	XChangeProperty(XDisplay, XWindow, net_wm_icon, cardinal, 32, PropModeReplace,
			(const unsigned char *)icon_buffer, icon_buffer_len);

	delete[] icon_buffer;

	return true;
}

//! notifies the device that it should close itself
void CIrrDeviceLinux::closeDevice()
{
	Close = true;
}

//! returns if window is active. if not, nothing need to be drawn
bool CIrrDeviceLinux::isWindowActive() const
{
	return (WindowHasFocus && !WindowMinimized);
}

//! returns if window has focus.
bool CIrrDeviceLinux::isWindowFocused() const
{
	return WindowHasFocus;
}

//! returns if window is minimized.
bool CIrrDeviceLinux::isWindowMinimized() const
{
	return WindowMinimized;
}

//! returns last state from maximizeWindow() and restoreWindow()
bool CIrrDeviceLinux::isWindowMaximized() const
{
	return WindowMaximized;
}

//! Checks if the Irrlicht device supports touch events.
bool CIrrDeviceLinux::supportsTouchEvents() const
{
#if defined(_IRR_LINUX_X11_XINPUT2_)
	return true;
#else
	return false;
#endif
}


//! returns color format of the window.
video::ECOLOR_FORMAT CIrrDeviceLinux::getColorFormat() const
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (VisualInfo && (VisualInfo->depth != 16))
		return video::ECF_R8G8B8;
	else
#endif
		return video::ECF_R5G6B5;
}

//! Sets if the window should be resizable in windowed mode.
void CIrrDeviceLinux::setResizable(bool resize)
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (CreationParams.DriverType == video::EDT_NULL || CreationParams.Fullscreen)
		return;

	if (!resize) {
		// Must be heap memory because data size depends on X Server
		XSizeHints *hints = XAllocSizeHints();
		hints->flags = PSize | PMinSize | PMaxSize;
		hints->min_width = hints->max_width = hints->base_width = Width;
		hints->min_height = hints->max_height = hints->base_height = Height;
		XSetWMNormalHints(XDisplay, XWindow, hints);
		XFree(hints);
	} else {
		XSetWMNormalHints(XDisplay, XWindow, StdHints);
	}
	XFlush(XDisplay);
#endif // #ifdef _IRR_COMPILE_WITH_X11_
}

//! Resize the render window.
void CIrrDeviceLinux::setWindowSize(const core::dimension2d<u32> &size)
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (CreationParams.DriverType == video::EDT_NULL || CreationParams.Fullscreen)
		return;

	XWindowChanges values;
	values.width = size.Width;
	values.height = size.Height;
	XConfigureWindow(XDisplay, XWindow, CWWidth | CWHeight, &values);
	XFlush(XDisplay);
#endif // #ifdef _IRR_COMPILE_WITH_X11_
}

//! Minimize window
void CIrrDeviceLinux::minimizeWindow()
{
#ifdef _IRR_COMPILE_WITH_X11_
	XIconifyWindow(XDisplay, XWindow, Screennr);
#endif
}

//! Maximize window
void CIrrDeviceLinux::maximizeWindow()
{
#ifdef _IRR_COMPILE_WITH_X11_
	// Maximize is not implemented in bare X, it's a WM construct.
	if (HasNetWM) {
		XEvent ev = {0};

		ev.type = ClientMessage;
		ev.xclient.window = XWindow;
		ev.xclient.message_type = X_ATOM_NETWM_STATE;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
		ev.xclient.data.l[1] = X_ATOM_NETWM_MAXIMIZE_VERT;
		ev.xclient.data.l[2] = X_ATOM_NETWM_MAXIMIZE_HORZ;

		XSendEvent(XDisplay, DefaultRootWindow(XDisplay), false,
				SubstructureNotifyMask | SubstructureRedirectMask, &ev);
	}

	XMapWindow(XDisplay, XWindow);

	WindowMaximized = true;
#endif
}

//! Restore original window size
void CIrrDeviceLinux::restoreWindow()
{
#ifdef _IRR_COMPILE_WITH_X11_
	// Maximize is not implemented in bare X, it's a WM construct.
	if (HasNetWM) {
		XEvent ev = {0};

		ev.type = ClientMessage;
		ev.xclient.window = XWindow;
		ev.xclient.message_type = X_ATOM_NETWM_STATE;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE
		ev.xclient.data.l[1] = X_ATOM_NETWM_MAXIMIZE_VERT;
		ev.xclient.data.l[2] = X_ATOM_NETWM_MAXIMIZE_HORZ;

		XSendEvent(XDisplay, DefaultRootWindow(XDisplay), false,
				SubstructureNotifyMask | SubstructureRedirectMask, &ev);
	}

	XMapWindow(XDisplay, XWindow);

	WindowMaximized = false;
#endif
}

core::position2di CIrrDeviceLinux::getWindowPosition()
{
	int wx = 0, wy = 0;
#ifdef _IRR_COMPILE_WITH_X11_
	Window child;
	XTranslateCoordinates(XDisplay, XWindow, DefaultRootWindow(XDisplay), 0, 0, &wx, &wy, &child);
#endif
	return core::position2di(wx, wy);
}

void CIrrDeviceLinux::createKeyMap()
{
	// I don't know if this is the best method  to create
	// the lookuptable, but I'll leave it like that until
	// I find a better version.
	// Search for missing numbers in keysymdef.h

#ifdef _IRR_COMPILE_WITH_X11_
	KeyMap.reallocate(190);
	KeyMap.push_back(SKeyMap(XK_BackSpace, KEY_BACK));
	KeyMap.push_back(SKeyMap(XK_Tab, KEY_TAB));
	KeyMap.push_back(SKeyMap(XK_ISO_Left_Tab, KEY_TAB));
	KeyMap.push_back(SKeyMap(XK_Linefeed, 0)); // ???
	KeyMap.push_back(SKeyMap(XK_Clear, KEY_CLEAR));
	KeyMap.push_back(SKeyMap(XK_Return, KEY_RETURN));
	KeyMap.push_back(SKeyMap(XK_Pause, KEY_PAUSE));
	KeyMap.push_back(SKeyMap(XK_Scroll_Lock, KEY_SCROLL));
	KeyMap.push_back(SKeyMap(XK_Sys_Req, 0)); // ???
	KeyMap.push_back(SKeyMap(XK_Escape, KEY_ESCAPE));
	KeyMap.push_back(SKeyMap(XK_Insert, KEY_INSERT));
	KeyMap.push_back(SKeyMap(XK_Delete, KEY_DELETE));
	KeyMap.push_back(SKeyMap(XK_Home, KEY_HOME));
	KeyMap.push_back(SKeyMap(XK_Left, KEY_LEFT));
	KeyMap.push_back(SKeyMap(XK_Up, KEY_UP));
	KeyMap.push_back(SKeyMap(XK_Right, KEY_RIGHT));
	KeyMap.push_back(SKeyMap(XK_Down, KEY_DOWN));
	KeyMap.push_back(SKeyMap(XK_Prior, KEY_PRIOR));
	KeyMap.push_back(SKeyMap(XK_Page_Up, KEY_PRIOR));
	KeyMap.push_back(SKeyMap(XK_Next, KEY_NEXT));
	KeyMap.push_back(SKeyMap(XK_Page_Down, KEY_NEXT));
	KeyMap.push_back(SKeyMap(XK_End, KEY_END));
	KeyMap.push_back(SKeyMap(XK_Begin, KEY_NUMPAD5));
	KeyMap.push_back(SKeyMap(XK_Num_Lock, KEY_NUMLOCK));
	KeyMap.push_back(SKeyMap(XK_KP_Space, KEY_SPACE));
	KeyMap.push_back(SKeyMap(XK_KP_Tab, KEY_TAB));
	KeyMap.push_back(SKeyMap(XK_KP_Enter, KEY_RETURN));
	KeyMap.push_back(SKeyMap(XK_KP_F1, KEY_F1));
	KeyMap.push_back(SKeyMap(XK_KP_F2, KEY_F2));
	KeyMap.push_back(SKeyMap(XK_KP_F3, KEY_F3));
	KeyMap.push_back(SKeyMap(XK_KP_F4, KEY_F4));
	KeyMap.push_back(SKeyMap(XK_KP_Home, KEY_HOME));
	KeyMap.push_back(SKeyMap(XK_KP_Left, KEY_LEFT));
	KeyMap.push_back(SKeyMap(XK_KP_Up, KEY_UP));
	KeyMap.push_back(SKeyMap(XK_KP_Right, KEY_RIGHT));
	KeyMap.push_back(SKeyMap(XK_KP_Down, KEY_DOWN));
	KeyMap.push_back(SKeyMap(XK_Print, KEY_PRINT));
	KeyMap.push_back(SKeyMap(XK_KP_Prior, KEY_PRIOR));
	KeyMap.push_back(SKeyMap(XK_KP_Page_Up, KEY_PRIOR));
	KeyMap.push_back(SKeyMap(XK_KP_Next, KEY_NEXT));
	KeyMap.push_back(SKeyMap(XK_KP_Page_Down, KEY_NEXT));
	KeyMap.push_back(SKeyMap(XK_KP_End, KEY_END));
	KeyMap.push_back(SKeyMap(XK_KP_Begin, KEY_NUMPAD5));
	KeyMap.push_back(SKeyMap(XK_KP_Insert, KEY_INSERT));
	KeyMap.push_back(SKeyMap(XK_KP_Delete, KEY_DELETE));
	KeyMap.push_back(SKeyMap(XK_KP_Equal, 0)); // ???
	KeyMap.push_back(SKeyMap(XK_KP_Multiply, KEY_MULTIPLY));
	KeyMap.push_back(SKeyMap(XK_KP_Add, KEY_ADD));
	KeyMap.push_back(SKeyMap(XK_KP_Separator, KEY_SEPARATOR));
	KeyMap.push_back(SKeyMap(XK_KP_Subtract, KEY_SUBTRACT));
	KeyMap.push_back(SKeyMap(XK_KP_Decimal, KEY_DECIMAL));
	KeyMap.push_back(SKeyMap(XK_KP_Divide, KEY_DIVIDE));
	KeyMap.push_back(SKeyMap(XK_KP_0, KEY_NUMPAD0));
	KeyMap.push_back(SKeyMap(XK_KP_1, KEY_NUMPAD1));
	KeyMap.push_back(SKeyMap(XK_KP_2, KEY_NUMPAD2));
	KeyMap.push_back(SKeyMap(XK_KP_3, KEY_NUMPAD3));
	KeyMap.push_back(SKeyMap(XK_KP_4, KEY_NUMPAD4));
	KeyMap.push_back(SKeyMap(XK_KP_5, KEY_NUMPAD5));
	KeyMap.push_back(SKeyMap(XK_KP_6, KEY_NUMPAD6));
	KeyMap.push_back(SKeyMap(XK_KP_7, KEY_NUMPAD7));
	KeyMap.push_back(SKeyMap(XK_KP_8, KEY_NUMPAD8));
	KeyMap.push_back(SKeyMap(XK_KP_9, KEY_NUMPAD9));
	KeyMap.push_back(SKeyMap(XK_F1, KEY_F1));
	KeyMap.push_back(SKeyMap(XK_F2, KEY_F2));
	KeyMap.push_back(SKeyMap(XK_F3, KEY_F3));
	KeyMap.push_back(SKeyMap(XK_F4, KEY_F4));
	KeyMap.push_back(SKeyMap(XK_F5, KEY_F5));
	KeyMap.push_back(SKeyMap(XK_F6, KEY_F6));
	KeyMap.push_back(SKeyMap(XK_F7, KEY_F7));
	KeyMap.push_back(SKeyMap(XK_F8, KEY_F8));
	KeyMap.push_back(SKeyMap(XK_F9, KEY_F9));
	KeyMap.push_back(SKeyMap(XK_F10, KEY_F10));
	KeyMap.push_back(SKeyMap(XK_F11, KEY_F11));
	KeyMap.push_back(SKeyMap(XK_F12, KEY_F12));
	KeyMap.push_back(SKeyMap(XK_Shift_L, KEY_LSHIFT));
	KeyMap.push_back(SKeyMap(XK_Shift_R, KEY_RSHIFT));
	KeyMap.push_back(SKeyMap(XK_Control_L, KEY_LCONTROL));
	KeyMap.push_back(SKeyMap(XK_Control_R, KEY_RCONTROL));
	KeyMap.push_back(SKeyMap(XK_Caps_Lock, KEY_CAPITAL));
	KeyMap.push_back(SKeyMap(XK_Shift_Lock, KEY_CAPITAL));
	KeyMap.push_back(SKeyMap(XK_Meta_L, KEY_LWIN));
	KeyMap.push_back(SKeyMap(XK_Meta_R, KEY_RWIN));
	KeyMap.push_back(SKeyMap(XK_Alt_L, KEY_LMENU));
	KeyMap.push_back(SKeyMap(XK_Alt_R, KEY_RMENU));
	KeyMap.push_back(SKeyMap(XK_ISO_Level3_Shift, KEY_RMENU));
	KeyMap.push_back(SKeyMap(XK_Menu, KEY_MENU));
	KeyMap.push_back(SKeyMap(XK_space, KEY_SPACE));
	KeyMap.push_back(SKeyMap(XK_exclam, 0));   //?
	KeyMap.push_back(SKeyMap(XK_quotedbl, 0)); //?
	KeyMap.push_back(SKeyMap(XK_section, 0));  //?
	KeyMap.push_back(SKeyMap(XK_numbersign, KEY_OEM_2));
	KeyMap.push_back(SKeyMap(XK_dollar, 0));    //?
	KeyMap.push_back(SKeyMap(XK_percent, 0));   //?
	KeyMap.push_back(SKeyMap(XK_ampersand, 0)); //?
	KeyMap.push_back(SKeyMap(XK_apostrophe, KEY_OEM_7));
	KeyMap.push_back(SKeyMap(XK_parenleft, 0));       //?
	KeyMap.push_back(SKeyMap(XK_parenright, 0));      //?
	KeyMap.push_back(SKeyMap(XK_asterisk, 0));        //?
	KeyMap.push_back(SKeyMap(XK_plus, KEY_PLUS));     //?
	KeyMap.push_back(SKeyMap(XK_comma, KEY_COMMA));   //?
	KeyMap.push_back(SKeyMap(XK_minus, KEY_MINUS));   //?
	KeyMap.push_back(SKeyMap(XK_period, KEY_PERIOD)); //?
	KeyMap.push_back(SKeyMap(XK_slash, KEY_OEM_2));   //?
	KeyMap.push_back(SKeyMap(XK_0, KEY_KEY_0));
	KeyMap.push_back(SKeyMap(XK_1, KEY_KEY_1));
	KeyMap.push_back(SKeyMap(XK_2, KEY_KEY_2));
	KeyMap.push_back(SKeyMap(XK_3, KEY_KEY_3));
	KeyMap.push_back(SKeyMap(XK_4, KEY_KEY_4));
	KeyMap.push_back(SKeyMap(XK_5, KEY_KEY_5));
	KeyMap.push_back(SKeyMap(XK_6, KEY_KEY_6));
	KeyMap.push_back(SKeyMap(XK_7, KEY_KEY_7));
	KeyMap.push_back(SKeyMap(XK_8, KEY_KEY_8));
	KeyMap.push_back(SKeyMap(XK_9, KEY_KEY_9));
	KeyMap.push_back(SKeyMap(XK_colon, 0)); //?
	KeyMap.push_back(SKeyMap(XK_semicolon, KEY_OEM_1));
	KeyMap.push_back(SKeyMap(XK_less, KEY_OEM_102));
	KeyMap.push_back(SKeyMap(XK_equal, KEY_PLUS));
	KeyMap.push_back(SKeyMap(XK_greater, 0));    //?
	KeyMap.push_back(SKeyMap(XK_question, 0));   //?
	KeyMap.push_back(SKeyMap(XK_at, KEY_KEY_2)); //?
	KeyMap.push_back(SKeyMap(XK_mu, 0));         //?
	KeyMap.push_back(SKeyMap(XK_EuroSign, 0));   //?
	KeyMap.push_back(SKeyMap(XK_A, KEY_KEY_A));
	KeyMap.push_back(SKeyMap(XK_B, KEY_KEY_B));
	KeyMap.push_back(SKeyMap(XK_C, KEY_KEY_C));
	KeyMap.push_back(SKeyMap(XK_D, KEY_KEY_D));
	KeyMap.push_back(SKeyMap(XK_E, KEY_KEY_E));
	KeyMap.push_back(SKeyMap(XK_F, KEY_KEY_F));
	KeyMap.push_back(SKeyMap(XK_G, KEY_KEY_G));
	KeyMap.push_back(SKeyMap(XK_H, KEY_KEY_H));
	KeyMap.push_back(SKeyMap(XK_I, KEY_KEY_I));
	KeyMap.push_back(SKeyMap(XK_J, KEY_KEY_J));
	KeyMap.push_back(SKeyMap(XK_K, KEY_KEY_K));
	KeyMap.push_back(SKeyMap(XK_L, KEY_KEY_L));
	KeyMap.push_back(SKeyMap(XK_M, KEY_KEY_M));
	KeyMap.push_back(SKeyMap(XK_N, KEY_KEY_N));
	KeyMap.push_back(SKeyMap(XK_O, KEY_KEY_O));
	KeyMap.push_back(SKeyMap(XK_P, KEY_KEY_P));
	KeyMap.push_back(SKeyMap(XK_Q, KEY_KEY_Q));
	KeyMap.push_back(SKeyMap(XK_R, KEY_KEY_R));
	KeyMap.push_back(SKeyMap(XK_S, KEY_KEY_S));
	KeyMap.push_back(SKeyMap(XK_T, KEY_KEY_T));
	KeyMap.push_back(SKeyMap(XK_U, KEY_KEY_U));
	KeyMap.push_back(SKeyMap(XK_V, KEY_KEY_V));
	KeyMap.push_back(SKeyMap(XK_W, KEY_KEY_W));
	KeyMap.push_back(SKeyMap(XK_X, KEY_KEY_X));
	KeyMap.push_back(SKeyMap(XK_Y, KEY_KEY_Y));
	KeyMap.push_back(SKeyMap(XK_Z, KEY_KEY_Z));
	KeyMap.push_back(SKeyMap(XK_bracketleft, KEY_OEM_4));
	KeyMap.push_back(SKeyMap(XK_backslash, KEY_OEM_5));
	KeyMap.push_back(SKeyMap(XK_bracketright, KEY_OEM_6));
	KeyMap.push_back(SKeyMap(XK_asciicircum, KEY_OEM_5));
	KeyMap.push_back(SKeyMap(XK_dead_circumflex, KEY_OEM_5));
	KeyMap.push_back(SKeyMap(XK_degree, 0));             //?
	KeyMap.push_back(SKeyMap(XK_underscore, KEY_MINUS)); //?
	KeyMap.push_back(SKeyMap(XK_grave, KEY_OEM_3));
	KeyMap.push_back(SKeyMap(XK_dead_grave, KEY_OEM_3));
	KeyMap.push_back(SKeyMap(XK_acute, KEY_OEM_6));
	KeyMap.push_back(SKeyMap(XK_dead_acute, KEY_OEM_6));
	KeyMap.push_back(SKeyMap(XK_a, KEY_KEY_A));
	KeyMap.push_back(SKeyMap(XK_b, KEY_KEY_B));
	KeyMap.push_back(SKeyMap(XK_c, KEY_KEY_C));
	KeyMap.push_back(SKeyMap(XK_d, KEY_KEY_D));
	KeyMap.push_back(SKeyMap(XK_e, KEY_KEY_E));
	KeyMap.push_back(SKeyMap(XK_f, KEY_KEY_F));
	KeyMap.push_back(SKeyMap(XK_g, KEY_KEY_G));
	KeyMap.push_back(SKeyMap(XK_h, KEY_KEY_H));
	KeyMap.push_back(SKeyMap(XK_i, KEY_KEY_I));
	KeyMap.push_back(SKeyMap(XK_j, KEY_KEY_J));
	KeyMap.push_back(SKeyMap(XK_k, KEY_KEY_K));
	KeyMap.push_back(SKeyMap(XK_l, KEY_KEY_L));
	KeyMap.push_back(SKeyMap(XK_m, KEY_KEY_M));
	KeyMap.push_back(SKeyMap(XK_n, KEY_KEY_N));
	KeyMap.push_back(SKeyMap(XK_o, KEY_KEY_O));
	KeyMap.push_back(SKeyMap(XK_p, KEY_KEY_P));
	KeyMap.push_back(SKeyMap(XK_q, KEY_KEY_Q));
	KeyMap.push_back(SKeyMap(XK_r, KEY_KEY_R));
	KeyMap.push_back(SKeyMap(XK_s, KEY_KEY_S));
	KeyMap.push_back(SKeyMap(XK_t, KEY_KEY_T));
	KeyMap.push_back(SKeyMap(XK_u, KEY_KEY_U));
	KeyMap.push_back(SKeyMap(XK_v, KEY_KEY_V));
	KeyMap.push_back(SKeyMap(XK_w, KEY_KEY_W));
	KeyMap.push_back(SKeyMap(XK_x, KEY_KEY_X));
	KeyMap.push_back(SKeyMap(XK_y, KEY_KEY_Y));
	KeyMap.push_back(SKeyMap(XK_z, KEY_KEY_Z));
	KeyMap.push_back(SKeyMap(XK_ssharp, KEY_OEM_4));
	KeyMap.push_back(SKeyMap(XK_adiaeresis, KEY_OEM_7));
	KeyMap.push_back(SKeyMap(XK_odiaeresis, KEY_OEM_3));
	KeyMap.push_back(SKeyMap(XK_udiaeresis, KEY_OEM_1));
	KeyMap.push_back(SKeyMap(XK_Super_L, KEY_LWIN));
	KeyMap.push_back(SKeyMap(XK_Super_R, KEY_RWIN));

	KeyMap.sort();
#endif
}

bool CIrrDeviceLinux::activateJoysticks(core::array<SJoystickInfo> &joystickInfo)
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)

	joystickInfo.clear();

	u32 joystick;
	for (joystick = 0; joystick < 32; ++joystick) {
		// The joystick device could be here...
		core::stringc devName = "/dev/js";
		devName += joystick;

		SJoystickInfo returnInfo;
		JoystickInfo info;

		info.fd = open(devName.c_str(), O_RDONLY);
		if (-1 == info.fd) {
			// ...but Ubuntu and possibly other distros
			// create the devices in /dev/input
			devName = "/dev/input/js";
			devName += joystick;
			info.fd = open(devName.c_str(), O_RDONLY);
			if (-1 == info.fd) {
				// and BSD here
				devName = "/dev/joy";
				devName += joystick;
				info.fd = open(devName.c_str(), O_RDONLY);
			}
		}

		if (-1 == info.fd)
			continue;

#ifdef __FreeBSD__
		info.axes = 2;
		info.buttons = 2;
#else
		ioctl(info.fd, JSIOCGAXES, &(info.axes));
		ioctl(info.fd, JSIOCGBUTTONS, &(info.buttons));
		fcntl(info.fd, F_SETFL, O_NONBLOCK);
#endif

		info.persistentData.EventType = EET_JOYSTICK_INPUT_EVENT;
		info.persistentData.JoystickEvent.Joystick = ActiveJoysticks.size();

		// There's no obvious way to determine which (if any) axes represent a POV
		// hat, so we'll just set it to "not used" and forget about it.
		info.persistentData.JoystickEvent.POV = 65535;

		ActiveJoysticks.push_back(info);

		returnInfo.Joystick = joystick;
		returnInfo.PovHat = SJoystickInfo::POV_HAT_UNKNOWN;
		returnInfo.Axes = info.axes;
		returnInfo.Buttons = info.buttons;

#ifndef __FreeBSD__
		char name[80];
		ioctl(info.fd, JSIOCGNAME(80), name);
		returnInfo.Name = name;
#endif

		joystickInfo.push_back(returnInfo);
	}

	for (joystick = 0; joystick < joystickInfo.size(); ++joystick) {
		char logString[256];
		snprintf_irr(logString, sizeof(logString), "Found joystick %d, %d axes, %d buttons '%s'",
				joystick, joystickInfo[joystick].Axes,
				joystickInfo[joystick].Buttons, joystickInfo[joystick].Name.c_str());
		os::Printer::log(logString, ELL_INFORMATION);
	}

	return true;
#else
	return false;
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
}

void CIrrDeviceLinux::pollJoysticks()
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	if (0 == ActiveJoysticks.size())
		return;

	for (u32 j = 0; j < ActiveJoysticks.size(); ++j) {
		JoystickInfo &info = ActiveJoysticks[j];

#ifdef __FreeBSD__
		struct joystick js;
		if (read(info.fd, &js, sizeof(js)) == sizeof(js)) {
			info.persistentData.JoystickEvent.ButtonStates = js.b1 | (js.b2 << 1); /* should be a two-bit field */
			info.persistentData.JoystickEvent.Axis[0] = js.x;                      /* X axis */
			info.persistentData.JoystickEvent.Axis[1] = js.y;                      /* Y axis */
		}
#else
		struct js_event event;
		while (sizeof(event) == read(info.fd, &event, sizeof(event))) {
			switch (event.type & ~JS_EVENT_INIT) {
			case JS_EVENT_BUTTON:
				if (event.value)
					info.persistentData.JoystickEvent.ButtonStates |= (1 << event.number);
				else
					info.persistentData.JoystickEvent.ButtonStates &= ~(1 << event.number);
				break;

			case JS_EVENT_AXIS:
				if (event.number < SEvent::SJoystickEvent::NUMBER_OF_AXES)
					info.persistentData.JoystickEvent.Axis[event.number] = event.value;
				break;

			default:
				break;
			}
		}
#endif

		// Send an irrlicht joystick event once per ::run() even if no new data were received.
		(void)postEventFromUser(info.persistentData);
	}
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
}

#if defined(_IRR_COMPILE_WITH_X11_)
//! gets text from the clipboard
//! \return Returns 0 if no string is in there, otherwise utf-8 text.
const c8 *CIrrDeviceLinux::getTextFromSelection(Atom selection, core::stringc &text_buffer) const
{
	Window ownerWindow = XGetSelectionOwner(XDisplay, selection);
	if (ownerWindow == XWindow) {
		return text_buffer.c_str();
	}

	text_buffer = "";

	if (ownerWindow == None) {
		return text_buffer.c_str();
	}

	// delete the property to be set beforehand
	XDeleteProperty(XDisplay, XWindow, XA_PRIMARY);

	XConvertSelection(XDisplay, selection, X_ATOM_UTF8_STRING, XA_PRIMARY,
			XWindow, CurrentTime);
	XFlush(XDisplay);

	// wait for event via a blocking call
	XEvent event_ret;
	std::pair<Window, Atom> args(XWindow, selection);
	XIfEvent(
			XDisplay, &event_ret, [](Display *_display, XEvent *event, XPointer arg) {
				auto p = reinterpret_cast<std::pair<Window, Atom> *>(arg);
				return (Bool)(event->type == SelectionNotify &&
							  event->xselection.requestor == p->first &&
							  event->xselection.selection == p->second &&
							  event->xselection.target == X_ATOM_UTF8_STRING);
			},
			(XPointer)&args);

	assert(event_ret.type == SelectionNotify &&
						  event_ret.xselection.requestor == XWindow &&
						  event_ret.xselection.selection == selection &&
						  event_ret.xselection.target == X_ATOM_UTF8_STRING);

	Atom property_set = event_ret.xselection.property;
	if (event_ret.xselection.property == None) {
		// request failed => empty string
		return text_buffer.c_str();
	}

	// check for data
	Atom type;
	int format;
	unsigned long numItems, bytesLeft, dummy;
	unsigned char *data = nullptr;
	XGetWindowProperty(XDisplay, XWindow,
			property_set,    // property name
			0,               // offset
			0,               // length (we only check for data, so 0)
			0,               // Delete 0==false
			AnyPropertyType, // AnyPropertyType or property identifier
			&type,           // return type
			&format,         // return format
			&numItems,       // number items
			&bytesLeft,      // remaining bytes for partial reads
			&data);          // data
	if (data) {
		XFree(data);
		data = nullptr;
	}

	// for debugging:
	//~ {
	//~ char *type_name = XGetAtomName(XDisplay, type);
	//~ fprintf(stderr, "CIrrDeviceLinux::getTextFromSelection: actual type: %s (=%ld)\n",
	//~ type_name, type);
	//~ XFree(type_name);
	//~ }

	if (type != X_ATOM_UTF8_STRING && type != X_ATOM_UTF8_MIME_TYPE) {
		os::Printer::log("CIrrDeviceLinux::getTextFromSelection: did not get utf-8 string",
				ELL_WARNING);
		return text_buffer.c_str();
	}

	if (bytesLeft > 0) {
		// there is some data to get
		int result = XGetWindowProperty(XDisplay, XWindow, property_set, 0,
				bytesLeft, 0, AnyPropertyType, &type, &format,
				&numItems, &dummy, &data);
		if (result == Success)
			text_buffer = (c8 *)data;
		XFree(data);
	}

	// delete the property again, to inform the owner about the successful transfer
	XDeleteProperty(XDisplay, XWindow, property_set);

	return text_buffer.c_str();
}
#endif

//! gets text from the clipboard
//! \return Returns 0 if no string is in there, otherwise utf-8 text.
const c8 *CIrrDeviceLinux::getTextFromClipboard() const
{
#if defined(_IRR_COMPILE_WITH_X11_)
	return getTextFromSelection(X_ATOM_CLIPBOARD, Clipboard);
#else
	return nullptr;
#endif
}

//! gets text from the primary selection
//! \return Returns 0 if no string is in there, otherwise utf-8 text.
const c8 *CIrrDeviceLinux::getTextFromPrimarySelection() const
{
#if defined(_IRR_COMPILE_WITH_X11_)
	return getTextFromSelection(XA_PRIMARY, PrimarySelection);
#else
	return nullptr;
#endif
}

#if defined(_IRR_COMPILE_WITH_X11_)
bool CIrrDeviceLinux::becomeSelectionOwner(Atom selection) const
{
	XSetSelectionOwner(XDisplay, selection, XWindow, CurrentTime);
	XFlush(XDisplay);
	Window owner = XGetSelectionOwner(XDisplay, selection);
	return owner == XWindow;
}
#endif

//! copies text to the clipboard
void CIrrDeviceLinux::copyToClipboard(const c8 *text) const
{
#if defined(_IRR_COMPILE_WITH_X11_)
	// Actually there is no clipboard on X but applications just say they own the clipboard and return text when asked.
	// Which btw. also means that on X you lose clipboard content when closing applications.
	Clipboard = text;
	if (!becomeSelectionOwner(X_ATOM_CLIPBOARD)) {
		os::Printer::log("CIrrDeviceLinux::copyToClipboard: failed to set owner", ELL_WARNING);
	}
#endif
}

//! copies text to the primary selection
void CIrrDeviceLinux::copyToPrimarySelection(const c8 *text) const
{
#if defined(_IRR_COMPILE_WITH_X11_)
	PrimarySelection = text;
	if (!becomeSelectionOwner(XA_PRIMARY)) {
		os::Printer::log("CIrrDeviceLinux::copyToPrimarySelection: failed to set owner", ELL_WARNING);
	}
#endif
}

#ifdef _IRR_COMPILE_WITH_X11_
// return true if the passed event has the type passed in parameter arg
Bool PredicateIsEventType(Display *display, XEvent *event, XPointer arg)
{
	if (event && event->type == *(int *)arg) {
		//		os::Printer::log("remove event", core::stringc((int)arg).c_str(), ELL_INFORMATION);
		return True;
	}
	return False;
}
#endif //_IRR_COMPILE_WITH_X11_

//! Remove all messages pending in the system message loop
void CIrrDeviceLinux::clearSystemMessages()
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (CreationParams.DriverType != video::EDT_NULL) {
		XEvent event;
		int usrArg = ButtonPress;
		while (XCheckIfEvent(XDisplay, &event, PredicateIsEventType, XPointer(&usrArg)) == True) {
		}
		usrArg = ButtonRelease;
		while (XCheckIfEvent(XDisplay, &event, PredicateIsEventType, XPointer(&usrArg)) == True) {
		}
		usrArg = MotionNotify;
		while (XCheckIfEvent(XDisplay, &event, PredicateIsEventType, XPointer(&usrArg)) == True) {
		}
		usrArg = KeyRelease;
		while (XCheckIfEvent(XDisplay, &event, PredicateIsEventType, XPointer(&usrArg)) == True) {
		}
		usrArg = KeyPress;
		while (XCheckIfEvent(XDisplay, &event, PredicateIsEventType, XPointer(&usrArg)) == True) {
		}
	}
#endif //_IRR_COMPILE_WITH_X11_
}

//! Get the display density in dots per inch.
float CIrrDeviceLinux::getDisplayDensity() const
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (XDisplay != NULL) {
		/* try x direct */
		int dh = DisplayHeight(XDisplay, 0);
		int dw = DisplayWidth(XDisplay, 0);
		int dh_mm = DisplayHeightMM(XDisplay, 0);
		int dw_mm = DisplayWidthMM(XDisplay, 0);

		if (dh_mm != 0 && dw_mm != 0) {
			float dpi_height = floor(dh / (dh_mm * 0.039370) + 0.5);
			float dpi_width = floor(dw / (dw_mm * 0.039370) + 0.5);
			return std::max(dpi_height, dpi_width);
		}
	}
#endif //_IRR_COMPILE_WITH_X11_

	return 0.0f;
}

void CIrrDeviceLinux::initXAtoms()
{
#ifdef _IRR_COMPILE_WITH_X11_
	X_ATOM_CLIPBOARD = XInternAtom(XDisplay, "CLIPBOARD", False);
	X_ATOM_TARGETS = XInternAtom(XDisplay, "TARGETS", False);
	X_ATOM_UTF8_STRING = XInternAtom(XDisplay, "UTF8_STRING", False);
	X_ATOM_UTF8_MIME_TYPE = XInternAtom(XDisplay, "text/plain;charset=utf-8", False);
	X_ATOM_TEXT = XInternAtom(XDisplay, "TEXT", False);
	X_ATOM_NETWM_MAXIMIZE_VERT = XInternAtom(XDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", true);
	X_ATOM_NETWM_MAXIMIZE_HORZ = XInternAtom(XDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", true);
	X_ATOM_NETWM_STATE = XInternAtom(XDisplay, "_NET_WM_STATE", true);
	X_ATOM_NETWM_STATE_FULLSCREEN = XInternAtom(XDisplay, "_NET_WM_STATE_FULLSCREEN", True);
#endif
}

void CIrrDeviceLinux::initXInput2()
{
#if defined(_IRR_LINUX_X11_XINPUT2_)
	int ev = 0;
	int err = 0;
	if (!XQueryExtension(XDisplay, "XInputExtension", &XI_EXTENSIONS_OPCODE, &ev, &err)) {
		os::Printer::log("X Input extension not available.", ELL_WARNING);
		return;
	}

	int major = 2;
	int minor = 3;
	int rc = XIQueryVersion(XDisplay, &major, &minor);
	if (rc != Success) {
		os::Printer::log("No XI2 support.", ELL_WARNING);
		return;
	}

	// So far we only use XInput2 for touch events.
	// So we enable those and disable all other events for now.
	XIEventMask eventMask;
	unsigned char mask[XIMaskLen(XI_TouchEnd)];
	memset(mask, 0, sizeof(mask));
	eventMask.deviceid = XIAllMasterDevices;
	eventMask.mask_len = sizeof(mask);
	eventMask.mask = mask;
	XISetMask(eventMask.mask, XI_TouchBegin);
	XISetMask(eventMask.mask, XI_TouchUpdate);
	XISetMask(eventMask.mask, XI_TouchEnd);

	XISelectEvents(XDisplay, XWindow, &eventMask, 1);
#endif
}

#ifdef _IRR_COMPILE_WITH_X11_

Cursor CIrrDeviceLinux::TextureToMonochromeCursor(video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot)
{
	XImage *sourceImage = XCreateImage(XDisplay, VisualInfo->visual,
			1,       // depth,
			ZPixmap, // XYBitmap (depth=1), ZPixmap(depth=x)
			0, 0, sourceRect.getWidth(), sourceRect.getHeight(),
			32, // bitmap_pad,
			0   // bytes_per_line (0 means continuous in memory)
	);
	sourceImage->data = new char[sourceImage->height * sourceImage->bytes_per_line];
	XImage *maskImage = XCreateImage(XDisplay, VisualInfo->visual,
			1, // depth,
			ZPixmap,
			0, 0, sourceRect.getWidth(), sourceRect.getHeight(),
			32, // bitmap_pad,
			0   // bytes_per_line
	);
	maskImage->data = new char[maskImage->height * maskImage->bytes_per_line];

	// write texture into XImage
	video::ECOLOR_FORMAT format = tex->getColorFormat();
	u32 bytesPerPixel = video::IImage::getBitsPerPixelFromFormat(format) / 8;
	u32 bytesLeftGap = sourceRect.UpperLeftCorner.X * bytesPerPixel;
	u32 bytesRightGap = tex->getPitch() - sourceRect.LowerRightCorner.X * bytesPerPixel;
	const u8 *data = (const u8 *)tex->lock(video::ETLM_READ_ONLY, 0);
	data += sourceRect.UpperLeftCorner.Y * tex->getPitch();
	for (s32 y = 0; y < sourceRect.getHeight(); ++y) {
		data += bytesLeftGap;
		for (s32 x = 0; x < sourceRect.getWidth(); ++x) {
			video::SColor pixelCol;
			pixelCol.setData((const void *)data, format);
			data += bytesPerPixel;

			if (pixelCol.getAlpha() == 0) { // transparent
				XPutPixel(maskImage, x, y, 0);
				XPutPixel(sourceImage, x, y, 0);
			} else // color
			{
				if ((pixelCol.getRed() + pixelCol.getGreen() +
						pixelCol.getBlue()) / 3 >= 127)
					XPutPixel(sourceImage, x, y, 1);
				else
					XPutPixel(sourceImage, x, y, 0);
				XPutPixel(maskImage, x, y, 1);
			}
		}
		data += bytesRightGap;
	}
	tex->unlock();

	Pixmap sourcePixmap = XCreatePixmap(XDisplay, XWindow, sourceImage->width, sourceImage->height, sourceImage->depth);
	Pixmap maskPixmap = XCreatePixmap(XDisplay, XWindow, maskImage->width, maskImage->height, maskImage->depth);

	XGCValues values;
	values.foreground = 1;
	values.background = 1;
	GC gc = XCreateGC(XDisplay, sourcePixmap, GCForeground | GCBackground, &values);

	XPutImage(XDisplay, sourcePixmap, gc, sourceImage, 0, 0, 0, 0, sourceImage->width, sourceImage->height);
	XPutImage(XDisplay, maskPixmap, gc, maskImage, 0, 0, 0, 0, maskImage->width, maskImage->height);

	XFreeGC(XDisplay, gc);
	XDestroyImage(sourceImage);
	XDestroyImage(maskImage);

	Cursor cursorResult = 0;
	XColor foreground, background;
	foreground.red = 65535;
	foreground.green = 65535;
	foreground.blue = 65535;
	foreground.flags = DoRed | DoGreen | DoBlue;
	background.red = 0;
	background.green = 0;
	background.blue = 0;
	background.flags = DoRed | DoGreen | DoBlue;

	cursorResult = XCreatePixmapCursor(XDisplay, sourcePixmap, maskPixmap, &foreground, &background, hotspot.X, hotspot.Y);

	XFreePixmap(XDisplay, sourcePixmap);
	XFreePixmap(XDisplay, maskPixmap);

	return cursorResult;
}

#ifdef _IRR_LINUX_XCURSOR_
Cursor CIrrDeviceLinux::TextureToARGBCursor(video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot)
{
	XcursorImage *image = XcursorImageCreate(sourceRect.getWidth(), sourceRect.getHeight());
	image->xhot = hotspot.X;
	image->yhot = hotspot.Y;

	// write texture into XcursorImage
	video::ECOLOR_FORMAT format = tex->getColorFormat();
	u32 bytesPerPixel = video::IImage::getBitsPerPixelFromFormat(format) / 8;
	u32 bytesLeftGap = sourceRect.UpperLeftCorner.X * bytesPerPixel;
	u32 bytesRightGap = tex->getPitch() - sourceRect.LowerRightCorner.X * bytesPerPixel;
	XcursorPixel *target = image->pixels;
	const u8 *data = (const u8 *)tex->lock(video::ETLM_READ_ONLY, 0);
	data += sourceRect.UpperLeftCorner.Y * tex->getPitch();
	for (s32 y = 0; y < sourceRect.getHeight(); ++y) {
		data += bytesLeftGap;
		for (s32 x = 0; x < sourceRect.getWidth(); ++x) {
			video::SColor pixelCol;
			pixelCol.setData((const void *)data, format);
			data += bytesPerPixel;

			*target = (XcursorPixel)pixelCol.color;
			++target;
		}
		data += bytesRightGap;
	}
	tex->unlock();

	Cursor cursorResult = XcursorImageLoadCursor(XDisplay, image);

	XcursorImageDestroy(image);

	return cursorResult;
}
#endif // #ifdef _IRR_LINUX_XCURSOR_

Cursor CIrrDeviceLinux::TextureToCursor(video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot)
{
#ifdef _IRR_LINUX_XCURSOR_
	return TextureToARGBCursor(tex, sourceRect, hotspot);
#else
	return TextureToMonochromeCursor(tex, sourceRect, hotspot);
#endif
}
#endif // _IRR_COMPILE_WITH_X11_

CIrrDeviceLinux::CCursorControl::CCursorControl(CIrrDeviceLinux *dev, bool null) :
		Device(dev)
#ifdef _IRR_COMPILE_WITH_X11_
		,
		PlatformBehavior(gui::ECPB_NONE), LastQuery(0)
#ifdef _IRR_LINUX_X11_XINPUT2_
		,
		DeviceId(0)
#endif
#endif
		,
		IsVisible(true), Null(null), UseReferenceRect(false), ActiveIcon(gui::ECI_NORMAL), ActiveIconStartTime(0)
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (!Null) {
#ifdef _IRR_LINUX_X11_XINPUT2_
		// XIWarpPointer is entirely broken on multi-head setups (see also [1]),
		// but behaves better in other cases so we can't just disable it outright.
		// [1] https://developer.blender.org/rB165caafb99c6846e53d11c4e966990aaffc06cea
		if (XScreenCount(Device->XDisplay) > 1) {
			os::Printer::log("Detected classic multi-head setup, not using XIWarpPointer");
		} else {
			XIGetClientPointer(Device->XDisplay, Device->XWindow, &DeviceId);
		}
#endif

		XGCValues values;
		unsigned long valuemask = 0;

		XColor fg, bg;

		// this code, for making the cursor invisible was sent in by
		// Sirshane, thank your very much!

		Pixmap invisBitmap = XCreatePixmap(Device->XDisplay, Device->XWindow, 32, 32, 1);
		Pixmap maskBitmap = XCreatePixmap(Device->XDisplay, Device->XWindow, 32, 32, 1);
		Colormap screen_colormap = DefaultColormap(Device->XDisplay, DefaultScreen(Device->XDisplay));
		XAllocNamedColor(Device->XDisplay, screen_colormap, "black", &fg, &fg);
		XAllocNamedColor(Device->XDisplay, screen_colormap, "white", &bg, &bg);

		GC gc = XCreateGC(Device->XDisplay, invisBitmap, valuemask, &values);

		XSetForeground(Device->XDisplay, gc, BlackPixel(Device->XDisplay, DefaultScreen(Device->XDisplay)));
		XFillRectangle(Device->XDisplay, invisBitmap, gc, 0, 0, 32, 32);
		XFillRectangle(Device->XDisplay, maskBitmap, gc, 0, 0, 32, 32);

		InvisCursor = XCreatePixmapCursor(Device->XDisplay, invisBitmap, maskBitmap, &fg, &bg, 1, 1);
		XFreeGC(Device->XDisplay, gc);
		XFreePixmap(Device->XDisplay, invisBitmap);
		XFreePixmap(Device->XDisplay, maskBitmap);

		initCursors();
	}
#endif
}

CIrrDeviceLinux::CCursorControl::~CCursorControl()
{
	// Do not clearCursors here as the display is already closed
	// TODO (cutealien): dropping cursorcontrol earlier might work, not sure about reason why that's done in stub currently.
}

#ifdef _IRR_COMPILE_WITH_X11_
void CIrrDeviceLinux::CCursorControl::clearCursors()
{
	if (!Null)
		XFreeCursor(Device->XDisplay, InvisCursor);
	for (u32 i = 0; i < Cursors.size(); ++i) {
		for (u32 f = 0; f < Cursors[i].Frames.size(); ++f) {
			XFreeCursor(Device->XDisplay, Cursors[i].Frames[f].IconHW);
		}
	}
}

void CIrrDeviceLinux::CCursorControl::initCursors()
{
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_top_left_arrow))); //  (or XC_arrow?)
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_crosshair)));
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_hand2))); // (or XC_hand1? )
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_question_arrow)));
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_xterm)));
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_X_cursor))); //  (or XC_pirate?)
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_watch)));    // (or XC_clock?)
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_fleur)));
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_top_right_corner))); // NESW not available in X11
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_top_left_corner)));  // NWSE not available in X11
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_sb_v_double_arrow)));
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_sb_h_double_arrow)));
	Cursors.push_back(CursorX11(XCreateFontCursor(Device->XDisplay, XC_sb_up_arrow))); // (or XC_center_ptr?)
}

void CIrrDeviceLinux::CCursorControl::update()
{
	if ((u32)ActiveIcon < Cursors.size() && !Cursors[ActiveIcon].Frames.empty() && Cursors[ActiveIcon].FrameTime) {
		// update animated cursors. This could also be done by X11 in case someone wants to figure that out (this way was just easier to implement)
		u32 now = Device->getTimer()->getRealTime();
		u32 frame = ((now - ActiveIconStartTime) / Cursors[ActiveIcon].FrameTime) % Cursors[ActiveIcon].Frames.size();
		XDefineCursor(Device->XDisplay, Device->XWindow, Cursors[ActiveIcon].Frames[frame].IconHW);
	}
}
#endif

//! Sets the active cursor icon
void CIrrDeviceLinux::CCursorControl::setActiveIcon(gui::ECURSOR_ICON iconId)
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (iconId >= (s32)Cursors.size())
		return;

	if (Cursors[iconId].Frames.size())
		XDefineCursor(Device->XDisplay, Device->XWindow, Cursors[iconId].Frames[0].IconHW);

	ActiveIconStartTime = Device->getTimer()->getRealTime();
	ActiveIcon = iconId;
#endif
}

//! Add a custom sprite as cursor icon.
gui::ECURSOR_ICON CIrrDeviceLinux::CCursorControl::addIcon(const gui::SCursorSprite &icon)
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (icon.SpriteId >= 0) {
		CursorX11 cX11;
		cX11.FrameTime = icon.SpriteBank->getSprites()[icon.SpriteId].frameTime;
		for (u32 i = 0; i < icon.SpriteBank->getSprites()[icon.SpriteId].Frames.size(); ++i) {
			u32 texId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].textureNumber;
			u32 rectId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].rectNumber;
			core::rect<s32> rectIcon = icon.SpriteBank->getPositions()[rectId];
			Cursor cursor = Device->TextureToCursor(icon.SpriteBank->getTexture(texId), rectIcon, icon.HotSpot);
			cX11.Frames.push_back(CursorFrameX11(cursor));
		}

		Cursors.push_back(cX11);

		return (gui::ECURSOR_ICON)(Cursors.size() - 1);
	}
#endif
	return gui::ECI_NORMAL;
}

//! replace the given cursor icon.
void CIrrDeviceLinux::CCursorControl::changeIcon(gui::ECURSOR_ICON iconId, const gui::SCursorSprite &icon)
{
#ifdef _IRR_COMPILE_WITH_X11_
	if (iconId >= (s32)Cursors.size())
		return;

	for (u32 i = 0; i < Cursors[iconId].Frames.size(); ++i)
		XFreeCursor(Device->XDisplay, Cursors[iconId].Frames[i].IconHW);

	if (icon.SpriteId >= 0) {
		CursorX11 cX11;
		cX11.FrameTime = icon.SpriteBank->getSprites()[icon.SpriteId].frameTime;
		for (u32 i = 0; i < icon.SpriteBank->getSprites()[icon.SpriteId].Frames.size(); ++i) {
			u32 texId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].textureNumber;
			u32 rectId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].rectNumber;
			core::rect<s32> rectIcon = icon.SpriteBank->getPositions()[rectId];
			Cursor cursor = Device->TextureToCursor(icon.SpriteBank->getTexture(texId), rectIcon, icon.HotSpot);
			cX11.Frames.push_back(CursorFrameX11(cursor));
		}

		Cursors[iconId] = cX11;
	}
#endif
}

core::dimension2di CIrrDeviceLinux::CCursorControl::getSupportedIconSize() const
{
	// this returns the closest match that is smaller or same size, so we just pass a value which should be large enough for cursors
	unsigned int width = 0, height = 0;
#ifdef _IRR_COMPILE_WITH_X11_
	XQueryBestCursor(Device->XDisplay, Device->XWindow, 64, 64, &width, &height);
#endif
	return core::dimension2di(width, height);
}

#endif // _IRR_COMPILE_WITH_X11_DEVICE_
