// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "SMaterial.h"
#include "aabbox3d.h"
#include "IVertexBuffer.h"
#include "IIndexBuffer.h"
#include "EHardwareBufferFlags.h"
#include "EPrimitiveTypes.h"
#include <cassert>

namespace scene
{
//! Struct for holding a mesh with a single material.
/** A part of an IMesh which has the same material on each face of that
group. Logical groups of an IMesh need not be put into separate mesh
buffers, but can be. Separately animated parts of the mesh must be put
into separate mesh buffers.
Some mesh buffer implementations have limitations on the number of
vertices the buffer can hold. In that case, logical grouping can help.
Moreover, the number of vertices should be optimized for the GPU upload,
which often depends on the type of gfx card. Typical figures are
1000-10000 vertices per buffer.
SMeshBuffer is a simple implementation of a MeshBuffer, which supports
up to 65535 vertices.

Since meshbuffers are used for drawing, and hence will be exposed
to the driver, chances are high that they are grab()'ed from somewhere.
It's therefore required to dynamically allocate meshbuffers which are
passed to a video driver and only drop the buffer once it's not used in
the current code block anymore.
*/
class IMeshBuffer : public virtual IReferenceCounted
{
public:
	//! Get the material of this meshbuffer
	/** \return Material of this buffer. */
	virtual video::SMaterial &getMaterial() = 0;

	//! Get the material of this meshbuffer
	/** \return Material of this buffer. */
	virtual const video::SMaterial &getMaterial() const = 0;

	/// Get the vertex buffer
	virtual const scene::IVertexBuffer *getVertexBuffer() const = 0;

	/// Get the vertex buffer
	virtual scene::IVertexBuffer *getVertexBuffer() = 0;

	/// Get the index buffer
	virtual const scene::IIndexBuffer *getIndexBuffer() const = 0;

	/// Get the index buffer
	virtual scene::IIndexBuffer *getIndexBuffer() = 0;

	//! Get the axis aligned bounding box of this meshbuffer.
	/** \return Axis aligned bounding box of this buffer. */
	virtual const core::aabbox3df &getBoundingBox() const = 0;

	//! Set axis aligned bounding box
	/** \param box User defined axis aligned bounding box to use
	for this buffer. */
	virtual void setBoundingBox(const core::aabbox3df &box) = 0;

	//! Recalculates the bounding box. Should be called if the mesh changed.
	virtual void recalculateBoundingBox() = 0;

	//! Append the vertices and indices to the current buffer
	/** Only works for compatible vertex types.
	\param vertices Pointer to a vertex array.
	\param numVertices Number of vertices in the array.
	\param indices Pointer to index array.
	\param numIndices Number of indices in array. */
	virtual void append(const void *const vertices, u32 numVertices, const u16 *const indices, u32 numIndices) = 0;

	/* Leftover functions that are now just helpers for accessing the respective buffer. */

	//! Get type of vertex data which is stored in this meshbuffer.
	/** \return Vertex type of this buffer. */
	inline video::E_VERTEX_TYPE getVertexType() const
	{
		return getVertexBuffer()->getType();
	}

	//! Get access to vertex data. The data is an array of vertices.
	/** Which vertex type is used can be determined by getVertexType().
	\return Pointer to array of vertices. */
	inline const void *getVertices() const
	{
		return getVertexBuffer()->getData();
	}

	//! Get access to vertex data. The data is an array of vertices.
	/** Which vertex type is used can be determined by getVertexType().
	\return Pointer to array of vertices. */
	inline void *getVertices()
	{
		return getVertexBuffer()->getData();
	}

	//! Get amount of vertices in meshbuffer.
	/** \return Number of vertices in this buffer. */
	inline u32 getVertexCount() const
	{
		return getVertexBuffer()->getCount();
	}

	//! Get type of index data which is stored in this meshbuffer.
	/** \return Index type of this buffer. */
	inline video::E_INDEX_TYPE getIndexType() const
	{
		return getIndexBuffer()->getType();
	}

	//! Get access to indices.
	/** \return Pointer to indices array. */
	inline const u16 *getIndices() const
	{
		assert(getIndexBuffer()->getType() == video::EIT_16BIT);
		return static_cast<const u16*>(getIndexBuffer()->getData());
	}

