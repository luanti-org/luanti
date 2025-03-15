// Copyright (C) 2022 sfan5
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "CSDLManager.h"

#if defined(_IRR_COMPILE_WITH_SDL_DEVICE_)

#include "CIrrDeviceSDL.h"

namespace irr
{
namespace video
{

CSDLManager::CSDLManager(CIrrDeviceSDL *device) :
		IContextManager(), SDLDevice(device)
{}

bool CSDLManager::initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &data)
{
	Data = data;
	return true;
}

const SExposedVideoData &CSDLManager::getContext() const
{
	return Data;
}

bool CSDLManager::activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero)
{
	return true;
}

void *CSDLManager::getProcAddress(const std::string &procName)
{
#ifndef _IRR_COMPILE_WITH_ANGLE_
	return SDL_GL_GetProcAddress(procName.c_str());
#else
	return (void *)eglGetProcAddress(procName.c_str());
#endif
}

bool CSDLManager::swapBuffers()
{
	SDLDevice->SwapWindow();
	return true;
}

}
}

#endif
