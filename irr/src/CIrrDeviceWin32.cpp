// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifdef _IRR_COMPILE_WITH_WINDOWS_DEVICE_

#if defined(__STRICT_ANSI__)
#error Compiling with __STRICT_ANSI__ not supported. g++ does set this when compiling with -std=c++11 or -std=c++0x. Use instead -std=gnu++11 or -std=gnu++0x. Or use -U__STRICT_ANSI__ to disable strict ansi.
#endif

#include "CIrrDeviceWin32.h"
#include "IEventReceiver.h"
#include "os.h"

#include "CTimer.h"
#include "irrString.h"
#include "COSOperator.h"
#include "dimension2d.h"
#include "IGUISpriteBank.h"
#include "IVideoDriver.h"
#include <winuser.h>
#include "SExposedVideoData.h"

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
#include <mmsystem.h>
#include <regstr.h>
#ifdef _IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#endif
#endif

#if defined(_IRR_COMPILE_WITH_OGLES2_)
#include "CEGLManager.h"
#endif

#if defined(_IRR_COMPILE_WITH_WGL_MANAGER_)
#include "CWGLManager.h"
#endif

struct SJoystickWin32Control
{
	CIrrDeviceWin32 *Device;

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_) && defined(_IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_)
	IDirectInput8 *DirectInputDevice;
#endif
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	struct JoystickInfo
	{
		u32 Index;
#ifdef _IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_
		core::stringc Name;
		GUID guid;
		LPDIRECTINPUTDEVICE8 lpdijoy;
		DIDEVCAPS devcaps;
		u8 axisValid[8];
#else
		JOYCAPS Caps;
#endif
	};
	core::array<JoystickInfo> ActiveJoysticks;
#endif

	SJoystickWin32Control(CIrrDeviceWin32 *dev);
	~SJoystickWin32Control();

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_) && defined(_IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_)
	static BOOL CALLBACK EnumJoysticks(LPCDIDEVICEINSTANCE lpddi, LPVOID cp);
	void directInputAddJoystick(LPCDIDEVICEINSTANCE lpddi);
#endif

	void pollJoysticks();
	bool activateJoysticks(core::array<SJoystickInfo> &joystickInfo);
	core::stringc findJoystickName(int index, const JOYCAPS &caps) const;
};

SJoystickWin32Control::SJoystickWin32Control(CIrrDeviceWin32 *dev) :
		Device(dev)
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_) && defined(_IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_)
	DirectInputDevice = 0;
	if (DI_OK != (DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void **)&DirectInputDevice, NULL))) {
		os::Printer::log("Could not create DirectInput8 Object", ELL_WARNING);
		return;
	}
#endif
}

SJoystickWin32Control::~SJoystickWin32Control()
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_) && defined(_IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_)
	for (u32 joystick = 0; joystick < ActiveJoysticks.size(); ++joystick) {
		LPDIRECTINPUTDEVICE8 dev = ActiveJoysticks[joystick].lpdijoy;
		if (dev) {
			dev->Unacquire();
			dev->Release();
		}
	}

	if (DirectInputDevice)
		DirectInputDevice->Release();
#endif
}

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_) && defined(_IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_)
BOOL CALLBACK SJoystickWin32Control::EnumJoysticks(LPCDIDEVICEINSTANCE lpddi, LPVOID cp)
{
	SJoystickWin32Control *p = (SJoystickWin32Control *)cp;
	p->directInputAddJoystick(lpddi);
	return DIENUM_CONTINUE;
}
void SJoystickWin32Control::directInputAddJoystick(LPCDIDEVICEINSTANCE lpddi)
{
	// Get the GUID of the joystuck
	const GUID guid = lpddi->guidInstance;

	JoystickInfo activeJoystick;
	activeJoystick.Index = ActiveJoysticks.size();
	activeJoystick.guid = guid;
	activeJoystick.Name = lpddi->tszProductName;
	if (FAILED(DirectInputDevice->CreateDevice(guid, &activeJoystick.lpdijoy, NULL))) {
		os::Printer::log("Could not create DirectInput device", ELL_WARNING);
		return;
	}

	activeJoystick.devcaps.dwSize = sizeof(activeJoystick.devcaps);
	if (FAILED(activeJoystick.lpdijoy->GetCapabilities(&activeJoystick.devcaps))) {
		os::Printer::log("Could not create DirectInput device", ELL_WARNING);
		return;
	}

	if (FAILED(activeJoystick.lpdijoy->SetCooperativeLevel(Device->HWnd, DISCL_BACKGROUND | DISCL_EXCLUSIVE))) {
		os::Printer::log("Could not set DirectInput device cooperative level", ELL_WARNING);
		return;
	}

	if (FAILED(activeJoystick.lpdijoy->SetDataFormat(&c_dfDIJoystick2))) {
		os::Printer::log("Could not set DirectInput device data format", ELL_WARNING);
		return;
	}

	if (FAILED(activeJoystick.lpdijoy->Acquire())) {
		os::Printer::log("Could not set DirectInput cooperative level", ELL_WARNING);
		return;
	}

	DIJOYSTATE2 info;
	if (FAILED(activeJoystick.lpdijoy->GetDeviceState(sizeof(info), &info))) {
		os::Printer::log("Could not read DirectInput device state", ELL_WARNING);
		return;
	}

	ZeroMemory(activeJoystick.axisValid, sizeof(activeJoystick.axisValid));
	activeJoystick.axisValid[0] = (info.lX != 0) ? 1 : 0;
	activeJoystick.axisValid[1] = (info.lY != 0) ? 1 : 0;
	activeJoystick.axisValid[2] = (info.lZ != 0) ? 1 : 0;
	activeJoystick.axisValid[3] = (info.lRx != 0) ? 1 : 0;
	activeJoystick.axisValid[4] = (info.lRy != 0) ? 1 : 0;
	activeJoystick.axisValid[5] = (info.lRz != 0) ? 1 : 0;

	int caxis = 0;
	for (u8 i = 0; i < 6; i++) {
		if (activeJoystick.axisValid[i])
			caxis++;
	}

	for (u8 i = 0; i < (activeJoystick.devcaps.dwAxes) - caxis; i++) {
		if (i + caxis < 8)
			activeJoystick.axisValid[i + caxis] = 1;
	}

	ActiveJoysticks.push_back(activeJoystick);
}
#endif

