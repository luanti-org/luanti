// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMesh.h"
#include <string>
#include <optional>

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

	virtual std::optional<u16> getTrackNumber(const std::string &track_name) const = 0;

	//! Gets the maximum frame number, 0 if the mesh is static.
	virtual f32 getMaxFrameNumber(u16 track) const = 0;

	virtual void prepareForAnimation(u16 max_hw_joints) = 0;

	//! Whether the mesh needs shader-based hardware skinning
	virtual bool needsHwSkinning() const = 0;

	//! Returns the type of the animated mesh. Useful for safe downcasts.
	E_ANIMATED_MESH_TYPE getMeshType() const override = 0;
};

} // end namespace scene
