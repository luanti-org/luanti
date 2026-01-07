// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include <cstddef>

namespace video
{

class COpenGLCoreFeature
{
public:
	COpenGLCoreFeature() = default;

	virtual ~COpenGLCoreFeature()
	{
	}

	bool BlendOperation = false;
	bool TexStorage = false;

	u8 ColorAttachment = 0;
	u8 MultipleRenderTarget = 0;
	u8 MaxTextureUnits = 0;

	/// Maximum supported UBO size in bytes, 0 if not supported
	size_t MaxUBOSize = 0;
};

}