void SJoystickWin32Control::pollJoysticks()
{
#if defined _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
#ifdef _IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_
	if (0 == ActiveJoysticks.size())
		return;

	u32 joystick;
	DIJOYSTATE2 info;

	for (joystick = 0; joystick < ActiveJoysticks.size(); ++joystick) {
		// needs to be reset for each joystick
		// request ALL values and POV as continuous if possible

		const DIDEVCAPS &caps = ActiveJoysticks[joystick].devcaps;
		// if no POV is available don't ask for POV values

		if (!FAILED(ActiveJoysticks[joystick].lpdijoy->GetDeviceState(sizeof(info), &info))) {
			SEvent event;

			event.EventType = EET_JOYSTICK_INPUT_EVENT;
			event.JoystickEvent.Joystick = (u8)joystick;

			event.JoystickEvent.POV = (u16)info.rgdwPOV[0];
			// set to undefined if no POV value was returned or the value
			// is out of range
			if ((caps.dwPOVs == 0) || (event.JoystickEvent.POV > 35900))
				event.JoystickEvent.POV = 65535;

			for (int axis = 0; axis < SEvent::SJoystickEvent::NUMBER_OF_AXES; ++axis)
				event.JoystickEvent.Axis[axis] = 0;

			u16 dxAxis = 0;
			u16 irrAxis = 0;

			while (dxAxis < 6 && irrAxis < caps.dwAxes) {
				bool axisFound = 0;
				s32 axisValue = 0;

				switch (dxAxis) {
				case 0:
					axisValue = info.lX;
					break;
				case 1:
					axisValue = info.lY;
					break;
				case 2:
					axisValue = info.lZ;
					break;
				case 3:
					axisValue = info.lRx;
					break;
				case 4:
					axisValue = info.lRy;
					break;
				case 5:
					axisValue = info.lRz;
					break;
				case 6:
					axisValue = info.rglSlider[0];
					break;
				case 7:
					axisValue = info.rglSlider[1];
					break;
				default:
					break;
				}

				if (ActiveJoysticks[joystick].axisValid[dxAxis] > 0)
					axisFound = 1;

				if (axisFound) {
					s32 val = axisValue - 32768;

					if (val < -32767)
						val = -32767;
					if (val > 32767)
						val = 32767;
					event.JoystickEvent.Axis[irrAxis] = (s16)(val);
					irrAxis++;
				}

				dxAxis++;
			}

			u32 buttons = 0;
			BYTE *bytebuttons = info.rgbButtons;
			for (u16 i = 0; i < 32; i++) {
				if (bytebuttons[i] > 0) {
					buttons |= (1 << i);
				}
			}
			event.JoystickEvent.ButtonStates = buttons;

			(void)Device->postEventFromUser(event);
		}
	}
#else
	if (0 == ActiveJoysticks.size())
		return;

	u32 joystick;
	JOYINFOEX info;

	for (joystick = 0; joystick < ActiveJoysticks.size(); ++joystick) {
		// needs to be reset for each joystick
		// request ALL values and POV as continuous if possible
		info.dwSize = sizeof(info);
		info.dwFlags = JOY_RETURNALL | JOY_RETURNPOVCTS;
		const JOYCAPS &caps = ActiveJoysticks[joystick].Caps;
		// if no POV is available don't ask for POV values
		if (!(caps.wCaps & JOYCAPS_HASPOV))
			info.dwFlags &= ~(JOY_RETURNPOV | JOY_RETURNPOVCTS);
		if (JOYERR_NOERROR == joyGetPosEx(ActiveJoysticks[joystick].Index, &info)) {
			SEvent event;

			event.EventType = EET_JOYSTICK_INPUT_EVENT;
			event.JoystickEvent.Joystick = (u8)joystick;

			event.JoystickEvent.POV = (u16)info.dwPOV;
			// set to undefined if no POV value was returned or the value
			// is out of range
			if (!(info.dwFlags & JOY_RETURNPOV) || (event.JoystickEvent.POV > 35900))
				event.JoystickEvent.POV = 65535;

			for (int axis = 0; axis < SEvent::SJoystickEvent::NUMBER_OF_AXES; ++axis)
				event.JoystickEvent.Axis[axis] = 0;

			event.JoystickEvent.ButtonStates = info.dwButtons;

			switch (caps.wNumAxes) {
			default:
			case 6:
				event.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_V] =
						(s16)((65535 * (info.dwVpos - caps.wVmin)) / (caps.wVmax - caps.wVmin) - 32768);

			case 5:
				event.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_U] =
						(s16)((65535 * (info.dwUpos - caps.wUmin)) / (caps.wUmax - caps.wUmin) - 32768);

			case 4:
				event.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_R] =
						(s16)((65535 * (info.dwRpos - caps.wRmin)) / (caps.wRmax - caps.wRmin) - 32768);

			case 3:
				event.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Z] =
						(s16)((65535 * (info.dwZpos - caps.wZmin)) / (caps.wZmax - caps.wZmin) - 32768);

			case 2:
				event.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Y] =
						(s16)((65535 * (info.dwYpos - caps.wYmin)) / (caps.wYmax - caps.wYmin) - 32768);

			case 1:
				event.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_X] =
						(s16)((65535 * (info.dwXpos - caps.wXmin)) / (caps.wXmax - caps.wXmin) - 32768);
			}

			(void)Device->postEventFromUser(event);
		}
	}
#endif
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
}

/** This function is ported from SDL and released under zlib-license:
 * Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org> */