	//! Get access to indices.
	/** \return Pointer to indices array. */
	inline u16 *getIndices()
	{
		assert(getIndexBuffer()->getType() == video::EIT_16BIT);
		return static_cast<u16*>(getIndexBuffer()->getData());
	}

	//! Get amount of indices in this meshbuffer.
	/** \return Number of indices in this buffer. */
	inline u32 getIndexCount() const
	{
		return getIndexBuffer()->getCount();
	}

	//! returns position of vertex i
	inline const core::vector3df &getPosition(u32 i) const
	{
		return getVertexBuffer()->getPosition(i);
	}

	//! returns position of vertex i
	inline core::vector3df &getPosition(u32 i)
	{
		return getVertexBuffer()->getPosition(i);
	}

	//! returns normal of vertex i
	inline const core::vector3df &getNormal(u32 i) const
	{
		return getVertexBuffer()->getNormal(i);
	}

	//! returns normal of vertex i
	inline core::vector3df &getNormal(u32 i)
	{
		return getVertexBuffer()->getNormal(i);
	}

	//! returns texture coord of vertex i
	inline const core::vector2df &getTCoords(u32 i) const
	{
		return getVertexBuffer()->getTCoords(i);
	}

	//! returns texture coord of vertex i
	inline core::vector2df &getTCoords(u32 i)
	{
		return getVertexBuffer()->getTCoords(i);
	}

	//! set the hardware mapping hint, for driver
	inline void setHardwareMappingHint(E_HARDWARE_MAPPING newMappingHint, E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX)
	{
		if (buffer == EBT_VERTEX_AND_INDEX || buffer == EBT_VERTEX)
			getVertexBuffer()->setHardwareMappingHint(newMappingHint);
		if (buffer == EBT_VERTEX_AND_INDEX || buffer == EBT_INDEX)
			getIndexBuffer()->setHardwareMappingHint(newMappingHint);
	}

	//! flags the meshbuffer as changed, reloads hardware buffers
	inline void setDirty(E_BUFFER_TYPE buffer = EBT_VERTEX_AND_INDEX)
	{
		if (buffer == EBT_VERTEX_AND_INDEX || buffer == EBT_VERTEX)
			getVertexBuffer()->setDirty();
		if (buffer == EBT_VERTEX_AND_INDEX || buffer == EBT_INDEX)
			getIndexBuffer()->setDirty();
	}

	/* End helpers */

	//! Describe what kind of primitive geometry is used by the meshbuffer
	/** Note: Default is EPT_TRIANGLES. Using other types is fine for rendering.
	But meshbuffer manipulation functions might expect type EPT_TRIANGLES
	to work correctly. Also mesh writers will generally fail (badly!) with other
	types than EPT_TRIANGLES. */
	virtual void setPrimitiveType(E_PRIMITIVE_TYPE type) = 0;

	//! Get the kind of primitive geometry which is used by the meshbuffer
	virtual E_PRIMITIVE_TYPE getPrimitiveType() const = 0;

	//! Calculate how many geometric primitives are used by this meshbuffer
	u32 getPrimitiveCount() const
	{
		return getIndexBuffer()->getPrimitiveCount(getPrimitiveType());
	}

	//! Calculate size of vertices and indices in memory
	size_t getSize() const
	{
		size_t ret = 0;
		switch (getVertexType()) {
			case video::EVT_STANDARD:
				ret += sizeof(video::S3DVertex) * getVertexCount();
				break;
			case video::EVT_2TCOORDS:
				ret += sizeof(video::S3DVertex2TCoords) * getVertexCount();
				break;
			case video::EVT_TANGENTS:
				ret += sizeof(video::S3DVertexTangents) * getVertexCount();
				break;
		}
		switch (getIndexType()) {
			case video::EIT_16BIT:
				ret += sizeof(u16) * getIndexCount();
				break;
			case video::EIT_32BIT:
				ret += sizeof(u32) * getIndexCount();
				break;
		}
		return ret;
	}
};

} // end namespace scene
