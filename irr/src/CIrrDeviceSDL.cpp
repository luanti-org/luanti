// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_

#include "CIrrDeviceSDL.h"
#include "IEventReceiver.h"
#include "IGUIElement.h"
#include "IGUIEnvironment.h"
#include "IImageLoader.h"
#include "IFileSystem.h"
#include "IVideoDriver.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include "COSOperator.h"
#include "SIrrCreationParameters.h"
#include <SDL_video.h>

#include <cstdio>
#include <cstdlib>
#include <cassert>

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
#include <emscripten.h>
#endif

#include "CSDLManager.h"

// Since SDL doesn't have mouse keys as keycodes we need to fall back to EKEY_CODE in some cases.
static inline bool is_fake_key(EKEY_CODE key) {
	switch (key) {
	case KEY_LBUTTON:
	case KEY_MBUTTON:
	case KEY_RBUTTON:
	case KEY_XBUTTON1:
	case KEY_XBUTTON2:
		return true;

	default:
		return false;
	}
}

static int SDLDeviceInstances = 0;

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
EM_BOOL CIrrDeviceSDL::MouseUpDownCallback(int eventType, const EmscriptenMouseEvent *event, void *userData)
{
	// We need this callback so far only because otherwise "emscripten_request_pointerlock" calls will
	// fail as their request are infinitely deferred.
	// Not exactly certain why, maybe SDL does catch those mouse-events otherwise and not pass them on.
	return EM_FALSE;
}

EM_BOOL CIrrDeviceSDL::MouseEnterCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
	CIrrDeviceSDL *This = static_cast<CIrrDeviceSDL *>(userData);

	SEvent irrevent;

	irrevent.EventType = EET_MOUSE_INPUT_EVENT;
	irrevent.MouseInput.Event = EMIE_MOUSE_ENTER_CANVAS;
	This->MouseX = irrevent.MouseInput.X = mouseEvent->canvasX;
	This->MouseY = irrevent.MouseInput.Y = mouseEvent->canvasY;
	This->MouseXRel = mouseEvent->movementX; // should be 0 I guess? Or can it enter while pointer is locked()?
	This->MouseYRel = mouseEvent->movementY;
	irrevent.MouseInput.ButtonStates = This->MouseButtonStates; // TODO: not correct, but couldn't figure out the bitset of mouseEvent->buttons yet.
	irrevent.MouseInput.Shift = mouseEvent->shiftKey;
	irrevent.MouseInput.Control = mouseEvent->ctrlKey;

	This->postEventFromUser(irrevent);

	return EM_FALSE;
}

EM_BOOL CIrrDeviceSDL::MouseLeaveCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
	CIrrDeviceSDL *This = static_cast<CIrrDeviceSDL *>(userData);

	SEvent irrevent;

	irrevent.EventType = EET_MOUSE_INPUT_EVENT;
	irrevent.MouseInput.Event = EMIE_MOUSE_LEAVE_CANVAS;
	This->MouseX = irrevent.MouseInput.X = mouseEvent->canvasX;
	This->MouseY = irrevent.MouseInput.Y = mouseEvent->canvasY;
	This->MouseXRel = mouseEvent->movementX; // should be 0 I guess? Or can it enter while pointer is locked()?
	This->MouseYRel = mouseEvent->movementY;
	irrevent.MouseInput.ButtonStates = This->MouseButtonStates; // TODO: not correct, but couldn't figure out the bitset of mouseEvent->buttons yet.
	irrevent.MouseInput.Shift = mouseEvent->shiftKey;
	irrevent.MouseInput.Control = mouseEvent->ctrlKey;

	This->postEventFromUser(irrevent);

	return EM_FALSE;
}
#endif

bool CIrrDeviceSDL::keyIsKnownSpecial(EKEY_CODE irrlichtKey)
{
	switch (irrlichtKey) {
	// keys which are known to have safe special character interpretation
	// could need changes over time (removals and additions!)
	case KEY_RETURN:
	case KEY_PAUSE:
	case KEY_ESCAPE:
	case KEY_PRIOR:
	case KEY_NEXT:
	case KEY_HOME:
	case KEY_END:
	case KEY_LEFT:
	case KEY_UP:
	case KEY_RIGHT:
	case KEY_DOWN:
	case KEY_TAB:
	case KEY_PRINT:
	case KEY_SNAPSHOT:
	case KEY_INSERT:
	case KEY_BACK:
	case KEY_DELETE:
	case KEY_HELP:
	case KEY_APPS:
	case KEY_SLEEP:
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11:
	case KEY_F12:
	case KEY_F13:
	case KEY_F14:
	case KEY_F15:
	case KEY_F16:
	case KEY_F17:
	case KEY_F18:
	case KEY_F19:
	case KEY_F20:
	case KEY_F21:
	case KEY_F22:
	case KEY_F23:
	case KEY_F24:
	case KEY_NUMLOCK:
	case KEY_SCROLL:
	case KEY_LCONTROL:
	case KEY_RCONTROL:
		return true;

	default:
		return false;
	}
}

int CIrrDeviceSDL::findCharToPassToIrrlicht(uint32_t sdlKey, EKEY_CODE irrlichtKey, bool numlock)
{
	switch (irrlichtKey) {
	// special cases that always return a char regardless of how the SDL keycode
	// looks
	case KEY_RETURN:
	case KEY_ESCAPE:
		return (int)irrlichtKey;

	// This is necessary for keys on the numpad because they don't use the same
	// keycodes as their non-numpad versions (whose keycodes correspond to chars),
	// but have their own SDL keycodes and their own Irrlicht keycodes (which
	// don't correspond to chars).
	case KEY_MULTIPLY:
		return '*';
	case KEY_ADD:
		return '+';
	case KEY_SUBTRACT:
		return '-';
	case KEY_DIVIDE:
		return '/';

	default:
		break;
	}

	if (numlock) {
		// Number keys on the numpad are also affected, but we only want them
		// to produce number chars when numlock is enabled.
		switch (irrlichtKey) {
		case KEY_NUMPAD0:
			return '0';
		case KEY_NUMPAD1:
			return '1';
		case KEY_NUMPAD2:
			return '2';
		case KEY_NUMPAD3:
			return '3';
		case KEY_NUMPAD4:
			return '4';
		case KEY_NUMPAD5:
			return '5';
		case KEY_NUMPAD6:
			return '6';
		case KEY_NUMPAD7:
			return '7';
		case KEY_NUMPAD8:
			return '8';
		case KEY_NUMPAD9:
			return '9';
		default:
			break;
		}
	}

	// SDL in-place ORs values with no character representation with 1<<30
	// https://wiki.libsdl.org/SDL2/SDLKeycodeLookup
	// This also affects the numpad keys btw.
	if (sdlKey & (1 << 30))
		return 0;

	switch (irrlichtKey) {
	case KEY_PRIOR:
	case KEY_NEXT:
	case KEY_HOME:
	case KEY_END:
	case KEY_LEFT:
	case KEY_UP:
	case KEY_RIGHT:
	case KEY_DOWN:
	case KEY_NUMLOCK:
		return 0;
	default:
		return sdlKey;
	}
}

