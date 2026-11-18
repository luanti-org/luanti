// Copyright (C) 2022 sfan5
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "CSDLManager.h"

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)

#include "CIrrDeviceSDL.h"
#include "os.h"

namespace video
{

CSDLManager::CSDLManager(CIrrDeviceSDL *device) :
		IContextManager(), SDLDevice(device)
{}

bool CSDLManager::initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &data)
{
	Data = data;
	SwapInterval = params.Vsync ? 1 : 0;
	if (!SDLDevice->setSwapInterval(SwapInterval))
		os::Printer::log("Setting GL swap interval failed");
	return true;
}

const SExposedVideoData &CSDLManager::getContext() const
{
	return Data;
}

bool CSDLManager::activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero)
{
	if (videoData.Window == ActiveSurface)
		return true;

	if (!videoData.Window && !restorePrimaryOnZero)
		return true;

	if (!SDLDevice->makeWindowCurrent(videoData.Window))
		return false;
	ActiveSurface = videoData.Window;

	// Only the main window may wait for vblank, otherwise presenting two
	// windows per frame would halve the frame rate.
	SDLDevice->setSwapInterval(ActiveSurface ? 0 : SwapInterval);
	return true;
}

void *CSDLManager::getProcAddress(const std::string &procName)
{
	return (void *)SDL_GL_GetProcAddress(procName.c_str());
}

bool CSDLManager::swapBuffers()
{
	SDLDevice->SwapWindow(ActiveSurface);

	// Hand the context back to the main window after presenting a
	// secondary one, so that code outside beginScene()/endScene()
	// (texture uploads etc.) keeps working as before.
	if (ActiveSurface) {
		SDLDevice->makeWindowCurrent(nullptr);
		ActiveSurface = nullptr;
		SDLDevice->setSwapInterval(SwapInterval);
	}
	return true;
}

}

#endif
