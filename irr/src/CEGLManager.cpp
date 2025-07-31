// Copyright (C) 2013 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "CEGLManager.h"

#ifdef _IRR_COMPILE_WITH_EGL_MANAGER_

#include "irrString.h"
#include "irrArray.h"
#include "os.h"

namespace video
{

CEGLManager::CEGLManager() :
		IContextManager(), EglWindow(0), EglDisplay(EGL_NO_DISPLAY),
		EglSurface(EGL_NO_SURFACE), EglContext(EGL_NO_CONTEXT), EglConfig(0), MajorVersion(0), MinorVersion(0)
{}

CEGLManager::~CEGLManager()
{
	destroyContext();
	destroySurface();
	terminate();
}

bool CEGLManager::initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &data)
{
	// store new data
	Params = params;
	Data = data;

	if (EglWindow != 0 && EglDisplay != EGL_NO_DISPLAY)
		return true;

		// Window is depend on platform.
#if defined(_IRR_COMPILE_WITH_WINDOWS_DEVICE_)
	EglWindow = (NativeWindowType)Data.OpenGLWin32.HWnd;
	Data.OpenGLWin32.HDc = GetDC((HWND)EglWindow);
	EglDisplay = eglGetDisplay((NativeDisplayType)Data.OpenGLWin32.HDc);
#elif defined(_IRR_EMSCRIPTEN_PLATFORM_)
	EglWindow = 0;
	EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#elif defined(_IRR_COMPILE_WITH_X11_DEVICE_)
	EglWindow = (NativeWindowType)Data.OpenGLLinux.X11Window;
	EglDisplay = eglGetDisplay((NativeDisplayType)Data.OpenGLLinux.X11Display);
#endif

	// We must check if EGL display is valid.
	if (EglDisplay == EGL_NO_DISPLAY) {
		os::Printer::log("Could not get EGL display.");
		terminate();
		return false;
	}

	// Initialize EGL here.
	if (!eglInitialize(EglDisplay, &MajorVersion, &MinorVersion)) {
		os::Printer::log("Could not initialize EGL display.");

		EglDisplay = EGL_NO_DISPLAY;
		terminate();
		return false;
	} else
		os::Printer::log("EGL version", core::stringc(MajorVersion + (MinorVersion * 0.1f)).c_str());

	return true;
}