std::variant<u32, EKEY_CODE> CIrrDeviceSDL::getScancodeFromKey(const Keycode &key) const
{
	u32 keynum = 0;
	if (const auto *keycode = std::get_if<EKEY_CODE>(&key)) {
		// Fake keys (e.g. mouse buttons): use EKEY_CODE since there is no corresponding scancode.
		if (is_fake_key(*keycode))
			return *keycode;
		// Try to convert the EKEY_CODE to a SDL scancode.
		for (const auto &entry: KeyMap) {
			if (entry.second == *keycode) {
				keynum = entry.first;
				break;
			}
		}
	} else {
		keynum = std::get<wchar_t>(key);
	}
	return (u32)SDL_GetScancodeFromKey(keynum);
}

Keycode CIrrDeviceSDL::getKeyFromScancode(const u32 scancode) const
{
	auto keycode = SDL_GetKeyFromScancode((SDL_Scancode)scancode);
	const auto &keyentry = KeyMap.find(keycode);
	auto irrcode = keyentry != KeyMap.end() ? keyentry->second : KEY_UNKNOWN;
	auto keychar = findCharToPassToIrrlicht(keycode, irrcode, false);
	return Keycode(irrcode, keychar);
}

void CIrrDeviceSDL::resetReceiveTextInputEvents()
{
	gui::IGUIElement *elem = GUIEnvironment->getFocus();
	if (elem && elem->acceptsIME()) {
		// IBus seems to have an issue where dead keys and compose keys do not
		// work (specifically, the individual characters in the sequence are
		// sent as text input events instead of the result) when
		// SDL_StartTextInput() is called on the same input box.
		core::rect<s32> pos = elem->getAbsolutePosition();
		if (!SDL_IsTextInputActive() || lastElemPos != pos) {
			lastElemPos = pos;
			SDL_Rect rect;
			rect.x = pos.UpperLeftCorner.X;
			rect.y = pos.UpperLeftCorner.Y;
			rect.w = pos.getWidth();
			rect.h = pos.getHeight();
			SDL_SetTextInputRect(&rect);
			SDL_StartTextInput();
		}
	} else {
		SDL_StopTextInput();
	}
}

//! constructor
CIrrDeviceSDL::CIrrDeviceSDL(const SIrrlichtCreationParameters &param) :
		CIrrDeviceStub(param),
		Window((SDL_Window *)param.WindowId),
		MouseX(0), MouseY(0), MouseXRel(0), MouseYRel(0), MouseButtonStates(0),
		Width(param.WindowSize.Width), Height(param.WindowSize.Height),
		Resizable(param.WindowResizable == 1 ? true : false), CurrentTouchCount(0),
		IsInBackground(false)
{
	if (++SDLDeviceInstances == 1) {
#ifdef __ANDROID__
		// Blocking on pause causes problems with multiplayer.
		// see <https://github.com/luanti-org/luanti/issues/10842>
		SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
		SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO, "0");

		SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");

		// Minetest does its own screen keyboard handling.
		SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
#endif

		// Minetest has its own signal handler
		SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

		// Disabling the compositor is not a good idea in windowed mode.
		// see <https://github.com/luanti-org/luanti/issues/14596>
		SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
		// These are not interesting for our use
		SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
		SDL_SetHint(SDL_HINT_TV_REMOTE_AS_JOYSTICK, "0");
#endif

#if SDL_VERSION_ATLEAST(2, 24, 0)
		// highdpi support on Windows
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif

		// Minetest has its own code to synthesize mouse events from touch events,
		// so we prevent SDL from doing it.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
		SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

#if defined(SDL_HINT_APP_NAME)
		SDL_SetHint(SDL_HINT_APP_NAME, "Luanti");
#endif

		// Set IME hints
		SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");
#if defined(SDL_HINT_IME_SHOW_UI)
		SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

		u32 flags = SDL_INIT_TIMER | SDL_INIT_EVENTS;
		if (CreationParams.DriverType != video::EDT_NULL)
			flags |= SDL_INIT_VIDEO;
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
		flags |= SDL_INIT_JOYSTICK;
#endif
		if (SDL_Init(flags) < 0) {
			os::Printer::log("Unable to initialize SDL", SDL_GetError(), ELL_ERROR);
			Close = true;
		}
	}

	// create keymap
	createKeyMap();

	// create window
	if (CreationParams.DriverType != video::EDT_NULL) {
		if (!createWindow()) {
			Close = true;
			return;
		}
	}

	core::stringc sdlver = "SDL ";
	{
		SDL_version v{};
		SDL_GetVersion(&v);
		sdlver += v.major;
		sdlver += ".";
		sdlver += v.minor;
		sdlver += ".";
		sdlver += v.patch;
		// the SDL team seems to intentionally number sdl2-compat this way:
		// <https://github.com/libsdl-org/sdl2-compat/tags>
		if (v.patch >= 50)
			sdlver += " (compat)";

		sdlver += " on ";
		sdlver += SDL_GetPlatform();
	}

	Operator = new COSOperator(sdlver);
	if (SDLDeviceInstances == 1) {
		os::Printer::log(sdlver.c_str(), ELL_INFORMATION);
	}

	// create cursor control
	CursorControl = new CCursorControl(this);

	// create driver
	createDriver();

	if (VideoDriver) {
		createGUIAndScene();
		VideoDriver->OnResize(core::dimension2d<u32>(Width, Height));
	}
}

//! destructor
CIrrDeviceSDL::~CIrrDeviceSDL()
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	const u32 numJoysticks = Joysticks.size();
	for (u32 i = 0; i < numJoysticks; ++i)
		SDL_JoystickClose(Joysticks[i]);
#endif
	if (Window && Context) {
		SDL_GL_MakeCurrent(Window, NULL);
		SDL_GL_DeleteContext(Context);
	}
	if (Window) {
		SDL_DestroyWindow(Window);
	}

	if (--SDLDeviceInstances == 0) {
		SDL_Quit();
		os::Printer::log("Quit SDL", ELL_INFORMATION);
	}
}

