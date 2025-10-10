// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "IVertexBuffer.h"

// Define to receive warnings when violating the hw mapping hints
//#define VERTEXBUFFER_HINT_DEBUG

#ifdef VERTEXBUFFER_HINT_DEBUG
#include "../src/os.h"
#endif

namespace scene
{
//! Template implementation of the IVertexBuffer interface
template <class T>
class CVertexBuffer final : public IVertexBuffer
{
public:
	//! Default constructor for empty buffer
	CVertexBuffer() {}

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

	E_HARDWARE_MAPPING getHardwareMappingHint() const override
	{
		return MappingHint;
	}

	void setHardwareMappingHint(E_HARDWARE_MAPPING NewMappingHint) override
	{
		MappingHint = NewMappingHint;
	}

	void setDirty() override
	{
		++ChangedID;
#ifdef VERTEXBUFFER_HINT_DEBUG
		if (MappingHint == EHM_STATIC && HWBuffer) {
			char buf[100];
			snprintf_irr(buf, sizeof(buf), "CVertexBuffer @ %p modified, but it has a static hint", this);
			os::Printer::log(buf, ELL_WARNING);
		}
#endif
	}

	u32 getChangedID() const override { return ChangedID; }

	void setHWBuffer(void *ptr) const override
	{
		HWBuffer = ptr;
	}

	void *getHWBuffer() const override
	{
		return HWBuffer;
	}

	u32 ChangedID = 1;

	//! hardware mapping hint
	E_HARDWARE_MAPPING MappingHint = EHM_NEVER;
	mutable void *HWBuffer = nullptr;

	//! Vertices of this buffer
	std::vector<T> Data;
};

//! Standard buffer
typedef CVertexBuffer<video::S3DVertex> SVertexBuffer;
//! Buffer with two texture coords per vertex, e.g. for lightmaps
typedef CVertexBuffer<video::S3DVertex2TCoords> SVertexBufferLightMap;
//! Buffer with vertices having tangents stored, e.g. for normal mapping
typedef CVertexBuffer<video::S3DVertexTangents> SVertexBufferTangents;

} // end namespace scene