core::stringc SJoystickWin32Control::findJoystickName(int index, const JOYCAPS &caps) const
{
#if defined _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

	// As a default use the name given in the joystick structure.
	// It is always the same name, independent of joystick.
	core::stringc result(caps.szPname);

	core::stringc key = core::stringc(REGSTR_PATH_JOYCONFIG) + "\\" + caps.szRegKey + "\\" + REGSTR_KEY_JOYCURR;
	HKEY hTopKey = HKEY_LOCAL_MACHINE;
	HKEY hKey;
	long regresult = RegOpenKeyExA(hTopKey, key.c_str(), 0, KEY_READ, &hKey);
	if (regresult != ERROR_SUCCESS) {
		hTopKey = HKEY_CURRENT_USER;
		regresult = RegOpenKeyExA(hTopKey, key.c_str(), 0, KEY_READ, &hKey);
	}
	if (regresult != ERROR_SUCCESS)
		return result;

	/* find the registry key name for the joystick's properties */
	char regname[256];
	DWORD regsize = sizeof(regname);
	core::stringc regvalue = core::stringc("Joystick") + core::stringc(index + 1) + REGSTR_VAL_JOYOEMNAME;
	regresult = RegQueryValueExA(hKey, regvalue.c_str(), 0, 0, (LPBYTE)regname, &regsize);
	RegCloseKey(hKey);
	if (regresult != ERROR_SUCCESS)
		return result;

	/* open that registry key */
	core::stringc regkey = core::stringc(REGSTR_PATH_JOYOEM) + "\\" + regname;
	regresult = RegOpenKeyExA(hTopKey, regkey.c_str(), 0, KEY_READ, &hKey);
	if (regresult != ERROR_SUCCESS)
		return result;

	/* find the size for the OEM name text */
	regsize = sizeof(regvalue);
	regresult = RegQueryValueEx(hKey, REGSTR_VAL_JOYOEMNAME, 0, 0,
			NULL, &regsize);
	if (regresult == ERROR_SUCCESS) {
		char *name;
		/* allocate enough memory for the OEM name text ... */
		name = new char[regsize];
		if (name) {
			/* ... and read it from the registry */
			regresult = RegQueryValueEx(hKey, REGSTR_VAL_JOYOEMNAME, 0, 0,
					(LPBYTE)name, &regsize);
			result = name;
		}
		delete[] name;
	}
	RegCloseKey(hKey);

	return result;
#endif
	return "";
}

bool SJoystickWin32Control::activateJoysticks(core::array<SJoystickInfo> &joystickInfo)
{
#if defined _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
	joystickInfo.clear();
	ActiveJoysticks.clear();
#ifdef _IRR_COMPILE_WITH_DIRECTINPUT_JOYSTICK_
	if (!DirectInputDevice || (DirectInputDevice->EnumDevices(DI8DEVCLASS_GAMECTRL, SJoystickWin32Control::EnumJoysticks, this, DIEDFL_ATTACHEDONLY))) {
		os::Printer::log("Could not enum DirectInput8 controllers", ELL_WARNING);
		return false;
	}

	for (u32 joystick = 0; joystick < ActiveJoysticks.size(); ++joystick) {
		JoystickInfo &activeJoystick = ActiveJoysticks[joystick];
		SJoystickInfo info;
		info.Axes = activeJoystick.devcaps.dwAxes;
		info.Buttons = activeJoystick.devcaps.dwButtons;
		info.Name = activeJoystick.Name;
		info.PovHat = (activeJoystick.devcaps.dwPOVs != 0)
							  ? SJoystickInfo::POV_HAT_PRESENT
							  : SJoystickInfo::POV_HAT_ABSENT;
		joystickInfo.push_back(info);
	}
	return true;
#else
	const u32 numberOfJoysticks = ::joyGetNumDevs();
	JOYINFOEX info;
	info.dwSize = sizeof(info);
	info.dwFlags = JOY_RETURNALL;

	JoystickInfo activeJoystick;
	SJoystickInfo returnInfo;

	joystickInfo.reallocate(numberOfJoysticks);
	ActiveJoysticks.reallocate(numberOfJoysticks);

	u32 joystick = 0;
	for (; joystick < numberOfJoysticks; ++joystick) {
		if (JOYERR_NOERROR == joyGetPosEx(joystick, &info) &&
				JOYERR_NOERROR == joyGetDevCaps(joystick,
										  &activeJoystick.Caps,
										  sizeof(activeJoystick.Caps))) {
			activeJoystick.Index = joystick;
			ActiveJoysticks.push_back(activeJoystick);

			returnInfo.Joystick = (u8)joystick;
			returnInfo.Axes = activeJoystick.Caps.wNumAxes;
			returnInfo.Buttons = activeJoystick.Caps.wNumButtons;
			returnInfo.Name = findJoystickName(joystick, activeJoystick.Caps);
			returnInfo.PovHat = ((activeJoystick.Caps.wCaps & JOYCAPS_HASPOV) == JOYCAPS_HASPOV)
										? SJoystickInfo::POV_HAT_PRESENT
										: SJoystickInfo::POV_HAT_ABSENT;

			joystickInfo.push_back(returnInfo);
		}
	}

	for (joystick = 0; joystick < joystickInfo.size(); ++joystick) {
		char logString[256];
		snprintf_irr(logString, sizeof(logString), "Found joystick %d, %d axes, %d buttons '%s'",
				joystick, joystickInfo[joystick].Axes,
				joystickInfo[joystick].Buttons, joystickInfo[joystick].Name.c_str());
		os::Printer::log(logString, ELL_INFORMATION);
	}

	return true;
#endif
#else
	return false;
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
}

namespace
{
struct SEnvMapper
{
	HWND hWnd;
	CIrrDeviceWin32 *irrDev;
};
// NOTE: This is global. We can have more than one Irrlicht Device at same time.
core::array<SEnvMapper> EnvMap;

HKL KEYBOARD_INPUT_HKL = 0;
}

