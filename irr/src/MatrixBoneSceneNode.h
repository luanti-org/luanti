// Copyright (C) 2025 Joe Mama
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "IBoneSceneNode.h"
#include "matrix4.h"
#include "vector3d.h"
#include <iostream>

#include <optional>

// We must represent some transforms differently:
// Bones can have static non-TRS matrix transforms,
// for example shearing (can not be decomposed at all)
// or negatively scaling an axis (can not be decomposed uniquely).
// Hence well-defined animation is not possible for such nodes
// (and in fact glTF even guarantees that they will never be animated).

namespace irr
{
namespace scene
{

class MatrixBoneSceneNode : public IBoneSceneNode
{
public:
	//! constructor
	MatrixBoneSceneNode(ISceneNode *parent, ISceneManager *mgr,
			s32 id = -1, u32 boneIndex = 0,
			const std::optional<std::string> &boneName = std::nullopt,
			const core::matrix4 &matrix = core::IdentityMatrix) :
		IBoneSceneNode(parent, mgr, id, boneIndex, boneName),
		matrix(matrix)
	{}

	const core::matrix4 matrix;

	// Matrix nodes should not be fake decomposed.
	const core::vector3df &getPosition() const override { assert(false); }
	const core::vector3df &getRotation() const override { assert(false); }
	const core::vector3df &getScale() const override { assert(false); }

	// This node should be static.
	void setPosition(const core::vector3df &pos) override { assert(false); }
	void setRotation(const core::vector3df &euler_deg) override { assert(false); }
	void setScale(const core::vector3df &scale) override { assert(false); }

	core::matrix4 getRelativeTransformation() const override
	{
		return matrix;
	}
};

} // end namespace scene
} // end namespace irr
