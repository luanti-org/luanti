// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMesh.h"

namespace scene
{
//! Interface for an animated mesh.
/** There are already simple implementations of this interface available so
you don't have to implement this interface on your own if you need to:
You might want to use scene::SMesh, scene::SMeshBuffer etc.
*/
class IAnimatedMesh : public IMesh
{
public:
	//! Gets the maximum frame number, 0 if the mesh is static.
	virtual f32 getMaxFrameNumber() const = 0;

	//! Returns the type of the animated mesh. Useful for safe downcasts.
	E_ANIMATED_MESH_TYPE getMeshType() const = 0;
};

} // end namespace scene
