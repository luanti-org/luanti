// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

//! An enum for the different device types supported by the Irrlicht Engine.
enum E_DEVICE_TYPE
{
	EIDT_WIN32,

	EIDT_X11,

	EIDT_OSX,

	//! A device which uses Simple DirectMedia Layer
	/** The SDL device works under all platforms supported by SDL2 or SDL3.
	See also: CMake option 'USE_SDL3' */
	EIDT_SDL,

	//! This selection allows Irrlicht to choose the best device from the ones available.
	/** If this selection is chosen then Irrlicht will try to use the IrrlichtDevice native
	to your operating system. If this is unavailable then the X11, SDL and then console device
	will be tried. This ensures that Irrlicht will run even if your platform is unsupported,
	although it may not be able to render anything. */
	EIDT_BEST,

	EIDT_ANDROID,
};
