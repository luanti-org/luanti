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
#include "ISceneManager.h"
#include "IVideoDriver.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include "COSOperator.h"
#include "SIrrCreationParameters.h"

#ifdef _IRR_USE_SDL3_
#include <SDL3/SDL_version.h>
#include <SDL3/SDL_messagebox.h>
#else
#include <SDL_video.h>
#include <SDL_messagebox.h>
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cassert>

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>
#include <emscripten/threading.h>
#endif

#include "CSDLManager.h"

#ifndef _IRR_USE_SDL3_
	// SDL2 backwards compatibility for things that were renamed in SDL3.
	#define SDL_INIT_GAMEPAD SDL_INIT_GAMECONTROLLER

	#define SDL_KMOD_SHIFT KMOD_SHIFT
	#define SDL_KMOD_CTRL  KMOD_CTRL
	#define SDL_KMOD_NUM   KMOD_NUM

	#define SDL_TextInputActive(unused) SDL_IsTextInputActive()
	#define SDL_GL_DestroyContext SDL_GL_DeleteContext

	#define SDL_WINDOW_HIGH_PIXEL_DENSITY SDL_WINDOW_ALLOW_HIGHDPI
	#define SDL_StartTextInput(unused) SDL_StartTextInput()
	#define SDL_StopTextInput(unused) SDL_StopTextInput()

	#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
	#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
	#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
	#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP

	#define SDL_EVENT_TEXT_INPUT SDL_TEXTINPUT
	#define SDL_EVENT_KEY_DOWN SDL_KEYDOWN
	#define SDL_EVENT_KEY_UP SDL_KEYUP
	#define SDL_EVENT_QUIT SDL_QUIT

	#define SDL_EVENT_USER SDL_USEREVENT
	#define SDL_EVENT_FINGER_DOWN SDL_FINGERDOWN
	#define SDL_EVENT_FINGER_MOTION SDL_FINGERMOTION
	#define SDL_EVENT_FINGER_UP SDL_FINGERUP

	#define SDL_EVENT_WILL_ENTER_BACKGROUND SDL_APP_WILLENTERBACKGROUND
	#define SDL_EVENT_WILL_ENTER_FOREGROUND SDL_APP_WILLENTERFOREGROUND
	#define SDL_EVENT_RENDER_TARGETS_RESET SDL_RENDER_TARGETS_RESET
	#define SDL_EVENT_RENDER_DEVICE_LOST SDL_RENDER_DEVICE_RESET

	#define SDL_EVENT_GAMEPAD_ADDED SDL_CONTROLLERDEVICEADDED
	#define SDL_EVENT_GAMEPAD_REMOVED SDL_CONTROLLERDEVICEREMOVED
	#define SDL_EVENT_GAMEPAD_BUTTON_DOWN SDL_CONTROLLERBUTTONDOWN
	#define SDL_EVENT_GAMEPAD_BUTTON_UP SDL_CONTROLLERBUTTONUP
	#define SDL_EVENT_GAMEPAD_AXIS_MOTION SDL_CONTROLLERAXISMOTION

	#define SDL_OpenGamepad SDL_GameControllerOpen
	#define SDL_CloseGamepad SDL_GameControllerClose
	#define SDL_GetGamepadName SDL_GameControllerName
	#define SDL_GetGamepadMapping SDL_GameControllerMapping
	#define SDL_GetGamepadType SDL_GameControllerGetType
	#define SDL_GetGamepadStringForType(type) nullptr

	#define SDL_GetWindowSizeInPixels SDL_GL_GetDrawableSize
	#define SDL_DestroySurface SDL_FreeSurface

	#define SDL_SYSTEM_CURSOR_DEFAULT SDL_SYSTEM_CURSOR_ARROW
	#define SDL_SYSTEM_CURSOR_POINTER SDL_SYSTEM_CURSOR_HAND
	#define SDL_SYSTEM_CURSOR_TEXT SDL_SYSTEM_CURSOR_IBEAM
	#define SDL_SYSTEM_CURSOR_NOT_ALLOWED SDL_SYSTEM_CURSOR_NO
	#define SDL_SYSTEM_CURSOR_MOVE SDL_SYSTEM_CURSOR_SIZEALL
	#define SDL_SYSTEM_CURSOR_NESW_RESIZE SDL_SYSTEM_CURSOR_SIZENESW
	#define SDL_SYSTEM_CURSOR_NWSE_RESIZE SDL_SYSTEM_CURSOR_SIZENWSE
	#define SDL_SYSTEM_CURSOR_NS_RESIZE SDL_SYSTEM_CURSOR_SIZENS
	#define SDL_SYSTEM_CURSOR_EW_RESIZE SDL_SYSTEM_CURSOR_SIZEWE
#endif

#ifdef _IRR_USE_SDL3_
	#define SDL_FINGER_ID(ev) ((ev).tfinger.fingerID)
#else
	#define SDL_FINGER_ID(ev) ((ev).tfinger.fingerId)
#endif

static int SDLDeviceInstances = 0;

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
// Defined with the rest of the browser WebGL context code below.
static void destroyBrowserWebGLContext(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle);

EM_BOOL CIrrDeviceSDL::MouseUpDownCallback(int eventType, const EmscriptenMouseEvent *event, void *userData)
{
	// We need this callback so far only because otherwise "emscripten_request_pointerlock" calls will
	// fail as their request are infinitely deferred.
	// Not exactly certain why, maybe SDL does catch those mouse-events otherwise and not pass them on.
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

wchar_t CIrrDeviceSDL::findCharToPassToIrrlicht(uint32_t sdlKey, EKEY_CODE irrlichtKey, u16 keymod)
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

	if (keymod & SDL_KMOD_NUM) {
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

u32 CIrrDeviceSDL::getScancodeFromKey(const Keycode &key) const
{
	u32 keynum = 0;
	if (const auto *keycode = std::get_if<EKEY_CODE>(&key)) {
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

	// SDL3 returns a valid scancode for keycode 0. This is undesired.
	if (keynum == 0)
		return 0;

#ifdef _IRR_USE_SDL3_
	SDL_Keymod kmod = SDL_KMOD_NONE; // TODO: respect modifiers
	return SDL_GetScancodeFromKey(keynum, &kmod);
#else
	// Modifiers not supported
	return SDL_GetScancodeFromKey(keynum);
#endif
}

Keycode CIrrDeviceSDL::getKeyFromScancode(const u32 scancode) const
{
#ifdef _IRR_USE_SDL3_
	// TODO: SDL_HINT_KEYCODE_OPTIONS ?
	auto keycode = SDL_GetKeyFromScancode((SDL_Scancode)scancode, SDL_KMOD_NONE, true);
#else
	// Modifiers not supported
	auto keycode = SDL_GetKeyFromScancode((SDL_Scancode)scancode);
#endif
	const auto &keyentry = KeyMap.find(keycode);
	auto irrcode = keyentry != KeyMap.end() ? keyentry->second : KEY_UNKNOWN;
	wchar_t keychar = findCharToPassToIrrlicht(keycode, irrcode, false);
	return Keycode(irrcode, keychar);
}

GamepadButtonLabel CIrrDeviceSDL::getGamepadButtonLabel(const GamepadButton button) const
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_) && defined(_IRR_USE_SDL3_)
	if (auto gamepad = getRecentGamepad(); gamepad != nullptr)
		return static_cast<GamepadButtonLabel>(SDL_GetGamepadButtonLabel(gamepad, static_cast<SDL_GamepadButton>(button)));
#endif
	return GamepadButtonLabel::UNKNOWN;
}

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_) && defined(_IRR_USE_SDL3_)
SDL_Gamepad *CIrrDeviceSDL::getRecentGamepad() const
{
	auto p = gamepads.find(recentGamepadID);
	if (p != gamepads.end())
		return p->second;
	p = gamepads.begin();
	if (p != gamepads.end())
		return p->second;
	return nullptr;
}
#endif

