// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

namespace irr
{
namespace scene
{

//! Interface for bones used for skeletal animation.
/** Used with SkinnedMesh and IAnimatedMeshSceneNode. */
class IBoneSceneNode : public ISceneNode
{
public:
	IBoneSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id = -1) :
			ISceneNode(parent, mgr, id) {}

	//! Get the index of the bone
	virtual u32 getBoneIndex() const = 0;

	//! Get the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override = 0;

	//! Returns the relative transformation of the scene node.
	// virtual core::matrix4 getRelativeTransformation() const = 0;

	//! The render method.
	/** Does nothing as bones are not visible. */
	void render() override {}
};

} // end namespace scene
} // end namespace irr