void CIrrDeviceSDL::logAttributes()
{
	core::stringc sdl_attr("SDL attribs:");
	int value = 0;
	if (SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value) == 0)
		sdl_attr += core::stringc(" r:") + core::stringc(value);
	if (SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &value) == 0)
		sdl_attr += core::stringc(" g:") + core::stringc(value);
	if (SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value) == 0)
		sdl_attr += core::stringc(" b:") + core::stringc(value);
	if (SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &value) == 0)
		sdl_attr += core::stringc(" a:") + core::stringc(value);

	if (SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value) == 0)
		sdl_attr += core::stringc(" depth:") + core::stringc(value);
	if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &value) == 0)
		sdl_attr += core::stringc(" stencil:") + core::stringc(value);
	if (SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &value) == 0)
		sdl_attr += core::stringc(" doublebuf:") + core::stringc(value);
	if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLEBUFFERS, &value) == 0)
		sdl_attr += core::stringc(" aa:") + core::stringc(value);
	if (SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &value) == 0)
		sdl_attr += core::stringc(" aa-samples:") + core::stringc(value);

	os::Printer::log(sdl_attr.c_str());
}

bool CIrrDeviceSDL::createWindow()
{
	if (Close)
		return false;

	if (createWindowWithContext())
		return true;

	if (CreationParams.DriverDebug) {
		CreationParams.DriverDebug = false;
		if (createWindowWithContext()) {
			os::Printer::log("DriverDebug reduced due to lack of support!");
			// Turn it back on because the GL driver can maybe still do something useful.
			CreationParams.DriverDebug = true;
			return true;
		}
	}

	while (CreationParams.AntiAlias > 0) {
		CreationParams.AntiAlias--;
		if (createWindowWithContext()) {
			os::Printer::log("AntiAlias reduced/disabled due to lack of support!");
			return true;
		}
	}

	if (CreationParams.WithAlphaChannel) {
		CreationParams.WithAlphaChannel = false;
		if (createWindowWithContext()) {
			os::Printer::log("WithAlphaChannel disabled due to lack of support!");
			return true;
		}
	}

	if (CreationParams.Stencilbuffer) {
		CreationParams.Stencilbuffer = false;
		if (createWindowWithContext()) {
			os::Printer::log("Stencilbuffer disabled due to lack of support!");
			return true;
		}
	}

	while (CreationParams.ZBufferBits > 16) {
		CreationParams.ZBufferBits -= 8;
		if (createWindowWithContext()) {
			os::Printer::log("ZBufferBits reduced due to lack of support!");
			return true;
		}
	}

	while (CreationParams.Bits > 16) {
		CreationParams.Bits -= 8;
		if (createWindowWithContext()) {
			os::Printer::log("Bits reduced due to lack of support!");
			return true;
		}
	}

	if (CreationParams.Stereobuffer) {
		CreationParams.Stereobuffer = false;
		if (createWindowWithContext()) {
			os::Printer::log("Stereobuffer disabled due to lack of support!");
			return true;
		}
	}

	if (CreationParams.Doublebuffer) {
		// Try single buffer
		CreationParams.Doublebuffer = false;
		if (createWindowWithContext()) {
			os::Printer::log("Doublebuffer disabled due to lack of support!");
			return true;
		}
	}

	os::Printer::log("Could not create window and context!", ELL_ERROR);
	return false;
}

bool CIrrDeviceSDL::createWindowWithContext()
{
	u32 SDL_Flags = 0;
	SDL_Flags |= SDL_WINDOW_ALLOW_HIGHDPI;

	SDL_Flags |= getFullscreenFlag(CreationParams.Fullscreen);
	if (Resizable)
		SDL_Flags |= SDL_WINDOW_RESIZABLE;
	if (CreationParams.WindowMaximized)
		SDL_Flags |= SDL_WINDOW_MAXIMIZED;
	SDL_Flags |= SDL_WINDOW_OPENGL;

	SDL_GL_ResetAttributes();

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	if (Width != 0 || Height != 0)
		emscripten_set_canvas_size(Width, Height);
	else {
		int w, h, fs;
		emscripten_get_canvas_size(&w, &h, &fs);
		Width = w;
		Height = h;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, CreationParams.WithAlphaChannel ? 8 : 0);

	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, CreationParams.ZBufferBits);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, CreationParams.Stencilbuffer ? 8 : 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, CreationParams.Doublebuffer ? 1 : 0);

	if (CreationParams.AntiAlias > 1) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, CreationParams.AntiAlias);
	} else {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	SDL_CreateWindowAndRenderer(0, 0, SDL_Flags, &Window, &Renderer); // 0,0 will use the canvas size

	logAttributes();

	// "#canvas" is for the opengl context
	emscripten_set_mousedown_callback("#canvas", (void *)this, true, MouseUpDownCallback);
	emscripten_set_mouseup_callback("#canvas", (void *)this, true, MouseUpDownCallback);
	emscripten_set_mouseenter_callback("#canvas", (void *)this, false, MouseEnterCallback);
	emscripten_set_mouseleave_callback("#canvas", (void *)this, false, MouseLeaveCallback);

	return true;
#else // !_IRR_EMSCRIPTEN_PLATFORM_
	switch (CreationParams.DriverType) {
	case video::EDT_OPENGL:
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		break;
	case video::EDT_OPENGL3:
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		break;
	case video::EDT_OGLES2:
	case video::EDT_WEBGL1:
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		break;
	default:
		assert(false);
	}

	if (CreationParams.DriverDebug) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG | SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG);
	}

	if (CreationParams.Bits == 16) {
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, CreationParams.WithAlphaChannel ? 1 : 0);
	} else {
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, CreationParams.WithAlphaChannel ? 8 : 0);
	}
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, CreationParams.ZBufferBits);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, CreationParams.Doublebuffer);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, CreationParams.Stencilbuffer ? 8 : 0);
	SDL_GL_SetAttribute(SDL_GL_STEREO, CreationParams.Stereobuffer);
	if (CreationParams.AntiAlias > 1) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, CreationParams.AntiAlias);
	} else {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	}

	Window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Width, Height, SDL_Flags);
	if (!Window) {
		os::Printer::log("Could not create window", SDL_GetError(), ELL_WARNING);
		return false;
	}

	Context = SDL_GL_CreateContext(Window);
	if (!Context) {
		os::Printer::log("Could not create context", SDL_GetError(), ELL_WARNING);
		SDL_DestroyWindow(Window);
		Window = nullptr;
		return false;
	}

	updateSizeAndScale();
	if (ScaleX != 1.0f || ScaleY != 1.0f) {
		// The given window size is in pixels, not in screen coordinates.
		// We can only do the conversion now since we didn't know the scale before.
		SDL_SetWindowSize(Window,
				static_cast<int>(CreationParams.WindowSize.Width / ScaleX),
				static_cast<int>(CreationParams.WindowSize.Height / ScaleY));
		// Re-center, otherwise large, non-maximized windows go offscreen.
		SDL_SetWindowPosition(Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		updateSizeAndScale();
	}

	return true;
#endif // !_IRR_EMSCRIPTEN_PLATFORM_
}