void CIrrDeviceSDL::resetReceiveTextInputEvents()
{
	gui::IGUIElement *elem = GUIEnvironment->getFocus();
	if (elem && elem->acceptsIME()) {
		// IBus seems to have an issue where dead keys and compose keys do not
		// work (specifically, the individual characters in the sequence are
		// sent as text input events instead of the result) when
		// SDL_StartTextInput() is called on the same input box.
		core::rect<s32> pos = elem->getAbsolutePosition();
		if (!SDL_TextInputActive(Window) || lastElemPos != pos) {
			lastElemPos = pos;
			SDL_Rect rect;
			rect.x = pos.UpperLeftCorner.X;
			rect.y = pos.UpperLeftCorner.Y;
			rect.w = pos.getWidth();
			rect.h = pos.getHeight();
#ifdef _IRR_USE_SDL3_
			SDL_SetTextInputArea(Window, &rect, 10);
#else
			SDL_SetTextInputRect(&rect);
#endif
			SDL_StartTextInput(Window);
		}
	} else {
		SDL_StopTextInput(Window);
	}
}

//! constructor
CIrrDeviceSDL::CIrrDeviceSDL(const SIrrlichtCreationParameters &param) :
		CIrrDeviceStub(param),
		Context(nullptr),
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
#ifndef _IRR_USE_SDL3_
		SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO, "0");
#endif // SDL3: Handled automatically

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
	#ifndef _IRR_USE_SDL3_
		SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
	#endif // SDL3: Removed hint
		SDL_SetHint(SDL_HINT_TV_REMOTE_AS_JOYSTICK, "0");
#endif

#if SDL_VERSION_ATLEAST(2, 24, 0) && !SDL_VERSION_ATLEAST(3, 0, 0)
		// highdpi support on Windows
		SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif // SDL3: Removed hint

		// Minetest has its own code to synthesize mouse events from touch events,
		// so we prevent SDL from doing it.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
		SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

#if defined(SDL_HINT_APP_NAME)
		SDL_SetHint(SDL_HINT_APP_NAME, "Luanti");
#endif

		// Set IME hints
#ifdef _IRR_USE_SDL3_
		SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "0");
#else
		SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");
#endif
#if defined(SDL_HINT_IME_SHOW_UI)
		SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

		// Initialize SDL

		u32 flags = SDL_INIT_EVENTS;
#ifndef _IRR_USE_SDL3_
		flags |= SDL_INIT_TIMER;
#endif
		if (CreationParams.DriverType != video::EDT_NULL)
			flags |= SDL_INIT_VIDEO;
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
		flags |= SDL_INIT_JOYSTICK;
		flags |= SDL_INIT_GAMEPAD;
#endif

#ifdef _IRR_USE_SDL3_
		if (!SDL_Init(flags))
#else
		if (SDL_Init(flags) < 0)
#endif
		{
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
		sdlver += getVersionString();
		sdlver += " on ";
		sdlver += SDL_GetPlatform();
	}

	Operator = new COSOperator();
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
	for (const auto &p: gamepads)
		SDL_CloseGamepad(p.second);
#endif
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	// Drop GL-backed Irrlicht objects before destroying the WebGL context.
	// ~CIrrDeviceStub() runs afterward and would call glDeleteTextures on a
	// context that is already gone (GLctx undefined in Emscripten glue).
	if (GUIEnvironment) {
		GUIEnvironment->drop();
		GUIEnvironment = nullptr;
	}
	if (SceneManager) {
		SceneManager->drop();
		SceneManager = nullptr;
	}
	if (VideoDriver) {
		VideoDriver->drop();
		VideoDriver = nullptr;
	}
	if (ContextManager) {
		ContextManager->drop();
		ContextManager = nullptr;
	}

	if (Context) {
		destroyBrowserWebGLContext(
			reinterpret_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(Context));
		Context = nullptr;
	}
#else
	if (Window && Context) {
		SDL_GL_MakeCurrent(Window, NULL);
		SDL_GL_DestroyContext(Context);
	}
#endif
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

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
// Page state the browser main thread publishes into shared memory from its own
// event listeners (see installBrowserPageState). The frame loop used to ask
// for these with synchronous main-thread calls (canvas layout every frame,
// visibility every frame, pointer lock on every mouse move), each of which
// blocks the application worker until the main thread gets to it. Field order
// is read by the JS in installBrowserPageState.
struct BrowserPageState
{
	std::atomic<s32> published; // publish count; 0 = not installed yet
	std::atomic<s32> css_w;
	std::atomic<s32> css_h;
	std::atomic<s32> canvas_w;
	std::atomic<s32> canvas_h;
	std::atomic<s32> hidden;
	std::atomic<s32> pointer_locked;
};
static_assert(sizeof(std::atomic<s32>) == 4, "page state is read as Int32Array");
alignas(4) static BrowserPageState g_pageState;

static bool browserPageStateReady()
{
	return g_pageState.published.load(std::memory_order_acquire) > 0;
}

// One synchronous hop, once per device: install listeners that republish on
// resize, layout and attribute changes of #canvas, visibility and pointer lock.
static void installBrowserPageState()
{
	if (browserPageStateReady())
		return;
	MAIN_THREAD_EM_ASM({
		var base = $0 >> 2;
		var c = Module['canvas'] || document.getElementById('canvas');
		var publish = function() {
			Atomics.store(HEAP32, base + 1, c ? (c.clientWidth | 0) : 0);
			Atomics.store(HEAP32, base + 2, c ? (c.clientHeight | 0) : 0);
			Atomics.store(HEAP32, base + 3, c ? (c.width | 0) : 0);
			Atomics.store(HEAP32, base + 4, c ? (c.height | 0) : 0);
			Atomics.store(HEAP32, base + 5, document.hidden ? 1 : 0);
			Atomics.store(HEAP32, base + 6,
				c && document.pointerLockElement === c ? 1 : 0);
			Atomics.add(HEAP32, base, 1);
		};
		window.addEventListener('resize', publish);
		document.addEventListener('visibilitychange', publish);
		document.addEventListener('pointerlockchange', publish);
		document.addEventListener('fullscreenchange', publish);
		if (c && typeof ResizeObserver !== 'undefined')
			new ResizeObserver(publish).observe(c);
		// canvas.width/height assignments reflect to the attributes.
		if (c && typeof MutationObserver !== 'undefined')
			new MutationObserver(publish).observe(c,
				{attributes: true, attributeFilter: ['width', 'height']});
		Module['luantiPublishPageState'] = publish;
		publish();
	}, &g_pageState);
}

// After the worker changed the canvas size itself: the observer that would
// republish runs only after the proxied resize call has already returned, so
// publish synchronously once instead of resizing twice. Resizes are rare.
static void republishBrowserPageState()
{
	if (!browserPageStateReady())
		return;
	MAIN_THREAD_EM_ASM({
		var publish = Module['luantiPublishPageState'];
		if (publish)
			publish();
	});
}

// CSS box and backing store of #canvas, so we never assign canvas.width unless
// it actually changed (that assignment resets the WebGL drawing buffer).
static bool queryCanvasLayoutSize(int *css_w, int *css_h, int *canvas_w, int *canvas_h)
{
	int cw = 0, ch = 0, bw = 0, bh = 0;
	if (browserPageStateReady()) {
		cw = g_pageState.css_w.load(std::memory_order_relaxed);
		ch = g_pageState.css_h.load(std::memory_order_relaxed);
		bw = g_pageState.canvas_w.load(std::memory_order_relaxed);
		bh = g_pageState.canvas_h.load(std::memory_order_relaxed);
	} else {
		// One main-thread hop so width/height cannot tear.
		MAIN_THREAD_EM_ASM({
			var c = Module['canvas'] || document.getElementById('canvas');
			HEAP32[$0 >> 2] = c ? (c.clientWidth | 0) : 0;
			HEAP32[$1 >> 2] = c ? (c.clientHeight | 0) : 0;
			HEAP32[$2 >> 2] = c ? (c.width | 0) : 0;
			HEAP32[$3 >> 2] = c ? (c.height | 0) : 0;
		}, &cw, &ch, &bw, &bh);
	}
	if (cw < 2 || ch < 2)
		return false;
	*css_w = cw;
	*css_h = ch;
	*canvas_w = bw;
	*canvas_h = bh;
	return true;
}

static bool browserPointerLocked()
{
	if (browserPageStateReady())
		return g_pageState.pointer_locked.load(std::memory_order_relaxed) != 0;
	EmscriptenPointerlockChangeEvent pointerlock{};
	return emscripten_get_pointerlock_status(&pointerlock) ==
			EMSCRIPTEN_RESULT_SUCCESS && pointerlock.isActive;
}