void CEGLManager::terminate()
{
	if (EglWindow == 0 && EglDisplay == EGL_NO_DISPLAY)
		return;

	if (EglDisplay != EGL_NO_DISPLAY) {
		// We should unbind current EGL context before terminate EGL.
		eglMakeCurrent(EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		eglTerminate(EglDisplay);
		EglDisplay = EGL_NO_DISPLAY;
	}

#if defined(_IRR_COMPILE_WITH_WINDOWS_DEVICE_)
	if (Data.OpenGLWin32.HDc) {
		ReleaseDC((HWND)EglWindow, (HDC)Data.OpenGLWin32.HDc);
		Data.OpenGLWin32.HDc = 0;
	}
#endif

	MajorVersion = 0;
	MinorVersion = 0;
}

bool CEGLManager::generateSurface()
{
	if (EglDisplay == EGL_NO_DISPLAY)
		return false;

	if (EglSurface != EGL_NO_SURFACE)
		return true;

		// We should assign new WindowID on platforms, where WindowID may change at runtime,
		// at this time only Android support this feature.
		// this needs an update method instead!

#if defined(_IRR_EMSCRIPTEN_PLATFORM_)
	// eglChooseConfig is currently only implemented as stub in emscripten (version 1.37.22 at point of writing)
	// But the other solution would also be fine as it also only generates a single context so there is not much to choose from.
	EglConfig = chooseConfig(ECS_IRR_CHOOSE);
#else
	EglConfig = chooseConfig(ECS_EGL_CHOOSE_FIRST_LOWER_EXPECTATIONS);
#endif

	if (EglConfig == 0) {
		os::Printer::log("Could not get config for EGL display.");
		return false;
	}

	// Now we are able to create EGL surface.
	EglSurface = eglCreateWindowSurface(EglDisplay, EglConfig, EglWindow, 0);

	if (EGL_NO_SURFACE == EglSurface)
		EglSurface = eglCreateWindowSurface(EglDisplay, EglConfig, 0, 0);

	if (EGL_NO_SURFACE == EglSurface)
		os::Printer::log("Could not create EGL surface.");

#ifdef EGL_VERSION_1_2
	if (MinorVersion > 1)
		eglBindAPI(EGL_OPENGL_ES_API);
#endif

	if (Params.Vsync)
		eglSwapInterval(EglDisplay, 1);

	return true;
}

EGLConfig CEGLManager::chooseConfig(EConfigStyle confStyle)
{
	EGLConfig configResult = 0;

	// Find proper OpenGL BIT.
	EGLint eglOpenGLBIT = 0;
	switch (Params.DriverType) {
	case EDT_OGLES2:
	case EDT_WEBGL1:
		eglOpenGLBIT = EGL_OPENGL_ES2_BIT;
		break;
	default:
		break;
	}

	if (confStyle == ECS_EGL_CHOOSE_FIRST_LOWER_EXPECTATIONS) {
		EGLint Attribs[] = {
				EGL_RED_SIZE, 8,
				EGL_GREEN_SIZE, 8,
				EGL_BLUE_SIZE, 8,
				EGL_ALPHA_SIZE, Params.WithAlphaChannel ? 1 : 0,
				EGL_BUFFER_SIZE, Params.Bits,
				EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
				EGL_DEPTH_SIZE, Params.ZBufferBits,
				EGL_STENCIL_SIZE, Params.Stencilbuffer,
				EGL_SAMPLE_BUFFERS, Params.AntiAlias ? 1 : 0,
				EGL_SAMPLES, Params.AntiAlias,
#ifdef EGL_VERSION_1_3
				EGL_RENDERABLE_TYPE, eglOpenGLBIT,
#endif
				EGL_NONE, 0,
			};

		EGLint numConfigs = 0;
		u32 steps = 5;

		// Choose the best EGL config.
		// TODO: We should also have a confStyle ECS_EGL_CHOOSE_CLOSEST
		//       which doesn't take first result of eglChooseConfigs,
		//       but the closest to requested parameters. eglChooseConfigs
		//       can return more than 1 result and first one might have
		//       "better" values than requested (more bits per pixel etc).
		//       So this returns the config which can do most, not the
		//       config which is closest to the requested parameters.
		//
		while (!eglChooseConfig(EglDisplay, Attribs, &configResult, 1, &numConfigs) || !numConfigs) {
			switch (steps) {
			case 5:                  // samples
				if (Attribs[19] > 2) // Params.AntiAlias
					--Attribs[19];
				else {
					Attribs[17] = 0; // Params.Stencilbuffer
					Attribs[19] = 0; // Params.AntiAlias
					--steps;
				}
				break;
			case 4:             // alpha
				if (Attribs[7]) { // Params.WithAlphaChannel
					Attribs[7] = 0;

					if (Params.AntiAlias) {
						Attribs[17] = 1;
						Attribs[19] = Params.AntiAlias;
						steps = 5;
					}
				} else
					--steps;
				break;
			case 3:              // stencil
				if (Attribs[15]) { // Params.Stencilbuffer
					Attribs[15] = 0;

					if (Params.AntiAlias) {
						Attribs[17] = 1;
						Attribs[19] = Params.AntiAlias;
						steps = 5;
					}
				} else
					--steps;
				break;
			case 2:                   // depth size
				if (Attribs[13] > 16) { // Params.ZBufferBits
					Attribs[13] -= 8;
				} else
					--steps;
				break;
			case 1:                  // buffer size
				if (Attribs[9] > 16) { // Params.Bits
					Attribs[9] -= 8;
				} else
					--steps;
				break;
			default:
				return 0;
			}
		}

		if (Params.AntiAlias && !Attribs[17])
			os::Printer::log("No multisampling.");

		if (Params.WithAlphaChannel && !Attribs[7])
			os::Printer::log("No alpha.");

		if (Params.Stencilbuffer && !Attribs[15])
			os::Printer::log("No stencil buffer.");

		if (Params.ZBufferBits > Attribs[13])
			os::Printer::log("No full depth buffer.");

		if (Params.Bits > Attribs[9])
			os::Printer::log("No full color buffer.");
	} else if (confStyle == ECS_IRR_CHOOSE) {
		// find number of available configs
		EGLint numConfigs;
		if (eglGetConfigs(EglDisplay, NULL, 0, &numConfigs) == EGL_FALSE) {
			testEGLError();
			return 0;
		}

		if (numConfigs <= 0)
			return 0;

		// Get all available configs.
		EGLConfig *configs = new EGLConfig[numConfigs];
		if (eglGetConfigs(EglDisplay, configs, numConfigs, &numConfigs) == EGL_FALSE) {
			testEGLError();
			return 0;
		}

		// Find the best one.
		core::array<SConfigRating> ratings((u32)numConfigs);
		for (u32 i = 0; i < (u32)numConfigs; ++i) {
			SConfigRating r;
			r.config = configs[i];
			r.rating = rateConfig(r.config, eglOpenGLBIT);

			if (r.rating >= 0)
				ratings.push_back(r);
		}

		if (ratings.size() > 0) {
			ratings.sort();
			configResult = ratings[0].config;

			if (ratings[0].rating != 0) {
				// This is just to print some log info (it also rates again while doing that, but rating is cheap enough, so that doesn't matter here).
				rateConfig(ratings[0].config, eglOpenGLBIT, true);
			}
		}

		delete[] configs;
	}

	return configResult;
}

s32 CEGLManager::rateConfig(EGLConfig config, EGLint eglOpenGLBIT, bool log)
{
	// some values must be there or we ignore the config
#ifdef EGL_VERSION_1_3
	EGLint attribRenderableType = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_RENDERABLE_TYPE, &attribRenderableType);
	if (attribRenderableType != eglOpenGLBIT) {
		if (log)
			os::Printer::log("EGL_RENDERABLE_TYPE != eglOpenGLBIT");
		return -1;
	}
#endif
	EGLint attribSurfaceType = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_SURFACE_TYPE, &attribSurfaceType);
	if (attribSurfaceType != EGL_WINDOW_BIT) {
		if (log)
			os::Printer::log("EGL_SURFACE_TYPE!= EGL_WINDOW_BIT");
		return -1;
	}

	// Generally we give a really bad rating if attributes are worse than requested
	// We give a slight worse rating if attributes are not exact as requested
	// And we use some priorities which might make sense (but not really fine-tuned,
	// so if you think other priorities would be better don't worry about changing the values.
	int rating = 0;

	EGLint attribBufferSize = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_BUFFER_SIZE, &attribBufferSize);
	if (attribBufferSize < Params.Bits) {
		if (log)
			os::Printer::log("No full color buffer.");
		rating += 100;
	}
	if (attribBufferSize > Params.Bits) {
		if (log)
			os::Printer::log("Larger color buffer.", ELL_DEBUG);
		++rating;
	}

	EGLint attribRedSize = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_RED_SIZE, &attribRedSize);
	if (attribRedSize < 5 && Params.Bits >= 4)
		rating += 100;
	else if (attribRedSize < 8 && Params.Bits >= 24)
		rating += 10;
	else if (attribRedSize >= 8 && Params.Bits < 24)
		rating++;
	EGLint attribGreenSize = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_GREEN_SIZE, &attribGreenSize);
	if (attribGreenSize < 5 && Params.Bits >= 4)
		rating += 100;
	else if (attribGreenSize < 8 && Params.Bits >= 24)
		rating += 10;
	else if (attribGreenSize >= 8 && Params.Bits < 24)
		rating++;
	EGLint attribBlueSize = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_BLUE_SIZE, &attribBlueSize);
	if (attribBlueSize < 5 && Params.Bits >= 4)
		rating += 100;
	else if (attribBlueSize < 8 && Params.Bits >= 24)
		rating += 10;
	else if (attribBlueSize >= 8 && Params.Bits < 24)
		rating++;

	EGLint attribAlphaSize = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_ALPHA_SIZE, &attribAlphaSize);
	if (Params.WithAlphaChannel && attribAlphaSize == 0) {
		if (log)
			os::Printer::log("No alpha.");
		rating += 10;
	} else if (!Params.WithAlphaChannel && attribAlphaSize > 0) {
		if (log)
			os::Printer::log("Got alpha (unrequested).", ELL_DEBUG);
		rating++;
	}

	EGLint attribStencilSize = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_STENCIL_SIZE, &attribStencilSize);
	if (Params.Stencilbuffer && attribStencilSize == 0) {
		if (log)
			os::Printer::log("No stencil buffer.");
		rating += 10;
	} else if (!Params.Stencilbuffer && attribStencilSize > 0) {
		if (log)
			os::Printer::log("Got a stencil buffer (unrequested).", ELL_DEBUG);
		rating++;
	}

	EGLint attribDepthSize = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_DEPTH_SIZE, &attribDepthSize);
	if (attribDepthSize < Params.ZBufferBits) {
		if (log) {
			if (attribDepthSize > 0)
				os::Printer::log("No full depth buffer.");
			else
				os::Printer::log("No depth buffer.");
		}
		rating += 50;
	} else if (attribDepthSize != Params.ZBufferBits) {
		if (log) {
			if (Params.ZBufferBits == 0)
				os::Printer::log("Got a depth buffer (unrequested).", ELL_DEBUG);
			else
				os::Printer::log("Got a larger depth buffer.", ELL_DEBUG);
		}
		rating++;
	}

	EGLint attribSampleBuffers = 0, attribSamples = 0;
	eglGetConfigAttrib(EglDisplay, config, EGL_SAMPLE_BUFFERS, &attribSampleBuffers);
	eglGetConfigAttrib(EglDisplay, config, EGL_SAMPLES, &attribSamples);
	if (Params.AntiAlias && attribSampleBuffers == 0) {
		if (log)
			os::Printer::log("No multisampling.");
		rating += 20;
	} else if (Params.AntiAlias && attribSampleBuffers && attribSamples < Params.AntiAlias) {
		if (log)
			os::Printer::log("Multisampling with less samples than requested.", ELL_DEBUG);
		rating += 10;
	} else if (Params.AntiAlias && attribSampleBuffers && attribSamples > Params.AntiAlias) {
		if (log)
			os::Printer::log("Multisampling with more samples than requested.", ELL_DEBUG);
		rating += 5;
	} else if (!Params.AntiAlias && attribSampleBuffers > 0) {
		if (log)
			os::Printer::log("Got multisampling (unrequested).", ELL_DEBUG);
		rating += 3;
	}

	return rating;
}