//! create the driver
void CIrrDeviceSDL::createDriver()
{
	if (CreationParams.DriverType == video::EDT_NULL) {
		VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
		return;
	}

	ContextManager = new video::CSDLManager(this);
	switch (CreationParams.DriverType) {
	case video::EDT_OPENGL:
		VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, ContextManager);
		break;
	case video::EDT_OPENGL3:
		VideoDriver = video::createOpenGL3Driver(CreationParams, FileSystem, ContextManager);
		break;
	case video::EDT_OGLES2:
		VideoDriver = video::createOGLES2Driver(CreationParams, FileSystem, ContextManager);
		break;
	case video::EDT_WEBGL1:
		VideoDriver = video::createWebGL1Driver(CreationParams, FileSystem, ContextManager);
		break;
	default:;
	}
	if (!VideoDriver)
		os::Printer::log("Could not create video driver", ELL_ERROR);
}

static int wrap_PollEvent(SDL_Event *ev)
{
	u32 t0 = os::Timer::getRealTime();
	int ret = SDL_PollEvent(ev);
	u32 d = os::Timer::getRealTime() - t0;
	if (d >= 5) {
		auto msg = std::string("SDL_PollEvent took too long: ") + std::to_string(d) + "ms";
		// 50ms delay => more than three missed frames (at 60fps)
		os::Printer::log(msg.c_str(), d >= 50 ? ELL_WARNING : ELL_INFORMATION);
	}
	return ret;
}