static bool browserPageHidden()
{
	if (browserPageStateReady())
		return g_pageState.hidden.load(std::memory_order_relaxed) != 0;
	EmscriptenVisibilityChangeEvent visibility;
	return emscripten_get_visibility_status(&visibility) ==
			EMSCRIPTEN_RESULT_SUCCESS && visibility.hidden;
}

static void logBrowserWebGLFailure()
{
	static bool logged = false;
	if (logged)
		return;
	logged = true;
	MAIN_THREAD_EM_ASM({
		var c = Module['canvas'] || document.getElementById('canvas');
		var parent = c && c.parentElement;
		var info = ({
			found: !!c,
			connected: !!(c && c.isConnected),
			hidden: !!(c && c.hidden),
			parentHidden: !!(parent && parent.hidden),
			closestHidden: !!(c && c.closest('[hidden]')),
			width: c ? c.width : 0,
			height: c ? c.height : 0,
			clientWidth: c ? c.clientWidth : 0,
			clientHeight: c ? c.clientHeight : 0,
			innerWidth: window.innerWidth | 0,
			innerHeight: window.innerHeight | 0
		});
		console.error('Luanti: WebGL context creation failed', info);
	});
}

// Canvas was display:none in the launcher until Play. CSS size can still be
// 0x0 on the first worker hop; never hand a degenerate backing store to WebGL.
static void ensureBrowserCanvasSize(u32 *width, u32 *height)
{
	int css_w = 0, css_h = 0, canvas_w = 0, canvas_h = 0;
	if (queryCanvasLayoutSize(&css_w, &css_h, &canvas_w, &canvas_h)) {
		*width = static_cast<u32>(css_w);
		*height = static_cast<u32>(css_h);
		if (canvas_w != css_w || canvas_h != css_h) {
			emscripten_set_canvas_element_size("#canvas", css_w, css_h);
			republishBrowserPageState();
		}
		return;
	}
	int fallback_w = 0, fallback_h = 0;
	MAIN_THREAD_EM_ASM({
		HEAP32[$0 >> 2] = (window.innerWidth | 0) || 1280;
		HEAP32[$1 >> 2] = (window.innerHeight | 0) || 720;
	}, &fallback_w, &fallback_h);
	if (fallback_w < 2)
		fallback_w = 1280;
	if (fallback_h < 2)
		fallback_h = 720;
	*width = static_cast<u32>(fallback_w);
	*height = static_cast<u32>(fallback_h);
	emscripten_set_canvas_element_size("#canvas", fallback_w, fallback_h);
	republishBrowserPageState();
}

// Optional overrides from the page: Module.luantiWebGL = { major: 1|2 }.
// "major" forces a single WebGL version (used by tests and for bisecting
// driver problems); 0 or absent tries WebGL2 and then WebGL1.
static int browserWebGLForcedMajor()
{
	return MAIN_THREAD_EM_ASM_INT({
		var o = Module['luantiWebGL'];
		var m = o && o['major'] | 0;
		return (m === 1 || m === 2) ? m : 0;
	});
}

// Which thread owns the WebGL context. Module.luantiWebGL.render (?render=):
//   "proxy"     - the browser main thread; every GL call is proxied to it and
//                 frames are presented with emscripten_webgl_commit_frame().
//   "offscreen" - this application worker, on an OffscreenCanvas of its own;
//                 GL calls are direct and frames are handed to the page as
//                 ImageBitmaps (client/web/present.js). No fallback.
//   "auto"      - offscreen if the browser can, otherwise proxy.
// Absent: the build default, WASM_RENDER_THREAD in CMake.
#ifndef LUANTI_WASM_RENDER_THREAD_DEFAULT
#define LUANTI_WASM_RENDER_THREAD_DEFAULT 0
#endif
enum BrowserRenderMode
{
	BROWSER_RENDER_PROXY = 0,
	BROWSER_RENDER_OFFSCREEN = 1,
	BROWSER_RENDER_AUTO = 2,
};

static int browserRenderMode()
{
	return MAIN_THREAD_EM_ASM_INT({
		var o = Module['luantiWebGL'];
		var r = o && o['render'];
		if (r === 'proxy')
			return 0;
		if (r === 'offscreen')
			return 1;
		if (r === 'auto')
			return 2;
		return $0;
	}, LUANTI_WASM_RENDER_THREAD_DEFAULT);
}

// Offscreen render mode. Why not transferControlToOffscreen() on #canvas: a
// transferred canvas only presents when its owning thread returns to the event
// loop, and Luanti's loop on this worker never does, so the game stayed blank.
// A worker-created OffscreenCanvas presents explicitly instead, through
// transferToImageBitmap(), and #canvas stays a normal DOM canvas for input,
// pointer lock, fullscreen and layout.
static bool g_offscreenRender = false; // application worker only
// Frames handed to the page and frames it has shown or dropped. The page
// increments the second (present.js), so it is read as an Int32Array there.
static s32 g_offscreenFramesSent = 0;
static std::atomic<s32> g_offscreenFramesAcked{0};
// One frame waiting for the display while the next is rendered. Running
// further ahead only renders frames the page then drops, and the extra GPU
// work starves the compositor (measured on SwiftShader: requestAnimationFrame
// fell from 13 Hz to 2 Hz with the worker unthrottled).
static constexpr s32 OFFSCREEN_MAX_FRAMES_IN_FLIGHT = 1;
// Safety net only: a hidden page acknowledges immediately (present.js).
static constexpr double OFFSCREEN_MAX_PRESENT_WAIT_MS = 1000.0;
// Set by luanti_debug_lose_webgl_context() from the page (tests).
static std::atomic<bool> g_debugLoseContext{false};

// Creates a WebGL context on a new OffscreenCanvas owned by the calling thread
// and registers it with Emscripten's GL library, which then dispatches GL
// calls from this thread directly instead of proxying them. Returns the
// context handle or 0.
EM_JS(int, luanti_offscreen_create_context, (int major, int width, int height,
		int alpha, int depth, int stencil), {
	if (typeof OffscreenCanvas === 'undefined' ||
			typeof OffscreenCanvas.prototype.transferToImageBitmap !== 'function' ||
			typeof self.postMessage !== 'function')
		return 0;
	var attrs = {
		'alpha': !!alpha,
		'depth': !!depth,
		'stencil': !!stencil,
		'antialias': false,
		'premultipliedAlpha': true,
		'preserveDrawingBuffer': false,
		'powerPreference': 'high-performance',
		'failIfMajorPerformanceCaveat': false
	};
	var ctx = null;
	try {
		var canvas = new OffscreenCanvas(Math.max(1, width), Math.max(1, height));
		ctx = canvas.getContext(major == 2 ? 'webgl2' : 'webgl', attrs);
	} catch (e) {
		console.warn('Luanti: OffscreenCanvas WebGL' + major + ' failed', e);
	}
	if (!ctx)
		return 0;
	// The attributes Emscripten's GL library reads back from a context.
	attrs['majorVersion'] = major;
	attrs['minorVersion'] = 0;
	attrs['enableExtensionsByDefault'] = 1;
	attrs['explicitSwapControl'] = 0;
	attrs['renderViaOffscreenBackBuffer'] = 0;
	attrs['proxyContextToMainThread'] = 0;
	return GL.registerContext(ctx, attrs);
});

// Hands the finished frame to the page. 1: posted, 0: no context, -1: lost.
EM_JS(int, luanti_offscreen_present, (void *ack_counter), {
	var ctx = GLctx;
	if (!ctx || !ctx.canvas)
		return 0;
	if (ctx.isContextLost())
		return -1;
	var bitmap = ctx.canvas.transferToImageBitmap();
	// No 'cmd' field: Emscripten's own handler for this worker ignores the
	// message, and the listener present.js adds to every worker takes it.
	// (Emscripten's "callHandler" command changed shape between releases.)
	self.postMessage({'luantiFrame': bitmap, 'luantiAck': ack_counter}, [bitmap]);
	return 1;
});

