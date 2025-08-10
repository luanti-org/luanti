// Copyright (C) 2013-2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "SExposedVideoData.h"
#include "SIrrCreationParameters.h"
#include <string>

namespace video
{
// For system specific window contexts (used for OpenGL)
class IContextManager : public virtual IReferenceCounted
{
public:
	//! Initialize manager with device creation parameters and device window (passed as exposed video data)
	virtual bool initialize(const SIrrlichtCreationParameters &params, const SExposedVideoData &data) = 0;

	//! Terminate manager, any cleanup that is left over. Manager needs a new initialize to be usable again
	virtual void terminate() = 0;

	//! Create surface based on current window set
	virtual bool generateSurface() = 0;

	//! Destroy current surface
	virtual void destroySurface() = 0;

	//! Create context based on current surface
	virtual bool generateContext() = 0;

	//! Destroy current context
	virtual void destroyContext() = 0;

	//! Get current context
	virtual const SExposedVideoData &getContext() const = 0;

	//! Change render context, disable old and activate new defined by videoData
	//\param restorePrimaryOnZero When true: restore original driver context when videoData is set to 0 values.
	//                            When false: resets the context when videoData is set to 0 values.
	/** This is mostly used internally by IVideoDriver::beginScene().
		But if you want to switch threads which access your OpenGL driver you will have to
		call this function as follows:
		Old thread gives up context with: activateContext(video::SExposedVideoData());
		New thread takes over context with: activateContext(videoDriver->getExposedVideoData());
		Note that only 1 thread at a time may access an OpenGL context.	*/
	virtual bool activateContext(const SExposedVideoData &videoData, bool restorePrimaryOnZero = false) = 0;

	//! Get the address of any OpenGL procedure (including core procedures).
	virtual void *getProcAddress(const std::string &procName) = 0;

	//! Swap buffers.
	virtual bool swapBuffers() = 0;
};

} // end namespace video
