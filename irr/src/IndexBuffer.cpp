// Copyright (C) 2008-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "IndexBuffer.h"

#include <algorithm>
#include <numeric>

template<class T>
static std::vector<T> iota_descending(T n)
{
	std::vector<T> v(n);
	std::iota(v.begin(), v.end(), 0);
	std::reverse(v.begin(), v.end());
	return v;
}

namespace scene
{

irr_ptr<IndexBuffer> IndexBuffer::descending(u32 n)
{
	auto res = make_irr<IndexBuffer>();
	if (n <= U16_MAX) {
		res->data = iota_descending<u16>(n);
	} else {
		res->data = iota_descending<u32>(n);
	}
	return res;
}

const void *IndexBuffer::getData() const
{
	return std::visit([](const auto &vec) {
		return static_cast<const void *>(vec.data());
	}, data);
}

void *IndexBuffer::getData()
{
	return std::visit([](auto &vec) {
		return static_cast<void *>(vec.data());
	}, data);
}

u32 IndexBuffer::getElementSize() const
{
	if (std::holds_alternative<std::vector<u16>>(data))
		return sizeof(u16);
	if (std::holds_alternative<std::vector<u32>>(data))
		return sizeof(u32);
	IRR_CODE_UNREACHABLE();
}

u32 IndexBuffer::getCount() const
{
	return std::visit([](const auto &vec) {
		return static_cast<u32>(vec.size());
	}, data);
}

video::E_INDEX_TYPE IndexBuffer::getIndexType() const
{
	if (std::holds_alternative<std::vector<u16>>(data))
		return video::EIT_16BIT;
	if (std::holds_alternative<std::vector<u32>>(data))
		return video::EIT_32BIT;
	IRR_CODE_UNREACHABLE();
}

u32 IndexBuffer::get(u32 i) const
{
	return std::visit([i](const auto &vec) {
		return static_cast<u32>(vec[i]);
	}, data);
}

void IndexBuffer::pushBack(u32 index)
{
	if (auto *vec_16 = std::get_if<std::vector<u16>>(&data)) {
		// Note: < instead of <= to be safe
		if (index < U16_MAX) {
			vec_16->push_back(static_cast<u16>(index));
			return;
		}
		data = std::vector<u32>(vec_16->begin(), vec_16->end());
	}
	std::get<std::vector<u32>>(data).push_back(index);
}

void IndexBuffer::applyOffset(u32 i, u32 j, s32 offset)
{
	if (offset == 0)
		return;
	return std::visit([=](auto &vec) {
		for (u32 k = i; k < j; ++k)
			vec[k] += offset;
	}, data);
}

u32 IndexBuffer::getPrimitiveCount(E_PRIMITIVE_TYPE primitiveType) const
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

} // end namespace scene
