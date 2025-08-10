// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "vector3d.h"
#include "aabbox3d.h"
#include "IAnimatedMesh.h"
#include "IMeshBuffer.h"
#include "SVertexManipulator.h"

namespace scene
{

struct SMesh;

//! An interface for easy manipulation of meshes.
/** Scale, set alpha value, flip surfaces, and so on. This exists for
fixing problems with wrong imported or exported meshes quickly after
loading. It is not intended for doing mesh modifications and/or
animations during runtime.
*/
class IMeshManipulator : public virtual IReferenceCounted
{
public:
	//! Recalculates all normals of the mesh.
	/** \param mesh: Mesh on which the operation is performed.
	\param smooth: If the normals shall be smoothed.
	\param angleWeighted: If the normals shall be smoothed in relation to their angles. More expensive, but also higher precision. */
	virtual void recalculateNormals(IMesh *mesh, bool smooth = false,
			bool angleWeighted = false) const = 0;

	//! Recalculates all normals of the mesh buffer.
	/** \param buffer: Mesh buffer on which the operation is performed.
	\param smooth: If the normals shall be smoothed.
	\param angleWeighted: If the normals shall be smoothed in relation to their angles. More expensive, but also higher precision. */
	virtual void recalculateNormals(IMeshBuffer *buffer,
			bool smooth = false, bool angleWeighted = false) const = 0;

	//! Scales the actual mesh, not a scene node.
	/** \param mesh Mesh on which the operation is performed.
	\param factor Scale factor for each axis. */
	void scale(IMesh *mesh, const core::vector3df &factor) const
	{
		apply(SVertexPositionScaleManipulator(factor), mesh, true);
	}

	//! Scales the actual meshbuffer, not a scene node.
	/** \param buffer Meshbuffer on which the operation is performed.
	\param factor Scale factor for each axis. */
	void scale(IMeshBuffer *buffer, const core::vector3df &factor) const
	{
		apply(SVertexPositionScaleManipulator(factor), buffer, true);
	}

	//! Clones a static IMesh into a modifiable SMesh.
	/** All meshbuffers in the returned SMesh
	are of type SMeshBuffer or SMeshBufferLightMap.
	\param mesh Mesh to copy.
	\return Cloned mesh. If you no longer need the
	cloned mesh, you should call SMesh::drop(). See
	IReferenceCounted::drop() for more information. */
	virtual SMesh *createMeshCopy(IMesh *mesh) const = 0;

	//! Apply a manipulator on the Meshbuffer
	/** \param func A functor defining the mesh manipulation.
	\param buffer The Meshbuffer to apply the manipulator to.
	\param boundingBoxUpdate Specifies if the bounding box should be updated during manipulation.
	\return True if the functor was successfully applied, else false. */
	template <typename Functor>
	bool apply(const Functor &func, IMeshBuffer *buffer, bool boundingBoxUpdate = false) const
	{
		return apply_(func, buffer, boundingBoxUpdate, func);
	}

	//! Apply a manipulator on the Mesh
	/** \param func A functor defining the mesh manipulation.
	\param mesh The Mesh to apply the manipulator to.
	\param boundingBoxUpdate Specifies if the bounding box should be updated during manipulation.
	\return True if the functor was successfully applied, else false. */
	template <typename Functor>
	bool apply(const Functor &func, IMesh *mesh, bool boundingBoxUpdate = false) const
	{
		if (!mesh)
			return true;
		bool result = true;
		core::aabbox3df bufferbox{{0, 0, 0}};
		for (u32 i = 0; i < mesh->getMeshBufferCount(); ++i) {
			result &= apply(func, mesh->getMeshBuffer(i), boundingBoxUpdate);
			if (boundingBoxUpdate) {
				if (0 == i)
					bufferbox.reset(mesh->getMeshBuffer(i)->getBoundingBox());
				else
					bufferbox.addInternalBox(mesh->getMeshBuffer(i)->getBoundingBox());
			}
		}
		if (boundingBoxUpdate)
			mesh->setBoundingBox(bufferbox);
		return result;
	}

protected:
	//! Apply a manipulator based on the type of the functor
	/** \param func A functor defining the mesh manipulation.
	\param buffer The Meshbuffer to apply the manipulator to.
	\param boundingBoxUpdate Specifies if the bounding box should be updated during manipulation.
	\param typeTest Unused parameter, which handles the proper call selection based on the type of the Functor which is passed in two times.
	\return True if the functor was successfully applied, else false. */
	template <typename Functor>
	bool apply_(const Functor &func, IMeshBuffer *buffer, bool boundingBoxUpdate, const IVertexManipulator &typeTest) const
	{
		if (!buffer)
			return true;

		core::aabbox3df bufferbox{{0, 0, 0}};
		for (u32 i = 0; i < buffer->getVertexCount(); ++i) {
			switch (buffer->getVertexType()) {
			case video::EVT_STANDARD: {
				video::S3DVertex *verts = (video::S3DVertex *)buffer->getVertices();
				func(verts[i]);
			} break;
			case video::EVT_2TCOORDS: {
				video::S3DVertex2TCoords *verts = (video::S3DVertex2TCoords *)buffer->getVertices();
				func(verts[i]);
			} break;
			case video::EVT_TANGENTS: {
				video::S3DVertexTangents *verts = (video::S3DVertexTangents *)buffer->getVertices();
				func(verts[i]);
			} break;
			}
			if (boundingBoxUpdate) {
				if (0 == i)
					bufferbox.reset(buffer->getPosition(0));
				else
					bufferbox.addInternalPoint(buffer->getPosition(i));
			}
		}
		if (boundingBoxUpdate)
			buffer->setBoundingBox(bufferbox);
		return true;
	}
};

} // end namespace scene
