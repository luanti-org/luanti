// Copyright (C) 2005-2006 Etienne Petitjean
// Copyright (C) 2007-2012 Christian Stehno
// Copyright (C) 2013-2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#ifdef _IRR_COMPILE_WITH_OSX_DEVICE_

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>

#include "CIrrDeviceOSX.h"

#include "IEventReceiver.h"
#include "IVideoDriver.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include <stdio.h>
#include <sys/utsname.h>
#include "COSOperator.h"
#include "CColorConverter.h"
#include "irrlicht.h"
#include <algorithm>

#include <wchar.h>
#include <time.h>

#include "CNSOGLManager.h"

#if defined _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>

struct JoystickComponent
{
	IOHIDElementCookie cookie; // unique value which identifies element, will NOT change
	long min;                  // reported min value possible
	long max;                  // reported max value possible

	long minRead; // min read value
	long maxRead; // max read value

	JoystickComponent() :
			min(0), minRead(0), max(0), maxRead(0)
	{
	}
};

struct JoystickInfo
{
	core::array<JoystickComponent> axisComp;
	core::array<JoystickComponent> buttonComp;
	core::array<JoystickComponent> hatComp;

	int hats;
	int axes;
	int buttons;
	int numActiveJoysticks;

	SEvent persistentData;

	IOHIDDeviceInterface **interface;
	bool removed;
	char joystickName[256];
	long usage;     // usage page from IOUSBHID Parser.h which defines general usage
	long usagePage; // usage within above page from IOUSBHID Parser.h which defines specific usage

	JoystickInfo() :
			hats(0), axes(0), buttons(0), interface(0), removed(false), usage(0), usagePage(0), numActiveJoysticks(0)
	{
		interface = NULL;
		memset(joystickName, '\0', 256);
		axisComp.clear();
		buttonComp.clear();
		hatComp.clear();

		persistentData.EventType = EET_JOYSTICK_INPUT_EVENT;
		persistentData.JoystickEvent.POV = 65535;
		persistentData.JoystickEvent.ButtonStates = 0;
	}
};
core::array<JoystickInfo> ActiveJoysticks;

// helper functions for init joystick
static IOReturn closeJoystickDevice(JoystickInfo *joyInfo)
{
	IOReturn result = kIOReturnSuccess;
	if (joyInfo && joyInfo->interface) {
		/* close the interface */
		result = (*(joyInfo->interface))->close(joyInfo->interface);
		if (kIOReturnNotOpen == result) {
			/* do nothing as device was not opened, thus can't be closed */
		} else if (kIOReturnSuccess != result)
			os::Printer::log("IOHIDDeviceInterface failed to close", ELL_ERROR);
		/* release the interface */
		result = (*(joyInfo->interface))->Release(joyInfo->interface);
		if (kIOReturnSuccess != result)
			os::Printer::log("IOHIDDeviceInterface failed to release", ELL_ERROR);
		joyInfo->interface = NULL;
	}
	return result;
}

static void addComponentInfo(CFTypeRef refElement, JoystickComponent *pComponent, int numActiveJoysticks)
{
	long number;
	CFTypeRef refType;

	refType = CFDictionaryGetValue((CFDictionaryRef)refElement, CFSTR(kIOHIDElementCookieKey));
	if (refType && CFNumberGetValue((CFNumberRef)refType, kCFNumberLongType, &number))
		pComponent->cookie = (IOHIDElementCookie)number;
	refType = CFDictionaryGetValue((CFDictionaryRef)refElement, CFSTR(kIOHIDElementMinKey));
	if (refType && CFNumberGetValue((CFNumberRef)refType, kCFNumberLongType, &number))
		pComponent->minRead = pComponent->min = number;
	refType = CFDictionaryGetValue((CFDictionaryRef)refElement, CFSTR(kIOHIDElementMaxKey));
	if (refType && CFNumberGetValue((CFNumberRef)refType, kCFNumberLongType, &number))
		pComponent->maxRead = pComponent->max = number;
}

static void getJoystickComponentArrayHandler(const void *value, void *parameter);

static void addJoystickComponent(CFTypeRef refElement, JoystickInfo *joyInfo)
{
	long elementType, usagePage, usage;
	CFTypeRef refElementType = CFDictionaryGetValue((CFDictionaryRef)refElement, CFSTR(kIOHIDElementTypeKey));
	CFTypeRef refUsagePage = CFDictionaryGetValue((CFDictionaryRef)refElement, CFSTR(kIOHIDElementUsagePageKey));
	CFTypeRef refUsage = CFDictionaryGetValue((CFDictionaryRef)refElement, CFSTR(kIOHIDElementUsageKey));

	if ((refElementType) && (CFNumberGetValue((CFNumberRef)refElementType, kCFNumberLongType, &elementType))) {
		/* look at types of interest */
		if ((elementType == kIOHIDElementTypeInput_Misc) || (elementType == kIOHIDElementTypeInput_Button) ||
				(elementType == kIOHIDElementTypeInput_Axis)) {
			if (refUsagePage && CFNumberGetValue((CFNumberRef)refUsagePage, kCFNumberLongType, &usagePage) &&
					refUsage && CFNumberGetValue((CFNumberRef)refUsage, kCFNumberLongType, &usage)) {
				switch (usagePage) /* only interested in kHIDPage_GenericDesktop and kHIDPage_Button */
				{
				case kHIDPage_GenericDesktop: {
					switch (usage) /* look at usage to determine function */
					{
					case kHIDUsage_GD_X:
					case kHIDUsage_GD_Y:
					case kHIDUsage_GD_Z:
					case kHIDUsage_GD_Rx:
					case kHIDUsage_GD_Ry:
					case kHIDUsage_GD_Rz:
					case kHIDUsage_GD_Slider:
					case kHIDUsage_GD_Dial:
					case kHIDUsage_GD_Wheel: {
						joyInfo->axes++;
						JoystickComponent newComponent;
						addComponentInfo(refElement, &newComponent, joyInfo->numActiveJoysticks);
						joyInfo->axisComp.push_back(newComponent);
					} break;
					case kHIDUsage_GD_Hatswitch: {
						joyInfo->hats++;
						JoystickComponent newComponent;
						addComponentInfo(refElement, &newComponent, joyInfo->numActiveJoysticks);
						joyInfo->hatComp.push_back(newComponent);
					} break;
					}
				} break;
				case kHIDPage_Button: {
					joyInfo->buttons++;
					JoystickComponent newComponent;
					addComponentInfo(refElement, &newComponent, joyInfo->numActiveJoysticks);
					joyInfo->buttonComp.push_back(newComponent);
				} break;
				default:
					break;
				}
			}
		} else if (kIOHIDElementTypeCollection == elementType) {
			// get elements
			CFTypeRef refElementTop = CFDictionaryGetValue((CFMutableDictionaryRef)refElement, CFSTR(kIOHIDElementKey));
			if (refElementTop) {
				CFTypeID type = CFGetTypeID(refElementTop);
				if (type == CFArrayGetTypeID()) {
					CFRange range = {0, CFArrayGetCount((CFArrayRef)refElementTop)};
					CFArrayApplyFunction((CFArrayRef)refElementTop, range, getJoystickComponentArrayHandler, joyInfo);
				}
			}
		}
	}
}

