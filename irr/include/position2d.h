// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

//! As of Irrlicht 1.6, position2d is a synonym for vector2d.
/** You should consider position2d to be deprecated, and use vector2d by preference. */

#pragma once

#include "vector2d.h"

namespace core
{

// Use typedefs where possible as they are more explicit...

//! \deprecated position2d is now a synonym for vector2d, but vector2d should be used directly.
typedef vector2d<f32> position2df;

//! \deprecated position2d is now a synonym for vector2d, but vector2d should be used directly.
typedef vector2d<s32> position2di;
} // namespace core

// ...and use a #define to catch the rest, for (e.g.) position2d<f64>
#define position2d vector2d
