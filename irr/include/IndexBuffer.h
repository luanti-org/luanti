// Copyright (C) 2008-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "EPrimitiveTypes.h"
#include "SVertexIndex.h"
#include "HWBuffer.h"
#include <vector>

namespace scene
{

class IndexBuffer : public virtual IReferenceCounted, public HWBuffer
{
public:

	HWBuffer::Type getBufferType() const override
	{ return HWBuffer::Type::INDEX; }

	const void *getData() const override
	{ return data.data(); }

	void *getData()
	{ return data.data(); }

	u32 getElementSize() const override
	{ return sizeof(u16); }

	u32 getCount() const override
	{ return static_cast<u32>(data.size()); }

	// TODO support 32-bit by std::variant or similar
	video::E_INDEX_TYPE getIndexType() const
	{ return video::EIT_16BIT; }

	//! Calculate how many geometric primitives would be drawn
	u32 getPrimitiveCount(E_PRIMITIVE_TYPE primitiveType) const
	{
		const u32 indexCount = getCount();
		switch (primitiveType) {
		case scene::EPT_POINTS:
			return indexCount;
		case scene::EPT_LINE_STRIP:
			return indexCount - 1;
		case scene::EPT_LINE_LOOP:
			return indexCount;
		case scene::EPT_LINES:
			return indexCount / 2;
		case scene::EPT_TRIANGLE_STRIP:
			return (indexCount - 2);
		case scene::EPT_TRIANGLE_FAN:
			return (indexCount - 2);
		case scene::EPT_TRIANGLES:
			return indexCount / 3;
		case scene::EPT_POINT_SPRITES:
			return indexCount;
		}
		return 0;
	}

	std::vector<u16> data;
};

} // end namespace scene
