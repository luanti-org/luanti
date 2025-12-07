// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "IIndexBuffer.h"

namespace scene
{
//! Template implementation of the IIndexBuffer interface
template <class T>
class CIndexBuffer final : public IIndexBuffer
{
public:
	//! Default constructor for empty buffer
	CIndexBuffer() {}

	HWBuffer::Type getBufferType() const override
	{
		return HWBuffer::Type::INDEX;
	}

	video::E_INDEX_TYPE getType() const override
	{
		static_assert(sizeof(T) == 2 || sizeof(T) == 4, "invalid index type");
		return sizeof(T) == 2 ? video::EIT_16BIT : video::EIT_32BIT;
	}

	const void *getData() const override
	{
		return Data.data();
	}

	void *getData() override
	{
		return Data.data();
	}

	u32 getElementSize() const override
	{
		return sizeof(T);
	}

	u32 getCount() const override
	{
		return static_cast<u32>(Data.size());
	}

	//! Indices of this buffer
	std::vector<T> Data;
};

//! Standard 16-bit buffer
typedef CIndexBuffer<u16> SIndexBuffer;

} // end namespace scene
