// Copyright (C) 2015 Patryk Nadrowski
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"

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
};

}
