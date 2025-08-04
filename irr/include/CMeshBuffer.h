// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "IMeshBuffer.h"
#include "CVertexBuffer.h"
#include "CIndexBuffer.h"

namespace scene
{
//! Template implementation of the IMeshBuffer interface
template <class T>
class CMeshBuffer final : public IMeshBuffer
{
public:
	//! Default constructor for empty meshbuffer
	CMeshBuffer() :
			PrimitiveType(EPT_TRIANGLES)
	{
		Vertices = new CVertexBuffer<T>();
		Indices = new SIndexBuffer();
	}

	~CMeshBuffer()
	{
		Vertices->drop();
		Indices->drop();
	}

	//! Get material of this meshbuffer
	/** \return Material of this buffer */
	const video::SMaterial &getMaterial() const override
	{
		return Material;
	}

	//! Get material of this meshbuffer
	/** \return Material of this buffer */
	video::SMaterial &getMaterial() override
	{
		return Material;
	}

	const scene::IVertexBuffer *getVertexBuffer() const override
	{
		return Vertices;
	}

	scene::IVertexBuffer *getVertexBuffer() override
	{
		return Vertices;
	}

	const scene::IIndexBuffer *getIndexBuffer() const override
	{
		return Indices;
	}

	scene::IIndexBuffer *getIndexBuffer() override
	{
		return Indices;
	}

	//! Get the axis aligned bounding box
	/** \return Axis aligned bounding box of this buffer. */
	const core::aabbox3d<f32> &getBoundingBox() const override
	{
		return BoundingBox;
	}

	//! Set the axis aligned bounding box
	/** \param box New axis aligned bounding box for this buffer. */
	//! set user axis aligned bounding box
	void setBoundingBox(const core::aabbox3df &box) override
	{
		BoundingBox = box;
	}

	//! Recalculate the bounding box.
	/** should be called if the mesh changed. */
	void recalculateBoundingBox() override
	{
		if (Vertices->getCount()) {
			BoundingBox.reset(Vertices->getPosition(0));
			const u32 vsize = Vertices->getCount();
			for (u32 i = 1; i < vsize; ++i)
				BoundingBox.addInternalPoint(Vertices->getPosition(i));
		} else
			BoundingBox.reset(0, 0, 0);
	}

	//! Append the vertices and indices to the current buffer
	void append(const void *const vertices, u32 numVertices, const u16 *const indices, u32 numIndices) override
	{
		if (vertices == getVertices())
			return;

		const u32 vertexCount = getVertexCount();
		const u32 indexCount = getIndexCount();

		auto *vt = static_cast<const T *>(vertices);
		Vertices->Data.insert(Vertices->Data.end(), vt, vt + numVertices);
		for (u32 i = vertexCount; i < getVertexCount(); i++)
			BoundingBox.addInternalPoint(Vertices->getPosition(i));

		Indices->Data.insert(Indices->Data.end(), indices, indices + numIndices);
		if (vertexCount != 0) {
			for (u32 i = indexCount; i < getIndexCount(); i++)
				Indices->Data[i] += vertexCount;
		}
	}

	//! Describe what kind of primitive geometry is used by the meshbuffer
	void setPrimitiveType(E_PRIMITIVE_TYPE type) override
	{
		PrimitiveType = type;
	}

	//! Get the kind of primitive geometry which is used by the meshbuffer
	E_PRIMITIVE_TYPE getPrimitiveType() const override
	{
		return PrimitiveType;
	}

	//! Material for this meshbuffer.
	video::SMaterial Material;
	//! Vertex buffer
	CVertexBuffer<T> *Vertices;
	//! Index buffer
	SIndexBuffer *Indices;
	//! Bounding box of this meshbuffer.
	core::aabbox3d<f32> BoundingBox{{0, 0, 0}};
	//! Primitive type used for rendering (triangles, lines, ...)
	E_PRIMITIVE_TYPE PrimitiveType;
};

//! Standard meshbuffer
typedef CMeshBuffer<video::S3DVertex> SMeshBuffer;
//! Meshbuffer with two texture coords per vertex, e.g. for lightmaps
typedef CMeshBuffer<video::S3DVertex2TCoords> SMeshBufferLightMap;
//! Meshbuffer with vertices having tangents stored, e.g. for normal mapping
typedef CMeshBuffer<video::S3DVertexTangents> SMeshBufferTangents;
} // end namespace scene
