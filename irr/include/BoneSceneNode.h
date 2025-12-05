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
private:

	//! For performance reasons, we prefer to store the rotation as quaternion
	//! to avoid expensive conversions back and forth between Euler angles and quaternions.
	core::quaternion Rotation;

	const u32 BoneIndex;

public:

	//! Some file formats alternatively let bones specify a transformation matrix.
	//! If this is set, it overrides the TRS properties.
	std::optional<core::matrix4> Matrix;

	BoneSceneNode(ISceneNode *parent,
			ISceneManager *mgr,
			s32 id = -1,
			u32 boneIndex = 0,
			const std::optional<std::string> &boneName = std::nullopt,
			const core::Transform &transform = {},
			const std::optional<core::matrix4> &matrix = std::nullopt) :
		ISceneNode(parent, mgr, id),
		BoneIndex(boneIndex),
		Matrix(matrix)
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

	void setRotation(const core::vector3df &rotation) override
	{
		Rotation = core::quaternion(rotation * core::DEGTORAD).makeInverse();
	}

	core::vector3df getRotation() const override
	{
		auto rot = Rotation;
		rot.makeInverse();
		core::vector3df euler;
		rot.toEuler(euler);
		return euler * core::RADTODEG;
	}

	void setTransform(const core::Transform &transform)
	{
		setPosition(transform.translation);
		Rotation = transform.rotation;
		setScale(transform.scale);
	}

	core::Transform getTransform() const
	{
		return {
			getPosition(),
			Rotation,
			getScale()
		};
	}

	core::matrix4 getRelativeTransformation() const override
	{
		if (Matrix)
			return *Matrix;
		return getTransform().buildMatrix();
	}
};

} // end namespace scene
