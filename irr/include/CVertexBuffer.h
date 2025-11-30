// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <memory>
#include <vector>
#include "EHardwareBufferFlags.h"
#include "HWBuffer.h"
#include "IVertexBuffer.h"
#include "WeightBuffer.h"

namespace scene
{

//! Template implementation of the IVertexBuffer interface
template <class T>
struct CVertexBuffer final : public IVertexBuffer
{
	//! Default constructor for empty buffer
	CVertexBuffer() {}

	HWBuffer::Type getBufferType() const override
	{
		return HWBuffer::Type::VERTEX;
	}

	const void *getData() const override
	{
		return Data.data();
	}

	void *getData() override
	{
		return Data.data();
	}

	u32 getCount() const override
	{
		return static_cast<u32>(Data.size());
	}

	video::E_VERTEX_TYPE getType() const override
	{
		return T::getType();
	}

	u32 getElementSize() const override
	{
		return sizeof(T);
	}

	const core::vector3df &getPosition(u32 i) const override
	{
		return Data[i].Pos;
	}

	core::vector3df &getPosition(u32 i) override
	{
		return Data[i].Pos;
	}

	const core::vector3df &getNormal(u32 i) const override
	{
		return Data[i].Normal;
	}

	core::vector3df &getNormal(u32 i) override
	{
		return Data[i].Normal;
	}

	const core::vector2df &getTCoords(u32 i) const override
	{
		return Data[i].TCoords;
	}

	core::vector2df &getTCoords(u32 i) override
	{
		return Data[i].TCoords;
	}

	//! Vertices of this buffer
	std::vector<T> Data;

	//! Optional weights for skinning
	std::unique_ptr<WeightBuffer> Weights;
};

//! Standard buffer
typedef CVertexBuffer<video::S3DVertex> SVertexBuffer;
//! Buffer with two texture coords per vertex, e.g. for lightmaps
typedef CVertexBuffer<video::S3DVertex2TCoords> SVertexBufferLightMap;
//! Buffer with vertices having tangents stored, e.g. for normal mapping
typedef CVertexBuffer<video::S3DVertexTangents> SVertexBufferTangents;

} // end namespace scene
