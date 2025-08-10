// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

namespace scene
{

//! Interface for bones used for skeletal animation.
/** Used with SkinnedMesh and IAnimatedMeshSceneNode. */
class IBoneSceneNode : public ISceneNode
{
public:
	IBoneSceneNode(ISceneNode *parent, ISceneManager *mgr,
			s32 id = -1, u32 boneIndex = 0,
			const std::optional<std::string> &boneName = std::nullopt)
	:
			ISceneNode(parent, mgr, id),
			BoneIndex(boneIndex)
	{
		setName(boneName);
	}

	//! Returns the index of the bone
	u32 getBoneIndex() const
	{
		return BoneIndex;
	}

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override
	{
		return Box;
	}

	const u32 BoneIndex;

	// Bogus box; bone scene nodes are not rendered anyways.
	static constexpr core::aabbox3d<f32> Box = {{0, 0, 0}};

	//! The render method.
	/** Does nothing as bones are not visible. */
	void render() override {}
};

} // end namespace scene