static void getJoystickComponentArrayHandler(const void *value, void *parameter)
{
	if (CFGetTypeID(value) == CFDictionaryGetTypeID())
		addJoystickComponent((CFTypeRef)value, (JoystickInfo *)parameter);
}

static void joystickTopLevelElementHandler(const void *value, void *parameter)
{
	CFTypeRef refCF = 0;
	if (CFGetTypeID(value) != CFDictionaryGetTypeID())
		return;
	refCF = CFDictionaryGetValue((CFDictionaryRef)value, CFSTR(kIOHIDElementUsagePageKey));
	if (!CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &((JoystickInfo *)parameter)->usagePage))
		os::Printer::log("CFNumberGetValue error retrieving JoystickInfo->usagePage", ELL_ERROR);
	refCF = CFDictionaryGetValue((CFDictionaryRef)value, CFSTR(kIOHIDElementUsageKey));
	if (!CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &((JoystickInfo *)parameter)->usage))
		os::Printer::log("CFNumberGetValue error retrieving JoystickInfo->usage", ELL_ERROR);
}

static void getJoystickDeviceInfo(io_object_t hidDevice, CFMutableDictionaryRef hidProperties, JoystickInfo *joyInfo)
{
	CFMutableDictionaryRef usbProperties = 0;
	io_registry_entry_t parent1, parent2;

	/* Mac OS X currently is not mirroring all USB properties to HID page so need to look at USB device page also
	 * get dictionary for usb properties: step up two levels and get CF dictionary for USB properties
	 */
	if ((KERN_SUCCESS == IORegistryEntryGetParentEntry(hidDevice, kIOServicePlane, &parent1)) &&
			(KERN_SUCCESS == IORegistryEntryGetParentEntry(parent1, kIOServicePlane, &parent2)) &&
			(KERN_SUCCESS == IORegistryEntryCreateCFProperties(parent2, &usbProperties, kCFAllocatorDefault, kNilOptions))) {
		if (usbProperties) {
			CFTypeRef refCF = 0;
			/* get device info
			 * try hid dictionary first, if fail then go to usb dictionary
			 */

			/* get joystickName name */
			refCF = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDProductKey));
			if (!refCF)
				refCF = CFDictionaryGetValue(usbProperties, CFSTR("USB Product Name"));
			if (refCF) {
				if (!CFStringGetCString((CFStringRef)refCF, joyInfo->joystickName, 256, CFStringGetSystemEncoding()))
					os::Printer::log("CFStringGetCString error getting joyInfo->joystickName", ELL_ERROR);
			}

			/* get usage page and usage */
			refCF = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDPrimaryUsagePageKey));
			if (refCF) {
				if (!CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &joyInfo->usagePage))
					os::Printer::log("CFNumberGetValue error getting joyInfo->usagePage", ELL_ERROR);
				refCF = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDPrimaryUsageKey));
				if (refCF)
					if (!CFNumberGetValue((CFNumberRef)refCF, kCFNumberLongType, &joyInfo->usage))
						os::Printer::log("CFNumberGetValue error getting joyInfo->usage", ELL_ERROR);
			}

			if (NULL == refCF) /* get top level element HID usage page or usage */
			{
				/* use top level element instead */
				CFTypeRef refCFTopElement = 0;
				refCFTopElement = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDElementKey));
				{
					/* refCFTopElement points to an array of element dictionaries */
					CFRange range = {0, CFArrayGetCount((CFArrayRef)refCFTopElement)};
					CFArrayApplyFunction((CFArrayRef)refCFTopElement, range, joystickTopLevelElementHandler, joyInfo);
				}
			}

			CFRelease(usbProperties);
		} else
			os::Printer::log("IORegistryEntryCreateCFProperties failed to create usbProperties", ELL_ERROR);

		if (kIOReturnSuccess != IOObjectRelease(parent2))
			os::Printer::log("IOObjectRelease failed to release parent2", ELL_ERROR);
		if (kIOReturnSuccess != IOObjectRelease(parent1))
			os::Printer::log("IOObjectRelease failed to release parent1", ELL_ERROR);
	}
}

#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

// Contents from Events.h from Carbon/HIToolbox but we need it with Cocoa too
// and for some reason no Cocoa equivalent of these constants seems provided.
// So I'm doing like everyone else and using copy-and-paste.

/*
 *  Summary:
 *	Virtual keycodes
 *
 *  Discussion:
 *	These constants are the virtual keycodes defined originally in
 *	Inside Mac Volume V, pg. V-191. They identify physical keys on a
 *	keyboard. Those constants with "ANSI" in the name are labeled
 *	according to the key position on an ANSI-standard US keyboard.
 *	For example, kVK_ANSI_A indicates the virtual keycode for the key
 *	with the letter 'A' in the US keyboard layout. Other keyboard
 *	layouts may have the 'A' key label on a different physical key;
 *	in this case, pressing 'A' will generate a different virtual
 *	keycode.
 */
enum
{
	kVK_ANSI_A = 0x00,
	kVK_ANSI_S = 0x01,
	kVK_ANSI_D = 0x02,
	kVK_ANSI_F = 0x03,
	kVK_ANSI_H = 0x04,
	kVK_ANSI_G = 0x05,
	kVK_ANSI_Z = 0x06,
	kVK_ANSI_X = 0x07,
	kVK_ANSI_C = 0x08,
	kVK_ANSI_V = 0x09,
	kVK_ANSI_B = 0x0B,
	kVK_ANSI_Q = 0x0C,
	kVK_ANSI_W = 0x0D,
	kVK_ANSI_E = 0x0E,
	kVK_ANSI_R = 0x0F,
	kVK_ANSI_Y = 0x10,
	kVK_ANSI_T = 0x11,
	kVK_ANSI_1 = 0x12,
	kVK_ANSI_2 = 0x13,
	kVK_ANSI_3 = 0x14,
	kVK_ANSI_4 = 0x15,
	kVK_ANSI_6 = 0x16,
	kVK_ANSI_5 = 0x17,
	kVK_ANSI_Equal = 0x18,
	kVK_ANSI_9 = 0x19,
	kVK_ANSI_7 = 0x1A,
	kVK_ANSI_Minus = 0x1B,
	kVK_ANSI_8 = 0x1C,
	kVK_ANSI_0 = 0x1D,
	kVK_ANSI_RightBracket = 0x1E,
	kVK_ANSI_O = 0x1F,
	kVK_ANSI_U = 0x20,
	kVK_ANSI_LeftBracket = 0x21,
	kVK_ANSI_I = 0x22,
	kVK_ANSI_P = 0x23,
	kVK_ANSI_L = 0x25,
	kVK_ANSI_J = 0x26,
	kVK_ANSI_Quote = 0x27,
	kVK_ANSI_K = 0x28,
	kVK_ANSI_Semicolon = 0x29,
	kVK_ANSI_Backslash = 0x2A,
	kVK_ANSI_Comma = 0x2B,
	kVK_ANSI_Slash = 0x2C,
	kVK_ANSI_N = 0x2D,
	kVK_ANSI_M = 0x2E,
	kVK_ANSI_Period = 0x2F,
	kVK_ANSI_Grave = 0x32,
	kVK_ANSI_KeypadDecimal = 0x41,
	kVK_ANSI_KeypadMultiply = 0x43,
	kVK_ANSI_KeypadPlus = 0x45,
	kVK_ANSI_KeypadClear = 0x47,
	kVK_ANSI_KeypadDivide = 0x4B,
	kVK_ANSI_KeypadEnter = 0x4C,
	kVK_ANSI_KeypadMinus = 0x4E,
	kVK_ANSI_KeypadEquals = 0x51,
	kVK_ANSI_Keypad0 = 0x52,
	kVK_ANSI_Keypad1 = 0x53,
	kVK_ANSI_Keypad2 = 0x54,
	kVK_ANSI_Keypad3 = 0x55,
	kVK_ANSI_Keypad4 = 0x56,
	kVK_ANSI_Keypad5 = 0x57,
	kVK_ANSI_Keypad6 = 0x58,
	kVK_ANSI_Keypad7 = 0x59,
	kVK_ANSI_Keypad8 = 0x5B,
	kVK_ANSI_Keypad9 = 0x5C
};