//! runs the device. Returns false if device wants to be deleted
bool CIrrDeviceSDL::run()
{
	os::Timer::tick();

	SEvent irrevent;
	SDL_Event SDL_event;

	while (!Close && wrap_PollEvent(&SDL_event)) {
		// os::Printer::log("event: ", core::stringc((int)SDL_event.type).c_str(),   ELL_INFORMATION);	// just for debugging
		irrevent = {};

		switch (SDL_event.type) {
		case SDL_MOUSEMOTION: {
			SDL_Keymod keymod = SDL_GetModState();

			irrevent.EventType = EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = EMIE_MOUSE_MOVED;

			MouseXRel = static_cast<s32>(SDL_event.motion.xrel * ScaleX);
			MouseYRel = static_cast<s32>(SDL_event.motion.yrel * ScaleY);
			if (!SDL_GetRelativeMouseMode()) {
				MouseX = static_cast<s32>(SDL_event.motion.x * ScaleX);
				MouseY = static_cast<s32>(SDL_event.motion.y * ScaleY);
			} else {
				MouseX += MouseXRel;
				MouseY += MouseYRel;
			}
			irrevent.MouseInput.X = MouseX;
			irrevent.MouseInput.Y = MouseY;

			irrevent.MouseInput.ButtonStates = MouseButtonStates;
			irrevent.MouseInput.Shift = (keymod & KMOD_SHIFT) != 0;
			irrevent.MouseInput.Control = (keymod & KMOD_CTRL) != 0;

			postEventFromUser(irrevent);
			break;
		}
		case SDL_MOUSEWHEEL: {
			SDL_Keymod keymod = SDL_GetModState();

			irrevent.EventType = EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = EMIE_MOUSE_WHEEL;
#if SDL_VERSION_ATLEAST(2, 0, 18)
			irrevent.MouseInput.Wheel = SDL_event.wheel.preciseY;
#else
			irrevent.MouseInput.Wheel = SDL_event.wheel.y;
#endif
			irrevent.MouseInput.ButtonStates = MouseButtonStates;
			irrevent.MouseInput.Shift = (keymod & KMOD_SHIFT) != 0;
			irrevent.MouseInput.Control = (keymod & KMOD_CTRL) != 0;
			irrevent.MouseInput.X = MouseX;
			irrevent.MouseInput.Y = MouseY;

			// wheel y can be 0 if scrolling sideways
			if (irrevent.MouseInput.Wheel == 0.0f)
				break;

			postEventFromUser(irrevent);
			break;
		}
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: {
			SDL_Keymod keymod = SDL_GetModState();

			irrevent.EventType = EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = EMIE_MOUSE_MOVED; // value to be ignored

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
			// Handle mouselocking in emscripten in Windowed mode.
			// In fullscreen SDL will handle it.
			// The behavior we want windowed is - when the canvas was clicked then
			// we will lock the mouse-pointer if it should be invisible.
			// For security reasons this will be delayed until the next mouse-up event.
			// We do not pass on this event as we don't want the activation click to do anything.
			if (SDL_event.type == SDL_MOUSEBUTTONDOWN && !isFullscreen()) {
				EmscriptenPointerlockChangeEvent pointerlockStatus; // let's hope that test is not expensive ...
				if (emscripten_get_pointerlock_status(&pointerlockStatus) == EMSCRIPTEN_RESULT_SUCCESS) {
					if (CursorControl->isVisible() && pointerlockStatus.isActive) {
						emscripten_exit_pointerlock();
						return !Close;
					} else if (!CursorControl->isVisible() && !pointerlockStatus.isActive) {
						emscripten_request_pointerlock(0, true);
						return !Close;
					}
				}
			}
#endif

			auto button = SDL_event.button.button;
#ifdef __ANDROID__
			// Android likes to send the right mouse button as the back button.
			// According to some web searches I did, this is probably
			// vendor/device-specific.
			// Since a working right mouse button is very important for
			// Minetest, we have this little hack.
			if (button == SDL_BUTTON_X2)
				button = SDL_BUTTON_RIGHT;
#endif
			switch (button) {
			case SDL_BUTTON_LEFT:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN) {
					irrevent.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;
					MouseButtonStates |= EMBSM_LEFT;
				} else {
					irrevent.MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
					MouseButtonStates &= ~EMBSM_LEFT;
				}
				break;

			case SDL_BUTTON_RIGHT:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN) {
					irrevent.MouseInput.Event = EMIE_RMOUSE_PRESSED_DOWN;
					MouseButtonStates |= EMBSM_RIGHT;
				} else {
					irrevent.MouseInput.Event = EMIE_RMOUSE_LEFT_UP;
					MouseButtonStates &= ~EMBSM_RIGHT;
				}
				break;

			case SDL_BUTTON_MIDDLE:
				if (SDL_event.type == SDL_MOUSEBUTTONDOWN) {
					irrevent.MouseInput.Event = EMIE_MMOUSE_PRESSED_DOWN;
					MouseButtonStates |= EMBSM_MIDDLE;
				} else {
					irrevent.MouseInput.Event = EMIE_MMOUSE_LEFT_UP;
					MouseButtonStates &= ~EMBSM_MIDDLE;
				}
				break;

			// Since Irrlicht does not have event types for X1/X2 buttons, we simply pass
			// those as keycodes instead. This is relatively hacky but avoids the effort of
			// adding more mouse events that will be discarded anyway once we switch to SDL
			case SDL_BUTTON_X1:
				irrevent.EventType = EET_KEY_INPUT_EVENT;
				irrevent.KeyInput.Key = KEY_XBUTTON1;
				break;

			case SDL_BUTTON_X2:
				irrevent.EventType = EET_KEY_INPUT_EVENT;
				irrevent.KeyInput.Key = KEY_XBUTTON2;
				break;
			}

			bool shift = (keymod & KMOD_SHIFT) != 0;
			bool control = (keymod & KMOD_CTRL) != 0;
			if (irrevent.EventType == EET_MOUSE_INPUT_EVENT && irrevent.MouseInput.Event != EMIE_MOUSE_MOVED) {
				irrevent.MouseInput.ButtonStates = MouseButtonStates;
				irrevent.MouseInput.X = static_cast<s32>(SDL_event.button.x * ScaleX);
				irrevent.MouseInput.Y = static_cast<s32>(SDL_event.button.y * ScaleY);
				irrevent.MouseInput.Shift = shift;
				irrevent.MouseInput.Control = control;
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
			} else if (irrevent.EventType == EET_KEY_INPUT_EVENT) {
				irrevent.KeyInput.Char = 0;
				irrevent.KeyInput.PressedDown = SDL_event.type == SDL_MOUSEBUTTONDOWN;
				irrevent.KeyInput.Shift = shift;
				irrevent.KeyInput.Control = control;
				postEventFromUser(irrevent);
			}
			break;
		}

		case SDL_TEXTINPUT: {
			irrevent.EventType = EET_STRING_INPUT_EVENT;
			irrevent.StringInput.Str = new core::stringw();
			core::utf8ToWString(*irrevent.StringInput.Str, SDL_event.text.text);
			postEventFromUser(irrevent);
			delete irrevent.StringInput.Str;
			irrevent.StringInput.Str = NULL;
		} break;

		case SDL_KEYDOWN:
		case SDL_KEYUP: {
			auto keysym = SDL_event.key.keysym.sym;
			auto scancode = SDL_event.key.keysym.scancode;

			// Treat AC_BACK as the Escape key
			if (scancode == SDL_SCANCODE_AC_BACK) {
				scancode = SDL_SCANCODE_ESCAPE;
				keysym = SDLK_ESCAPE;
			}

			const auto &entry = KeyMap.find(keysym);
			auto key = entry == KeyMap.end() ? KEY_UNKNOWN : entry->second;

			if (!Keycode::isValid(key))
				os::Printer::log("keycode not mapped", core::stringc(keysym), ELL_DEBUG);

			// Make sure to only input special characters if something is in focus, as SDL_TEXTINPUT handles normal unicode already
			if (SDL_IsTextInputActive() && !keyIsKnownSpecial(key) && (SDL_event.key.keysym.mod & KMOD_CTRL) == 0)
				break;

			irrevent.EventType = EET_KEY_INPUT_EVENT;
			irrevent.KeyInput.Key = key;
			irrevent.KeyInput.PressedDown = (SDL_event.type == SDL_KEYDOWN);
			irrevent.KeyInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;
			irrevent.KeyInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL) != 0;
			irrevent.KeyInput.Char = findCharToPassToIrrlicht(keysym, key,
					(SDL_event.key.keysym.mod & KMOD_NUM) != 0);
			irrevent.KeyInput.SystemKeyCode = scancode;

			postEventFromUser(irrevent);
		} break;

		case SDL_QUIT:
			Close = true;
			break;

		case SDL_WINDOWEVENT:
			switch (SDL_event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
			case SDL_WINDOWEVENT_SIZE_CHANGED:
#if SDL_VERSION_ATLEAST(2, 0, 18)
			case SDL_WINDOWEVENT_DISPLAY_CHANGED:
#endif
				u32 old_w = Width, old_h = Height;
				f32 old_scale_x = ScaleX, old_scale_y = ScaleY;
				updateSizeAndScale();
				if (old_w != Width || old_h != Height) {
					if (VideoDriver)
						VideoDriver->OnResize(core::dimension2d<u32>(Width, Height));
				}
				if (old_scale_x != ScaleX || old_scale_y != ScaleY) {
					irrevent.EventType = EET_APPLICATION_EVENT;
					irrevent.ApplicationEvent.EventType = EAET_DPI_CHANGED;
					postEventFromUser(irrevent);
				}
				break;
			}
			break;

		case SDL_USEREVENT:
			irrevent.EventType = EET_USER_EVENT;
			irrevent.UserEvent.UserData1 = reinterpret_cast<uintptr_t>(SDL_event.user.data1);
			irrevent.UserEvent.UserData2 = reinterpret_cast<uintptr_t>(SDL_event.user.data2);

			postEventFromUser(irrevent);
			break;

		case SDL_FINGERDOWN:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_PRESSED_DOWN;
			irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
			irrevent.TouchInput.X = static_cast<s32>(SDL_event.tfinger.x * Width);
			irrevent.TouchInput.Y = static_cast<s32>(SDL_event.tfinger.y * Height);
			CurrentTouchCount++;
			irrevent.TouchInput.touchedCount = CurrentTouchCount;

			postEventFromUser(irrevent);
			break;

		case SDL_FINGERMOTION:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_MOVED;
			irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
			irrevent.TouchInput.X = static_cast<s32>(SDL_event.tfinger.x * Width);
			irrevent.TouchInput.Y = static_cast<s32>(SDL_event.tfinger.y * Height);
			irrevent.TouchInput.touchedCount = CurrentTouchCount;

			postEventFromUser(irrevent);
			break;

		case SDL_FINGERUP:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_LEFT_UP;
			irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
			irrevent.TouchInput.X = static_cast<s32>(SDL_event.tfinger.x * Width);
			irrevent.TouchInput.Y = static_cast<s32>(SDL_event.tfinger.y * Height);
			// To match Android behavior, still count the pointer that was
			// just released.
			irrevent.TouchInput.touchedCount = CurrentTouchCount;
			if (CurrentTouchCount > 0) {
				CurrentTouchCount--;
			}

			postEventFromUser(irrevent);
			break;

		// Contrary to what the SDL documentation says, SDL_APP_WILLENTERBACKGROUND
		// and SDL_APP_WILLENTERFOREGROUND are actually sent in onStop/onStart,
		// not onPause/onResume, on recent Android versions. This can be verified
		// by testing or by looking at the org.libsdl.app.SDLActivity Java code.
		// -> This means we can use them to implement isWindowVisible().

		case SDL_APP_WILLENTERBACKGROUND:
			IsInBackground = true;
			break;

		case SDL_APP_WILLENTERFOREGROUND:
			IsInBackground = false;
			break;

		case SDL_RENDER_TARGETS_RESET:
			os::Printer::log("Received SDL_RENDER_TARGETS_RESET. Rendering is probably broken.", ELL_ERROR);
			break;

		case SDL_RENDER_DEVICE_RESET:
			os::Printer::log("Received SDL_RENDER_DEVICE_RESET. Rendering is probably broken.", ELL_ERROR);
			break;

		default:
			break;
		} // end switch
		resetReceiveTextInputEvents();
	} // end while

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	// TODO: Check if the multiple open/close calls are too expensive, then
	// open/close in the constructor/destructor instead

	// update joystick states manually
	SDL_JoystickUpdate();
	// we'll always send joystick input events...
	SEvent joyevent;
	joyevent.EventType = EET_JOYSTICK_INPUT_EVENT;
	for (u32 i = 0; i < Joysticks.size(); ++i) {
		SDL_Joystick *joystick = Joysticks[i];
		if (joystick) {
			int j;
			// query all buttons
			const int numButtons = core::min_(SDL_JoystickNumButtons(joystick), 32);
			joyevent.JoystickEvent.ButtonStates = 0;
			for (j = 0; j < numButtons; ++j)
				joyevent.JoystickEvent.ButtonStates |= (SDL_JoystickGetButton(joystick, j) << j);

			// query all axes, already in correct range
			const int numAxes = core::min_(SDL_JoystickNumAxes(joystick), (int)SEvent::SJoystickEvent::NUMBER_OF_AXES);
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_X] = 0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Y] = 0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Z] = 0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_R] = 0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_U] = 0;
			joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_V] = 0;
			for (j = 0; j < numAxes; ++j)
				joyevent.JoystickEvent.Axis[j] = SDL_JoystickGetAxis(joystick, j);

			// we can only query one hat, SDL only supports 8 directions
			if (SDL_JoystickNumHats(joystick) > 0) {
				switch (SDL_JoystickGetHat(joystick, 0)) {
				case SDL_HAT_UP:
					joyevent.JoystickEvent.POV = 0;
					break;
				case SDL_HAT_RIGHTUP:
					joyevent.JoystickEvent.POV = 4500;
					break;
				case SDL_HAT_RIGHT:
					joyevent.JoystickEvent.POV = 9000;
					break;
				case SDL_HAT_RIGHTDOWN:
					joyevent.JoystickEvent.POV = 13500;
					break;
				case SDL_HAT_DOWN:
					joyevent.JoystickEvent.POV = 18000;
					break;
				case SDL_HAT_LEFTDOWN:
					joyevent.JoystickEvent.POV = 22500;
					break;
				case SDL_HAT_LEFT:
					joyevent.JoystickEvent.POV = 27000;
					break;
				case SDL_HAT_LEFTUP:
					joyevent.JoystickEvent.POV = 31500;
					break;
				case SDL_HAT_CENTERED:
				default:
					joyevent.JoystickEvent.POV = 65535;
					break;
				}
			} else {
				joyevent.JoystickEvent.POV = 65535;
			}

			// we map the number directly
			joyevent.JoystickEvent.Joystick = static_cast<u8>(i);
			// now post the event
			postEventFromUser(joyevent);
			// and close the joystick
		}
	}
