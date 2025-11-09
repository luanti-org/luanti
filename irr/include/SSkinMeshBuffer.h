// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMeshBuffer.h"
#include "CVertexBuffer.h"
#include "CIndexBuffer.h"
#include "IVertexBuffer.h"
#include "S3DVertex.h"
#include "vector3d.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <numeric>
#include <optional>

namespace scene
{

struct WeightBuffer
{
	constexpr static u16 MAX_WEIGHTS_PER_VERTEX = 4;
	size_t n_verts;
	std::unique_ptr<u16[]> joint_idxs;
	std::unique_ptr<f32[]> weights;

	std::optional<std::vector<u32>> animated_vertices;

	// A bit of a hack for now: Store static positions here so we can use them for skinning.
	// Ideally we might want a design where we do not mutate the original vertex buffer at all.
	std::unique_ptr<core::vector3df[]> static_positions;
	std::unique_ptr<core::vector3df[]> static_normals;

	WeightBuffer(size_t n_verts) :
		n_verts(n_verts),
		joint_idxs(std::make_unique<u16[]>(MAX_WEIGHTS_PER_VERTEX * n_verts)),
		weights(std::make_unique<f32[]>(MAX_WEIGHTS_PER_VERTEX * n_verts))
	{}

	u16 *getJointIndices(u32 vertex_id)
	{ return &joint_idxs[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }
	const u16 *getJointIndices(u32 vertex_id) const
	{ return &joint_idxs[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }

	f32 *getWeights(u32 vertex_id)
	{ return &weights[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }
	const f32 *getWeights(u32 vertex_id) const
	{ return &weights[vertex_id * MAX_WEIGHTS_PER_VERTEX]; }

	void addWeight(u32 vertex_id, u16 joint_id, f32 weight)
	{
		assert(vertex_id < n_verts);
		assert(weight >= 0.f);
		f32 *weights = getWeights(vertex_id);
		f32 *min_weight = std::min_element(weights, weights + MAX_WEIGHTS_PER_VERTEX);
		if (*min_weight > weight)
			return;

		*min_weight = weight;
		getJointIndices(vertex_id)[min_weight - weights] = joint_id;
	}

	void skinVertex(u32 vertex_id, core::vector3df &pos, core::vector3df &normal,
			const std::vector<core::matrix4> &joint_transforms) const
	{
		const u16 *joint_idxs = getJointIndices(vertex_id);
		const f32 *weights = getWeights(vertex_id);
		f32 total_weight = 0.f;
		core::vector3df skinned_pos;
		core::vector3df skinned_normal;
		for (u16 i = 0; i < MAX_WEIGHTS_PER_VERTEX; ++i) {
			u16 joint_id = joint_idxs[i];
			f32 weight = weights[i];
			if (weight <= 0.f)
				continue;

			const auto &transform = joint_transforms[joint_id];
			core::vector3df transformed_pos = pos;
			transform.transformVect(transformed_pos);
			skinned_pos += weight * transformed_pos;
			skinned_normal += weight * transform.rotateAndScaleVect(normal);
			total_weight += weight;
		}
		if (core::equals(total_weight, 0.f))
			return;

		pos = skinned_pos;
		// Need to renormalize normal after potentially scaling
		normal = skinned_normal.normalize();
	}

	/// @note src and dst can be the same buffer
	void skin(IVertexBuffer *dst,
			const std::vector<core::matrix4> &joint_transforms) const
	{
		assert(animated_vertices.has_value());
		for (u32 i = 0; i < animated_vertices->size(); ++i) {

			u32 vertex_id = (*animated_vertices)[i];
			auto pos = static_positions[i];
			auto normal = static_normals[i];
			skinVertex(vertex_id, pos, normal, joint_transforms);
			dst->getPosition(vertex_id) = pos;
			dst->getNormal(vertex_id) = normal;
		}
		if (!animated_vertices->empty())
			dst->setDirty();
	}

	/// Normalizes weights so that they sum to 1.0 per vertex,
	/// stores which vertices are animated.
	void finalize() {
		assert(!animated_vertices.has_value());
		animated_vertices.emplace();
		for (u32 i = 0; i < n_verts; ++i) {
			auto *weights_i = getWeights(i);
			f32 total_weight = std::accumulate(weights_i, weights_i + MAX_WEIGHTS_PER_VERTEX, 0.0f);
			if (core::equals(total_weight, 0.0f)) {
				std::fill(weights_i, weights_i + MAX_WEIGHTS_PER_VERTEX, 0.0f);
				continue;
			}
			animated_vertices->emplace_back(i);
			if (core::equals(total_weight, 1.0f))
				continue;
			for (u16 j = 0; j < MAX_WEIGHTS_PER_VERTEX; ++j)
				weights_i[j] /= total_weight;
		}
		animated_vertices->shrink_to_fit();
	}

	void updateStaticPose(const IVertexBuffer *vbuf)
	{
		static_normals = std::make_unique<core::vector3df[]>(animated_vertices->size());
		static_positions = std::make_unique<core::vector3df[]>(animated_vertices->size());
		for (size_t idx = 0; idx < animated_vertices->size(); ++idx) {
			u32 vertex_id = (*animated_vertices)[idx];
			static_positions[idx] = vbuf->getPosition(vertex_id);
			static_normals[idx] = vbuf->getNormal(vertex_id);
		}
	}

	void resetToStatic(IVertexBuffer *vbuf) const
	{
		assert(animated_vertices.has_value());
		for (size_t idx = 0; idx < animated_vertices->size(); ++idx) {
			u32 vertex_id = (*animated_vertices)[idx];
			vbuf->getPosition(vertex_id) = static_positions[idx];
			vbuf->getNormal(vertex_id) = static_normals[idx];
		}
		if (!animated_vertices->empty())
			vbuf->setDirty();
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
	video::S3DVertex *getVertex(u32 index)
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

	std::optional<WeightBuffer> Weights;

	core::matrix4 Transformation;

	video::SMaterial Material;
	video::E_VERTEX_TYPE VertexType;

	core::aabbox3d<f32> BoundingBox{{0, 0, 0}};

	//! Primitive type used for rendering (triangles, lines, ...)
	E_PRIMITIVE_TYPE PrimitiveType;

	bool BoundingBoxNeedsRecalculated;
};

} // end namespace scene
