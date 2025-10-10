// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"

namespace scene
{

//! An enumeration for all types of automatic culling for built-in scene nodes
enum E_CULLING_TYPE
{
	EAC_OFF = 0,
	EAC_BOX = 1,
	EAC_FRUSTUM_BOX = 2,
	EAC_FRUSTUM_SPHERE = 4,
	EAC_OCC_QUERY = 8
};

//! Names for culling type
const c8 *const AutomaticCullingNames[] = {
		"false",
		"box",            // camera box against node box
		"frustum_box",    // camera frustum against node box
		"frustum_sphere", // camera frustum against node sphere
		"occ_query",      // occlusion query
		0,
	};

} // end namespace scene
