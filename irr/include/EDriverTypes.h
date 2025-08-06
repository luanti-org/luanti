// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

namespace video
{

//! An enum for all types of drivers the Irrlicht Engine supports.
enum E_DRIVER_TYPE
{
	//! Null driver, useful for applications to run the engine without visualization.
	/** The null device is able to load textures, but does not
	render and display any graphics. */
	EDT_NULL,

	//! OpenGL device, available on most platforms.
	/** Performs hardware accelerated rendering of 3D and 2D
	primitives. */
	EDT_OPENGL,

	//! OpenGL-ES 2.x driver, for embedded and mobile systems
	/** Supports shaders etc. */
	EDT_OGLES2,

	//! WebGL1 friendly subset of OpenGL-ES 2.x driver for Emscripten
	EDT_WEBGL1,

	EDT_OPENGL3,

	//! No driver, just for counting the elements
	EDT_COUNT
};

} // end namespace video
