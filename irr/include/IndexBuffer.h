// Copyright (C) 2008-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irr_ptr.h"
#include "irr_types.h"
#include "IReferenceCounted.h"
#include "EPrimitiveTypes.h"
#include "SVertexIndex.h"
#include "HWBuffer.h"
#include <algorithm>
#include <vector>
#include <variant>

namespace scene
{

struct IndexBuffer : public virtual IReferenceCounted, public HWBuffer
{
	static irr_ptr<IndexBuffer> descending(u32 n);

	HWBuffer::Type getBufferType() const override
	{ return HWBuffer::Type::INDEX; }

	const void *getData() const override;
	void *getData();

	u32 getElementSize() const override;

	u32 getCount() const override;

	video::E_INDEX_TYPE getIndexType() const;

	//! Get the i-th index
	u32 get(u32 i) const;

	//! Add an index, upgrading the buffer to 32-bit if necessary
	void pushBack(u32 index);

	template<typename T>
	void append(T first, T last, s32 offset = 0)
	{
		if (auto *vec_16 = std::get_if<std::vector<u16>>(&data)) {
			if (std::all_of(first, last, [](auto index) { return index <= U16_MAX; })) {
				vec_16->insert(vec_16->end(), first, last);
				return;
			}
			data = std::vector<u32>(vec_16->begin(), vec_16->end());
		}
		auto &vec_32 = std::get<std::vector<u32>>(data);
		vec_32.insert(vec_32.end(), first, last);
	}

	template<typename T>
	void appendWithOffset(T first, T last, s32 offset)
	{
		u32 i = getCount();
		append(first, last);
		u32 j = getCount();
		applyOffset(i, j, offset);
	}

	//! Apply an offset to indices i, i+1, ..., j-1
	void applyOffset(u32 i, u32 j, s32 offset);

	//! Calculate how many geometric primitives would be drawn
	u32 getPrimitiveCount(E_PRIMITIVE_TYPE primitiveType) const;

	std::variant<std::vector<u16>, std::vector<u32>> data;
};

} // end namespace scene
