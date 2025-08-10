// Copyright (C) 2013 Christian Stehno
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_WGL_MANAGER_

#include "SIrrCreationParameters.h"
#include "SExposedVideoData.h"
#include "IContextManager.h"
#include "SColor.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <libloaderapi.h>

namespace video
{
// WGL manager.
class CWGLManager : public IContextManager
{
public:
	//! Constructor.
	CWGLManager();

	//! Destructor
	~CWGLManager();

	// Initialize
	bool initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &data) override;

	// Terminate
	void terminate() override;

	// Create surface.
	bool generateSurface() override;

	// Destroy surface.
	void destroySurface() override;

	// Create context.
	bool generateContext() override;

	// Destroy EGL context.
	void destroyContext() override;

	//! Get current context
	const SExposedVideoData &getContext() const override;

	//! Change render context, disable old and activate new defined by videoData
	bool activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero) override;

	// Get procedure address.
	void *getProcAddress(const std::string &procName) override;

	// Swap buffers.
	bool swapBuffers() override;

private:
	SIrrlichtCreationParameters Params;
	SExposedVideoData PrimaryContext;
	SExposedVideoData CurrentContext;
	s32 PixelFormat;
	PIXELFORMATDESCRIPTOR pfd;
	ECOLOR_FORMAT ColorFormat;
	void *FunctionPointers[1];

	HMODULE libHandle;
};
}

#endif
