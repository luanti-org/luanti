// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

namespace scene
{

//! Dummy scene node for adding additional transformations to the scene graph.
/** This scene node does not render itself, and does not respond to set/getPosition,
set/getRotation and set/getScale. Its just a simple scene node that takes a
matrix as relative transformation, making it possible to insert any transformation
anywhere into the scene graph.
This scene node is for example used by the IAnimatedMeshSceneNode for emulating
joint scene nodes when playing skeletal animations.
*/
class IDummyTransformationSceneNode : public ISceneNode
{
public:
	//! Constructor
	IDummyTransformationSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id) :
			ISceneNode(parent, mgr, id) {}

	//! Returns a reference to the current relative transformation matrix.
	/** This is the matrix, this scene node uses instead of scale, translation
	and rotation. */
	virtual core::matrix4 &getRelativeTransformationMatrix() = 0;
};

} // end namespace scene
