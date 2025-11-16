// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "IIndexBuffer.h"

// Define to receive warnings when violating the hw mapping hints
//#define INDEXBUFFER_HINT_DEBUG

#ifdef INDEXBUFFER_HINT_DEBUG
#include "../src/os.h"
#endif

namespace scene
{
//! Template implementation of the IIndexBuffer interface
template <class T>
class CIndexBuffer final : public IIndexBuffer
{
public:
	//! Default constructor for empty buffer
	CIndexBuffer() {}

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

	u32 getCount() const override
	{
		return static_cast<u32>(Data.size());
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
#ifdef INDEXBUFFER_HINT_DEBUG
		if (MappingHint == EHM_STATIC && HWBuffer) {
			char buf[100];
			snprintf_irr(buf, sizeof(buf), "CIndexBuffer @ %p modified, but it has a static hint", this);
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

	//! Indices of this buffer
	std::vector<T> Data;
};

//! Standard 16-bit buffer
typedef CIndexBuffer<u16> SIndexBuffer;

} // end namespace scene
