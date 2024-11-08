// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "IAnimatedMesh.h"
#include "IMesh.h"
#include "aabbox3d.h"

namespace irr
{
namespace scene
{

//! Simple implementation of the IAnimatedMesh interface.
struct SAnimatedMesh final : public IAnimatedMesh
{
	//! constructor
	SAnimatedMesh(scene::IMesh *mesh = 0, scene::E_ANIMATED_MESH_TYPE type = scene::EAMT_UNKNOWN) :
			IAnimatedMesh(), FramesPerSecond(25.f), Type(type)
	{
#ifdef _DEBUG
		setDebugName("SAnimatedMesh");
#endif
		addMesh(mesh);
		recalculateBoundingBox();
	}

	//! destructor
	virtual ~SAnimatedMesh()
	{
		// drop meshes
		for (auto *mesh : Meshes)
			mesh->drop();
	}

	f32 getMaxFrameNumber() const override
	{
		return static_cast<f32>(Meshes.size() - 1);
	}

	//! Gets the default animation speed of the animated mesh.
	/** \return Amount of frames per second. If the amount is 0, it is a static, non animated mesh. */
	f32 getAnimationSpeed() const override
	{
		return FramesPerSecond;
	}

	//! Gets the frame count of the animated mesh.
	/** \param fps Frames per second to play the animation with. If the amount is 0, it is not animated.
	The actual speed is set in the scene node the mesh is instantiated in.*/
	void setAnimationSpeed(f32 fps) override
	{
		FramesPerSecond = fps;
	}

	//! Returns the IMesh interface for a frame.
	/** \param frame: Frame number as zero based index.
	\return The animated mesh based for the given frame */
	IMesh *getMesh(f32 frame) override
	{
		if (Meshes.empty())
			return nullptr;

		return Meshes[static_cast<s32>(frame)];
	}

	//! adds a Mesh
	void addMesh(IMesh *mesh)
	{
		if (mesh) {
			mesh->grab();
			Meshes.push_back(mesh);
		}
	}

	//! Returns an axis aligned bounding box of the mesh.
	/** \return A bounding box of this mesh is returned. */
	const core::aabbox3d<f32> &getBoundingBox() const override
	{
		return Box;
	}

	//! set user axis aligned bounding box
	void setBoundingBox(const core::aabbox3df &box) override
	{
		Box = box;
	}

	//! Recalculates the bounding box.
	void recalculateBoundingBox()
	{
		Box.reset(0, 0, 0);

		if (Meshes.empty())
			return;

		Box = Meshes[0]->getBoundingBox();

		for (u32 i = 1; i < Meshes.size(); ++i)
			Box.addInternalBox(Meshes[i]->getBoundingBox());
	}

	//! Returns the type of the animated mesh.
	E_ANIMATED_MESH_TYPE getMeshType() const override
	{
		return Type;
	}

	//! returns amount of mesh buffers.
	u32 getMeshBufferCount() const override
	{
		if (Meshes.empty())
			return 0;

		return Meshes[0]->getMeshBufferCount();
	}

	//! returns pointer to a mesh buffer
	IMeshBuffer *getMeshBuffer(u32 nr) const override
	{
		if (Meshes.empty())
			return 0;

		return Meshes[0]->getMeshBuffer(nr);
	}

	//! Returns pointer to a mesh buffer which fits a material
	/** \param material: material to search for
	\return Returns the pointer to the mesh buffer or
	NULL if there is no such mesh buffer. */
	IMeshBuffer *getMeshBuffer(const video::SMaterial &material) const override
	{
		if (Meshes.empty())
			return 0;

		return Meshes[0]->getMeshBuffer(material);
	}

	//! set the hardware mapping hint, for driver
	void setHardwareMappingHint(E_HARDWARE_MAPPING newMappingHint, E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX) override
	{
		for (u32 i = 0; i < Meshes.size(); ++i)
			Meshes[i]->setHardwareMappingHint(newMappingHint, buffer);
	}

	//! flags the meshbuffer as changed, reloads hardware buffers
	void setDirty(E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX) override
	{
		for (u32 i = 0; i < Meshes.size(); ++i)
			Meshes[i]->setDirty(buffer);
	}

	//! All meshes defining the animated mesh
	std::vector<IMesh *> Meshes;

	//! The bounding box of this mesh
	core::aabbox3d<f32> Box;

	//! Default animation speed of this mesh.
	f32 FramesPerSecond;

	//! The type of the mesh.
	E_ANIMATED_MESH_TYPE Type;
};

} // end namespace scene
} // end namespace irr
