// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

// Used with SkinnedMesh and IAnimatedMeshSceneNode, for boned meshes

#include "IBoneSceneNode.h"

#include <optional>

namespace irr
{
namespace scene
{

class CBoneSceneNode : public IBoneSceneNode
{
public:
	//! constructor
	CBoneSceneNode(ISceneNode *parent, ISceneManager *mgr,
			s32 id = -1, u32 boneIndex = 0,
			const std::optional<std::string> &boneName = std::nullopt) :
		IBoneSceneNode(parent, mgr, id),
		BoneIndex(boneIndex)
	{
		setName(boneName);
	}

	//! Returns the index of the bone
	u32 getBoneIndex() const override
	{
		return BoneIndex;
	}

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override
	{
		return Box;
	}

	void OnAnimate(u32 timeMs) override;

	void updateAbsolutePositionOfAllChildren() override;

private:
	void helper_updateAbsolutePositionOfAllChildren(ISceneNode *Node);

	const u32 BoneIndex;

	// Bogus box; bone scene nodes are not rendered anyways.
	static constexpr core::aabbox3d<f32> Box = {{0, 0, 0}};
};

} // end namespace scene
} // end namespace irr
