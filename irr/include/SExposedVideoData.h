// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "dimension2d.h"

namespace video
{

//! structure for holding data describing a driver and operating system specific data.
/** This data can be retrieved by IVideoDriver::getExposedVideoData(). Use this with caution.
This only should be used to make it possible to extend the engine easily without
modification of its source. Note that this structure does not contain any valid data, if
you are using the software or the null device.
*/
struct SExposedVideoData
{
	//! Native handle of a secondary window to render into (an SDL_Window*
	//! for the SDL device). nullptr means the device's main window.
	void *Window = nullptr;

	//! Drawable size of Window in pixels. Ignored if Window is nullptr.
	core::dimension2du Size;
};

} // end namespace video