CIrrDeviceWin32 *getDeviceFromHWnd(HWND hWnd)
{
	const u32 end = EnvMap.size();
	for (u32 i = 0; i < end; ++i) {
		const SEnvMapper &env = EnvMap[i];
		if (env.hWnd == hWnd)
			return env.irrDev;
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

	CIrrDeviceWin32 *dev = 0;
	SEvent event;

	static s32 ClickCount = 0;
	if (GetCapture() != hWnd && ClickCount > 0)
		ClickCount = 0;

	struct messageMap
	{
		s32 group;
		UINT winMessage;
		s32 irrMessage;
	};

	static messageMap mouseMap[] = {
			{0, WM_LBUTTONDOWN, EMIE_LMOUSE_PRESSED_DOWN},
			{1, WM_LBUTTONUP, EMIE_LMOUSE_LEFT_UP},
			{0, WM_RBUTTONDOWN, EMIE_RMOUSE_PRESSED_DOWN},
			{1, WM_RBUTTONUP, EMIE_RMOUSE_LEFT_UP},
			{0, WM_MBUTTONDOWN, EMIE_MMOUSE_PRESSED_DOWN},
			{1, WM_MBUTTONUP, EMIE_MMOUSE_LEFT_UP},
			{2, WM_MOUSEMOVE, EMIE_MOUSE_MOVED},
			{3, WM_MOUSEWHEEL, EMIE_MOUSE_WHEEL},
			{-1, 0, 0},
		};

	// handle grouped events
	messageMap *m = mouseMap;
	while (m->group >= 0 && m->winMessage != message)
		m += 1;

	if (m->group >= 0) {
		if (m->group == 0) { // down
			ClickCount++;
			SetCapture(hWnd);
		} else if (m->group == 1) { // up
			ClickCount--;
			if (ClickCount < 1) {
				ClickCount = 0;
				ReleaseCapture();
			}
		}

		event.EventType = EET_MOUSE_INPUT_EVENT;
		event.MouseInput.Event = (EMOUSE_INPUT_EVENT)m->irrMessage;
		event.MouseInput.X = (short)LOWORD(lParam);
		event.MouseInput.Y = (short)HIWORD(lParam);
		event.MouseInput.Shift = ((LOWORD(wParam) & MK_SHIFT) != 0);
		event.MouseInput.Control = ((LOWORD(wParam) & MK_CONTROL) != 0);
		// left and right mouse buttons
		event.MouseInput.ButtonStates = wParam & (MK_LBUTTON | MK_RBUTTON);
		// middle and extra buttons
		if (wParam & MK_MBUTTON)
			event.MouseInput.ButtonStates |= EMBSM_MIDDLE;
		if (wParam & MK_XBUTTON1)
			event.MouseInput.ButtonStates |= EMBSM_EXTRA1;
		if (wParam & MK_XBUTTON2)
			event.MouseInput.ButtonStates |= EMBSM_EXTRA2;
		event.MouseInput.Wheel = 0.f;

		// wheel
		if (m->group == 3) {
			POINT p; // fixed by jox
			p.x = 0;
			p.y = 0;
			ClientToScreen(hWnd, &p);
			event.MouseInput.X -= p.x;
			event.MouseInput.Y -= p.y;
			event.MouseInput.Wheel = ((f32)((short)HIWORD(wParam))) / (f32)WHEEL_DELTA;
		}

		dev = getDeviceFromHWnd(hWnd);
		if (dev) {
			dev->postEventFromUser(event);

			if (event.MouseInput.Event >= EMIE_LMOUSE_PRESSED_DOWN && event.MouseInput.Event <= EMIE_MMOUSE_PRESSED_DOWN) {
				u32 clicks = dev->checkSuccessiveClicks(event.MouseInput.X, event.MouseInput.Y, event.MouseInput.Event);
				if (clicks == 2) {
					event.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_DOUBLE_CLICK + event.MouseInput.Event - EMIE_LMOUSE_PRESSED_DOWN);
					dev->postEventFromUser(event);
				} else if (clicks == 3) {
					event.MouseInput.Event = (EMOUSE_INPUT_EVENT)(EMIE_LMOUSE_TRIPLE_CLICK + event.MouseInput.Event - EMIE_LMOUSE_PRESSED_DOWN);
					dev->postEventFromUser(event);
				}
			}
		}
		return 0;
	}

	switch (message) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
	}
		return 0;

	case WM_ERASEBKGND:
		return 0;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		BYTE allKeys[256];

		event.EventType = EET_KEY_INPUT_EVENT;
		event.KeyInput.Key = (EKEY_CODE)wParam;
		event.KeyInput.PressedDown = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);

		if (event.KeyInput.Key == KEY_SHIFT) {
			event.KeyInput.Key = (EKEY_CODE)MapVirtualKey(((lParam >> 16) & 255), MAPVK_VSC_TO_VK_EX);
		}
		if (event.KeyInput.Key == KEY_CONTROL) {
			event.KeyInput.Key = (EKEY_CODE)MapVirtualKey(((lParam >> 16) & 255), MAPVK_VSC_TO_VK_EX);
			// some keyboards will just return LEFT for both - left and right keys. So also check extend bit.
			if (lParam & 0x1000000)
				event.KeyInput.Key = KEY_RCONTROL;
		}
		if (event.KeyInput.Key == KEY_MENU) {
			event.KeyInput.Key = (EKEY_CODE)MapVirtualKey(((lParam >> 16) & 255), MAPVK_VSC_TO_VK_EX);
			if (lParam & 0x1000000)
				event.KeyInput.Key = KEY_RMENU;
		}

		GetKeyboardState(allKeys);

		event.KeyInput.Shift = ((allKeys[VK_SHIFT] & 0x80) != 0);
		event.KeyInput.Control = ((allKeys[VK_CONTROL] & 0x80) != 0);

		// Handle unicode and deadkeys
		WCHAR keyChars[2];
		UINT scanCode = HIWORD(lParam);
		int conversionResult = ToUnicodeEx(static_cast<UINT>(wParam), scanCode, allKeys, keyChars, 2, 0, KEYBOARD_INPUT_HKL);
		if (conversionResult == 1)
			event.KeyInput.Char = keyChars[0];
		else
			event.KeyInput.Char = 0;

		// allow composing characters like '@' with Alt Gr on non-US keyboards
		if ((allKeys[VK_MENU] & 0x80) != 0)
			event.KeyInput.Control = 0;

		dev = getDeviceFromHWnd(hWnd);
		if (dev)
			dev->postEventFromUser(event);

		if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
			return DefWindowProcW(hWnd, message, wParam, lParam);
		else
			return 0;
	}

	case WM_SIZE: {
		// resize
		dev = getDeviceFromHWnd(hWnd);
		if (dev)
			dev->OnResized();
	}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SYSCOMMAND:
		// prevent screensaver or monitor powersave mode from starting
		if ((wParam & 0xFFF0) == SC_SCREENSAVE ||
				(wParam & 0xFFF0) == SC_MONITORPOWER ||
				(wParam & 0xFFF0) == SC_KEYMENU)
			return 0;

		break;

	case WM_USER:
		event.EventType = EET_USER_EVENT;
		event.UserEvent.UserData1 = static_cast<size_t>(wParam);
		event.UserEvent.UserData2 = static_cast<size_t>(lParam);
		dev = getDeviceFromHWnd(hWnd);

		if (dev)
			dev->postEventFromUser(event);

		return 0;

	case WM_SETCURSOR:
		// because Windows forgot about that in the meantime
		dev = getDeviceFromHWnd(hWnd);
		if (dev) {
			dev->getCursorControl()->setActiveIcon(dev->getCursorControl()->getActiveIcon());
			dev->getCursorControl()->setVisible(dev->getCursorControl()->isVisible());
		}
		break;

	case WM_INPUTLANGCHANGE:
		// get the new codepage used for keyboard input
		KEYBOARD_INPUT_HKL = GetKeyboardLayout(0);
		return 0;
	}
	return DefWindowProcW(hWnd, message, wParam, lParam);
}

