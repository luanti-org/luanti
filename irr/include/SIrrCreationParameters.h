// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "EDriverTypes.h"
#include "EDeviceTypes.h"
#include "dimension2d.h"
#include "ILogger.h"
#include "position2d.h"
#include "path.h"

class IEventReceiver;

//! Structure for holding Irrlicht Device creation parameters.
/** This structure is used in the createDeviceEx() function. */
struct SIrrlichtCreationParameters
{
	//! Constructs a SIrrlichtCreationParameters structure with default values.
	SIrrlichtCreationParameters() :
			DeviceType(EIDT_BEST),
			DriverType(video::EDT_OPENGL),
			WindowSize(core::dimension2d<u32>(800, 600)),
			WindowPosition(core::position2di(-1, -1)),
			Bits(32),
			ZBufferBits(24),
			Fullscreen(false),
			WindowMaximized(false),
			WindowResizable(2),
			Stencilbuffer(true),
			Vsync(false),
			AntiAlias(0),
			WithAlphaChannel(false),
			Doublebuffer(true),
			Stereobuffer(false),
			EventReceiver(0),
			WindowId(0),
#ifdef _DEBUG
			LoggingLevel(ELL_DEBUG),
#else
			LoggingLevel(ELL_INFORMATION),
#endif
			PrivateData(0),
#ifdef IRR_MOBILE_PATHS
			OGLES2ShaderPath("media/Shaders/"),
#else
			OGLES2ShaderPath("../../media/Shaders/"),
#endif
			DriverDebug(false)
	{
	}

	//! Type of the device.
	/** This setting decides the windowing system used by the device, most device types are native
	to a specific operating system and so may not be available.
	EIDT_WIN32 is only available on Windows desktops,
	EIDT_COCOA is only available on Mac OSX,
	EIDT_X11 is available on Linux, Solaris, BSD and other operating systems which use X11,
	EIDT_SDL is available on most systems if compiled in,
	EIDT_BEST will select the best available device for your operating system.
	Default: EIDT_BEST. */
	E_DEVICE_TYPE DeviceType;

	//! Type of video driver used to render graphics.
	video::E_DRIVER_TYPE DriverType;

	//! Size of the window or the video mode in fullscreen mode. Default: 800x600
	core::dimension2d<u32> WindowSize;

	//! Position of the window on-screen. Default: (-1, -1) or centered.
	core::position2di WindowPosition;

	//! Minimum Bits per pixel of the color buffer in fullscreen mode. Ignored if windowed mode. Default: 32.
	u8 Bits;

	//! Minimum Bits per pixel of the depth buffer. Default: 24.
	u8 ZBufferBits;

	//! Should be set to true if the device should run in fullscreen.
	/** Otherwise the device runs in windowed mode. Default: false. */
	bool Fullscreen;

	//! Maximised window. (Only supported on SDL.) Default: false
	bool WindowMaximized;

	//! Should a non-fullscreen window be resizable.
	/** Might not be supported by all devices. Ignored when Fullscreen is true.
	Values: 0 = not resizable, 1 = resizable, 2 = system decides default itself
	Default: 2*/
	u8 WindowResizable;

	//! Specifies if the stencil buffer should be enabled.
	/** Set this to true, if you want the engine be able to draw
	stencil buffer shadows. Note that not all drivers are able to
	use the stencil buffer, hence it can be ignored during device
	creation. Without the stencil buffer no shadows will be drawn.
	Default: true. */
	bool Stencilbuffer;

	//! Specifies vertical synchronization.
	/** If set to true, the driver will wait for the vertical
	retrace period, otherwise not. May be silently ignored.
	Default: false */
	bool Vsync;