EM_JS(void, luanti_offscreen_context_attributes, (int handle, int *major,
		EM_BOOL *alpha, EM_BOOL *depth, EM_BOOL *stencil, EM_BOOL *antialias), {
	var context = GL.contexts[handle];
	var ctx = context ? context.GLctx : null;
	var a = ctx ? ctx.getContextAttributes() : null;
	HEAP32[major >> 2] = (typeof WebGL2RenderingContext !== 'undefined' &&
		ctx instanceof WebGL2RenderingContext) ? 2 : 1;
	HEAP32[alpha >> 2] = a && a.alpha ? 1 : 0;
	HEAP32[depth >> 2] = a && a.depth ? 1 : 0;
	HEAP32[stencil >> 2] = a && a.stencil ? 1 : 0;
	HEAP32[antialias >> 2] = a && a.antialias ? 1 : 0;
});

// Resizing resets the drawing buffer, so only between frames.
EM_JS(void, luanti_offscreen_resize, (int width, int height), {
	var canvas = GLctx && GLctx.canvas;
	if (!canvas)
		return;
	width = Math.max(1, width);
	height = Math.max(1, height);
	if (canvas.width !== width || canvas.height !== height) {
		canvas.width = width;
		canvas.height = height;
	}
});

// Emscripten proxies emscripten_webgl_destroy_context() to the main thread
// unless it is built with OFFSCREENCANVAS_SUPPORT, and the main thread has no
// such context; delete the worker-owned one here, on its own thread.
EM_JS(void, luanti_offscreen_destroy_context, (int handle), {
	if (!GL.contexts[handle])
		return;
	if (GL.currentContext === GL.contexts[handle])
		GL.makeContextCurrent(0);
	GL.deleteContext(handle);
});

static void destroyBrowserWebGLContext(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle)
{
	emscripten_webgl_make_context_current(0);
	if (g_offscreenRender)
		luanti_offscreen_destroy_context(static_cast<int>(handle));
	else
		emscripten_webgl_destroy_context(handle);
}

EM_JS(void, luanti_offscreen_lose_context, (), {
	var ext = GLctx && GLctx.getExtension('WEBGL_lose_context');
	if (ext)
		ext.loseContext();
});

// Module.luantiWebGL.swapSleepMs: milliseconds to sleep after presenting a
// frame (0 or absent: none, only drain this thread's proxied calls).
static int browserSwapSleepMs()
{
	return MAIN_THREAD_EM_ASM_INT({
		var o = Module['luantiWebGL'];
		var ms = o && o['swapSleepMs'] | 0;
		return ms > 0 && ms <= 100 ? ms : 0;
	});
}

static void reportBrowserWebGLContext(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx,
		int attempt, bool explicit_swap)
{
	const char *render = g_offscreenRender ? "offscreen" : "proxy";
	EmscriptenWebGLContextAttributes got;
	if (g_offscreenRender) {
		// Our own context on this thread: ask WebGL directly.
		emscripten_webgl_init_context_attributes(&got);
		luanti_offscreen_context_attributes(static_cast<int>(ctx),
			&got.majorVersion, &got.alpha,
			&got.depth, &got.stencil, &got.antialias);
		got.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
	} else if (emscripten_webgl_get_context_attributes(ctx, &got) != EMSCRIPTEN_RESULT_SUCCESS) {
		os::Printer::log("Could not query the WebGL context attributes", ELL_WARNING);
		return;
	}
	char msg[200];
	snprintf(msg, sizeof(msg),
		"WebGL%d context (%s, attempt %d, explicitSwap=%d, alpha=%d depth=%d "
		"stencil=%d antialias=%d powerPreference=%d)", got.majorVersion, render,
		attempt, explicit_swap ? 1 : 0, got.alpha, got.depth, got.stencil,
		got.antialias, got.powerPreference);
	os::Printer::log(msg, ELL_INFORMATION);
	// Visible to the launcher console and to tests without a GL query. Built as
	// a string because commas in a JS object literal would split the macro args.
	char json[256];
	snprintf(json, sizeof(json),
		"{\"version\":%d,\"render\":\"%s\",\"attempt\":%d,\"explicitSwap\":%s,"
		"\"alpha\":%s,\"depth\":%s,\"stencil\":%s,\"antialias\":%s,"
		"\"powerPreference\":%d}",
		got.majorVersion, render, attempt, explicit_swap ? "true" : "false",
		got.alpha ? "true" : "false", got.depth ? "true" : "false",
		got.stencil ? "true" : "false", got.antialias ? "true" : "false",
		static_cast<int>(got.powerPreference));
	MAIN_THREAD_EM_ASM({
		Module['luantiWebGLInfo'] = JSON.parse(UTF8ToString($0));
		console.info('Luanti: WebGL context', UTF8ToString($0));
	}, json);
}

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE createBrowserWebGLContext(
		const SIrrlichtCreationParameters &params, u32 width, u32 height)
{
	auto try_create = [&](int major_version, bool explicit_swap) {
		EmscriptenWebGLContextAttributes attributes;
		emscripten_webgl_init_context_attributes(&attributes);
		attributes.alpha = params.WithAlphaChannel;
		attributes.depth = params.ZBufferBits > 0;
		attributes.stencil = params.Stencilbuffer;
		// The offscreen back buffer is what the game renders to, so canvas
		// MSAA only costs memory; anti-aliasing is handled by the engine.
		attributes.antialias = false;
		attributes.majorVersion = major_version;
		attributes.minorVersion = 0;
		attributes.enableExtensionsByDefault = true;
		attributes.explicitSwapControl = explicit_swap;
		attributes.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_ALWAYS;
		attributes.renderViaOffscreenBackBuffer = true;
		attributes.failIfMajorPerformanceCaveat = false;
		// Dual-GPU laptops otherwise default to the integrated GPU.
		attributes.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
		attributes.preserveDrawingBuffer = false;
		return emscripten_webgl_create_context("#canvas", &attributes);
	};

	// Newest first: offscreen WebGL2 and WebGL1 on this worker (unless the
	// render mode is "proxy"), then the proxied ladder. Some drivers or
	// blocklists reject WebGL2 or refuse explicit swap control; WebGL1 (with
	// the ES2 shaders) is the last resort.
	const int forced = browserWebGLForcedMajor();
	const int mode = browserRenderMode();
	int n = 0;
	if (mode != BROWSER_RENDER_PROXY) {
		for (int major : {2, 1}) {
			n++;
			if (forced && major != forced)
				continue;
			const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = luanti_offscreen_create_context(
				major, width, height,
				params.WithAlphaChannel, params.ZBufferBits > 0, params.Stencilbuffer);
			if (ctx) {
				g_offscreenRender = true;
				g_offscreenFramesSent = 0;
				g_offscreenFramesAcked = 0;
				reportBrowserWebGLContext(ctx, n, false);
				return ctx;
			}
			char msg[80];
			snprintf(msg, sizeof(msg), "Offscreen WebGL%d context creation failed", major);
			os::Printer::log(msg, ELL_WARNING);
		}
		if (mode == BROWSER_RENDER_OFFSCREEN) {
			os::Printer::log("Render mode \"offscreen\" was requested; not falling "
				"back to the proxied context", ELL_ERROR);
			return 0;
		}
	}
	g_offscreenRender = false;
	struct Attempt { int major; bool explicit_swap; };
	static const Attempt attempts[] = {
		{2, true}, {2, false}, {1, true}, {1, false},
	};
	for (const Attempt &a : attempts) {
		n++;
		if (forced && a.major != forced)
			continue;
		EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = try_create(a.major, a.explicit_swap);
		if (ctx) {
			reportBrowserWebGLContext(ctx, n, a.explicit_swap);
			return ctx;
		}
		char msg[80];
		snprintf(msg, sizeof(msg), "WebGL%d context creation failed (explicitSwap=%d)",
			a.major, a.explicit_swap ? 1 : 0);
		os::Printer::log(msg, ELL_WARNING);
	}
	return 0;
}

// Set from the webglcontextlost event (delivered to the thread that
// registered it). The engine cannot rebuild its GL resources, so after a loss
// rendering stays paused until the page is reloaded. File-level state because
// Emscripten cannot reliably unregister these handlers; there is one device
// per page.
static std::atomic<bool> g_webglContextLost{false};

