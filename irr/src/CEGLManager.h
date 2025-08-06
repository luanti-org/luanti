// Copyright (C) 2013 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#pragma once

#ifdef _IRR_COMPILE_WITH_EGL_MANAGER_

#include <EGL/egl.h>

#include "SIrrCreationParameters.h"
#include "SExposedVideoData.h"
#include "IContextManager.h"

namespace video
{
// EGL manager.
class CEGLManager : public IContextManager
{
public:
	//! Constructor.
	CEGLManager();

	//! Destructor.
	virtual ~CEGLManager();

	// Initialize EGL.
	/* This method initialize EGLand create EGL display, anyway surface and context
	aren't create. */
	bool initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &data) override;

	// Terminate EGL.
	/* Terminate EGL context. This method break both existed surface and context. */
	void terminate() override;

	// Create EGL surface.
	/* This method create EGL surface. On some platforms eg. Android, we must
	recreate surface on each resume, because WindowID may change, so existed
	surface may not be valid. If EGL context already exist, this method
	automatically activates it. */
	bool generateSurface() override;

	// Destroy EGL surface.
	/* This method destroy EGL. On some platforms eg. Android, we should call
	this method on each pause, because after resume this surface may not be valid.
	Hovewer this method doesn'r break EGL context. */
	void destroySurface() override;

	// Create EGL context.
	/* This method create and activate EGL context. */
	bool generateContext() override;

	// Destroy EGL context.
	/* This method destroy EGL context. */
	void destroyContext() override;

	const SExposedVideoData &getContext() const override;

	bool activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero) override;

	// Get procedure address.
	void *getProcAddress(const std::string &procName) override;

	// Swap buffers.
	bool swapBuffers() override;

protected:
	enum EConfigStyle
	{
		//! Get first result of eglChooseConfigs and if that fails try again by requesting simpler attributes
		ECS_EGL_CHOOSE_FIRST_LOWER_EXPECTATIONS,

		//! We select our own best fit and avoid using eglChooseConfigs
		ECS_IRR_CHOOSE,
	};

	EGLConfig chooseConfig(EConfigStyle confStyle);

	//! Check how close this config is to the parameters we requested
	//! returns 0 is perfect, larger values are worse and < 0 is unusable.
	s32 rateConfig(EGLConfig config, EGLint eglOpenGLBIT, bool log = false);

	// Helper to sort EGLConfig's. (because we got no std::pair....)
	struct SConfigRating
	{
		EGLConfig config;
		s32 rating;
		bool operator<(const SConfigRating &other) const
		{
			return rating < other.rating;
		}
	};

private:
	bool testEGLError();

	NativeWindowType EglWindow;
	EGLDisplay EglDisplay;
	EGLSurface EglSurface;
	EGLContext EglContext;

	EGLConfig EglConfig;

	SIrrlichtCreationParameters Params;
	SExposedVideoData Data;

	EGLint MajorVersion;
	EGLint MinorVersion;
};
}
#endif
