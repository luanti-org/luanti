// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_OPENGL_

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_) && defined(IRR_PREFER_SDL_GL_HEADER)
#include <SDL_video.h>
#include <SDL_opengl.h>
#else
#include "vendor/gl.h"
#endif

#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT

// To check if this header is in the current compile unit (different GL driver implementations use different "GLCommon" headers in Irrlicht)
#define IRR_COMPILE_GL_COMMON

// macro used with COpenGLDriver
#define TEST_GL_ERROR(cls) (cls)->testGLError(__LINE__)

namespace video
{

// Forward declarations.

class COpenGLCoreFeature;

template <class TOpenGLDriver>
class COpenGLCoreTexture;

template <class TOpenGLDriver, class TOpenGLTexture>
class COpenGLCoreRenderTarget;

template <class TOpenGLDriver, class TOpenGLTexture>
class COpenGLCoreCacheHandler;

class COpenGLDriver;
typedef COpenGLCoreTexture<COpenGLDriver> COpenGLTexture;
typedef COpenGLCoreRenderTarget<COpenGLDriver, COpenGLTexture> COpenGLRenderTarget;
class COpenGLCacheHandler;

}

#endif
