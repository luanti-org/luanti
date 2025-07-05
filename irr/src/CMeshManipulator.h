// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMeshManipulator.h"

namespace scene
{

//! An interface for easy manipulation of meshes.
/** Scale, set alpha value, flip surfaces, and so on. This exists for fixing
problems with wrong imported or exported meshes quickly after loading. It is
not intended for doing mesh modifications and/or animations during runtime.
*/
class CMeshManipulator : public IMeshManipulator
{
public:
	//! Recalculates all normals of the mesh.
	/** \param mesh: Mesh on which the operation is performed.
	\param smooth: Whether to use smoothed normals. */
	void recalculateNormals(scene::IMesh *mesh, bool smooth = false, bool angleWeighted = false) const override;

	//! Recalculates all normals of the mesh buffer.
	/** \param buffer: Mesh buffer on which the operation is performed.
	\param smooth: Whether to use smoothed normals. */
	void recalculateNormals(IMeshBuffer *buffer, bool smooth = false, bool angleWeighted = false) const override;

	//! Clones a static IMesh into a modifiable SMesh.
	SMesh *createMeshCopy(scene::IMesh *mesh) const override;
};

} // end namespace scene