//! constructor
CIrrDeviceWin32::CIrrDeviceWin32(const SIrrlichtCreationParameters &params) :
		CIrrDeviceStub(params), HWnd(0), Resized(false),
		ExternalWindow(false), Win32CursorControl(0), JoyControl(0),
		WindowMaximized(params.WindowMaximized)
{
	// get windows version and create OS operator
	core::stringc winversion;
	getWindowsVersion(winversion);
	Operator = new COSOperator(winversion);
	os::Printer::log(winversion.c_str(), ELL_INFORMATION);

	// get handle to exe file
	HINSTANCE hInstance = GetModuleHandle(0);

	// create the window if we need to and we do not use the null device
	if (!CreationParams.WindowId && CreationParams.DriverType != video::EDT_NULL) {
		const wchar_t *ClassName = L"CIrrDeviceWin32";

		// Register Class
		WNDCLASSEXW wcex;
		wcex.cbSize = sizeof(WNDCLASSEXW);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = NULL;
		wcex.hCursor = 0; // LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = 0;
		wcex.lpszClassName = ClassName;
		wcex.hIconSm = 0;

		RegisterClassExW(&wcex);

		// calculate client size

		RECT clientSize;
		clientSize.top = 0;
		clientSize.left = 0;
		clientSize.right = CreationParams.WindowSize.Width;
		clientSize.bottom = CreationParams.WindowSize.Height;

		DWORD style = getWindowStyle(CreationParams.Fullscreen, CreationParams.WindowResizable > 0 ? true : false);
		AdjustWindowRect(&clientSize, style, FALSE);

		const s32 realWidth = clientSize.right - clientSize.left;
		const s32 realHeight = clientSize.bottom - clientSize.top;

		s32 windowLeft = (CreationParams.WindowPosition.X == -1 ? (GetSystemMetrics(SM_CXSCREEN) - realWidth) / 2 : CreationParams.WindowPosition.X);
		s32 windowTop = (CreationParams.WindowPosition.Y == -1 ? (GetSystemMetrics(SM_CYSCREEN) - realHeight) / 2 : CreationParams.WindowPosition.Y);

		if (windowLeft < 0)
			windowLeft = 0;
		if (windowTop < 0)
			windowTop = 0; // make sure window menus are in screen on creation

		if (CreationParams.Fullscreen) {
			windowLeft = 0;
			windowTop = 0;
		}

		// create window
		HWnd = CreateWindowW(ClassName, L"", style, windowLeft, windowTop,
				realWidth, realHeight, NULL, NULL, hInstance, NULL);
		if (!HWnd) {
			os::Printer::log("Window could not be created.", ELL_ERROR);
		}

		CreationParams.WindowId = HWnd;
		//		CreationParams.WindowSize.Width = realWidth;
		//		CreationParams.WindowSize.Height = realHeight;

		ShowWindow(HWnd, SW_SHOWNORMAL);
		UpdateWindow(HWnd);

		// fix ugly ATI driver bugs. Thanks to ariaci
		MoveWindow(HWnd, windowLeft, windowTop, realWidth, realHeight, TRUE);

		// make sure everything gets updated to the real sizes
		Resized = true;
	} else if (CreationParams.WindowId) {
		// attach external window
		HWnd = static_cast<HWND>(CreationParams.WindowId);
		RECT r;
		GetWindowRect(HWnd, &r);
		CreationParams.WindowSize.Width = r.right - r.left;
		CreationParams.WindowSize.Height = r.bottom - r.top;
		CreationParams.Fullscreen = false;
		ExternalWindow = true;
	}

	// create cursor control

	Win32CursorControl = new CCursorControl(this, CreationParams.WindowSize, HWnd, CreationParams.Fullscreen);
	CursorControl = Win32CursorControl;
	JoyControl = new SJoystickWin32Control(this);

	// initialize doubleclicks with system values
	MouseMultiClicks.DoubleClickTime = GetDoubleClickTime();

	// create driver

	createDriver();

	if (VideoDriver)
		createGUIAndScene();

	// register environment

	SEnvMapper em;
	em.irrDev = this;
	em.hWnd = HWnd;
	EnvMap.push_back(em);

	// set this as active window
	if (!ExternalWindow) {
		SetActiveWindow(HWnd);
		SetForegroundWindow(HWnd);
	}

	KEYBOARD_INPUT_HKL = GetKeyboardLayout(0);

	// inform driver about the window size etc.
	resizeIfNecessary();

	if (params.WindowMaximized)
		maximizeWindow();
}

//! destructor
CIrrDeviceWin32::~CIrrDeviceWin32()
{
	delete JoyControl;

	// unregister environment
	for (u32 i = 0; i < EnvMap.size(); ++i) {
		if (EnvMap[i].hWnd == HWnd) {
			EnvMap.erase(i);
			break;
		}
	}
}

