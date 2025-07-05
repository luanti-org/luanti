// Copyright (C) 2013 Christian Stehno
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_GLX_MANAGER_

#include "SIrrCreationParameters.h"
#include "SExposedVideoData.h"
#include "IContextManager.h"
#include "SColor.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// we can't include glx.h here, because gl.h has incompatible types with ogl es headers and it
// cause redefinition errors, thats why we use ugly trick with void* types and casts.

namespace video
{
// GLX manager.
class CGLXManager : public IContextManager
{
public:
	//! Constructor.
	CGLXManager(const SIrrlichtCreationParameters &params, const SExposedVideoData &videodata, int screennr);

	//! Destructor
	~CGLXManager();

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

	// Destroy context.
	void destroyContext() override;

	//! Get current context
	const SExposedVideoData &getContext() const override;

	//! Change render context, disable old and activate new defined by videoData
	bool activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero) override;

	// Get procedure address.
	void *getProcAddress(const std::string &procName) override;

	// Swap buffers.
	bool swapBuffers() override;

	XVisualInfo *getVisual() const { return VisualInfo; } // return XVisualInfo

private:
	SIrrlichtCreationParameters Params;
	SExposedVideoData PrimaryContext;
	SExposedVideoData CurrentContext;
	XVisualInfo *VisualInfo;
	void *glxFBConfig; // GLXFBConfig
	XID GlxWin;        // GLXWindow
};
}

#endif
