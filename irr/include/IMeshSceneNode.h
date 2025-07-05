// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneNode.h"

namespace scene
{

class IMesh;

//! A scene node displaying a static mesh
class IMeshSceneNode : public ISceneNode
{
public:
	//! Constructor
	/** Use setMesh() to set the mesh to display.
	 */
	IMeshSceneNode(ISceneNode *parent, ISceneManager *mgr, s32 id,
			const core::vector3df &position = core::vector3df(0, 0, 0),
			const core::vector3df &rotation = core::vector3df(0, 0, 0),
			const core::vector3df &scale = core::vector3df(1, 1, 1)) :
			ISceneNode(parent, mgr, id, position, rotation, scale) {}

	//! Sets a new mesh to display
	/** \param mesh Mesh to display. */
	virtual void setMesh(IMesh *mesh) = 0;

	//! Get the currently defined mesh for display.
	/** \return Pointer to mesh which is displayed by this node. */
	virtual IMesh *getMesh() = 0;

	//! Sets if the scene node should not copy the materials of the mesh but use them directly.
	/** In this way it is possible to change the materials of a mesh
	causing all mesh scene nodes referencing this mesh to change, too.
	\param shared Flag if the materials shall be shared. */
	virtual void setSharedMaterials(bool shared) = 0;

	//! Check if the scene node does not copy the materials of the mesh but uses them directly.
	/** This flag can be set by setSharedMaterials().
	\return Whether the materials are shared. */
	virtual bool isSharedMaterials() const = 0;
};

} // end namespace scene