#endif
	return !Close;
}

//! Activate any joysticks, and generate events for them.
bool CIrrDeviceSDL::activateJoysticks(core::array<SJoystickInfo> &joystickInfo)
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	joystickInfo.clear();

	// we can name up to 256 different joysticks
	const int numJoysticks = core::min_(SDL_NumJoysticks(), 256);
	Joysticks.reallocate(numJoysticks);
	joystickInfo.reallocate(numJoysticks);

	int joystick = 0;
	for (; joystick < numJoysticks; ++joystick) {
		Joysticks.push_back(SDL_JoystickOpen(joystick));
		SJoystickInfo info;

		info.Joystick = joystick;
		info.Axes = SDL_JoystickNumAxes(Joysticks[joystick]);
		info.Buttons = SDL_JoystickNumButtons(Joysticks[joystick]);
		info.Name = SDL_JoystickName(Joysticks[joystick]);
		info.PovHat = (SDL_JoystickNumHats(Joysticks[joystick]) > 0)
							  ? SJoystickInfo::POV_HAT_PRESENT
							  : SJoystickInfo::POV_HAT_ABSENT;

		joystickInfo.push_back(info);
	}

	for (joystick = 0; joystick < (int)joystickInfo.size(); ++joystick) {
		char logString[256];
		snprintf_irr(logString, sizeof(logString), "Found joystick %d, %d axes, %d buttons '%s'",
				joystick, joystickInfo[joystick].Axes,
				joystickInfo[joystick].Buttons, joystickInfo[joystick].Name.c_str());
		os::Printer::log(logString, ELL_INFORMATION);
	}

	return true;

#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

	return false;
}

void CIrrDeviceSDL::updateSizeAndScale()
{
	int window_w, window_h;
	SDL_GetWindowSize(Window, &window_w, &window_h);

	int drawable_w, drawable_h;
	SDL_GL_GetDrawableSize(Window, &drawable_w, &drawable_h);

	ScaleX = (float)drawable_w / (float)window_w;
	ScaleY = (float)drawable_h / (float)window_h;

	Width = drawable_w;
	Height = drawable_h;
}

//! Get the display density in dots per inch.
float CIrrDeviceSDL::getDisplayDensity() const
{
	// assume 96 dpi
	return std::max(ScaleX * 96.0f, ScaleY * 96.0f);
}

void CIrrDeviceSDL::SwapWindow()
{
	SDL_GL_SwapWindow(Window);
}

//! pause execution temporarily
void CIrrDeviceSDL::yield()
{
	SDL_Delay(0);
}

//! pause execution for a specified time
void CIrrDeviceSDL::sleep(u32 timeMs, bool pauseTimer)
{
	const bool wasStopped = Timer ? Timer->isStopped() : true;
	if (pauseTimer && !wasStopped)
		Timer->stop();

	SDL_Delay(timeMs);

	if (pauseTimer && !wasStopped)
		Timer->start();
}

//! sets the caption of the window
void CIrrDeviceSDL::setWindowCaption(const wchar_t *text)
{
	core::stringc textc;
	core::wStringToUTF8(textc, text);
	SDL_SetWindowTitle(Window, textc.c_str());
}