//! create the driver
void CIrrDeviceWin32::createDriver()
{
	switch (CreationParams.DriverType) {
	case video::EDT_OPENGL:
#ifdef _IRR_COMPILE_WITH_OPENGL_
		switchToFullScreen();

		ContextManager = new video::CWGLManager();
		ContextManager->initialize(CreationParams, video::SExposedVideoData(HWnd));
#endif
		VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, ContextManager);

		if (!VideoDriver)
			os::Printer::log("Could not create OpenGL driver.", ELL_ERROR);
		break;
	case video::EDT_OPENGL3:
#ifdef ENABLE_OPENGL3
		switchToFullScreen();

		ContextManager = new video::CWGLManager();
		ContextManager->initialize(CreationParams, video::SExposedVideoData(HWnd));
#endif
		VideoDriver = video::createOpenGL3Driver(CreationParams, FileSystem, ContextManager);

		if (!VideoDriver)
			os::Printer::log("Could not create OpenGL 3 driver.", ELL_ERROR);
		break;
	case video::EDT_OGLES2:
#ifdef _IRR_COMPILE_WITH_OGLES2_
		switchToFullScreen();

		ContextManager = new video::CEGLManager();
		ContextManager->initialize(CreationParams, video::SExposedVideoData(HWnd));
#endif
		VideoDriver = video::createOGLES2Driver(CreationParams, FileSystem, ContextManager);

		if (!VideoDriver)
			os::Printer::log("Could not create OpenGL-ES2 driver.", ELL_ERROR);
		break;
	case video::EDT_WEBGL1:
		os::Printer::log("WebGL1 driver not supported on Win32 device.", ELL_ERROR);
		break;
	case video::EDT_NULL:
		VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
		break;
	default:
		os::Printer::log("Unable to create video driver of unknown type.", ELL_ERROR);
		break;
	}
}

//! runs the device. Returns false if device wants to be deleted
bool CIrrDeviceWin32::run()
{
	os::Timer::tick();

	static_cast<CCursorControl *>(CursorControl)->update();

	handleSystemMessages();

	if (!Close)
		resizeIfNecessary();

	if (!Close && JoyControl)
		JoyControl->pollJoysticks();

	return !Close;
}

//! Pause the current process for the minimum time allowed only to allow other processes to execute
void CIrrDeviceWin32::yield()
{
	Sleep(0);
}

//! Pause execution and let other processes to run for a specified amount of time.
void CIrrDeviceWin32::sleep(u32 timeMs, bool pauseTimer)
{
	const bool wasStopped = Timer ? Timer->isStopped() : true;
	if (pauseTimer && !wasStopped)
		Timer->stop();

	Sleep(timeMs);

	if (pauseTimer && !wasStopped)
		Timer->start();
}

void CIrrDeviceWin32::resizeIfNecessary()
{
	if (!Resized || !getVideoDriver())
		return;

	RECT r;
	GetClientRect(HWnd, &r);

	char tmp[255];

	if (r.right < 2 || r.bottom < 2) {
		snprintf_irr(tmp, sizeof(tmp), "Ignoring resize operation to (%ld %ld)", r.right, r.bottom);
		os::Printer::log(tmp);
	} else {
		snprintf_irr(tmp, sizeof(tmp), "Resizing window (%ld %ld)", r.right, r.bottom);
		os::Printer::log(tmp);

		getVideoDriver()->OnResize(core::dimension2du((u32)r.right, (u32)r.bottom));
		getWin32CursorControl()->OnResize(getVideoDriver()->getScreenSize());
	}

	Resized = false;
}

DWORD CIrrDeviceWin32::getWindowStyle(bool fullscreen, bool resizable) const
{
	if (fullscreen)
		return WS_POPUP;

	if (resizable)
		return WS_THICKFRAME | WS_SYSMENU | WS_CAPTION | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

	return WS_BORDER | WS_SYSMENU | WS_CAPTION | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
}

//! sets the caption of the window
void CIrrDeviceWin32::setWindowCaption(const wchar_t *text)
{
	// We use SendMessage instead of SetText to ensure proper
	// function even in cases where the HWND was created in a different thread
	DWORD_PTR dwResult;
	SendMessageTimeoutW(HWnd, WM_SETTEXT, 0,
			reinterpret_cast<LPARAM>(text),
			SMTO_ABORTIFHUNG, 2000, &dwResult);
}

//! Sets the window icon.
bool CIrrDeviceWin32::setWindowIcon(const video::IImage *img)
{
	// Ignore the img, instead load the ICON from resource file
	// (This is minetest-specific!)
	const HICON hicon = LoadIcon(GetModuleHandle(NULL),
			MAKEINTRESOURCE(130) // The ID of the ICON defined in
								 // winresource.rc
	);

	if (hicon) {
		SendMessage(HWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hicon));
		SendMessage(HWnd, WM_SETICON, ICON_SMALL,
				reinterpret_cast<LPARAM>(hicon));
		return true;
	}
	return false;
}

//! notifies the device that it should close itself
void CIrrDeviceWin32::closeDevice()
{
	if (!ExternalWindow) {
		MSG msg;
		PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE);
		PostQuitMessage(0);
		PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE);
		DestroyWindow(HWnd);
		const wchar_t *ClassName = L"CIrrDeviceWin32";
		HINSTANCE hInstance = GetModuleHandle(0);
		UnregisterClassW(ClassName, hInstance);
	}
	Close = true;
}

//! returns if window is active. if not, nothing needs to be drawn
bool CIrrDeviceWin32::isWindowActive() const
{
	return (GetActiveWindow() == HWnd);
}

//! returns if window has focus
bool CIrrDeviceWin32::isWindowFocused() const
{
	bool ret = (GetFocus() == HWnd);
	return ret;
}

//! returns if window is minimized
bool CIrrDeviceWin32::isWindowMinimized() const
{
	WINDOWPLACEMENT plc;
	plc.length = sizeof(WINDOWPLACEMENT);
	bool ret = false;
	if (GetWindowPlacement(HWnd, &plc))
		ret = plc.showCmd == SW_SHOWMINIMIZED;
	return ret;
}

//! returns last state from maximizeWindow() and restoreWindow()
bool CIrrDeviceWin32::isWindowMaximized() const
{
	return WindowMaximized;
}

//! switches to fullscreen
bool CIrrDeviceWin32::switchToFullScreen()
{
	if (!CreationParams.Fullscreen)
		return true;

	// No border, title bar, etc. is already set up through getWindowStyle()
	// We only set the window size to match the monitor.

	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfo(MonitorFromWindow(HWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
		UINT flags = SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_FRAMECHANGED;
		SetWindowPos(HWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left,
				mi.rcMonitor.bottom - mi.rcMonitor.top, flags);
	} else {
		CreationParams.Fullscreen = false;
	}

	return CreationParams.Fullscreen;
}

//! returns the win32 cursor control
CIrrDeviceWin32::CCursorControl *CIrrDeviceWin32::getWin32CursorControl()
{
	return Win32CursorControl;
}

void CIrrDeviceWin32::getWindowsVersion(core::stringc &out)
{
	OSVERSIONINFO osvi;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);

	char tmp[255];
	snprintf(tmp, sizeof(tmp), "Microsoft Windows %lu.%lu %s", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.szCSDVersion);
	out.append(tmp);
}