static EM_BOOL webglContextEventCallback(int eventType, const void *, void *)
{
	if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTLOST) {
		g_webglContextLost = true;
		os::Printer::log("WebGL context lost; rendering paused", ELL_ERROR);
		// Returning true calls preventDefault(), which is what allows the
		// browser to later restore the context at all.
		return EM_TRUE;
	}
	if (eventType == EMSCRIPTEN_EVENT_WEBGLCONTEXTRESTORED) {
		// Textures, buffers, shaders and FBOs are gone and Irrlicht keeps no
		// recreate path, so stay paused; the page offers a reload (world data
		// is flushed to IndexedDB first).
		os::Printer::log("WebGL context restored but engine GL state is stale; "
			"reload required", ELL_WARNING);
		return EM_TRUE;
	}
	return EM_FALSE;
}

// Offscreen mode: the worker never runs its event loop, so the context-lost
// event is never delivered; SwapWindow polls isContextLost() and tells the page
// with this instead (the launcher shows the same overlay as for the DOM event).
static void notifyPageContextLost()
{
	MAIN_THREAD_ASYNC_EM_ASM({
		var launcher = Module['luantiLauncher'];
		if (launcher && typeof launcher['notifyContextLost'] === 'function')
			launcher['notifyContextLost']();
	});
}

extern "C" {
// For tests: lose the engine's WebGL context from the page in either render
// mode (in offscreen mode the page cannot reach the context itself). Takes
// effect at the next presented frame.
EMSCRIPTEN_KEEPALIVE void luanti_debug_lose_webgl_context()
{
	g_debugLoseContext = true;
}
}
#endif

bool CIrrDeviceSDL::createWindowWithContext()
{
	u32 SDL_Flags = 0;
	// HiDPI backing stores make SDL scale mouse X/Y by window/CSS ratios that
	// differ per axis. Keep CSS pixels, backing store, and mouse deltas 1:1.
#ifndef _IRR_EMSCRIPTEN_PLATFORM_
	SDL_Flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

#ifndef _IRR_USE_SDL3_
	SDL_Flags |= getFullscreenFlag(CreationParams.Fullscreen);
#endif
	if (Resizable)
		SDL_Flags |= SDL_WINDOW_RESIZABLE;
	if (CreationParams.WindowMaximized)
		SDL_Flags |= SDL_WINDOW_MAXIMIZED;
#ifndef _IRR_EMSCRIPTEN_PLATFORM_
	SDL_Flags |= SDL_WINDOW_OPENGL;
#endif

	SDL_GL_ResetAttributes();

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	installBrowserPageState();
	updateSizeAndScale();
	ensureBrowserCanvasSize(&Width, &Height);

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

	Window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED, Width, Height, SDL_Flags);
	if (!Window) {
		os::Printer::log("Could not create browser window", SDL_GetError(), ELL_WARNING);
		return false;
	}

	ensureBrowserCanvasSize(&Width, &Height);

	const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_context =
		createBrowserWebGLContext(CreationParams, Width, Height);
	if (!webgl_context) {
		os::Printer::log("Could not create browser WebGL context", ELL_WARNING);
		logBrowserWebGLFailure();
		SDL_DestroyWindow(Window);
		Window = nullptr;
		return false;
	}
	Context = reinterpret_cast<SDL_GLContext>(webgl_context);
	if (emscripten_webgl_make_context_current(webgl_context) != EMSCRIPTEN_RESULT_SUCCESS) {
		os::Printer::log("Could not activate browser WebGL context", ELL_WARNING);
		destroyBrowserWebGLContext(webgl_context);
		Context = nullptr;
		SDL_DestroyWindow(Window);
		Window = nullptr;
		return false;
	}

	// SDL_GL_GetAttribute resolves GL entry points dynamically. Under
	// PROXY_TO_PTHREAD + OFFSCREEN_FRAMEBUFFER those indirect calls do not carry
	// Emscripten's worker-side GL proxy context, so this diagnostic query fails
	// even though the context itself was created and made current successfully.
	// Rendering uses the normal linked GL entry points and remains proxied.
#ifndef _IRR_EMSCRIPTEN_PLATFORM_
	logAttributes();
#endif

	// SDL_CreateWindow probes CSS by temporarily setting the canvas to 1x1.
	// Re-apply the layout size now that the WebGL context (and offscreen FBO)
	// exist so viewport, backing store, and mouse space stay aligned.
	updateSizeAndScale();

	g_webglContextLost = false;
	static bool contextEventsRegistered = false;
	if (!contextEventsRegistered) {
		contextEventsRegistered = true;
		emscripten_set_webglcontextlost_callback("#canvas", nullptr, true,
			webglContextEventCallback);
		emscripten_set_webglcontextrestored_callback("#canvas", nullptr, true,
			webglContextEventCallback);
	}

	// "#canvas" is for the opengl context
	emscripten_set_mousedown_callback("#canvas", (void *)this, true, MouseUpDownCallback);
	emscripten_set_mouseup_callback("#canvas", (void *)this, true, MouseUpDownCallback);

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

#ifdef _IRR_USE_SDL3_
	Window = SDL_CreateWindow("", Width, Height, SDL_Flags);
#else
	Window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Width, Height, SDL_Flags);
#endif
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

#ifdef _IRR_USE_SDL3_
	if (CreationParams.Fullscreen)
		SDL_SetWindowFullscreen(Window, true);
#endif

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
	ContextManager->initialize(CreationParams, {});

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

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	// Align CSS size with the engine. updateSizeAndScale must not assign
	// canvas.width unless the backing store actually changed — that reset
	// clears the offscreen FBO and flickers 2D quads (HUD text).
	const u32 old_w = Width;
	const u32 old_h = Height;
	updateSizeAndScale();
	if (g_offscreenRender)
		luanti_offscreen_resize(Width, Height);
	if (VideoDriver && (old_w != Width || old_h != Height))
		VideoDriver->OnResize(core::dimension2d<u32>(Width, Height));
