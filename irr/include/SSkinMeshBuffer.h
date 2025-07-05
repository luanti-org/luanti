// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMeshBuffer.h"
#include "CVertexBuffer.h"
#include "CIndexBuffer.h"
#include "S3DVertex.h"
#include <cassert>

namespace scene
{

//! A mesh buffer able to choose between S3DVertex2TCoords, S3DVertex and S3DVertexTangents at runtime
struct SSkinMeshBuffer final : public IMeshBuffer
{
	//! Default constructor
	SSkinMeshBuffer(video::E_VERTEX_TYPE vt = video::EVT_STANDARD) :
			VertexType(vt), PrimitiveType(EPT_TRIANGLES),
			BoundingBoxNeedsRecalculated(true)
	{
		Vertices_Tangents = new SVertexBufferTangents();
		Vertices_2TCoords = new SVertexBufferLightMap();
		Vertices_Standard = new SVertexBuffer();
		Indices = new SIndexBuffer();
	}

	//! Constructor for standard vertices
	SSkinMeshBuffer(std::vector<video::S3DVertex> &&vertices, std::vector<u16> &&indices) :
			SSkinMeshBuffer()
	{
		Vertices_Standard->Data = std::move(vertices);
		Indices->Data = std::move(indices);
	}

	~SSkinMeshBuffer()
	{
		Vertices_Tangents->drop();
		Vertices_2TCoords->drop();
		Vertices_Standard->drop();
		Indices->drop();
	}

	//! Get Material of this buffer.
	const video::SMaterial &getMaterial() const override
	{
		return Material;
	}

	//! Get Material of this buffer.
	video::SMaterial &getMaterial() override
	{
		return Material;
	}

	const scene::IVertexBuffer *getVertexBuffer() const override
	{
		switch (VertexType) {
		case video::EVT_2TCOORDS:
			return Vertices_2TCoords;
		case video::EVT_TANGENTS:
			return Vertices_Tangents;
		default:
			return Vertices_Standard;
		}
	}

	scene::IVertexBuffer *getVertexBuffer() override
	{
		switch (VertexType) {
		case video::EVT_2TCOORDS:
			return Vertices_2TCoords;
		case video::EVT_TANGENTS:
			return Vertices_Tangents;
		default:
			return Vertices_Standard;
		}
	}

	const scene::IIndexBuffer *getIndexBuffer() const override
	{
		return Indices;
	}

	scene::IIndexBuffer *getIndexBuffer() override
	{
		return Indices;
	}

	//! Get standard vertex at given index
	virtual video::S3DVertex *getVertex(u32 index)
	{
		switch (VertexType) {
		case video::EVT_2TCOORDS:
			return &Vertices_2TCoords->Data[index];
		case video::EVT_TANGENTS:
			return &Vertices_Tangents->Data[index];
		default:
			return &Vertices_Standard->Data[index];
		}
	}

	//! Get bounding box
	const core::aabbox3d<f32> &getBoundingBox() const override
	{
		return BoundingBox;
	}

	//! Set bounding box
	void setBoundingBox(const core::aabbox3df &box) override
	{
		BoundingBox = box;
	}

private:
	template <typename T> void recalculateBoundingBox(const CVertexBuffer<T> *buf)
	{
		if (!buf->getCount()) {
			BoundingBox.reset(0, 0, 0);
		} else {
			auto &vertices = buf->Data;
			BoundingBox.reset(vertices[0].Pos);
			for (size_t i = 1; i < vertices.size(); ++i)
				BoundingBox.addInternalPoint(vertices[i].Pos);
		}
	}

	template <typename T1, typename T2> static void copyVertex(const T1 &src, T2 &dst)
	{
		dst.Pos = src.Pos;
		dst.Normal = src.Normal;
		dst.Color = src.Color;
		dst.TCoords = src.TCoords;
	}
public:

	//! Recalculate bounding box
	void recalculateBoundingBox() override
	{
		if (!BoundingBoxNeedsRecalculated)
			return;

		BoundingBoxNeedsRecalculated = false;

		switch (VertexType) {
		case video::EVT_STANDARD: {
			recalculateBoundingBox(Vertices_Standard);
			break;
		}
		case video::EVT_2TCOORDS: {
			recalculateBoundingBox(Vertices_2TCoords);
			break;
		}
		case video::EVT_TANGENTS: {
			recalculateBoundingBox(Vertices_Tangents);
			break;
		}
		}
	}

	//! Convert to 2tcoords vertex type
	void convertTo2TCoords()
	{
		if (VertexType == video::EVT_STANDARD) {
			video::S3DVertex2TCoords Vertex;
			for (const auto &Vertex_Standard : Vertices_Standard->Data) {
				copyVertex(Vertex_Standard, Vertex);
				Vertices_2TCoords->Data.push_back(Vertex);
			}
			Vertices_Standard->Data.clear();
			VertexType = video::EVT_2TCOORDS;
		}
	}

	//! Convert to tangents vertex type
	void convertToTangents()
	{
		if (VertexType == video::EVT_STANDARD) {
			video::S3DVertexTangents Vertex;
			for (const auto &Vertex_Standard : Vertices_Standard->Data) {
				copyVertex(Vertex_Standard, Vertex);
				Vertices_Tangents->Data.push_back(Vertex);
			}
			Vertices_Standard->Data.clear();
			VertexType = video::EVT_TANGENTS;
		} else if (VertexType == video::EVT_2TCOORDS) {
			video::S3DVertexTangents Vertex;
			for (const auto &Vertex_2TCoords : Vertices_2TCoords->Data) {
				copyVertex(Vertex_2TCoords, Vertex);
				Vertices_Tangents->Data.push_back(Vertex);
			}
			Vertices_2TCoords->Data.clear();
			VertexType = video::EVT_TANGENTS;
		}
	}

	//! append the vertices and indices to the current buffer
	void append(const void *const vertices, u32 numVertices, const u16 *const indices, u32 numIndices) override
	{
		assert(false);
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

	//! Call this after changing the positions of any vertex.
	void boundingBoxNeedsRecalculated(void) { BoundingBoxNeedsRecalculated = true; }

	SVertexBufferTangents *Vertices_Tangents;
	SVertexBufferLightMap *Vertices_2TCoords;
	SVertexBuffer *Vertices_Standard;
	SIndexBuffer *Indices;

	core::matrix4 Transformation;

	video::SMaterial Material;
	video::E_VERTEX_TYPE VertexType;

	core::aabbox3d<f32> BoundingBox{{0, 0, 0}};

	//! Primitive type used for rendering (triangles, lines, ...)
	E_PRIMITIVE_TYPE PrimitiveType;

	bool BoundingBoxNeedsRecalculated;
};

} // end namespace scene