/* keycodes for keys that are independent of keyboard layout*/
enum
{
	kVK_Return = 0x24,
	kVK_Tab = 0x30,
	kVK_Space = 0x31,
	kVK_Delete = 0x33,
	kVK_Escape = 0x35,
	kVK_Command = 0x37,
	kVK_Shift = 0x38,
	kVK_CapsLock = 0x39,
	kVK_Option = 0x3A,
	kVK_Control = 0x3B,
	kVK_RightShift = 0x3C,
	kVK_RightOption = 0x3D,
	kVK_RightControl = 0x3E,
	kVK_Function = 0x3F,
	kVK_F17 = 0x40,
	kVK_VolumeUp = 0x48,
	kVK_VolumeDown = 0x49,
	kVK_Mute = 0x4A,
	kVK_F18 = 0x4F,
	kVK_F19 = 0x50,
	kVK_F20 = 0x5A,
	kVK_F5 = 0x60,
	kVK_F6 = 0x61,
	kVK_F7 = 0x62,
	kVK_F3 = 0x63,
	kVK_F8 = 0x64,
	kVK_F9 = 0x65,
	kVK_F11 = 0x67,
	kVK_F13 = 0x69,
	kVK_F16 = 0x6A,
	kVK_F14 = 0x6B,
	kVK_F10 = 0x6D,
	kVK_F12 = 0x6F,
	kVK_F15 = 0x71,
	kVK_Help = 0x72,
	kVK_Home = 0x73,
	kVK_PageUp = 0x74,
	kVK_ForwardDelete = 0x75,
	kVK_F4 = 0x76,
	kVK_End = 0x77,
	kVK_F2 = 0x78,
	kVK_PageDown = 0x79,
	kVK_F1 = 0x7A,
	kVK_LeftArrow = 0x7B,
	kVK_RightArrow = 0x7C,
	kVK_DownArrow = 0x7D,
	kVK_UpArrow = 0x7E
};

//------------------------------------------------------------------------------------------
Boolean GetDictionaryBoolean(CFDictionaryRef theDict, const void *key)
{
	// get a boolean from the dictionary
	Boolean value = false;
	CFBooleanRef boolRef;
	boolRef = (CFBooleanRef)CFDictionaryGetValue(theDict, key);
	if (boolRef != NULL)
		value = CFBooleanGetValue(boolRef);
	return value;
}
//------------------------------------------------------------------------------------------
long GetDictionaryLong(CFDictionaryRef theDict, const void *key)
{
	// get a long from the dictionary
	long value = 0;
	CFNumberRef numRef;
	numRef = (CFNumberRef)CFDictionaryGetValue(theDict, key);
	if (numRef != NULL)
		CFNumberGetValue(numRef, kCFNumberLongType, &value);
	return value;
}

static bool firstLaunch = true;

@implementation CIrrDelegateOSX {
	CIrrDeviceMacOSX *Device;
	bool Quit;
}

- (id)initWithDevice:(CIrrDeviceMacOSX *)device
{
	self = [super init];

	if (self)
		Device = device;

	Quit = false;

	return (self);
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
	Quit = false;
}

- (void)orderFrontStandardAboutPanel:(id)sender
{
	[NSApp orderFrontStandardAboutPanel:sender];
}

- (void)unhideAllApplications:(id)sender
{
	[NSApp unhideAllApplications:sender];
}

- (void)hide:(id)sender
{
	[NSApp hide:sender];
}

- (void)hideOtherApplications:(id)sender
{
	[NSApp hideOtherApplications:sender];
}

- (void)terminate:(id)sender
{
	Quit = true;
}

- (void)windowWillClose:(id)sender
{
	Device->setWindow(nil);
	Quit = true;
}

- (NSSize)windowWillResize:(NSWindow *)window toSize:(NSSize)proposedFrameSize
{
	if (Device->isResizable())
		return proposedFrameSize;
	else
		return [window frame].size;
}

- (void)windowDidResize:(NSNotification *)aNotification
{
	NSWindow *window;
	NSRect frame;

	window = [aNotification object];
	frame = [window frame];
	Device->setResize((int)frame.size.width, (int)frame.size.height);
}

- (BOOL)isQuit
{
	return (Quit);
}

@end