//! Sets the window icon.
bool CIrrDeviceSDL::setWindowIcon(const video::IImage *img)
{
	if (!Window)
		return false;

	u32 height = img->getDimension().Height;
	u32 width = img->getDimension().Width;

	SDL_Surface *surface = SDL_CreateRGBSurface(0, width, height, 32,
			0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

	if (!surface) {
		os::Printer::log("Failed to create SDL suface", ELL_ERROR);
		return false;
	}

	SDL_LockSurface(surface);
	bool succ = img->copyToNoScaling(surface->pixels, width, height, video::ECF_A8R8G8B8, surface->pitch);
	SDL_UnlockSurface(surface);

	if (!succ) {
		os::Printer::log("Could not copy icon image. Is the format not ECF_A8R8G8B8?", ELL_ERROR);
		SDL_FreeSurface(surface);
		return false;
	}

	SDL_SetWindowIcon(Window, surface);

	SDL_FreeSurface(surface);

	return true;
}

//! notifies the device that it should close itself
void CIrrDeviceSDL::closeDevice()
{
	Close = true;
}

//! Sets if the window should be resizable in windowed mode.
void CIrrDeviceSDL::setResizable(bool resize)
{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	os::Printer::log("Resizable not available on the web.", ELL_WARNING);
	return;
#else  // !_IRR_EMSCRIPTEN_PLATFORM_
	if (resize != Resizable) {
		if (Window) {
			SDL_SetWindowResizable(Window, (SDL_bool)resize);
		}
		Resizable = resize;
	}
#endif // !_IRR_EMSCRIPTEN_PLATFORM_
}

//! Minimizes window if possible
void CIrrDeviceSDL::minimizeWindow()
{
	if (Window)
		SDL_MinimizeWindow(Window);
}

//! Maximize window
void CIrrDeviceSDL::maximizeWindow()
{
	if (Window)
		SDL_MaximizeWindow(Window);
}

//! Get the position of this window on screen
core::position2di CIrrDeviceSDL::getWindowPosition()
{
	return core::position2di(-1, -1);
}

//! Restore original window size
void CIrrDeviceSDL::restoreWindow()
{
	if (Window)
		SDL_RestoreWindow(Window);
}

bool CIrrDeviceSDL::isWindowMaximized() const
{
	return Window && (SDL_GetWindowFlags(Window) & SDL_WINDOW_MAXIMIZED) != 0;
}

bool CIrrDeviceSDL::isFullscreen() const
{
	if (!Window)
		return false;
	u32 flags = SDL_GetWindowFlags(Window);
	return (flags & SDL_WINDOW_FULLSCREEN) != 0 ||
			(flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

u32 CIrrDeviceSDL::getFullscreenFlag(bool fullscreen)
{
	if (!fullscreen)
		return 0;
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	return SDL_WINDOW_FULLSCREEN;
#else
	return SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
}

bool CIrrDeviceSDL::setFullscreen(bool fullscreen)
{
	if (!Window)
		return false;
	// The SDL wiki says that this may trigger SDL_RENDER_TARGETS_RESET, but
	// looking at the SDL source, this only happens with D3D, so it's not
	// relevant to us.
	bool success = SDL_SetWindowFullscreen(Window, getFullscreenFlag(fullscreen)) == 0;
	if (!success)
		os::Printer::log("SDL_SetWindowFullscreen failed", SDL_GetError(), ELL_ERROR);
	return success;
}

bool CIrrDeviceSDL::isWindowVisible() const
{
	return !IsInBackground;
}

//! Checks if the Irrlicht device supports touch events.
bool CIrrDeviceSDL::supportsTouchEvents() const
{
	return true;
}

//! returns if window is active. if not, nothing need to be drawn
bool CIrrDeviceSDL::isWindowActive() const
{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	// Hidden test only does something in some browsers (when tab in background or window is minimized)
	// In other browsers code automatically doesn't seem to be called anymore.
	EmscriptenVisibilityChangeEvent emVisibility;
	if (emscripten_get_visibility_status(&emVisibility) == EMSCRIPTEN_RESULT_SUCCESS) {
		if (emVisibility.hidden)
			return false;
	}
#endif
	const u32 windowFlags = SDL_GetWindowFlags(Window);
	return windowFlags & SDL_WINDOW_SHOWN && windowFlags & SDL_WINDOW_INPUT_FOCUS;
}

//! returns if window has focus.
bool CIrrDeviceSDL::isWindowFocused() const
{
	return Window && (SDL_GetWindowFlags(Window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

//! returns if window is minimized.
bool CIrrDeviceSDL::isWindowMinimized() const
{
	return Window && (SDL_GetWindowFlags(Window) & SDL_WINDOW_MINIMIZED) != 0;
}

//! returns color format of the window.
video::ECOLOR_FORMAT CIrrDeviceSDL::getColorFormat() const
{
	if (Window) {
		SDL_Surface *surface = SDL_GetWindowSurface(Window);
		if (surface->format->BitsPerPixel == 16) {
			if (surface->format->Amask != 0)
				return video::ECF_A1R5G5B5;
			else
				return video::ECF_R5G6B5;
		} else {
			if (surface->format->Amask != 0)
				return video::ECF_A8R8G8B8;
			else
				return video::ECF_R8G8B8;
		}
	} else
		return CIrrDeviceStub::getColorFormat();
}

void CIrrDeviceSDL::createKeyMap()
{
	// I don't know if this is the best method  to create
	// the lookuptable, but I'll leave it like that until
	// I find a better version.

	// buttons missing

	KeyMap.emplace(SDLK_BACKSPACE, KEY_BACK);
	KeyMap.emplace(SDLK_TAB, KEY_TAB);
	KeyMap.emplace(SDLK_CLEAR, KEY_CLEAR);
	KeyMap.emplace(SDLK_RETURN, KEY_RETURN);

	// combined modifiers missing

	KeyMap.emplace(SDLK_PAUSE, KEY_PAUSE);
	KeyMap.emplace(SDLK_CAPSLOCK, KEY_CAPITAL);

	// asian letter keys missing

	KeyMap.emplace(SDLK_ESCAPE, KEY_ESCAPE);

	// asian letter keys missing

	KeyMap.emplace(SDLK_SPACE, KEY_SPACE);
	KeyMap.emplace(SDLK_PAGEUP, KEY_PRIOR);
	KeyMap.emplace(SDLK_PAGEDOWN, KEY_NEXT);
	KeyMap.emplace(SDLK_END, KEY_END);
	KeyMap.emplace(SDLK_HOME, KEY_HOME);
	KeyMap.emplace(SDLK_LEFT, KEY_LEFT);
	KeyMap.emplace(SDLK_UP, KEY_UP);
	KeyMap.emplace(SDLK_RIGHT, KEY_RIGHT);
	KeyMap.emplace(SDLK_DOWN, KEY_DOWN);

	// select missing
	KeyMap.emplace(SDLK_PRINTSCREEN, KEY_PRINT);
	// execute missing
	// snapshot missing

	KeyMap.emplace(SDLK_INSERT, KEY_INSERT);
	KeyMap.emplace(SDLK_DELETE, KEY_DELETE);
	KeyMap.emplace(SDLK_HELP, KEY_HELP);

	KeyMap.emplace(SDLK_0, KEY_KEY_0);
	KeyMap.emplace(SDLK_1, KEY_KEY_1);
	KeyMap.emplace(SDLK_2, KEY_KEY_2);
	KeyMap.emplace(SDLK_3, KEY_KEY_3);
	KeyMap.emplace(SDLK_4, KEY_KEY_4);
	KeyMap.emplace(SDLK_5, KEY_KEY_5);
	KeyMap.emplace(SDLK_6, KEY_KEY_6);
	KeyMap.emplace(SDLK_7, KEY_KEY_7);
	KeyMap.emplace(SDLK_8, KEY_KEY_8);
	KeyMap.emplace(SDLK_9, KEY_KEY_9);

	KeyMap.emplace(SDLK_a, KEY_KEY_A);
	KeyMap.emplace(SDLK_b, KEY_KEY_B);
	KeyMap.emplace(SDLK_c, KEY_KEY_C);
	KeyMap.emplace(SDLK_d, KEY_KEY_D);
	KeyMap.emplace(SDLK_e, KEY_KEY_E);
	KeyMap.emplace(SDLK_f, KEY_KEY_F);
	KeyMap.emplace(SDLK_g, KEY_KEY_G);
	KeyMap.emplace(SDLK_h, KEY_KEY_H);
	KeyMap.emplace(SDLK_i, KEY_KEY_I);
	KeyMap.emplace(SDLK_j, KEY_KEY_J);
	KeyMap.emplace(SDLK_k, KEY_KEY_K);
	KeyMap.emplace(SDLK_l, KEY_KEY_L);
	KeyMap.emplace(SDLK_m, KEY_KEY_M);
	KeyMap.emplace(SDLK_n, KEY_KEY_N);
	KeyMap.emplace(SDLK_o, KEY_KEY_O);
	KeyMap.emplace(SDLK_p, KEY_KEY_P);
	KeyMap.emplace(SDLK_q, KEY_KEY_Q);
	KeyMap.emplace(SDLK_r, KEY_KEY_R);
	KeyMap.emplace(SDLK_s, KEY_KEY_S);
	KeyMap.emplace(SDLK_t, KEY_KEY_T);
	KeyMap.emplace(SDLK_u, KEY_KEY_U);
	KeyMap.emplace(SDLK_v, KEY_KEY_V);
	KeyMap.emplace(SDLK_w, KEY_KEY_W);
	KeyMap.emplace(SDLK_x, KEY_KEY_X);
	KeyMap.emplace(SDLK_y, KEY_KEY_Y);
	KeyMap.emplace(SDLK_z, KEY_KEY_Z);

	KeyMap.emplace(SDLK_LGUI, KEY_LWIN);
	KeyMap.emplace(SDLK_RGUI, KEY_RWIN);
	// apps missing
	KeyMap.emplace(SDLK_POWER, KEY_SLEEP); //??

	KeyMap.emplace(SDLK_KP_0, KEY_NUMPAD0);
	KeyMap.emplace(SDLK_KP_1, KEY_NUMPAD1);
	KeyMap.emplace(SDLK_KP_2, KEY_NUMPAD2);
	KeyMap.emplace(SDLK_KP_3, KEY_NUMPAD3);
	KeyMap.emplace(SDLK_KP_4, KEY_NUMPAD4);
	KeyMap.emplace(SDLK_KP_5, KEY_NUMPAD5);
	KeyMap.emplace(SDLK_KP_6, KEY_NUMPAD6);
	KeyMap.emplace(SDLK_KP_7, KEY_NUMPAD7);
	KeyMap.emplace(SDLK_KP_8, KEY_NUMPAD8);
	KeyMap.emplace(SDLK_KP_9, KEY_NUMPAD9);
	KeyMap.emplace(SDLK_KP_MULTIPLY, KEY_MULTIPLY);
	KeyMap.emplace(SDLK_KP_PLUS, KEY_ADD);
	KeyMap.emplace(SDLK_KP_ENTER, KEY_RETURN);
	KeyMap.emplace(SDLK_KP_MINUS, KEY_SUBTRACT);
	KeyMap.emplace(SDLK_KP_PERIOD, KEY_DECIMAL);
	KeyMap.emplace(SDLK_KP_DIVIDE, KEY_DIVIDE);

	KeyMap.emplace(SDLK_F1, KEY_F1);
	KeyMap.emplace(SDLK_F2, KEY_F2);
	KeyMap.emplace(SDLK_F3, KEY_F3);
	KeyMap.emplace(SDLK_F4, KEY_F4);
	KeyMap.emplace(SDLK_F5, KEY_F5);
	KeyMap.emplace(SDLK_F6, KEY_F6);
	KeyMap.emplace(SDLK_F7, KEY_F7);
	KeyMap.emplace(SDLK_F8, KEY_F8);
	KeyMap.emplace(SDLK_F9, KEY_F9);
	KeyMap.emplace(SDLK_F10, KEY_F10);
	KeyMap.emplace(SDLK_F11, KEY_F11);
	KeyMap.emplace(SDLK_F12, KEY_F12);
	KeyMap.emplace(SDLK_F13, KEY_F13);
	KeyMap.emplace(SDLK_F14, KEY_F14);
	KeyMap.emplace(SDLK_F15, KEY_F15);
	// no higher F-keys

	KeyMap.emplace(SDLK_NUMLOCKCLEAR, KEY_NUMLOCK);
	KeyMap.emplace(SDLK_SCROLLLOCK, KEY_SCROLL);
	KeyMap.emplace(SDLK_LSHIFT, KEY_LSHIFT);
	KeyMap.emplace(SDLK_RSHIFT, KEY_RSHIFT);
	KeyMap.emplace(SDLK_LCTRL, KEY_LCONTROL);
	KeyMap.emplace(SDLK_RCTRL, KEY_RCONTROL);
	KeyMap.emplace(SDLK_LALT, KEY_LMENU);
	KeyMap.emplace(SDLK_RALT, KEY_RMENU);

	KeyMap.emplace(SDLK_PLUS, KEY_PLUS);
	KeyMap.emplace(SDLK_COMMA, KEY_COMMA);
	KeyMap.emplace(SDLK_MINUS, KEY_MINUS);
	KeyMap.emplace(SDLK_PERIOD, KEY_PERIOD);

	// some special keys missing
}

void CIrrDeviceSDL::CCursorControl::initCursors()
{
	Cursors.reserve(gui::ECI_COUNT);

	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));     // ECI_NORMAL
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR)); // ECI_CROSS
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));      // ECI_HAND
	Cursors.emplace_back(nullptr);                                             // ECI_HELP
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));     // ECI_IBEAM
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO));        // ECI_NO
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT));      // ECI_WAIT
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL));   // ECI_SIZEALL
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW));  // ECI_SIZENESW
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE));  // ECI_SIZENWSE
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS));    // ECI_SIZENS
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE));    // ECI_SIZEWE
	Cursors.emplace_back(nullptr);                                             // ECI_UP
}

#endif // _IRR_COMPILE_WITH_SDL_DEVICE_
