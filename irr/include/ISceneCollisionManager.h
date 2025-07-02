// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "position2d.h"
#include "line3d.h"

namespace scene
{
class ICameraSceneNode;

class ISceneCollisionManager : public virtual IReferenceCounted
{
public:
	//! Returns a 3d ray which would go through the 2d screen coordinates.
	/** \param pos: Screen coordinates in pixels.
	\param camera: Camera from which the ray starts. If null, the
	active camera is used.
	\return Ray starting from the position of the camera and ending
	at a length of the far value of the camera at a position which
	would be behind the 2d screen coordinates. */
	virtual core::line3d<f32> getRayFromScreenCoordinates(
			const core::position2d<s32> &pos, const ICameraSceneNode *camera = 0) = 0;
};

} // end namespace scene
