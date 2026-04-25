// Copyright (C) 2008-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

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

// "This would have been five and a half hieroglyphs in an array language" - Anne APLbaum
class IndexBuffer : public virtual IReferenceCounted, public HWBuffer
{
public:

	HWBuffer::Type getBufferType() const override
	{ return HWBuffer::Type::INDEX; }

	const void *getData() const override
	{
		return std::visit([](const auto &vec) {
			return static_cast<const void *>(vec.data());
		}, data);
	}

	void *getData()
	{
		return std::visit([](auto &vec) {
			return static_cast<void *>(vec.data());
		}, data);
	}

	u32 getElementSize() const override
	{
		if (std::holds_alternative<std::vector<u16>>(data))
			return sizeof(u16);
		if (std::holds_alternative<std::vector<u32>>(data))
			return sizeof(u32);
		IRR_CODE_UNREACHABLE();
	}

	u32 getCount() const override
	{
		return std::visit([](const auto &vec) {
			return static_cast<u32>(vec.size());
		}, data);
	}

	video::E_INDEX_TYPE getIndexType() const
	{
		if (std::holds_alternative<std::vector<u16>>(data))
			return video::EIT_16BIT;
		if (std::holds_alternative<std::vector<u32>>(data))
			return video::EIT_32BIT;
		IRR_CODE_UNREACHABLE();
	}

	//! Get the i-th index
	u32 get(u32 i) const
	{
		return std::visit([i](const auto &vec) {
			return static_cast<u32>(vec[i]);
		}, data);
	}

	//! Add an index, upgrading the buffer to 32-bit if necessary
	void pushBack(u32 index)
	{
		if (auto *vec_16 = std::get_if<std::vector<u16>>(&data)) {
			if (index <= U16_MAX) {
				vec_16->push_back(static_cast<u16>(index));
				return;
			}
			data = std::vector<u32>(vec_16->begin(), vec_16->end());
		}
		std::get<std::vector<u16>>(data).push_back(index);
	}

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
	void applyOffset(u32 i, u32 j, s32 offset)
	{
		if (offset == 0)
			return;
		return std::visit([=](auto &vec) {
			for (u32 k = i; k < j; ++k)
				vec[k] += offset;
		}, data);
	}

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

	std::variant<std::vector<u16>, std::vector<u32>> data;
};

} // end namespace scene
