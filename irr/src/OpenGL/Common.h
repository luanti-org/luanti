// Copyright (C) 2023 Vitaliy Lobachevskiy
// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
// even though we have mt_opengl.h our driver code still uses GL_* constants
#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)
#include <SDL_video.h>
#include <SDL_opengl.h>
#else
#include "vendor/gl.h"
#endif

// macro used with COpenGL3DriverBase
#define TEST_GL_ERROR(cls) (cls)->testGLError(__FILE__, __LINE__)

namespace video
{

// Forward declarations.


template <class TOpenGLDriver>
class COpenGLCoreTexture;

template <class TOpenGLDriver, class TOpenGLTexture>
class COpenGLCoreRenderTarget;

template <class TOpenGLDriver, class TOpenGLTexture>
class COpenGLCoreCacheHandler;

class COpenGL3DriverBase;
typedef COpenGLCoreTexture<COpenGL3DriverBase> COpenGL3Texture;
typedef COpenGLCoreRenderTarget<COpenGL3DriverBase, COpenGL3Texture> COpenGL3RenderTarget;
typedef COpenGLCoreCacheHandler<COpenGL3DriverBase, COpenGL3Texture> COpenGL3CacheHandler;


enum OpenGLSpec : u8
{
	Core,
	Compat,
	ES,
	// WebGL, // TODO
};

struct OpenGLVersion
{
	OpenGLSpec Spec;
	u8 Major;
	u8 Minor;
	u8 Release;
};

}
