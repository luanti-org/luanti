// Copyright (C) 2014 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "CNSOGLManager.h"

#ifdef _IRR_COMPILE_WITH_NSOGL_MANAGER_

#include <mach-o/dyld.h>
#include "os.h"

namespace video
{

CNSOGLManager::CNSOGLManager() :
		PrimaryContext(SExposedVideoData(0)), PixelFormat(nil)
{}

CNSOGLManager::~CNSOGLManager()
{
}

bool CNSOGLManager::initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &videodata)
{
	Params = params;

	return true;
}

void CNSOGLManager::terminate()
{
}

bool CNSOGLManager::generateSurface()
{
	if (Params.DriverType == video::EDT_OPENGL) {
		int alphaSize = Params.WithAlphaChannel ? 4 : 0;
		int depthSize = Params.ZBufferBits;

		if (Params.WithAlphaChannel && Params.Bits == 32)
			alphaSize = 8;

		NSOpenGLPixelFormatAttribute Attribs[] = {
				NSOpenGLPFANoRecovery,
				NSOpenGLPFAAccelerated,
				NSOpenGLPFADoubleBuffer,
				NSOpenGLPFADepthSize, static_cast<NSOpenGLPixelFormatAttribute>(depthSize),
				NSOpenGLPFAColorSize, Params.Bits,
				NSOpenGLPFAAlphaSize, static_cast<NSOpenGLPixelFormatAttribute>(alphaSize),
				NSOpenGLPFASampleBuffers, 1,
				NSOpenGLPFASamples, Params.AntiAlias,
				NSOpenGLPFAStencilSize, static_cast<NSOpenGLPixelFormatAttribute>(Params.Stencilbuffer ? 1 : 0),
				// NSOpenGLPFAFullScreen,
				0,
			};

		u32 Steps = 6;

		// Choose the best pixel format.
		do {
			switch (Steps) {
			case 6: // decrease step.
				--Steps;
				break;
			case 5: // samples
				if (Attribs[12] > 2)
					--Attribs[12];
				else {
					Attribs[10] = 0;
					Attribs[12] = 0;
					--Steps;
				}
				break;
			case 4: // alpha
				if (Attribs[8]) {
					Attribs[8] = 0;

					if (Params.AntiAlias) {
						Attribs[10] = 1;
						Attribs[12] = Params.AntiAlias;
						Steps = 5;
					}
				} else
					--Steps;
				break;
			case 3: // stencil
				if (Attribs[14]) {
					Attribs[14] = 0;

					if (Params.AntiAlias) {
						Attribs[10] = 1;
						Attribs[12] = Params.AntiAlias;
						Steps = 5;
					}
				} else
					--Steps;
				break;
			case 2: // depth size
				if (Attribs[4] > 16) {
					Attribs[4] = Attribs[4] - 8;
				} else
					--Steps;
				break;
			case 1: // buffer size
				if (Attribs[6] > 16) {
					Attribs[6] = Attribs[6] - 8;
				} else
					--Steps;
				break;
			default:
				os::Printer::log("Could not get pixel format.");
				return false;
			}

			PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:Attribs];
		} while (PixelFormat == nil);

		if (Params.AntiAlias && !Attribs[10])
			os::Printer::log("No multisampling.");

		if (Params.WithAlphaChannel && !Attribs[8])
			os::Printer::log("No alpha.");

		if (Params.Stencilbuffer && !Attribs[14])
			os::Printer::log("No stencil buffer.");

		if (Params.ZBufferBits > Attribs[4])
			os::Printer::log("No full depth buffer.");

		if (Params.Bits > Attribs[6])
			os::Printer::log("No full color buffer.");
	}

	return true;
}

void CNSOGLManager::destroySurface()
{
	[PixelFormat release];
	PixelFormat = nil;
}

bool CNSOGLManager::generateContext()
{
	NSOpenGLContext *Context = [[NSOpenGLContext alloc] initWithFormat:PixelFormat shareContext:nil];

	GLint Vsync = Params.Vsync ? 1 : 0;
	[Context setValues:&Vsync forParameter:NSOpenGLCPSwapInterval];

	if (Context == nil) {
		os::Printer::log("Could not create OpenGL context.", ELL_ERROR);
		return false;
	}

	// set exposed data
	CurrentContext.OpenGLOSX.Context = Context;

	if (!PrimaryContext.OpenGLOSX.Context)
		PrimaryContext.OpenGLOSX.Context = CurrentContext.OpenGLOSX.Context;

	return true;
}

const SExposedVideoData &CNSOGLManager::getContext() const
{
	return CurrentContext;
}

bool CNSOGLManager::activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero)
{
	// TODO: handle restorePrimaryOnZero
	if (videoData.OpenGLOSX.Context) {
		if ((NSOpenGLContext *)videoData.OpenGLOSX.Context != [NSOpenGLContext currentContext]) {
			[(NSOpenGLContext *)videoData.OpenGLOSX.Context makeCurrentContext];

			CurrentContext = videoData;
		}
	}
	// set back to main context
	else {
		if ((NSOpenGLContext *)PrimaryContext.OpenGLOSX.Context != [NSOpenGLContext currentContext]) {
			[(NSOpenGLContext *)PrimaryContext.OpenGLOSX.Context makeCurrentContext];

			CurrentContext = PrimaryContext;
		}
	}

	return true;
}

void CNSOGLManager::destroyContext()
{
	if (CurrentContext.OpenGLOSX.Context) {
		if (PrimaryContext.OpenGLOSX.Context == CurrentContext.OpenGLOSX.Context)
			PrimaryContext.OpenGLOSX.Context = nil;

		[(NSOpenGLContext *)CurrentContext.OpenGLOSX.Context makeCurrentContext];
		[(NSOpenGLContext *)CurrentContext.OpenGLOSX.Context clearDrawable];
		[(NSOpenGLContext *)CurrentContext.OpenGLOSX.Context release];
		[NSOpenGLContext clearCurrentContext];

		CurrentContext.OpenGLOSX.Context = nil;
	}
}

// It appears that there is no separate GL proc address getter on OSX.
// https://developer.apple.com/library/archive/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_entrypts/opengl_entrypts.html
void *CNSOGLManager::getProcAddress(const std::string &procName)
{
	NSSymbol symbol = NULL;
	// Allocate a buffer for the name, an underscore prefix, and a cstring terminator.
	std::string mangledName = "_" + procName;
	if (NSIsSymbolNameDefined(mangledName.c_str()))
		symbol = NSLookupAndBindSymbol(mangledName.c_str());
	return symbol ? NSAddressOfSymbol(symbol) : NULL;
}

bool CNSOGLManager::swapBuffers()
{
	[(NSOpenGLContext *)CurrentContext.OpenGLOSX.Context flushBuffer];

	return true;
}

}

#endif
