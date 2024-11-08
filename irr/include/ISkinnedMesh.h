// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrArray.h"
#include "IAnimatedMesh.h"
#include "SSkinMeshBuffer.h"
#include "quaternion.h"

#include <optional>
#include <string>

namespace irr
{
namespace scene
{

enum E_INTERPOLATION_MODE
{
	// constant does use the current key-values without interpolation
	EIM_CONSTANT = 0,

	// linear interpolation
	EIM_LINEAR,

	//! count of all available interpolation modes
	EIM_COUNT
};

//! Interface for using some special functions of Skinned meshes
class ISkinnedMesh : public IAnimatedMesh
{
public:
	//! Gets joint count.
	/** \return Amount of joints in the skeletal animated mesh. */
	virtual u32 getJointCount() const = 0;

	//! Gets the name of a joint.
	/** \param number: Zero based index of joint. The last joint
	has the number getJointCount()-1;
	\return Name of joint and null if an error happened. */
	virtual const std::optional<std::string> &getJointName(u32 number) const = 0;

	//! Gets a joint number from its name
	/** \param name: Name of the joint.
	\return Number of the joint or std::nullopt if not found. */
	virtual std::optional<u32> getJointNumber(const std::string &name) const = 0;

	//! Use animation from another mesh
	/** The animation is linked (not copied) based on joint names
	so make sure they are unique.
	\return True if all joints in this mesh were
	matched up (empty names will not be matched, and it's case
	sensitive). Unmatched joints will not be animated. */
	virtual bool useAnimationFrom(const ISkinnedMesh *mesh) = 0;

	//! Update Normals when Animating
	/** \param on If false don't animate, which is faster.
	Else update normals, which allows for proper lighting of
	animated meshes. */
	virtual void updateNormalsWhenAnimating(bool on) = 0;

	//! Sets Interpolation Mode
	virtual void setInterpolationMode(E_INTERPOLATION_MODE mode) = 0;

	//! Animates this mesh's joints based on frame input
	virtual void animateMesh(f32 frame, f32 blend) = 0;

	//! Preforms a software skin on this mesh based of joint positions
	virtual void skinMesh() = 0;

	//! converts the vertex type of all meshbuffers to tangents.
	/** E.g. used for bump mapping. */
	virtual void convertMeshToTangents() = 0;

	//! Allows to enable hardware skinning.
	/* This feature is not implemented in Irrlicht yet */
	virtual bool setHardwareSkinning(bool on) = 0;

	//! Refreshes vertex data cached in joints such as positions and normals
	virtual void refreshJointCache() = 0;

	//! Moves the mesh into static position.
	virtual void resetAnimation() = 0;

	//! A vertex weight
	struct SWeight
	{
		//! Index of the mesh buffer
		u16 buffer_id; // I doubt 32bits is needed

		//! Index of the vertex
		u32 vertex_id; // Store global ID here

		//! Weight Strength/Percentage (0-1)
		f32 strength;

	private:
		//! Internal members used by CSkinnedMesh
		friend class CSkinnedMesh;
		char *Moved;
		core::vector3df StaticPos;
		core::vector3df StaticNormal;
	};

	//! Animation keyframe which describes a new position
	struct SPositionKey
	{
		f32 frame;
		core::vector3df position;
	};

	//! Animation keyframe which describes a new scale
	struct SScaleKey
	{
		f32 frame;
		core::vector3df scale;
	};

	//! Animation keyframe which describes a new rotation
	struct SRotationKey
	{
		f32 frame;
		core::quaternion rotation;
	};

	//! Joints
	struct SJoint
	{
		SJoint() :
				UseAnimationFrom(0), GlobalSkinningSpace(false),
				positionHint(-1), scaleHint(-1), rotationHint(-1)
		{
		}

		//! The name of this joint
		std::optional<std::string> Name;

		//! Local matrix of this joint
		core::matrix4 LocalMatrix;

		//! List of child joints
		core::array<SJoint *> Children;

		//! List of attached meshes
		core::array<u32> AttachedMeshes;

		//! Animation keys causing translation change
		core::array<SPositionKey> PositionKeys;

		//! Animation keys causing scale change
		core::array<SScaleKey> ScaleKeys;

		//! Animation keys causing rotation change
		core::array<SRotationKey> RotationKeys;

		//! Skin weights
		core::array<SWeight> Weights;

		//! Unnecessary for loaders, will be overwritten on finalize
		core::matrix4 GlobalMatrix; // loaders may still choose to set this (temporarily) to calculate absolute vertex data.
		core::matrix4 GlobalAnimatedMatrix;
		core::matrix4 LocalAnimatedMatrix;

		//! These should be set by loaders.
		core::vector3df Animatedposition;
		core::vector3df Animatedscale;
		core::quaternion Animatedrotation;

		// The .x and .gltf formats pre-calculate this
		std::optional<core::matrix4> GlobalInversedMatrix;
	private:
		//! Internal members used by CSkinnedMesh
		friend class CSkinnedMesh;

		SJoint *UseAnimationFrom;
		bool GlobalSkinningSpace;

		s32 positionHint;
		s32 scaleHint;
		s32 rotationHint;
	};

	// Interface for the mesh loaders (finalize should lock these functions, and they should have some prefix like loader_

	// these functions will use the needed arrays, set values, etc to help the loaders

	//! exposed for loaders: to add mesh buffers
	virtual core::array<SSkinMeshBuffer *> &getMeshBuffers() = 0;

	//! exposed for loaders: joints list
	virtual core::array<SJoint *> &getAllJoints() = 0;

	//! exposed for loaders: joints list
	virtual const core::array<SJoint *> &getAllJoints() const = 0;

	//! loaders should call this after populating the mesh
	virtual void finalize() = 0;

	//! Adds a new meshbuffer to the mesh, access it as last one
	virtual SSkinMeshBuffer *addMeshBuffer() = 0;

	//! Adds a new meshbuffer to the mesh, access it as last one
	virtual void addMeshBuffer(SSkinMeshBuffer *meshbuf) = 0;

	//! Adds a new joint to the mesh, access it as last one
	virtual SJoint *addJoint(SJoint *parent = 0) = 0;

	//! Adds a new weight to the mesh, access it as last one
	virtual SWeight *addWeight(SJoint *joint) = 0;

	//! Adds a new position key to the mesh, access it as last one
	virtual SPositionKey *addPositionKey(SJoint *joint) = 0;
	//! Adds a new scale key to the mesh, access it as last one
	virtual SScaleKey *addScaleKey(SJoint *joint) = 0;
	//! Adds a new rotation key to the mesh, access it as last one
	virtual SRotationKey *addRotationKey(SJoint *joint) = 0;

	//! Check if the mesh is non-animated
	virtual bool isStatic() = 0;
};

} // end namespace scene
} // end namespace irr
