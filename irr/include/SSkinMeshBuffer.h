// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMeshBuffer.h"
#include "CVertexBuffer.h"
#include "CIndexBuffer.h"
#include "IVertexBuffer.h"
#include "S3DVertex.h"
#include "irrTypes.h"
#include "vector3d.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <optional>
#include <vector>

namespace irr
{
namespace scene
{

struct MorphTargetDelta
{
	std::vector<core::vector3df> positions;
	std::vector<core::vector3df> normals;
	std::vector<core::vector2df> texcoords;

	void add(IVertexBuffer *buf, f32 weight)
	{
		size_t n = buf->getCount();
		void *data = buf->getData();
		switch (buf->getType()) {
		case video::EVT_TANGENTS:
			add<video::S3DVertexTangents>(data, n, weight);
		case video::EVT_2TCOORDS:
			add<video::S3DVertex2TCoords>(data, n, weight);
		case video::EVT_STANDARD:
		default:
			add<video::S3DVertex>(data, n, weight);
		}
		buf->setDirty();
	}

	void set(IVertexBuffer *buf)
	{
		size_t n = buf->getCount();
		void *data = buf->getData();
		switch (buf->getType()) {
		case video::EVT_TANGENTS:
			set<video::S3DVertexTangents>(data, n);
		case video::EVT_2TCOORDS:
			set<video::S3DVertex2TCoords>(data, n);
		case video::EVT_STANDARD:
		default:
			set<video::S3DVertex>(data, n);
		}
		buf->setDirty();
	}

	struct Flags {
		bool positions = false;
		bool normals = false;
		bool texcoords = false;
	};
	void get(IVertexBuffer *buf, Flags flags)
	{
		size_t n = buf->getCount();
		void *data = buf->getData();
		switch (buf->getType()) {
		case video::EVT_TANGENTS:
			get<video::S3DVertexTangents>(data, n, flags);
		case video::EVT_2TCOORDS:
			get<video::S3DVertex2TCoords>(data, n, flags);
		case video::EVT_STANDARD:
		default:
			get<video::S3DVertex>(data, n, flags);
		}
	}

private:

	template<class VertexType>
	void add(void *vertex_data, size_t n, f32 weight)
	{
		auto *vertices = static_cast<VertexType *>(vertex_data);
		if (!positions.empty()) {
			add<VertexType, core::vector3df>(&VertexType::Pos,
					vertices, positions.data(), n, weight);
		}
		if (!normals.empty()) {
			add<VertexType, core::vector3df>(&VertexType::Normal,
					vertices, normals.data(), n, weight);
		}
		if (!texcoords.empty()) {
			add<VertexType, core::vector2df>(&VertexType::TCoords,
					vertices, texcoords.data(), n, weight);
		}
	}

	template<class VertexType, class AttributeType>
	static void add(AttributeType VertexType::* attribute, VertexType *vertex_data,
			AttributeType *deltas, size_t n, f32 weight)
	{
		auto *vertices = static_cast<VertexType *>(vertex_data);
		for (size_t i = 0; i < n; ++i) {
			vertices[i].*attribute += weight * deltas[i];
		}
	}

	template<class VertexType>
	void set(void *vertex_data, size_t n)
	{
		auto *vertices = static_cast<VertexType *>(vertex_data);
		if (!positions.empty()) {
			set<VertexType, core::vector3df>(&VertexType::Pos,
					vertices, positions.data(), n);
		}
		if (!normals.empty()) {
			set<VertexType, core::vector3df>(&VertexType::Normal,
					vertices, normals.data(), n);
		}
		if (!texcoords.empty()) {
			set<VertexType, core::vector2df>(&VertexType::TCoords,
					vertices, texcoords.data(), n);
		}
	}

	template<class VertexType>
	void get(void *vertex_data, size_t n, Flags flags)
	{
		auto *vertices = static_cast<VertexType *>(vertex_data);
		if (flags.positions) {
			positions.reserve(n);
			get<VertexType, core::vector3df>(&VertexType::Pos,
					vertices, positions, n);
		}
		if (flags.normals) {
			normals.reserve(n);
			get<VertexType, core::vector3df>(&VertexType::Normal,
					vertices, normals, n);
		}
		if (flags.texcoords) {
			texcoords.reserve(n);
			get<VertexType, core::vector2df>(&VertexType::TCoords,
					vertices, texcoords, n);
		}
	}

	template<class VertexType, class AttributeType>
	static void set(AttributeType VertexType::* attribute, VertexType *vertex_data,
			AttributeType *deltas, size_t n)
	{
		auto *vertices = static_cast<VertexType *>(vertex_data);
		for (size_t i = 0; i < n; ++i) {
			vertices[i].*attribute = deltas[i];
		}
	}

	template<class VertexType, class AttributeType>
	static void get(AttributeType VertexType::* attribute, VertexType *vertex_data,
			std::vector<AttributeType> &deltas, size_t n)
	{
		auto *vertices = static_cast<VertexType *>(vertex_data);
		for (size_t i = 0; i < n; ++i) {
			deltas.push_back(vertices[i].*attribute);
		}
	}
};

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
	void boundingBoxNeedsRecalculated() { BoundingBoxNeedsRecalculated = true; }

	void setMorph(const std::optional<std::vector<f32>> &weights)
	{
		resetMorph();
		addMorph(weights.value_or(DefaultWeights));
	}

	void addMorph(const std::vector<f32> &weights)
	{
		assert(weights.size() == MorphTargets.size());
		for (size_t i = 0; i < weights.size(); ++i) {
			MorphTargets[i].add(getVertexBuffer(), weights[i]);
			if (!MorphTargets[i].positions.empty())
				boundingBoxNeedsRecalculated();
		}
	}

	void resetMorph()
	{
		MorphStaticPose.set(getVertexBuffer());
		if (!MorphStaticPose.positions.empty())
			boundingBoxNeedsRecalculated();
	}

	SVertexBufferTangents *Vertices_Tangents;
	SVertexBufferLightMap *Vertices_2TCoords;
	SVertexBuffer *Vertices_Standard;
	SIndexBuffer *Indices;

	core::matrix4 Transformation;

	// TODO consolidate with Static(Pos|Normal) in weights
	MorphTargetDelta MorphStaticPose;
	std::vector<MorphTargetDelta> MorphTargets;
	std::vector<f32> DefaultWeights;

	video::SMaterial Material;
	video::E_VERTEX_TYPE VertexType;

	core::aabbox3d<f32> BoundingBox{{0, 0, 0}};

	//! Primitive type used for rendering (triangles, lines, ...)
	E_PRIMITIVE_TYPE PrimitiveType;

	bool BoundingBoxNeedsRecalculated;
};

} // end namespace scene
} // end namespace irr