void CEGLManager::destroySurface()
{
	if (EglSurface == EGL_NO_SURFACE)
		return;

	// We should unbind current EGL context before destroy EGL surface.
	eglMakeCurrent(EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	eglDestroySurface(EglDisplay, EglSurface);
	EglSurface = EGL_NO_SURFACE;
}

bool CEGLManager::generateContext()
{
	if (EglDisplay == EGL_NO_DISPLAY || EglSurface == EGL_NO_SURFACE)
		return false;

	if (EglContext != EGL_NO_CONTEXT)
		return true;

	EGLint OpenGLESVersion = 0;

	switch (Params.DriverType) {
	case EDT_OGLES2:
	case EDT_WEBGL1:
		OpenGLESVersion = 2;
		break;
	default:
		break;
	}

	EGLint ContextAttrib[] = {
#ifdef EGL_VERSION_1_3
			EGL_CONTEXT_CLIENT_VERSION, OpenGLESVersion,
#endif
			EGL_NONE, 0,
		};

	EglContext = eglCreateContext(EglDisplay, EglConfig, EGL_NO_CONTEXT, ContextAttrib);

	if (testEGLError()) {
		os::Printer::log("Could not create EGL context.", ELL_ERROR);
		return false;
	}

	os::Printer::log("EGL context created with OpenGLESVersion: ", core::stringc((int)OpenGLESVersion), ELL_DEBUG);

	return true;
}

void CEGLManager::destroyContext()
{
	if (EglContext == EGL_NO_CONTEXT)
		return;

	// We must unbind current EGL context before destroy it.
	eglMakeCurrent(EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(EglDisplay, EglContext);

	EglContext = EGL_NO_CONTEXT;
}

bool CEGLManager::activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero)
{
	eglMakeCurrent(EglDisplay, EglSurface, EglSurface, EglContext);

	if (testEGLError()) {
		os::Printer::log("Could not make EGL context current.");
		return false;
	}
	return true;
}

const SExposedVideoData &CEGLManager::getContext() const
{
	return Data;
}

void *CEGLManager::getProcAddress(const std::string &procName)
{
	return (void *)eglGetProcAddress(procName.c_str());
}

bool CEGLManager::swapBuffers()
{
	return (eglSwapBuffers(EglDisplay, EglSurface) == EGL_TRUE);
}

bool CEGLManager::testEGLError()
{
	if (!Params.DriverDebug)
		return false;
	EGLint status = eglGetError();

	switch (status) {
	case EGL_SUCCESS:
		return false;
	case EGL_NOT_INITIALIZED:
		os::Printer::log("Not Initialized", ELL_ERROR);
		break;
	case EGL_BAD_ACCESS:
		os::Printer::log("Bad Access", ELL_ERROR);
		break;
	case EGL_BAD_ALLOC:
		os::Printer::log("Bad Alloc", ELL_ERROR);
		break;
	case EGL_BAD_ATTRIBUTE:
		os::Printer::log("Bad Attribute", ELL_ERROR);
		break;
	case EGL_BAD_CONTEXT:
		os::Printer::log("Bad Context", ELL_ERROR);
		break;
	case EGL_BAD_CONFIG:
		os::Printer::log("Bad Config", ELL_ERROR);
		break;
	case EGL_BAD_CURRENT_SURFACE:
		os::Printer::log("Bad Current Surface", ELL_ERROR);
		break;
	case EGL_BAD_DISPLAY:
		os::Printer::log("Bad Display", ELL_ERROR);
		break;
	case EGL_BAD_SURFACE:
		os::Printer::log("Bad Surface", ELL_ERROR);
		break;
	case EGL_BAD_MATCH:
		os::Printer::log("Bad Match", ELL_ERROR);
		break;
	case EGL_BAD_PARAMETER:
		os::Printer::log("Bad Parameter", ELL_ERROR);
		break;
	case EGL_BAD_NATIVE_PIXMAP:
		os::Printer::log("Bad Native Pixmap", ELL_ERROR);
		break;
	case EGL_BAD_NATIVE_WINDOW:
		os::Printer::log("Bad Native Window", ELL_ERROR);
		break;
	case EGL_CONTEXT_LOST:
		os::Printer::log("Context Lost", ELL_ERROR);
		break;
	default:
		break;
	};

	return true;
}

}

#endif