#endif

	SEvent irrevent;
	SDL_Event SDL_event;

	auto get_touch_id_x_y = [this, &irrevent, &SDL_event]() {
		irrevent.TouchInput.ID = SDL_FINGER_ID(SDL_event);
		irrevent.TouchInput.X = static_cast<s32>(SDL_event.tfinger.x * Width);
		irrevent.TouchInput.Y = static_cast<s32>(SDL_event.tfinger.y * Height);
	};

	auto handle_window_resize_event = [this, &irrevent]() {
		u32 old_w = Width, old_h = Height;
		f32 old_scale_x = ScaleX, old_scale_y = ScaleY;
		updateSizeAndScale();
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
		if (g_offscreenRender)
			luanti_offscreen_resize(Width, Height);
#endif
		if (old_w != Width || old_h != Height) {
			if (VideoDriver)
				VideoDriver->OnResize(core::dimension2d<u32>(Width, Height));
		}
		if (old_scale_x != ScaleX || old_scale_y != ScaleY) {
			irrevent.EventType = EET_APPLICATION_EVENT;
			irrevent.ApplicationEvent.EventType = EAET_DPI_CHANGED;
			postEventFromUser(irrevent);
		}
	};

	while (!Close && wrap_PollEvent(&SDL_event)) {
		// os::Printer::log("event: ", core::stringc((int)SDL_event.type).c_str(),   ELL_INFORMATION);	// just for debugging
		irrevent = {};

		switch (SDL_event.type) {
		case SDL_EVENT_MOUSE_MOTION: {
			SDL_Keymod keymod = SDL_GetModState();

			irrevent.EventType = EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = EMIE_MOUSE_MOVED;

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
			// Pointer-lock movementX/Y is already in CSS pixels, matching
			// Width/Height. Do not rescale by SDL window size: CreateWindow
			// often keeps a 1024×600 window while the canvas CSS box is the
			// full viewport, which multiplied yaw/pitch by ~2×.
			MouseXRel = SDL_event.motion.xrel;
			MouseYRel = SDL_event.motion.yrel;
			const bool pointer_locked = browserPointerLocked();
			if (pointer_locked || SDL_GetRelativeMouseMode()) {
				MouseX += MouseXRel;
				MouseY += MouseYRel;
			} else {
				MouseX = SDL_event.motion.x;
				MouseY = SDL_event.motion.y;
			}
#else
			MouseXRel = static_cast<s32>(SDL_event.motion.xrel * ScaleX);
			MouseYRel = static_cast<s32>(SDL_event.motion.yrel * ScaleY);
#ifdef _IRR_USE_SDL3_
			if (!SDL_GetWindowRelativeMouseMode(Window))
#else
			if (!SDL_GetRelativeMouseMode())
#endif
			{
				MouseX = static_cast<s32>(SDL_event.motion.x * ScaleX);
				MouseY = static_cast<s32>(SDL_event.motion.y * ScaleY);
			} else {
				MouseX += MouseXRel;
				MouseY += MouseYRel;
			}
#endif
			irrevent.MouseInput.X = MouseX;
			irrevent.MouseInput.Y = MouseY;
			irrevent.MouseInput.XRel = MouseXRel;
			irrevent.MouseInput.YRel = MouseYRel;

			irrevent.MouseInput.ButtonStates = MouseButtonStates;
			irrevent.MouseInput.Shift = (keymod & SDL_KMOD_SHIFT) != 0;
			irrevent.MouseInput.Control = (keymod & SDL_KMOD_CTRL) != 0;

			postEventFromUser(irrevent);
			break;
		}
		case SDL_EVENT_MOUSE_WHEEL: {
			SDL_Keymod keymod = SDL_GetModState();

			irrevent.EventType = EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = EMIE_MOUSE_WHEEL;
#ifdef _IRR_USE_SDL3_
			irrevent.MouseInput.Wheel = SDL_event.wheel.y;
#elif SDL_VERSION_ATLEAST(2, 0, 18)
			irrevent.MouseInput.Wheel = SDL_event.wheel.preciseY;
#else
			irrevent.MouseInput.Wheel = SDL_event.wheel.y;
#endif
			irrevent.MouseInput.ButtonStates = MouseButtonStates;
			irrevent.MouseInput.Shift = (keymod & SDL_KMOD_SHIFT) != 0;
			irrevent.MouseInput.Control = (keymod & SDL_KMOD_CTRL) != 0;
			irrevent.MouseInput.X = MouseX;
			irrevent.MouseInput.Y = MouseY;

			// wheel y can be 0 if scrolling sideways
			if (irrevent.MouseInput.Wheel == 0.0f)
				break;

			postEventFromUser(irrevent);
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			SDL_Keymod keymod = SDL_GetModState();

			irrevent.EventType = EET_MOUSE_INPUT_EVENT;
			irrevent.MouseInput.Event = EMIE_MOUSE_MOVED; // value to be ignored

#ifdef _IRR_EMSCRIPTEN_PLATFORM_
			// Handle mouselocking in emscripten.
			// When the canvas is clicked, lock the mouse-pointer if it should be invisible.
			// For security reasons this is delayed until the mouse-up event.
			// We do not pass on this event as we don't want the activation click to trigger in-game actions.
			if (SDL_event.type == SDL_MOUSEBUTTONDOWN) {
				EmscriptenPointerlockChangeEvent pointerlockStatus; // let's hope that test is not expensive ...
				if (emscripten_get_pointerlock_status(&pointerlockStatus) == EMSCRIPTEN_RESULT_SUCCESS) {
					if (CursorControl->isVisible() && pointerlockStatus.isActive) {
						emscripten_exit_pointerlock();
						return !Close;
					} else if (!CursorControl->isVisible() && !pointerlockStatus.isActive) {
						emscripten_request_pointerlock("#canvas", true);
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
			irrevent.MouseInput.Button = button;
			auto is_down = SDL_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;

			if (is_down)
				MouseButtonStates |= SDL_BUTTON_MASK(button);
			else
				MouseButtonStates &= ~SDL_BUTTON_MASK(button);

			switch (button) {
			case SDL_BUTTON_LEFT:
				irrevent.MouseInput.Event = is_down ? EMIE_LMOUSE_PRESSED_DOWN : EMIE_LMOUSE_LEFT_UP;
				break;

			case SDL_BUTTON_RIGHT:
				irrevent.MouseInput.Event = is_down ? EMIE_RMOUSE_PRESSED_DOWN : EMIE_RMOUSE_LEFT_UP;
				break;

			case SDL_BUTTON_MIDDLE:
				irrevent.MouseInput.Event = is_down ? EMIE_MMOUSE_PRESSED_DOWN : EMIE_MMOUSE_LEFT_UP;
				break;

			default: // Other mouse buttons do not have dedicated events:
				irrevent.MouseInput.Event = is_down ? EMIE_XMOUSE_PRESSED_DOWN : EMIE_XMOUSE_LEFT_UP;
				break;
			}

			bool shift = (keymod & SDL_KMOD_SHIFT) != 0;
			bool control = (keymod & SDL_KMOD_CTRL) != 0;
			if (irrevent.MouseInput.Event != EMIE_MOUSE_MOVED) {
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
			}
			break;
		}

		case SDL_EVENT_TEXT_INPUT: {
			irrevent.EventType = EET_STRING_INPUT_EVENT;
			irrevent.StringInput.Str = new core::stringw();
			core::utf8ToWString(*irrevent.StringInput.Str, SDL_event.text.text);
			postEventFromUser(irrevent);
			delete irrevent.StringInput.Str;
			irrevent.StringInput.Str = NULL;
		} break;

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP: {
#ifdef _IRR_USE_SDL3_
			SDL_Keycode keysym = SDL_event.key.key;
			SDL_Scancode scancode = SDL_event.key.scancode;
			SDL_Keymod keymod = SDL_event.key.mod;
#else
			auto keysym = SDL_event.key.keysym.sym;
			auto scancode = SDL_event.key.keysym.scancode;
			auto keymod = SDL_event.key.keysym.mod;
#endif

			// Treat AC_BACK as the Escape key
			if (scancode == SDL_SCANCODE_AC_BACK) {
				scancode = SDL_SCANCODE_ESCAPE;
				keysym = SDLK_ESCAPE;
			}

			const auto &entry = KeyMap.find(keysym);
			auto key = entry == KeyMap.end() ? KEY_UNKNOWN : entry->second;

			if (!Keycode::isValid(key))
				os::Printer::log("keycode not mapped", core::stringc(keysym), ELL_DEBUG);

			// Make sure to only input special characters if something is in focus,
			// as SDL_EVENT_TEXT_INPUT handles normal unicode already
			if (SDL_TextInputActive(Window) && !keyIsKnownSpecial(key) && (keymod & SDL_KMOD_CTRL) == 0)
				break;

			irrevent.EventType = EET_KEY_INPUT_EVENT;
			irrevent.KeyInput.Key = key;
			irrevent.KeyInput.PressedDown = (SDL_event.type == SDL_EVENT_KEY_DOWN);
			irrevent.KeyInput.Shift = (keymod & SDL_KMOD_SHIFT) != 0;
			irrevent.KeyInput.Control = (keymod & SDL_KMOD_CTRL) != 0;
#ifdef _IRR_USE_SDL3_
			{
				// Look up the printable character (like in an input box)
				SDL_Keycode keycode_Aa = SDL_GetKeyFromScancode(scancode, keymod, false);
				// Discard non-printable, such as CTRL, HOME, F3, ...
				irrevent.KeyInput.Char = keycode_Aa < SDLK_EXTENDED_MASK ? keycode_Aa : 0;
			}
#else
			irrevent.KeyInput.Char = findCharToPassToIrrlicht(keysym, key, keymod);
#endif
			irrevent.KeyInput.SystemKeyCode = scancode;

			postEventFromUser(irrevent);
		} break;

		case SDL_EVENT_QUIT:
			Close = true;
			break;

#ifdef _IRR_USE_SDL3_
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			handle_window_resize_event();
			break;
#else
		case SDL_WINDOWEVENT:
			switch (SDL_event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
			case SDL_WINDOWEVENT_SIZE_CHANGED:
#if SDL_VERSION_ATLEAST(2, 0, 18)
			case SDL_WINDOWEVENT_DISPLAY_CHANGED:
#endif
				handle_window_resize_event();
				break;
			}
			break;
#endif

		case SDL_EVENT_USER:
			irrevent.EventType = EET_USER_EVENT;
			irrevent.UserEvent.UserData1 = reinterpret_cast<uintptr_t>(SDL_event.user.data1);
			irrevent.UserEvent.UserData2 = reinterpret_cast<uintptr_t>(SDL_event.user.data2);

			postEventFromUser(irrevent);
			break;

		case SDL_EVENT_FINGER_DOWN:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_PRESSED_DOWN;
			get_touch_id_x_y();
			CurrentTouchCount++;
			irrevent.TouchInput.touchedCount = CurrentTouchCount;

			postEventFromUser(irrevent);
			break;

		case SDL_EVENT_FINGER_MOTION:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_MOVED;
			get_touch_id_x_y();
			irrevent.TouchInput.touchedCount = CurrentTouchCount;

			postEventFromUser(irrevent);
			break;

		case SDL_EVENT_FINGER_UP:
			irrevent.EventType = EET_TOUCH_INPUT_EVENT;
			irrevent.TouchInput.Event = ETIE_LEFT_UP;
			get_touch_id_x_y();
			// To match Android behavior, still count the pointer that was
			// just released.
			irrevent.TouchInput.touchedCount = CurrentTouchCount;
			if (CurrentTouchCount > 0) {
				CurrentTouchCount--;
			}

			postEventFromUser(irrevent);
			break;

		// SDL2: Contrary to what the SDL documentation says, SDL_APP_WILLENTERBACKGROUND
		// and SDL_APP_WILLENTERFOREGROUND are actually sent in onStop/onStart,
		// not onPause/onResume, on recent Android versions. This can be verified
		// by testing or by looking at the org.libsdl.app.SDLActivity Java code.
		// -> This means we can use them to implement isWindowVisible().

		case SDL_EVENT_WILL_ENTER_BACKGROUND:
			IsInBackground = true;
			break;

		case SDL_EVENT_WILL_ENTER_FOREGROUND:
			IsInBackground = false;
			break;

		case SDL_EVENT_RENDER_TARGETS_RESET:
			os::Printer::log("Received SDL_RENDER_TARGETS_RESET. Rendering is probably broken.", ELL_ERROR);
			break;

		case SDL_EVENT_RENDER_DEVICE_LOST:
			os::Printer::log("Received SDL_RENDER_DEVICE_RESET. Rendering is probably broken.", ELL_ERROR);
			break;

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
		case SDL_EVENT_GAMEPAD_ADDED: {
#ifdef _IRR_USE_SDL3_
			auto id = SDL_event.gdevice.which;
#else
			auto id = SDL_event.cdevice.which;
#endif
			if (auto gamepad = SDL_OpenGamepad(id); gamepad != nullptr) {
				gamepads.emplace(id, gamepad);
				recentGamepadID = id;
				os::Printer::log("Gamepad connected", SDL_GetGamepadName(gamepad), ELL_INFORMATION);
				os::Printer::log("Gamepad type", SDL_GetGamepadStringForType(SDL_GetGamepadType(gamepad)), ELL_INFORMATION);
				if (auto mapping = SDL_GetGamepadMapping(gamepad); mapping != nullptr) {
					os::Printer::log("Gamepad mapping", mapping, ELL_INFORMATION);
					SDL_free(mapping);
				}
			} else {
				os::Printer::log("Unable to open gamepad", SDL_GetError(), ELL_ERROR);
			}
			break;
		}

		case SDL_EVENT_GAMEPAD_REMOVED: {
#ifdef _IRR_USE_SDL3_
			auto id = SDL_event.gdevice.which;
#else
			auto id = SDL_event.cdevice.which;
#endif
			if (auto p = gamepads.find(id); p != gamepads.end()) {
				os::Printer::log("Gamepad disconnected", SDL_GetGamepadName(p->second), ELL_INFORMATION);
				SDL_CloseGamepad(p->second);
				gamepads.erase(id);
			}
			break;
		}

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP: {
			irrevent.EventType = EET_GAMEPAD_BUTTON_EVENT;
#ifdef _IRR_USE_SDL3_
			auto id = SDL_event.gbutton.which;
			irrevent.GamepadButtonEvent.Button = static_cast<GamepadButton>(SDL_event.gbutton.button);
#else
			auto id = SDL_event.cbutton.which;
			irrevent.GamepadButtonEvent.Button = static_cast<GamepadButton>(SDL_event.cbutton.button);
#endif
			irrevent.GamepadButtonEvent.ID = id;
			irrevent.GamepadButtonEvent.PressedDown = (SDL_event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
			recentGamepadID = id;
			postEventFromUser(irrevent);
			break;
		}

		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			irrevent.EventType = EET_GAMEPAD_AXIS_EVENT;
#if _IRR_USE_SDL3_
			auto id = SDL_event.gaxis.which;
			irrevent.GamepadAxisEvent.Axis = static_cast<GamepadAxis>(SDL_event.gaxis.axis);
			irrevent.GamepadAxisEvent.Value = SDL_event.gaxis.value;
#else
			auto id = SDL_event.caxis.which;
			irrevent.GamepadAxisEvent.Axis = static_cast<GamepadAxis>(SDL_event.caxis.axis);
			irrevent.GamepadAxisEvent.Value = SDL_event.caxis.value;
#endif
			irrevent.GamepadAxisEvent.ID = id;
			recentGamepadID = id;
			postEventFromUser(irrevent);
			break;
		}
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

		default:
			break;
		} // end switch
		resetReceiveTextInputEvents();
	} // end while

	return !Close;
}

void CIrrDeviceSDL::updateSizeAndScale()
{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	int css_w = 0, css_h = 0, canvas_w = 0, canvas_h = 0;
	if (queryCanvasLayoutSize(&css_w, &css_h, &canvas_w, &canvas_h)) {
		ScaleX = 1.f;
		ScaleY = 1.f;
		// Subpixel CSS layout can report clientWidth off by 1px. Treating
		// that as a resize assigns canvas.width and clears the FBO.
		auto close = [](int a, int b) {
			return a == b || a == b - 1 || a == b + 1;
		};
		const bool engine_matches = close(css_w, (int)Width) && close(css_h, (int)Height);
		const bool backing_matches = close(canvas_w, css_w) && close(canvas_h, css_h);
		if (engine_matches && backing_matches)
			return;
		Width = static_cast<u32>(css_w);
		Height = static_cast<u32>(css_h);
		if (backing_matches)
			return;
		// One resize path only. SDL_SetWindowSize also assigns canvas.width,
		// so calling both would clear the drawing buffer twice.
		if (Window) {
			int window_w = 0, window_h = 0;
			SDL_GetWindowSize(Window, &window_w, &window_h);
			if (window_w != css_w || window_h != css_h)
				SDL_SetWindowSize(Window, css_w, css_h);
			else
				emscripten_set_canvas_element_size("#canvas", css_w, css_h);
		} else {
			emscripten_set_canvas_element_size("#canvas", css_w, css_h);
		}
		republishBrowserPageState();
		return;
	}
#endif
	int window_w, window_h;
	SDL_GetWindowSize(Window, &window_w, &window_h);

	int drawable_w, drawable_h;
	SDL_GetWindowSizeInPixels(Window, &drawable_w, &drawable_h);

	ScaleX = (float)drawable_w / (float)window_w;
	ScaleY = (float)drawable_h / (float)window_h;

	Width = drawable_w;
	Height = drawable_h;
}

std::string CIrrDeviceSDL::getVersionString() const
{
	char buf[32];
#ifdef _IRR_USE_SDL3_
	int ver = SDL_GetVersion();
	snprintf_irr(buf, sizeof(buf), "%d.%d.%d (%d.%d.%d)",
		// Version of the dynamic library
		SDL_VERSIONNUM_MAJOR(ver),
		SDL_VERSIONNUM_MINOR(ver),
		SDL_VERSIONNUM_MICRO(ver),
		// Version of the headers
		SDL_MAJOR_VERSION,
		SDL_MINOR_VERSION,
		SDL_MICRO_VERSION
	);
	return std::string(buf);
#else
	SDL_version ver{};
	SDL_GetVersion(&ver);
	snprintf_irr(buf, sizeof(buf), "%d.%d.%d%s",
		ver.major, ver.minor, ver.patch,
		// the SDL team seems to intentionally number sdl2-compat this way:
		// <https://github.com/libsdl-org/sdl2-compat/tags>
		ver.patch >= 50 ? " (compat)" : ""
	);
	return std::string(buf);
#endif
}

//! Get the display density in dots per inch.
float CIrrDeviceSDL::getDisplayDensity() const
{
	// assume 96 dpi
	return std::max(ScaleX * 96.0f, ScaleY * 96.0f);
}

void CIrrDeviceSDL::SwapWindow()
{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	// Context was created with emscripten_webgl_create_context +
	// explicitSwapControl. SDL_GL_SwapWindow does not present that path;
	// emscripten_webgl_commit_frame must blit the default framebuffer.
	if (g_webglContextLost) {
		// Nothing can be presented (and GL calls are no-ops). Idle instead of
		// spinning; the launcher overlay tells the player what happened.
		emscripten_thread_sleep(50);
		return;
	}
	const auto webgl = reinterpret_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(Context);
	if (webgl)
		emscripten_webgl_make_context_current(webgl);

	// Render pipeline steps may leave an intermediate FBO bound. Both commit
	// and transferToImageBitmap present the default framebuffer.
	if (VideoDriver)
		VideoDriver->setRenderTargetEx(nullptr, video::ECBF_NONE);

	if (g_offscreenRender) {
		if (g_debugLoseContext.exchange(false))
			luanti_offscreen_lose_context();
		// Paced by the display; bounded so that a page that stops
		// acknowledging cannot stall the engine.
		const double wait_start = emscripten_get_now();
		while (g_offscreenFramesSent - g_offscreenFramesAcked.load(std::memory_order_acquire) >=
					OFFSCREEN_MAX_FRAMES_IN_FLIGHT &&
				emscripten_get_now() - wait_start < OFFSCREEN_MAX_PRESENT_WAIT_MS)
			emscripten_thread_sleep(0.25);
		const int presented = luanti_offscreen_present(&g_offscreenFramesAcked);
		if (presented > 0) {
			g_offscreenFramesSent++;
		} else if (presented < 0) {
			g_webglContextLost = true;
			os::Printer::log("WebGL context lost; rendering paused", ELL_ERROR);
			notifyPageContextLost();
		}
		// Deliver DOM input and resize callbacks queued for this thread.
		// present.js dismisses the loading shell, so no hop for that either.
		emscripten_current_thread_process_queued_calls();
		return;
	}

	const EMSCRIPTEN_RESULT rc = emscripten_webgl_commit_frame();
	if (rc != EMSCRIPTEN_RESULT_SUCCESS) {
		static bool logged = false;
		if (!logged) {
			logged = true;
			os::Printer::log("emscripten_webgl_commit_frame failed", ELL_ERROR);
		}
	}

	// commit_frame is synchronous, so the frame has already been composited
	// when it returns. What the worker still owes is its own proxied-call
	// queue: DOM input and resize callbacks registered for this thread are
	// delivered through it. Drain it without the fixed 1 ms sleep this used to
	// be (Module.luantiWebGL.swapSleepMs / ?swapsleep=N restores a sleep for
	// comparison).
	static const int swap_sleep_ms = browserSwapSleepMs();
	if (swap_sleep_ms > 0)
		emscripten_thread_sleep(swap_sleep_ms);
	else
		emscripten_current_thread_process_queued_calls();

	// Only the launcher decides when to hide the loading shell (after the
	// first gameplay frame). Do not force opacity here — that hid load
	// progress and fought prepareCompositor.
	// This hop is synchronous, so stop making it once the launcher reports
	// the shell dismissed; it never comes back for the life of the page.
	static bool loading_shell_dismissed = false;
	if (!loading_shell_dismissed) {
		loading_shell_dismissed = MAIN_THREAD_EM_ASM_INT({
			var launcher = Module["luantiLauncher"];
			if (!launcher || typeof launcher["notifyFramePresented"] !== "function")
				return 0;
			launcher["notifyFramePresented"]();
			return launcher["getState"]()["loadingDismissed"] ? 1 : 0;
		});
	}
#else
	SDL_GL_SwapWindow(Window);
#endif
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

	SDL_Surface *surface =
#ifdef _IRR_USE_SDL3_
		SDL_CreateSurface(width, height, SDL_GetPixelFormatForMasks(
			32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000));
#else
		SDL_CreateRGBSurface(0, width, height,
			32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
#endif

	if (!surface) {
		os::Printer::log("Failed to create SDL suface", ELL_ERROR);
		return false;
	}

	SDL_LockSurface(surface);
	bool succ = img->copyToNoScaling(surface->pixels, width, height, video::ECF_A8R8G8B8, surface->pitch);
	SDL_UnlockSurface(surface);

	if (!succ) {
		os::Printer::log("Could not copy icon image. Is the format not ECF_A8R8G8B8?", ELL_ERROR);
		SDL_DestroySurface(surface);
		return false;
	}

	SDL_SetWindowIcon(Window, surface);

	SDL_DestroySurface(surface);

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
#ifdef _IRR_USE_SDL3_
			SDL_SetWindowResizable(Window, resize);
#else
			SDL_SetWindowResizable(Window, (SDL_bool)resize);
#endif
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
#ifdef _IRR_USE_SDL3_
			!SDL_GetWindowFullscreenMode(Window); // borderless fullscreen
#else
			(flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
#endif
}

#ifndef _IRR_USE_SDL3_
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
#endif

bool CIrrDeviceSDL::setFullscreen(bool fullscreen)
{
	if (!Window)
		return false;
#ifdef _IRR_USE_SDL3_
	bool success = SDL_SetWindowFullscreen(Window, fullscreen);
#else
	// The SDL wiki says that this may trigger SDL_RENDER_TARGETS_RESET, but
	// looking at the SDL source, this only happens with D3D, so it's not
	// relevant to us.
	bool success = SDL_SetWindowFullscreen(Window, getFullscreenFlag(fullscreen)) == 0;
#endif
	if (!success)
		os::Printer::log("SDL_SetWindowFullscreen failed", SDL_GetError(), ELL_ERROR);
	return success;
}

bool CIrrDeviceSDL::isWindowVisible() const
{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	// SDL's background events are unreliable in browsers and can leave
	// IsInBackground set while the tab is still visible. Game::run() skips
	// all drawing when this returns false, which leaves a blank canvas.
	return !browserPageHidden();
#else
	return !IsInBackground;
#endif
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
	if (browserPageHidden())
		return false;
#endif
	const u64 windowFlags = SDL_GetWindowFlags(Window);
#ifdef _IRR_USE_SDL3_
	return !(windowFlags & SDL_WINDOW_HIDDEN) && (windowFlags & SDL_WINDOW_INPUT_FOCUS);
#else
	return  (windowFlags & SDL_WINDOW_SHOWN)  && (windowFlags & SDL_WINDOW_INPUT_FOCUS);
#endif
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

bool CIrrDeviceSDL::showErrorMessageBox(SDL_Window *window, const char *title, const char *message)
{
#ifdef _IRR_EMSCRIPTEN_PLATFORM_
	// PROXY_TO_PTHREAD runs the engine off the browser UI thread. SDL's
	// Emscripten message box ends up calling alert(), which is missing in
	// workers and aborts the process after the real error is logged.
	MAIN_THREAD_EM_ASM({
		const title = UTF8ToString($0);
		const message = UTF8ToString($1);
		const text = title + "\n\n" + message;
		console.error(text);
		if (typeof alert === "function")
			alert(text);
	}, title, message);
	return true;
#else
	auto ret = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, window);
#ifdef _IRR_USE_SDL3_
	return ret;
#else
	return ret == 0;
#endif
#endif
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

#ifdef _IRR_USE_SDL3_
	const u32 sdl_key_A = SDLK_A;
#else
	const u32 sdl_key_A = SDLK_a;
#endif
	for (u32 i = 0; i <= ('Z' - 'A'); ++i)
		KeyMap.emplace(sdl_key_A + i, (EKEY_CODE)(KEY_KEY_A + i));

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

	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT));     // ECI_NORMAL
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));   // ECI_CROSS
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER));     // ECI_HAND
	Cursors.emplace_back(nullptr);                                               // ECI_HELP
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT));        // ECI_IBEAM
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED)); // ECI_NO
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT));        // ECI_WAIT
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE));        // ECI_SIZEALL
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE)); // ECI_SIZENESW
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE)); // ECI_SIZENWSE
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE));   // ECI_SIZENS
	Cursors.emplace_back(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE));   // ECI_SIZEWE
	Cursors.emplace_back(nullptr);                                               // ECI_UP
}

#endif // _IRR_COMPILE_WITH_SDL_DEVICE_
