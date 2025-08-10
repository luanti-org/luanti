// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

namespace scene
{

class CEmptySceneNode : public ISceneNode
{
public:
	//! constructor
	CEmptySceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id);

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override;

	//! This method is called just before the rendering process of the whole scene.
	void OnRegisterSceneNode() override;

	//! does nothing.
	void render() override;

	//! Returns type of the scene node
	ESCENE_NODE_TYPE getType() const override { return ESNT_EMPTY; }

	//! Creates a clone of this scene node and its children.
	ISceneNode *clone(ISceneNode *newParent = 0, ISceneManager *newManager = 0) override;

private:
	core::aabbox3d<f32> Box{{0, 0, 0}};
};

} // end namespace scene
