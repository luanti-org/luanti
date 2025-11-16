// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CMeshManipulator.h"
#include "SkinnedMesh.h"
#include "SMesh.h"
#include "CMeshBuffer.h"
#include "os.h"

#include <cassert>

namespace scene
{

static inline core::vector3df getAngleWeight(const core::vector3df &v1,
		const core::vector3df &v2,
		const core::vector3df &v3)
{
	// Calculate this triangle's weight for each of its three vertices
	// start by calculating the lengths of its sides
	const f32 a = v2.getDistanceFromSQ(v3);
	const f32 asqrt = sqrtf(a);
	const f32 b = v1.getDistanceFromSQ(v3);
	const f32 bsqrt = sqrtf(b);
	const f32 c = v1.getDistanceFromSQ(v2);
	const f32 csqrt = sqrtf(c);

	// use them to find the angle at each vertex
	return core::vector3df(
			acosf((b + c - a) / (2.f * bsqrt * csqrt)),
			acosf((-b + c + a) / (2.f * asqrt * csqrt)),
			acosf((b - c + a) / (2.f * bsqrt * asqrt)));
}

namespace
{
template <typename T>
void recalculateNormalsT(IMeshBuffer *buffer, bool smooth, bool angleWeighted)
{
	const u32 vtxcnt = buffer->getVertexCount();
	const u32 idxcnt = buffer->getIndexCount();
	const T *idx = reinterpret_cast<T *>(buffer->getIndices());

	if (!smooth) {
		for (u32 i = 0; i < idxcnt; i += 3) {
			const core::vector3df &v1 = buffer->getPosition(idx[i + 0]);
			const core::vector3df &v2 = buffer->getPosition(idx[i + 1]);
			const core::vector3df &v3 = buffer->getPosition(idx[i + 2]);
			const core::vector3df normal = core::plane3d<f32>(v1, v2, v3).Normal;
			buffer->getNormal(idx[i + 0]) = normal;
			buffer->getNormal(idx[i + 1]) = normal;
			buffer->getNormal(idx[i + 2]) = normal;
		}
	} else {
		u32 i;

		for (i = 0; i != vtxcnt; ++i)
			buffer->getNormal(i).set(0.f, 0.f, 0.f);

		for (i = 0; i < idxcnt; i += 3) {
			const core::vector3df &v1 = buffer->getPosition(idx[i + 0]);
			const core::vector3df &v2 = buffer->getPosition(idx[i + 1]);
			const core::vector3df &v3 = buffer->getPosition(idx[i + 2]);
			const core::vector3df normal = core::plane3d<f32>(v1, v2, v3).Normal;

			core::vector3df weight(1.f, 1.f, 1.f);
			if (angleWeighted)
				weight = getAngleWeight(v1, v2, v3);

			buffer->getNormal(idx[i + 0]) += weight.X * normal;
			buffer->getNormal(idx[i + 1]) += weight.Y * normal;
			buffer->getNormal(idx[i + 2]) += weight.Z * normal;
		}

		for (i = 0; i != vtxcnt; ++i)
			buffer->getNormal(i).normalize();
	}
}
}

//! Recalculates all normals of the mesh buffer.
/** \param buffer: Mesh buffer on which the operation is performed. */
void CMeshManipulator::recalculateNormals(IMeshBuffer *buffer, bool smooth, bool angleWeighted) const
{
	if (!buffer)
		return;

	if (buffer->getIndexType() == video::EIT_16BIT)
		recalculateNormalsT<u16>(buffer, smooth, angleWeighted);
	else
		recalculateNormalsT<u32>(buffer, smooth, angleWeighted);
}

//! Recalculates all normals of the mesh.
//! \param mesh: Mesh on which the operation is performed.
void CMeshManipulator::recalculateNormals(scene::IMesh *mesh, bool smooth, bool angleWeighted) const
{
	if (!mesh)
		return;

	if (mesh->getMeshType() == EAMT_SKINNED) {
		auto *smesh = (SkinnedMesh *)mesh;
		smesh->resetAnimation();
	}

	const u32 bcount = mesh->getMeshBufferCount();
	for (u32 b = 0; b < bcount; ++b)
		recalculateNormals(mesh->getMeshBuffer(b), smooth, angleWeighted);

	if (mesh->getMeshType() == EAMT_SKINNED) {
		auto *smesh = (SkinnedMesh *)mesh;
		smesh->refreshJointCache();
	}
}

template <typename T>
void copyVertices(const scene::IVertexBuffer *src, scene::CVertexBuffer<T> *dst)
{
	assert(T::getType() == src->getType());
	auto *data = static_cast<const T*>(src->getData());
	dst->Data.assign(data, data + src->getCount());
}

static void copyIndices(const scene::IIndexBuffer *src, scene::SIndexBuffer *dst)
{
	assert(src->getType() == video::EIT_16BIT);
	auto *data = static_cast<const u16*>(src->getData());
	dst->Data.assign(data, data + src->getCount());
}

//! Clones a static IMesh into a modifyable SMesh.
// not yet 32bit
SMesh *CMeshManipulator::createMeshCopy(scene::IMesh *mesh) const
{
	if (!mesh)
		return 0;

	SMesh *clone = new SMesh();

	const u32 meshBufferCount = mesh->getMeshBufferCount();

	for (u32 b = 0; b < meshBufferCount; ++b) {
		const IMeshBuffer *const mb = mesh->getMeshBuffer(b);
		switch (mb->getVertexType()) {
		case video::EVT_STANDARD: {
			SMeshBuffer *buffer = new SMeshBuffer();
			buffer->Material = mb->getMaterial();
			copyVertices(mb->getVertexBuffer(), buffer->Vertices);
			copyIndices(mb->getIndexBuffer(), buffer->Indices);
			clone->addMeshBuffer(buffer);
			buffer->drop();
		} break;
		case video::EVT_2TCOORDS: {
			SMeshBufferLightMap *buffer = new SMeshBufferLightMap();
			buffer->Material = mb->getMaterial();
			copyVertices(mb->getVertexBuffer(), buffer->Vertices);
			copyIndices(mb->getIndexBuffer(), buffer->Indices);
			clone->addMeshBuffer(buffer);
			buffer->drop();
		} break;
		case video::EVT_TANGENTS: {
			SMeshBufferTangents *buffer = new SMeshBufferTangents();
			buffer->Material = mb->getMaterial();
			copyVertices(mb->getVertexBuffer(), buffer->Vertices);
			copyIndices(mb->getIndexBuffer(), buffer->Indices);
			clone->addMeshBuffer(buffer);
			buffer->drop();
		} break;
		} // end switch

	} // end for all mesh buffers

	clone->BoundingBox = mesh->getBoundingBox();
	return clone;
}

} // end namespace scene