//! constructor
CIrrDeviceMacOSX::CIrrDeviceMacOSX(const SIrrlichtCreationParameters &param) :
		CIrrDeviceStub(param), Window(NULL), Display(NULL),
		DeviceWidth(0), DeviceHeight(0),
		ScreenWidth(0), ScreenHeight(0), MouseButtonStates(0),
		IsActive(true), IsFullscreen(false), IsShiftDown(false), IsControlDown(false), IsResizable(false)
{
	struct utsname name;

	if (firstLaunch) {
		firstLaunch = false;

		if (!CreationParams.WindowId) {
			[[NSAutoreleasePool alloc] init];
			[[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
			[[NSApplication sharedApplication] setDelegate:[[[CIrrDelegateOSX alloc] initWithDevice:this] autorelease]];

			// Create menu

			NSString *bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
			if (bundleName == nil)
				bundleName = @"Irrlicht";

			NSMenu *mainMenu = [[[NSMenu alloc] initWithTitle:@"MainMenu"] autorelease];
			NSMenu *menu = [[[NSMenu alloc] initWithTitle:bundleName] autorelease];
			NSMenuItem *menuItem = [mainMenu addItemWithTitle:bundleName action:nil keyEquivalent:@""];
			[mainMenu setSubmenu:menu forItem:menuItem];
			menuItem = [menu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
			[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask];

			[NSApp setMainMenu:mainMenu];

			[NSApp finishLaunching];
		}

		NSString *path = [[NSBundle mainBundle] bundlePath];
		if (path != nil) {
			path = [path stringByAppendingString:@"/Contents/Resources"];
			chdir([path fileSystemRepresentation]);
			[path release];
		}
	}

	uname(&name);
	Operator = new COSOperator(name.version);
	os::Printer::log(name.version, ELL_INFORMATION);

	initKeycodes();

	bool success = true;

	if (CreationParams.DriverType != video::EDT_NULL)
		success = createWindow();

	// in case of failure, one can check VideoDriver for initialization
	if (!success)
		return;

	setResizable(false);
	CursorControl = new CCursorControl(CreationParams.WindowSize, this);

	createDriver();
	createGUIAndScene();
}

CIrrDeviceMacOSX::~CIrrDeviceMacOSX()
{
	[NSApp setPresentationOptions:(NSApplicationPresentationDefault)];
	closeDevice();
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	for (u32 joystick = 0; joystick < ActiveJoysticks.size(); ++joystick) {
		if (ActiveJoysticks[joystick].interface)
			closeJoystickDevice(&ActiveJoysticks[joystick]);
	}
#endif
}

void CIrrDeviceMacOSX::closeDevice()
{
	if (Window != nil) {
		[Window setIsVisible:FALSE];
		[Window setReleasedWhenClosed:TRUE];
		[Window release];
		Window = nil;
	}

	IsFullscreen = false;
	IsActive = false;
}

bool CIrrDeviceMacOSX::createWindow()
{
	CGDisplayErr error;
	bool result = false;
	Display = CGMainDisplayID();

	CGRect displayRect;
	CGDisplayModeRef displaymode, olddisplaymode;

	ScreenWidth = (int)CGDisplayPixelsWide(Display);
	ScreenHeight = (int)CGDisplayPixelsHigh(Display);

	const NSBackingStoreType type = (CreationParams.DriverType == video::EDT_OPENGL) ? NSBackingStoreBuffered : NSBackingStoreNonretained;

	// TODO: fullscreen
	// if (!CreationParams.Fullscreen)
	{
		if (!CreationParams.WindowId) { // create another window when WindowId is null
			int x = (CreationParams.WindowPosition.X > 0) ? CreationParams.WindowPosition.X : 0;
			int y = (CreationParams.WindowPosition.Y > 0) ? CreationParams.WindowPosition.Y : 0;

			if (CreationParams.WindowPosition.Y > -1) {
				int screenHeight = [[[NSScreen screens] objectAtIndex:0] frame].size.height;
				y = screenHeight - y - CreationParams.WindowSize.Height;
			}

			Window = [[NSWindow alloc] initWithContentRect:NSMakeRect(x, y, CreationParams.WindowSize.Width, CreationParams.WindowSize.Height) styleMask:NSTitledWindowMask + NSClosableWindowMask + NSResizableWindowMask backing:type defer:FALSE];

			if (CreationParams.WindowPosition.X == -1 && CreationParams.WindowPosition.Y == -1)
				[Window center];
		}

		DeviceWidth = CreationParams.WindowSize.Width;
		DeviceHeight = CreationParams.WindowSize.Height;

		result = true;
	}

	if (result) {
		if (Window) {
			[Window setDelegate:(CIrrDelegateOSX *)[NSApp delegate]];
			[Window setAcceptsMouseMovedEvents:TRUE];
			[Window setIsVisible:TRUE];
			[Window makeKeyAndOrderFront:nil];
		}
	}

	return result;
}

void CIrrDeviceMacOSX::setResize(int width, int height)
{
	// set new window size
	DeviceWidth = width;
	DeviceHeight = height;

#if defined(_IRR_COMPILE_WITH_OPENGL_)
	// update the size of the opengl rendering context
	if (CreationParams.DriverType == video::EDT_OPENGL) {
		NSOpenGLContext *Context = (NSOpenGLContext *)ContextManager->getContext().OpenGLOSX.Context;

		if (Context)
			[Context update];
	}
#endif

	// resize the driver to the inner pane size
	if (Window) {
		NSRect driverFrame = [Window contentRectForFrameRect:[Window frame]];
		getVideoDriver()->OnResize(core::dimension2d<u32>((s32)driverFrame.size.width, (s32)driverFrame.size.height));
		DeviceWidth = (s32)driverFrame.size.width;
		DeviceHeight = (s32)driverFrame.size.height;
	} else
		getVideoDriver()->OnResize(core::dimension2d<u32>((s32)width, (s32)height));
}

void CIrrDeviceMacOSX::createDriver()
{
	switch (CreationParams.DriverType) {
	case video::EDT_OPENGL:
#ifdef _IRR_COMPILE_WITH_OPENGL_
	{
		video::SExposedVideoData data;
		data.OpenGLOSX.Window = Window;
		ContextManager = new video::CNSOGLManager();
		ContextManager->initialize(CreationParams, data);
		VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, ContextManager);
		if (!VideoDriver) {
			os::Printer::log("Could not create OpenGL driver.", ELL_ERROR);
		}

		if (Window) {
			[[Window contentView] setWantsBestResolutionOpenGLSurface:NO];
			[(NSOpenGLContext *)ContextManager->getContext().OpenGLOSX.Context setView:[Window contentView]];
		} else {
			[(NSView *)CreationParams.WindowId setWantsBestResolutionOpenGLSurface:NO];
			[(NSOpenGLContext *)ContextManager->getContext().OpenGLOSX.Context setView:(NSView *)CreationParams.WindowId];
		}
	}
#else
		os::Printer::log("No OpenGL support compiled in.", ELL_ERROR);
#endif
	break;

	case video::EDT_OPENGL3:
	case video::EDT_OGLES2:
		os::Printer::log("This driver is not available on OSX.", ELL_ERROR);
		break;

	case video::EDT_NULL:
		VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
		break;

	default:
		os::Printer::log("Unable to create video driver of unknown type.", ELL_ERROR);
		break;
	}
}

bool CIrrDeviceMacOSX::run()
{
	NSAutoreleasePool *Pool = [[NSAutoreleasePool alloc] init];

	NSEvent *event;
	SEvent ievent;

	os::Timer::tick();
	storeMouseLocation();

	event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
	if (event != nil) {
		bzero(&ievent, sizeof(ievent));

		switch ([(NSEvent *)event type]) {
		case NSKeyDown:
			postKeyEvent(event, ievent, true);
			break;

		case NSKeyUp:
			postKeyEvent(event, ievent, false);
			break;

		case NSFlagsChanged:
			ievent.EventType = EET_KEY_INPUT_EVENT;
			ievent.KeyInput.Shift = ([(NSEvent *)event modifierFlags] & NSShiftKeyMask) != 0;
			ievent.KeyInput.Control = ([(NSEvent *)event modifierFlags] & NSControlKeyMask) != 0;

			if (IsShiftDown != ievent.KeyInput.Shift) {
				ievent.KeyInput.Char = KEY_SHIFT;
				ievent.KeyInput.Key = KEY_SHIFT;
				ievent.KeyInput.PressedDown = ievent.KeyInput.Shift;

				IsShiftDown = ievent.KeyInput.Shift;

				postEventFromUser(ievent);
			}

			if (IsControlDown != ievent.KeyInput.Control) {
				ievent.KeyInput.Char = KEY_CONTROL;
				ievent.KeyInput.Key = KEY_CONTROL;
				ievent.KeyInput.PressedDown = ievent.KeyInput.Control;

				IsControlDown = ievent.KeyInput.Control;

				postEventFromUser(ievent);
			}

			[NSApp sendEvent:event];
			break;

		case NSLeftMouseDown:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			ievent.MouseInput.Event = EMIE_LMOUSE_PRESSED_DOWN;
			MouseButtonStates |= EMBSM_LEFT;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			postMouseEvent(event, ievent);
			break;

		case NSLeftMouseUp:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			MouseButtonStates &= !EMBSM_LEFT;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			ievent.MouseInput.Event = EMIE_LMOUSE_LEFT_UP;
			postMouseEvent(event, ievent);
			break;

		case NSOtherMouseDown:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			ievent.MouseInput.Event = EMIE_MMOUSE_PRESSED_DOWN;
			MouseButtonStates |= EMBSM_MIDDLE;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			postMouseEvent(event, ievent);
			break;

		case NSOtherMouseUp:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			MouseButtonStates &= !EMBSM_MIDDLE;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			ievent.MouseInput.Event = EMIE_MMOUSE_LEFT_UP;
			postMouseEvent(event, ievent);
			break;

		case NSMouseMoved:
		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			ievent.MouseInput.Event = EMIE_MOUSE_MOVED;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			postMouseEvent(event, ievent);
			break;

		case NSRightMouseDown:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			ievent.MouseInput.Event = EMIE_RMOUSE_PRESSED_DOWN;
			MouseButtonStates |= EMBSM_RIGHT;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			postMouseEvent(event, ievent);
			break;

		case NSRightMouseUp:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			ievent.MouseInput.Event = EMIE_RMOUSE_LEFT_UP;
			MouseButtonStates &= !EMBSM_RIGHT;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			postMouseEvent(event, ievent);
			break;

		case NSScrollWheel:
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			ievent.MouseInput.Event = EMIE_MOUSE_WHEEL;
			ievent.MouseInput.Wheel = [(NSEvent *)event deltaY];
			if (ievent.MouseInput.Wheel < 1.0f)
				ievent.MouseInput.Wheel *= 10.0f;
			else
				ievent.MouseInput.Wheel *= 5.0f;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			postMouseEvent(event, ievent);
			break;

		default:
			[NSApp sendEvent:event];
			break;
		}
	}

	pollJoysticks();

	[Pool release];

	return (![[NSApp delegate] isQuit] && IsActive);
}

//! Pause the current process for the minimum time allowed only to allow other processes to execute
void CIrrDeviceMacOSX::yield()
{
	struct timespec ts = {0, 0};
	nanosleep(&ts, NULL);
}

//! Pause execution and let other processes to run for a specified amount of time.
void CIrrDeviceMacOSX::sleep(u32 timeMs, bool pauseTimer = false)
{
	bool wasStopped = Timer ? Timer->isStopped() : true;

	struct timespec ts;
	ts.tv_sec = (time_t)(timeMs / 1000);
	ts.tv_nsec = (long)(timeMs % 1000) * 1000000;

	if (pauseTimer && !wasStopped)
		Timer->stop();

	nanosleep(&ts, NULL);

	if (pauseTimer && !wasStopped)
		Timer->start();
}

void CIrrDeviceMacOSX::setWindowCaption(const wchar_t *text)
{
	if (Window != NULL) {
		if (text) {
			size_t numBytes = wcslen(text) * sizeof(wchar_t);

#ifdef __BIG_ENDIAN__
			NSStringEncoding encode = sizeof(wchar_t) == 4 ? NSUTF32BigEndianStringEncoding : NSUTF16BigEndianStringEncoding;
#else
			NSStringEncoding encode = sizeof(wchar_t) == 4 ? NSUTF32LittleEndianStringEncoding : NSUTF16LittleEndianStringEncoding;
#endif
			NSString *name = [[NSString alloc] initWithBytes:text length:numBytes encoding:encode];
			if (name) {
				[Window setTitle:name];
				[name release];
			}
		} else {
			[Window setTitle:@""];
		}
	}
}

bool CIrrDeviceMacOSX::isWindowActive() const
{
	return (IsActive);
}

bool CIrrDeviceMacOSX::isWindowFocused() const
{
	if (Window != NULL)
		return [Window isKeyWindow];
	return false;
}

bool CIrrDeviceMacOSX::isWindowMinimized() const
{
	if (Window != NULL)
		return [Window isMiniaturized];
	return false;
}

void CIrrDeviceMacOSX::postKeyEvent(void *event, SEvent &ievent, bool pressed)
{
	NSString *str;
	std::map<int, int>::const_iterator iter;
	unsigned int c, mkey, mchar;
	const unsigned char *cStr;
	BOOL skipCommand;

	str = [(NSEvent *)event characters];
	if ((str != nil) && ([str length] > 0)) {
		mkey = mchar = 0;
		skipCommand = false;
		c = [str characterAtIndex:0];
		mchar = c;

		iter = KeyCodes.find([(NSEvent *)event keyCode]);
		if (iter != KeyCodes.end())
			mkey = (*iter).second;
		else if ((iter = KeyCodes.find(c)) != KeyCodes.end())
			mkey = (*iter).second;
		else {
			// workaround for period character
			if (c == 0x2E) {
				mkey = KEY_PERIOD;
				mchar = '.';
			} else {
				cStr = (unsigned char *)[str cStringUsingEncoding:NSWindowsCP1252StringEncoding];
				if (cStr != NULL && strlen((char *)cStr) > 0) {
					mchar = cStr[0];
					mkey = toupper(mchar);
					if ([(NSEvent *)event modifierFlags] & NSCommandKeyMask) {
						if (mkey == 'C' || mkey == 'V' || mkey == 'X') {
							mchar = 0;
							skipCommand = true;
						}
					}
				}
			}
		}

		ievent.EventType = EET_KEY_INPUT_EVENT;
		ievent.KeyInput.Key = (EKEY_CODE)mkey;
		ievent.KeyInput.PressedDown = pressed;
		ievent.KeyInput.Shift = ([(NSEvent *)event modifierFlags] & NSShiftKeyMask) != 0;
		ievent.KeyInput.Control = ([(NSEvent *)event modifierFlags] & NSControlKeyMask) != 0;
		ievent.KeyInput.Char = mchar;

		if (skipCommand)
			ievent.KeyInput.Control = true;
		else if ([(NSEvent *)event modifierFlags] & NSCommandKeyMask)
			[NSApp sendEvent:(NSEvent *)event];

		postEventFromUser(ievent);
	}
}

void CIrrDeviceMacOSX::postMouseEvent(void *event, SEvent &ievent)
{
	bool post = true;

	if (Window != NULL) {
		ievent.MouseInput.X = (int)[(NSEvent *)event locationInWindow].x;
		ievent.MouseInput.Y = DeviceHeight - (int)[(NSEvent *)event locationInWindow].y;

		if (ievent.MouseInput.Y < 0)
			post = false;
	} else {
		CGEventRef ourEvent = CGEventCreate(NULL);
		CGPoint point = CGEventGetLocation(ourEvent);
		CFRelease(ourEvent);

		ievent.MouseInput.X = (int)point.x;
		ievent.MouseInput.Y = (int)point.y;

		if (ievent.MouseInput.Y < 0)
			post = false;
	}

	if (post) {
		ievent.MouseInput.Shift = ([(NSEvent *)event modifierFlags] & NSShiftKeyMask) != 0;
		ievent.MouseInput.Control = ([(NSEvent *)event modifierFlags] & NSControlKeyMask) != 0;

		postEventFromUser(ievent);
	}

	[NSApp sendEvent:(NSEvent *)event];
}

void CIrrDeviceMacOSX::storeMouseLocation()
{
	int x, y;

	if (Window != NULL) {
		NSPoint p;
		p = [NSEvent mouseLocation];
		p = [Window convertScreenToBase:p];
		x = (int)p.x;
		y = DeviceHeight - (int)p.y;
	} else {
		// Do we still need this?
		CGEventRef ourEvent = CGEventCreate(NULL);
		CGPoint point = CGEventGetLocation(ourEvent);
		CFRelease(ourEvent);

		x = (int)point.x;
		y = (int)point.y;

		const core::position2di &curr = ((CCursorControl *)CursorControl)->getPosition(true);
		if (curr.X != x || curr.Y != y) {
			SEvent ievent;
			ievent.EventType = EET_MOUSE_INPUT_EVENT;
			ievent.MouseInput.Event = EMIE_MOUSE_MOVED;
			ievent.MouseInput.X = x;
			ievent.MouseInput.Y = y;
			ievent.MouseInput.ButtonStates = MouseButtonStates;
			postEventFromUser(ievent);
		}
	}

	((CCursorControl *)CursorControl)->updateInternalCursorPosition(x, y);
}

void CIrrDeviceMacOSX::setMouseLocation(int x, int y)
{
	NSPoint p;
	CGPoint c;

	if (Window != NULL) {
		// Irrlicht window exists
		p.x = (float)x;
		p.y = (float)(DeviceHeight - y);
		p = [Window convertBaseToScreen:p];
		p.y = ScreenHeight - p.y;
	} else {
		p.x = (float)x;
		p.y = (float)y + (ScreenHeight - DeviceHeight);
	}

	c.x = p.x;
	c.y = p.y;

	CGWarpMouseCursorPosition(c);
	CGAssociateMouseAndMouseCursorPosition(YES);
}

void CIrrDeviceMacOSX::setCursorVisible(bool visible)
{
	if (visible)
		CGDisplayShowCursor(CGMainDisplayID());
	else
		CGDisplayHideCursor(CGMainDisplayID());
}

void CIrrDeviceMacOSX::setWindow(NSWindow *window)
{
	Window = window;
}

void CIrrDeviceMacOSX::initKeycodes()
{
	KeyCodes[kVK_UpArrow] = KEY_UP;
	KeyCodes[kVK_DownArrow] = KEY_DOWN;
	KeyCodes[kVK_LeftArrow] = KEY_LEFT;
	KeyCodes[kVK_RightArrow] = KEY_RIGHT;
	KeyCodes[kVK_F1] = KEY_F1;
	KeyCodes[kVK_F2] = KEY_F2;
	KeyCodes[kVK_F3] = KEY_F3;
	KeyCodes[kVK_F4] = KEY_F4;
	KeyCodes[kVK_F5] = KEY_F5;
	KeyCodes[kVK_F6] = KEY_F6;
	KeyCodes[kVK_F7] = KEY_F7;
	KeyCodes[kVK_F8] = KEY_F8;
	KeyCodes[kVK_F9] = KEY_F9;
	KeyCodes[kVK_F10] = KEY_F10;
	KeyCodes[kVK_F11] = KEY_F11;
	KeyCodes[kVK_F12] = KEY_F12;
	KeyCodes[kVK_F13] = KEY_F13;
	KeyCodes[kVK_F14] = KEY_F14;
	KeyCodes[kVK_F15] = KEY_F15;
	KeyCodes[kVK_F16] = KEY_F16;
	KeyCodes[kVK_F17] = KEY_F17;
	KeyCodes[kVK_F18] = KEY_F18;
	KeyCodes[kVK_F19] = KEY_F19;
	KeyCodes[kVK_F20] = KEY_F20;
	KeyCodes[kVK_Home] = KEY_HOME;
	KeyCodes[kVK_End] = KEY_END;
	KeyCodes[NSInsertFunctionKey] = KEY_INSERT;
	KeyCodes[kVK_ForwardDelete] = KEY_DELETE;
	KeyCodes[kVK_Help] = KEY_HELP;
	KeyCodes[NSSelectFunctionKey] = KEY_SELECT;
	KeyCodes[NSPrintFunctionKey] = KEY_PRINT;
	KeyCodes[NSExecuteFunctionKey] = KEY_EXECUT;
	KeyCodes[NSPrintScreenFunctionKey] = KEY_SNAPSHOT;
	KeyCodes[NSPauseFunctionKey] = KEY_PAUSE;
	KeyCodes[NSScrollLockFunctionKey] = KEY_SCROLL;
	KeyCodes[kVK_Delete] = KEY_BACK;
	KeyCodes[kVK_Tab] = KEY_TAB;
	KeyCodes[kVK_Return] = KEY_RETURN;
	KeyCodes[kVK_Escape] = KEY_ESCAPE;
	KeyCodes[kVK_Control] = KEY_CONTROL;
	KeyCodes[kVK_RightControl] = KEY_RCONTROL;
	KeyCodes[kVK_Command] = KEY_MENU;
	KeyCodes[kVK_Shift] = KEY_SHIFT;
	KeyCodes[kVK_RightShift] = KEY_RSHIFT;
	KeyCodes[kVK_Space] = KEY_SPACE;

	KeyCodes[kVK_ANSI_A] = KEY_KEY_A;
	KeyCodes[kVK_ANSI_B] = KEY_KEY_B;
	KeyCodes[kVK_ANSI_C] = KEY_KEY_C;
	KeyCodes[kVK_ANSI_D] = KEY_KEY_D;
	KeyCodes[kVK_ANSI_E] = KEY_KEY_E;
	KeyCodes[kVK_ANSI_F] = KEY_KEY_F;
	KeyCodes[kVK_ANSI_G] = KEY_KEY_G;
	KeyCodes[kVK_ANSI_H] = KEY_KEY_H;
	KeyCodes[kVK_ANSI_I] = KEY_KEY_I;
	KeyCodes[kVK_ANSI_J] = KEY_KEY_J;
	KeyCodes[kVK_ANSI_K] = KEY_KEY_K;
	KeyCodes[kVK_ANSI_L] = KEY_KEY_L;
	KeyCodes[kVK_ANSI_M] = KEY_KEY_M;
	KeyCodes[kVK_ANSI_N] = KEY_KEY_N;
	KeyCodes[kVK_ANSI_O] = KEY_KEY_O;
	KeyCodes[kVK_ANSI_P] = KEY_KEY_P;
	KeyCodes[kVK_ANSI_Q] = KEY_KEY_Q;
	KeyCodes[kVK_ANSI_R] = KEY_KEY_R;
	KeyCodes[kVK_ANSI_S] = KEY_KEY_S;
	KeyCodes[kVK_ANSI_T] = KEY_KEY_T;
	KeyCodes[kVK_ANSI_U] = KEY_KEY_U;
	KeyCodes[kVK_ANSI_V] = KEY_KEY_V;
	KeyCodes[kVK_ANSI_W] = KEY_KEY_W;
	KeyCodes[kVK_ANSI_X] = KEY_KEY_X;
	KeyCodes[kVK_ANSI_X] = KEY_KEY_X;
	KeyCodes[kVK_ANSI_Y] = KEY_KEY_Y;
	KeyCodes[kVK_ANSI_Z] = KEY_KEY_Z;

	KeyCodes[kVK_ANSI_0] = KEY_KEY_0;
	KeyCodes[kVK_ANSI_1] = KEY_KEY_1;
	KeyCodes[kVK_ANSI_2] = KEY_KEY_2;
	KeyCodes[kVK_ANSI_3] = KEY_KEY_3;
	KeyCodes[kVK_ANSI_4] = KEY_KEY_4;
	KeyCodes[kVK_ANSI_5] = KEY_KEY_5;
	KeyCodes[kVK_ANSI_6] = KEY_KEY_6;
	KeyCodes[kVK_ANSI_7] = KEY_KEY_7;
	KeyCodes[kVK_ANSI_8] = KEY_KEY_8;
	KeyCodes[kVK_ANSI_9] = KEY_KEY_9;

	KeyCodes[kVK_ANSI_Slash] = KEY_DIVIDE;
	KeyCodes[kVK_ANSI_Comma] = KEY_COMMA;
	KeyCodes[kVK_ANSI_Period] = KEY_PERIOD;
	KeyCodes[kVK_PageUp] = KEY_PRIOR;
	KeyCodes[kVK_PageDown] = KEY_NEXT;

	KeyCodes[kVK_ANSI_Keypad0] = KEY_NUMPAD0;
	KeyCodes[kVK_ANSI_Keypad1] = KEY_NUMPAD1;
	KeyCodes[kVK_ANSI_Keypad2] = KEY_NUMPAD2;
	KeyCodes[kVK_ANSI_Keypad3] = KEY_NUMPAD3;
	KeyCodes[kVK_ANSI_Keypad4] = KEY_NUMPAD4;
	KeyCodes[kVK_ANSI_Keypad5] = KEY_NUMPAD5;
	KeyCodes[kVK_ANSI_Keypad6] = KEY_NUMPAD6;
	KeyCodes[kVK_ANSI_Keypad7] = KEY_NUMPAD7;
	KeyCodes[kVK_ANSI_Keypad8] = KEY_NUMPAD8;
	KeyCodes[kVK_ANSI_Keypad9] = KEY_NUMPAD9;

	KeyCodes[kVK_ANSI_KeypadDecimal] = KEY_DECIMAL;
	KeyCodes[kVK_ANSI_KeypadMultiply] = KEY_MULTIPLY;
	KeyCodes[kVK_ANSI_KeypadPlus] = KEY_PLUS;
	KeyCodes[kVK_ANSI_KeypadClear] = KEY_OEM_CLEAR;
	KeyCodes[kVK_ANSI_KeypadDivide] = KEY_DIVIDE;
	KeyCodes[kVK_ANSI_KeypadEnter] = KEY_RETURN;
	KeyCodes[kVK_ANSI_KeypadMinus] = KEY_SUBTRACT;

	KeyCodes[kVK_ANSI_LeftBracket] = KEY_OEM_4;
	KeyCodes[kVK_ANSI_Backslash] = KEY_OEM_5;
	KeyCodes[kVK_ANSI_RightBracket] = KEY_OEM_6;
}

//! Sets if the window should be resizable in windowed mode.
void CIrrDeviceMacOSX::setResizable(bool resize)
{
	IsResizable = resize;
#if 0
	if (resize)
		[Window setStyleMask:NSTitledWindowMask|NSClosableWindowMask|NSMiniaturizableWindowMask|NSResizableWindowMask];
	else
		[Window setStyleMask:NSTitledWindowMask|NSClosableWindowMask];
#endif
}

bool CIrrDeviceMacOSX::isResizable() const
{
	return IsResizable;
}

void CIrrDeviceMacOSX::minimizeWindow()
{
	if (Window != NULL)
		[Window miniaturize:[NSApp self]];
}

//! Maximizes the window if possible.
void CIrrDeviceMacOSX::maximizeWindow()
{
	// todo: implement
}

//! get the window to normal size if possible.
void CIrrDeviceMacOSX::restoreWindow()
{
	[Window deminiaturize:[NSApp self]];
}

//! Get the position of this window on screen
core::position2di CIrrDeviceMacOSX::getWindowPosition()
{
	NSRect rect = [Window frame];
	int screenHeight = [[[NSScreen screens] objectAtIndex:0] frame].size.height;
	return core::position2di(rect.origin.x, screenHeight - rect.origin.y - rect.size.height);
}

#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
static void joystickRemovalCallback(void *target,
		IOReturn result, void *refcon, void *sender)
{
	JoystickInfo *joy = (JoystickInfo *)refcon;
	joy->removed = 1;
}
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

bool CIrrDeviceMacOSX::activateJoysticks(core::array<SJoystickInfo> &joystickInfo)
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	ActiveJoysticks.clear();
	joystickInfo.clear();

	io_object_t hidObject = 0;
	io_iterator_t hidIterator = 0;
	IOReturn result = kIOReturnSuccess;
	mach_port_t masterPort = 0;
	CFMutableDictionaryRef hidDictionaryRef = NULL;

	result = IOMasterPort(bootstrap_port, &masterPort);
	if (kIOReturnSuccess != result) {
		os::Printer::log("initialiseJoysticks IOMasterPort failed", ELL_ERROR);
		return false;
	}

	hidDictionaryRef = IOServiceMatching(kIOHIDDeviceKey);
	if (!hidDictionaryRef) {
		os::Printer::log("initialiseJoysticks IOServiceMatching failed", ELL_ERROR);
		return false;
	}
	result = IOServiceGetMatchingServices(masterPort, hidDictionaryRef, &hidIterator);

	if (kIOReturnSuccess != result) {
		os::Printer::log("initialiseJoysticks IOServiceGetMatchingServices failed", ELL_ERROR);
		return false;
	}

	// no joysticks just return
	if (!hidIterator)
		return false;

	u32 jindex = 0u;
	while ((hidObject = IOIteratorNext(hidIterator))) {
		JoystickInfo info;

		// get dictionary for HID properties
		CFMutableDictionaryRef hidProperties = 0;

		kern_return_t kern_result = IORegistryEntryCreateCFProperties(hidObject, &hidProperties, kCFAllocatorDefault, kNilOptions);
		if ((kern_result == KERN_SUCCESS) && hidProperties) {
			HRESULT plugInResult = S_OK;
			SInt32 score = 0;
			IOCFPlugInInterface **ppPlugInInterface = NULL;
			result = IOCreatePlugInInterfaceForService(hidObject, kIOHIDDeviceUserClientTypeID,
					kIOCFPlugInInterfaceID, &ppPlugInInterface, &score);
			if (kIOReturnSuccess == result) {
				plugInResult = (*ppPlugInInterface)->QueryInterface(ppPlugInInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), (void **)&(info.interface));
				if (plugInResult != S_OK)
					os::Printer::log("initialiseJoysticks query HID class device interface failed", ELL_ERROR);
				(*ppPlugInInterface)->Release(ppPlugInInterface);
			} else
				continue;

			if (info.interface != NULL) {
				result = (*(info.interface))->open(info.interface, 0);
				if (result == kIOReturnSuccess) {
					(*(info.interface))->setRemovalCallback(info.interface, joystickRemovalCallback, &info, &info);
					getJoystickDeviceInfo(hidObject, hidProperties, &info);

					// get elements
					CFTypeRef refElementTop = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDElementKey));
					if (refElementTop) {
						CFTypeID type = CFGetTypeID(refElementTop);
						if (type == CFArrayGetTypeID()) {
							CFRange range = {0, CFArrayGetCount((CFArrayRef)refElementTop)};
							info.numActiveJoysticks = ActiveJoysticks.size();
							CFArrayApplyFunction((CFArrayRef)refElementTop, range, getJoystickComponentArrayHandler, &info);
						}
					}
				} else {
					CFRelease(hidProperties);
					os::Printer::log("initialiseJoysticks Open interface failed", ELL_ERROR);
					continue;
				}

				CFRelease(hidProperties);

				result = IOObjectRelease(hidObject);

				if ((info.usagePage != kHIDPage_GenericDesktop) ||
						((info.usage != kHIDUsage_GD_Joystick &&
								info.usage != kHIDUsage_GD_GamePad &&
								info.usage != kHIDUsage_GD_MultiAxisController))) {
					closeJoystickDevice(&info);
					continue;
				}

				for (u32 i = 0; i < 6; ++i)
					info.persistentData.JoystickEvent.Axis[i] = 0;

				ActiveJoysticks.push_back(info);

				SJoystickInfo returnInfo;
				returnInfo.Joystick = jindex;
				returnInfo.Axes = info.axes;
				// returnInfo.Hats = info.hats;
				returnInfo.Buttons = info.buttons;
				returnInfo.Name = info.joystickName;
				returnInfo.PovHat = SJoystickInfo::POV_HAT_UNKNOWN;
				++jindex;

				// if (info.hatComp.size())
				//	returnInfo.PovHat = SJoystickInfo::POV_HAT_PRESENT;
				// else
				//	returnInfo.PovHat = SJoystickInfo::POV_HAT_ABSENT;

				joystickInfo.push_back(returnInfo);
			}

		} else {
			continue;
		}
	}
	result = IOObjectRelease(hidIterator);

	return true;
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_

	return false;
}