//! Notifies the device, that it has been resized
void CIrrDeviceWin32::OnResized()
{
	Resized = true;
}

//! Resize the render window.
void CIrrDeviceWin32::setWindowSize(const core::dimension2d<u32> &size)
{
	if (ExternalWindow || !getVideoDriver() || CreationParams.Fullscreen)
		return;

	// get size of the window for the give size of the client area
	DWORD style = static_cast<DWORD>(GetWindowLongPtr(HWnd, GWL_STYLE));
	DWORD exStyle = static_cast<DWORD>(GetWindowLongPtr(HWnd, GWL_EXSTYLE));
	RECT clientSize;
	clientSize.top = 0;
	clientSize.left = 0;
	clientSize.right = size.Width;
	clientSize.bottom = size.Height;
	AdjustWindowRectEx(&clientSize, style, false, exStyle);
	const s32 realWidth = clientSize.right - clientSize.left;
	const s32 realHeight = clientSize.bottom - clientSize.top;

	UINT flags = SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER;
	SetWindowPos(HWnd, HWND_TOP, 0, 0, realWidth, realHeight, flags);
}

//! Sets if the window should be resizable in windowed mode.
void CIrrDeviceWin32::setResizable(bool resize)
{
	if (ExternalWindow || !getVideoDriver() || CreationParams.Fullscreen)
		return;

	LONG_PTR style = (LONG_PTR)getWindowStyle(false, resize);
	if (!SetWindowLongPtr(HWnd, GWL_STYLE, style))
		os::Printer::log("Could not change window style.");

	RECT clientSize;
	clientSize.top = 0;
	clientSize.left = 0;
	clientSize.right = getVideoDriver()->getScreenSize().Width;
	clientSize.bottom = getVideoDriver()->getScreenSize().Height;

	AdjustWindowRect(&clientSize, static_cast<DWORD>(style), FALSE);

	const s32 realWidth = clientSize.right - clientSize.left;
	const s32 realHeight = clientSize.bottom - clientSize.top;

	const s32 windowLeft = (GetSystemMetrics(SM_CXSCREEN) - realWidth) / 2;
	const s32 windowTop = (GetSystemMetrics(SM_CYSCREEN) - realHeight) / 2;

	SetWindowPos(HWnd, HWND_TOP, windowLeft, windowTop, realWidth, realHeight,
			SWP_FRAMECHANGED | SWP_NOMOVE | SWP_SHOWWINDOW);

	static_cast<CCursorControl *>(CursorControl)->updateBorderSize(CreationParams.Fullscreen, resize);
}

//! Minimizes the window.
void CIrrDeviceWin32::minimizeWindow()
{
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(HWnd, &wndpl);
	wndpl.showCmd = SW_SHOWMINNOACTIVE;
	SetWindowPlacement(HWnd, &wndpl);
}

//! Maximizes the window.
void CIrrDeviceWin32::maximizeWindow()
{
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(HWnd, &wndpl);
	wndpl.showCmd = SW_SHOWMAXIMIZED;
	SetWindowPlacement(HWnd, &wndpl);

	WindowMaximized = true;
}

//! Restores the window to its original size.
void CIrrDeviceWin32::restoreWindow()
{
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(HWnd, &wndpl);
	wndpl.showCmd = SW_SHOWNORMAL;
	SetWindowPlacement(HWnd, &wndpl);

	WindowMaximized = false;
}

core::position2di CIrrDeviceWin32::getWindowPosition()
{
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);
	if (GetWindowPlacement(HWnd, &wndpl)) {
		return core::position2di((int)wndpl.rcNormalPosition.left,
				(int)wndpl.rcNormalPosition.top);
	} else {
		// No reason for this to happen
		os::Printer::log("Failed to retrieve window location", ELL_ERROR);
		return core::position2di(-1, -1);
	}
}

bool CIrrDeviceWin32::activateJoysticks(core::array<SJoystickInfo> &joystickInfo)
{
	if (JoyControl)
		return JoyControl->activateJoysticks(joystickInfo);
	else
		return false;
}

//! Process system events
void CIrrDeviceWin32::handleSystemMessages()
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (ExternalWindow && msg.hwnd == HWnd) {
			if (msg.hwnd == HWnd) {
				WndProc(HWnd, msg.message, msg.wParam, msg.lParam);
			} else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		} else {
			// No message translation because we don't use WM_CHAR and it would conflict with our
			// deadkey handling.
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
			Close = true;
	}
}

//! Remove all messages pending in the system message loop
void CIrrDeviceWin32::clearSystemMessages()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
	}
	while (PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {
	}
}

//! Get the display density in dots per inch.
float CIrrDeviceWin32::getDisplayDensity() const
{
	HDC hdc = GetDC(HWnd);
	float dpi = GetDeviceCaps(hdc, LOGPIXELSX);
	ReleaseDC(HWnd, hdc);
	return dpi;
}

// Convert an Irrlicht texture to a Windows cursor
// Based on http://www.codeguru.com/cpp/w-p/win32/cursors/article.php/c4529/
HCURSOR CIrrDeviceWin32::TextureToCursor(HWND hwnd, video::ITexture *tex, const core::rect<s32> &sourceRect, const core::position2d<s32> &hotspot)
{
	//
	// create the bitmaps needed for cursors from the texture

	HDC dc = GetDC(hwnd);
	HDC andDc = CreateCompatibleDC(dc);
	HDC xorDc = CreateCompatibleDC(dc);
	HBITMAP andBitmap = CreateCompatibleBitmap(dc, sourceRect.getWidth(), sourceRect.getHeight());
	HBITMAP xorBitmap = CreateCompatibleBitmap(dc, sourceRect.getWidth(), sourceRect.getHeight());

	HBITMAP oldAndBitmap = (HBITMAP)SelectObject(andDc, andBitmap);
	HBITMAP oldXorBitmap = (HBITMAP)SelectObject(xorDc, xorBitmap);

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
				SetPixel(andDc, x, y, RGB(255, 255, 255));
				SetPixel(xorDc, x, y, RGB(0, 0, 0));
			} else // color
			{
				SetPixel(andDc, x, y, RGB(0, 0, 0));
				SetPixel(xorDc, x, y, RGB(pixelCol.getRed(), pixelCol.getGreen(), pixelCol.getBlue()));
			}
		}
		data += bytesRightGap;
	}
	tex->unlock();

	SelectObject(andDc, oldAndBitmap);
	SelectObject(xorDc, oldXorBitmap);

	DeleteDC(xorDc);
	DeleteDC(andDc);

	ReleaseDC(hwnd, dc);

	// create the cursor

	ICONINFO iconinfo;
	iconinfo.fIcon = false; // type is cursor not icon
	iconinfo.xHotspot = hotspot.X;
	iconinfo.yHotspot = hotspot.Y;
	iconinfo.hbmMask = andBitmap;
	iconinfo.hbmColor = xorBitmap;

	HCURSOR cursor = CreateIconIndirect(&iconinfo);

	DeleteObject(andBitmap);
	DeleteObject(xorBitmap);

	return cursor;
}

