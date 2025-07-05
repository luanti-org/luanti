// Copyright (C) 2015 Patryk Nadrowski
// Copyright (C) 2009-2010 Amundis
// 2017 modified by Michael Zeilfelder (unifying extension handlers)
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in Irrlicht.h

#include "ExtensionHandler.h"

#include "irrString.h"
#include "SMaterial.h"
#include "fast_atof.h"
#include "os.h"
#include <mt_opengl.h>

namespace video
{

void COpenGL3ExtensionHandler::initExtensions()
{
	// reading extensions happens in mt_opengl.cpp
	for (size_t j = 0; j < IRR_OGLES_Feature_Count; ++j)
		FeatureAvailable[j] = queryExtension(getFeatureString(j));
}

} // end namespace video