void CIrrDeviceMacOSX::pollJoysticks()
{
#if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
	if (0 == ActiveJoysticks.size())
		return;

	u32 joystick;
	for (joystick = 0; joystick < ActiveJoysticks.size(); ++joystick) {
		if (ActiveJoysticks[joystick].removed)
			continue;

		bool found = false;
		ActiveJoysticks[joystick].persistentData.JoystickEvent.Joystick = joystick;

		if (ActiveJoysticks[joystick].interface) {
			for (u32 n = 0; n < ActiveJoysticks[joystick].axisComp.size(); n++) {
				IOReturn result = kIOReturnSuccess;
				IOHIDEventStruct hidEvent;
				hidEvent.value = 0;
				result = (*(ActiveJoysticks[joystick].interface))->getElementValue(ActiveJoysticks[joystick].interface, ActiveJoysticks[joystick].axisComp[n].cookie, &hidEvent);
				if (kIOReturnSuccess == result) {
					const f32 min = -32768.0f;
					const f32 max = 32767.0f;
					const f32 deviceScale = max - min;
					const f32 readScale = (f32)ActiveJoysticks[joystick].axisComp[n].maxRead - (f32)ActiveJoysticks[joystick].axisComp[n].minRead;

					if (hidEvent.value < ActiveJoysticks[joystick].axisComp[n].minRead)
						ActiveJoysticks[joystick].axisComp[n].minRead = hidEvent.value;
					if (hidEvent.value > ActiveJoysticks[joystick].axisComp[n].maxRead)
						ActiveJoysticks[joystick].axisComp[n].maxRead = hidEvent.value;

					if (readScale != 0.0f)
						hidEvent.value = (int)(((f32)((f32)hidEvent.value - (f32)ActiveJoysticks[joystick].axisComp[n].minRead) * deviceScale / readScale) + min);

					if (ActiveJoysticks[joystick].persistentData.JoystickEvent.Axis[n] != (s16)hidEvent.value)
						found = true;
					ActiveJoysticks[joystick].persistentData.JoystickEvent.Axis[n] = (s16)hidEvent.value;
				}
			} // axis check

			for (u32 n = 0; n < ActiveJoysticks[joystick].buttonComp.size(); n++) {
				IOReturn result = kIOReturnSuccess;
				IOHIDEventStruct hidEvent;
				hidEvent.value = 0;
				result = (*(ActiveJoysticks[joystick].interface))->getElementValue(ActiveJoysticks[joystick].interface, ActiveJoysticks[joystick].buttonComp[n].cookie, &hidEvent);
				if (kIOReturnSuccess == result) {
					if (hidEvent.value && !((ActiveJoysticks[joystick].persistentData.JoystickEvent.ButtonStates & (1 << n)) ? true : false))
						found = true;
					else if (!hidEvent.value && ((ActiveJoysticks[joystick].persistentData.JoystickEvent.ButtonStates & (1 << n)) ? true : false))
						found = true;

					if (hidEvent.value)
						ActiveJoysticks[joystick].persistentData.JoystickEvent.ButtonStates |= (1 << n);
					else
						ActiveJoysticks[joystick].persistentData.JoystickEvent.ButtonStates &= ~(1 << n);
				}
			} // button check
			// still ToDo..will be done soon :)
			/*
						for (u32 n = 0; n < ActiveJoysticks[joystick].hatComp.size(); n++)
						{
							IOReturn result = kIOReturnSuccess;
							IOHIDEventStruct hidEvent;
							hidEvent.value = 0;
							result = (*(ActiveJoysticks[joystick].interface))->getElementValue(ActiveJoysticks[joystick].interface, ActiveJoysticks[joystick].hatComp[n].cookie, &hidEvent);
							if (kIOReturnSuccess == result)
							{
								if (ActiveJoysticks[joystick].persistentData.JoystickEvent.POV != hidEvent.value)
									found = true;
								ActiveJoysticks[joystick].persistentData.JoystickEvent.POV = hidEvent.value;
							}
						}//hat check
			*/
		}

		if (found)
			postEventFromUser(ActiveJoysticks[joystick].persistentData);
	}
#endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
}

#endif // _IRR_COMPILE_WITH_OSX_DEVICE_
