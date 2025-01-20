// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

// Used with SkinnedMesh and IAnimatedMeshSceneNode, for boned meshes

#include "IBoneSceneNode.h"
#include "Transform.h"

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
			const std::optional<std::string> &boneName = std::nullopt,
			const core::Transform &transform = {}) :
		IBoneSceneNode(parent, mgr, id, boneIndex, boneName)
	{
		setTransform(transform);
	}

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
};

} // end namespace scene
} // end namespace irr
