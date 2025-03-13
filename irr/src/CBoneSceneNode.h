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
			const std::optional<std::string> &boneName = std::nullopt);

	//! Returns the index of the bone
	u32 getBoneIndex() const override;

	//! Sets the animation mode of the bone. Returns true if successful.
	bool setAnimationMode(E_BONE_ANIMATION_MODE mode) override;

	//! Gets the current animation mode of the bone
	E_BONE_ANIMATION_MODE getAnimationMode() const override;

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override;

	/*
	//! Returns the relative transformation of the scene node.
	//core::matrix4 getRelativeTransformation() const override;
	*/

	void OnAnimate(u32 timeMs) override;

	void updateAbsolutePositionOfAllChildren() override;

	//! How the relative transformation of the bone is used
	void setSkinningSpace(E_BONE_SKINNING_SPACE space) override
	{
		SkinningSpace = space;
	}

	E_BONE_SKINNING_SPACE getSkinningSpace() const override
	{
		return SkinningSpace;
	}

private:
	void helper_updateAbsolutePositionOfAllChildren(ISceneNode *Node);

	u32 BoneIndex;

	core::aabbox3d<f32> Box{-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};

	E_BONE_ANIMATION_MODE AnimationMode;
	E_BONE_SKINNING_SPACE SkinningSpace;
};

} // end namespace scene
} // end namespace irr
