// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

namespace irr
{
namespace scene
{

//! Enumeration for different bone animation modes
enum E_BONE_ANIMATION_MODE
{
	//! The bone is usually animated, unless it's parent is not animated
	EBAM_AUTOMATIC = 0,

	//! The bone is animated by the skin, if it's parent is not animated then animation will resume from this bone onward
	EBAM_ANIMATED,

	//! The bone is not animated by the skin
	EBAM_UNANIMATED,

	//! Not an animation mode, just here to count the available modes
	EBAM_COUNT

};

enum E_BONE_SKINNING_SPACE
{
	//! local skinning, standard
	EBSS_LOCAL = 0,

	//! global skinning
	EBSS_GLOBAL,

	EBSS_COUNT
};

//! Names for bone animation modes
const c8 *const BoneAnimationModeNames[] = {
		"automatic",
		"animated",
		"unanimated",
		0,
	};

//! Interface for bones used for skeletal animation.
/** Used with SkinnedMesh and IAnimatedMeshSceneNode. */
class IBoneSceneNode : public ISceneNode
{
public:
	IBoneSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id = -1) :
			ISceneNode(parent, mgr, id), positionHint(-1), scaleHint(-1), rotationHint(-1) {}

	//! Get the index of the bone
	virtual u32 getBoneIndex() const = 0;

	//! Sets the animation mode of the bone.
	/** \return True if successful. (Unused) */
	virtual bool setAnimationMode(E_BONE_ANIMATION_MODE mode) = 0;

	//! Gets the current animation mode of the bone
	virtual E_BONE_ANIMATION_MODE getAnimationMode() const = 0;

	//! Get the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override = 0;

	//! Returns the relative transformation of the scene node.
	// virtual core::matrix4 getRelativeTransformation() const = 0;

	//! The animation method.
	void OnAnimate(u32 timeMs) override = 0;

	//! The render method.
	/** Does nothing as bones are not visible. */
	void render() override {}

	//! How the relative transformation of the bone is used
	virtual void setSkinningSpace(E_BONE_SKINNING_SPACE space) = 0;

	//! How the relative transformation of the bone is used
	virtual E_BONE_SKINNING_SPACE getSkinningSpace() const = 0;

	//! Updates the absolute position based on the relative and the parents position
	virtual void updateAbsolutePositionOfAllChildren() = 0;

	s32 positionHint;
	s32 scaleHint;
	s32 rotationHint;
};

} // end namespace scene
} // end namespace irr