	//! Specifies if the device should use fullscreen anti aliasing
	/** Makes sharp/pixelated edges softer, but requires more
	performance. Also, 2D elements might look blurred with this
	switched on. The resulting rendering quality also depends on
	the hardware and driver you are using, your program might look
	different on different hardware with this. So if you are
	writing a game/application with AntiAlias switched on, it would
	be a good idea to make it possible to switch this option off
	again by the user.
	The value is the maximal antialiasing factor requested for
	the device. The creation method will automatically try smaller
	values if no window can be created with the given value.
	Value one is usually the same as 0 (disabled), but might be a
	special value on some platforms. On D3D devices it maps to
	NONMASKABLE.
	Default value: 0 - disabled */
	u8 AntiAlias;

	//! Whether the main framebuffer uses an alpha channel.
	/** In some situations it might be desirable to get a color
	buffer with an alpha channel, e.g. when rendering into a
	transparent window or overlay. If this flag is set the device
	tries to create a framebuffer with alpha channel.
	If this flag is set, only color buffers with alpha channel
	are considered. Otherwise, it depends on the actual hardware
	if the colorbuffer has an alpha channel or not.
	Default value: false */
	bool WithAlphaChannel;

	//! Whether the main framebuffer uses doublebuffering.
	/** This should be usually enabled, in order to avoid render
	artifacts on the visible framebuffer. However, it might be
	useful to use only one buffer on very small devices. If no
	doublebuffering is available, the drivers will fall back to
	single buffers. Default value: true */
	bool Doublebuffer;

	//! Specifies if the device should use stereo buffers
	/** Some high-end gfx cards support two framebuffers for direct
	support of stereoscopic output devices. If this flag is set the
	device tries to create a stereo context.
	Currently only supported by OpenGL.
	Default value: false */
	bool Stereobuffer;

	//! A user created event receiver.
	IEventReceiver *EventReceiver;

	//! Window Id.
	/** If this is set to a value other than 0, the Irrlicht Engine
	will be created in an already existing window.
	For Windows, set this to the HWND of the window you want.
	The windowSize and FullScreen options will be ignored when using
	the WindowId parameter. Default this is set to 0.
	To make Irrlicht run inside the custom window, you still will
	have to draw Irrlicht on your own. You can use this loop, as
	usual:
	\code
	while (device->run())
	{
		driver->beginScene(video::ECBF_COLOR | video::ECBF_DEPTH, 0);
		smgr->drawAll();
		driver->endScene();
	}
	\endcode
	Instead of this, you can also simply use your own message loop
	using GetMessage, DispatchMessage and whatever. Calling
	IrrlichtDevice::run() will cause Irrlicht to dispatch messages
	internally too.  You need not call Device->run() if you want to
	do your own message dispatching loop, but Irrlicht will not be
	able to fetch user input then and you have to do it on your own
	using the window messages, DirectInput, or whatever. Also,
	you'll have to increment the Irrlicht timer.
	An alternative, own message dispatching loop without
	device->run() would look like this:
	\code
	MSG msg;
	while (true)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				break;
		}

		// increase virtual timer time
		device->getTimer()->tick();

		// draw engine picture
		driver->beginScene(video::ECBF_COLOR | video::ECBF_DEPTH, 0);
		smgr->drawAll();
		driver->endScene();
	}
	\endcode
	However, there is no need to draw the picture this often. Just
	do it how you like. */
	void *WindowId;

	//! Specifies the logging level used in the logging interface.
	/** The default value is ELL_INFORMATION. You can access the ILogger interface
	later on from the IrrlichtDevice with getLogger() and set another level.
	But if you need more or less logging information already from device creation,
	then you have to change it here.
	*/
	ELOG_LEVEL LoggingLevel;

	//! Define some private data storage.
	/** Used when platform devices need access to OS specific data structures etc.
	This is only used for Android at the moment in order to access the native
	Java RE. */
	void *PrivateData;

	//! Set the path where default-shaders to simulate the fixed-function pipeline can be found.
	/** This is about the shaders which can be found in media/Shaders by default. It's only necessary
	to set when using OGL-ES 2.0 */
	io::path OGLES2ShaderPath;

	//! Enable debug and error checks in video driver.
	bool DriverDebug;
};
