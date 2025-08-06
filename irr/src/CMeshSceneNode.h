// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMeshSceneNode.h"
#include "IMesh.h"

namespace scene
{

class CMeshSceneNode : public IMeshSceneNode
{
public:
	//! constructor
	CMeshSceneNode(IMesh *mesh, ISceneNode *parent, ISceneManager *mgr, s32 id,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1.0f, 1.0f, 1.0f));

	//! destructor
	virtual ~CMeshSceneNode();

	//! frame
	void OnRegisterSceneNode() override;

	//! renders the node.
	void render() override;

	//! returns the axis aligned bounding box of this node
	const core::aabbox3d<f32> &getBoundingBox() const override;

	//! returns the material based on the zero based index i. To get the amount
	//! of materials used by this scene node, use getMaterialCount().
	//! This function is needed for inserting the node into the scene hierarchy on a
	//! optimal position for minimizing renderstate changes, but can also be used
	//! to directly modify the material of a scene node.
	video::SMaterial &getMaterial(u32 i) override;

	//! returns amount of materials used by this scene node.
	u32 getMaterialCount() const override;

	//! Returns type of the scene node
	ESCENE_NODE_TYPE getType() const override { return ESNT_MESH; }

	//! Sets a new mesh
	void setMesh(IMesh *mesh) override;

	//! Returns the current mesh
	IMesh *getMesh(void) override { return Mesh; }

	//! Sets if the scene node should not copy the materials of the mesh but use them directly.
	/** In this way it is possible to change the materials of a mesh
	causing all mesh scene nodes referencing this mesh to change, too.
	\param shared Flag if the materials shall be shared. */
	void setSharedMaterials(bool shared) override;

	//! Check if the scene node does not copy the materials of the mesh but uses them directly.
	/** This flag can be set by setSharedMaterials().
	\return Whether the materials are shared. */
	bool isSharedMaterials() const override;

	//! Creates a clone of this scene node and its children.
	ISceneNode *clone(ISceneNode *newParent = 0, ISceneManager *newManager = 0) override;

	//! Removes a child from this scene node.
	//! Implemented here, to be able to remove the shadow properly, if there is one,
	//! or to remove attached child.
	bool removeChild(ISceneNode *child) override;

protected:
	void copyMaterials();

	std::vector<video::SMaterial> Materials;
	core::aabbox3d<f32> Box{{0, 0, 0}};

	IMesh *Mesh;

	s32 PassCount;
	bool SharedMaterials;
};

} // end namespace scene