CIrrDeviceWin32::CCursorControl::CCursorControl(CIrrDeviceWin32 *device, const core::dimension2d<u32> &wsize, HWND hwnd, bool fullscreen) :
		Device(device), WindowSize(wsize), InvWindowSize(0.0f, 0.0f),
		HWnd(hwnd), BorderX(0), BorderY(0),
		UseReferenceRect(false), IsVisible(true), ActiveIcon(gui::ECI_NORMAL), ActiveIconStartTime(0)
{
	if (WindowSize.Width != 0)
		InvWindowSize.Width = 1.0f / WindowSize.Width;

	if (WindowSize.Height != 0)
		InvWindowSize.Height = 1.0f / WindowSize.Height;

	updateBorderSize(fullscreen, false);
	initCursors();
}

CIrrDeviceWin32::CCursorControl::~CCursorControl()
{
	for (u32 i = 0; i < Cursors.size(); ++i) {
		for (u32 f = 0; f < Cursors[i].Frames.size(); ++f) {
			DestroyCursor(Cursors[i].Frames[f].IconHW);
		}
	}
}

void CIrrDeviceWin32::CCursorControl::initCursors()
{
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_ARROW)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_CROSS)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_HAND)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_HELP)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_IBEAM)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_NO)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_WAIT)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_SIZEALL)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_SIZENESW)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_SIZENWSE)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_SIZENS)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_SIZEWE)));
	Cursors.push_back(CursorW32(LoadCursor(NULL, IDC_UPARROW)));
}

void CIrrDeviceWin32::CCursorControl::update()
{
	if (!Cursors[ActiveIcon].Frames.empty() && Cursors[ActiveIcon].FrameTime) {
		// update animated cursors. This could also be done by X11 in case someone wants to figure that out (this way was just easier to implement)
		u32 now = Device->getTimer()->getRealTime();
		u32 frame = ((now - ActiveIconStartTime) / Cursors[ActiveIcon].FrameTime) % Cursors[ActiveIcon].Frames.size();
		SetCursor(Cursors[ActiveIcon].Frames[frame].IconHW);
	}
}

//! Sets the active cursor icon
void CIrrDeviceWin32::CCursorControl::setActiveIcon(gui::ECURSOR_ICON iconId)
{
	if (iconId >= (s32)Cursors.size())
		return;

	ActiveIcon = iconId;
	ActiveIconStartTime = Device->getTimer()->getRealTime();
	if (Cursors[ActiveIcon].Frames.size())
		SetCursor(Cursors[ActiveIcon].Frames[0].IconHW);
}

//! Add a custom sprite as cursor icon.
gui::ECURSOR_ICON CIrrDeviceWin32::CCursorControl::addIcon(const gui::SCursorSprite &icon)
{
	if (icon.SpriteId >= 0) {
		CursorW32 cW32;
		cW32.FrameTime = icon.SpriteBank->getSprites()[icon.SpriteId].frameTime;

		for (u32 i = 0; i < icon.SpriteBank->getSprites()[icon.SpriteId].Frames.size(); ++i) {
			u32 texId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].textureNumber;
			u32 rectId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].rectNumber;
			core::rect<s32> rectIcon = icon.SpriteBank->getPositions()[rectId];

			HCURSOR hc = Device->TextureToCursor(HWnd, icon.SpriteBank->getTexture(texId), rectIcon, icon.HotSpot);
			cW32.Frames.push_back(CursorFrameW32(hc));
		}

		Cursors.push_back(cW32);
		return (gui::ECURSOR_ICON)(Cursors.size() - 1);
	}
	return gui::ECI_NORMAL;
}

//! replace the given cursor icon.
void CIrrDeviceWin32::CCursorControl::changeIcon(gui::ECURSOR_ICON iconId, const gui::SCursorSprite &icon)
{
	if (iconId >= (s32)Cursors.size())
		return;

	for (u32 i = 0; i < Cursors[iconId].Frames.size(); ++i)
		DestroyCursor(Cursors[iconId].Frames[i].IconHW);

	if (icon.SpriteId >= 0) {
		CursorW32 cW32;
		cW32.FrameTime = icon.SpriteBank->getSprites()[icon.SpriteId].frameTime;
		for (u32 i = 0; i < icon.SpriteBank->getSprites()[icon.SpriteId].Frames.size(); ++i) {
			u32 texId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].textureNumber;
			u32 rectId = icon.SpriteBank->getSprites()[icon.SpriteId].Frames[i].rectNumber;
			core::rect<s32> rectIcon = icon.SpriteBank->getPositions()[rectId];

			HCURSOR hc = Device->TextureToCursor(HWnd, icon.SpriteBank->getTexture(texId), rectIcon, icon.HotSpot);
			cW32.Frames.push_back(CursorFrameW32(hc));
		}

		Cursors[iconId] = cW32;
	}
}

//! Return a system-specific size which is supported for cursors. Larger icons will fail, smaller icons might work.
core::dimension2di CIrrDeviceWin32::CCursorControl::getSupportedIconSize() const
{
	core::dimension2di result;

	result.Width = GetSystemMetrics(SM_CXCURSOR);
	result.Height = GetSystemMetrics(SM_CYCURSOR);

	return result;
}

#endif // _IRR_COMPILE_WITH_WINDOWS_DEVICE_
