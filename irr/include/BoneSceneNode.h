// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

#include "Transform.h"
#include "matrix4.h"

#include <optional>

namespace scene
{

//! Interface for bones used for skeletal animation.
/** Used with SkinnedMesh and AnimatedMeshSceneNode. */
class BoneSceneNode : public ISceneNode
{
public:
	BoneSceneNode(ISceneNode *parent,
			ISceneManager *mgr,
			s32 id = -1,
			u32 boneIndex = 0,
			const std::optional<std::string> &boneName = std::nullopt,
			const core::Transform &transform = {},
			const std::optional<core::matrix4> &matrix = std::nullopt) :
		ISceneNode(parent, mgr, id),
		Matrix(matrix),
		BoneIndex(boneIndex)
	{
		setName(boneName);
		setTransform(transform);
	}

	//! Returns the index of the bone
	u32 getBoneIndex() const
	{
		return BoneIndex;
	}

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override
	{
		//! Bogus box; bone scene nodes are not rendered anyways.
		static constexpr core::aabbox3d<f32> Box = {{0, 0, 0}};
		return Box;
	}

	//! The render method.
	/** Does nothing as bones are not visible. */
	void render() override {}

	void setTransform(const core::Transform &transform)
	{
		setPosition(transform.translation);
		{
			core::vector3df euler;
			auto rot = transform.rotation;
			// Invert to be consistent with setRotationDegrees
			rot.makeInverse();
			rot.toEuler(euler);
			setRotation(euler * core::RADTODEG);
		}
		setScale(transform.scale);
	}

	core::Transform getTransform() const
	{
		return {
			getPosition(),
			core::quaternion(getRotation() * core::DEGTORAD).makeInverse(),
			getScale()
		};
	}

	core::matrix4 getRelativeTransformation() const override
	{
		if (Matrix)
			return *Matrix;
		return ISceneNode::getRelativeTransformation();
	}

	//! Some file formats alternatively let bones specify a transformation matrix.
	//! If this is set, it overrides the TRS properties.
	std::optional<core::matrix4> Matrix;

private:

	const u32 BoneIndex;
};

} // end namespace scene
