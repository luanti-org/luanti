// Copyright (C) 2008-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "EHardwareBufferFlags.h"
#include "EPrimitiveTypes.h"
#include "SVertexIndex.h"

namespace scene
{

class IIndexBuffer : public virtual IReferenceCounted
{
public:
	//! Get type of index data which is stored in this meshbuffer.
	/** \return Index type of this buffer. */
	virtual video::E_INDEX_TYPE getType() const = 0;

	//! Get access to indices.
	/** \return Pointer to indices array. */
	virtual const void *getData() const = 0;

	//! Get access to indices.
	/** \return Pointer to indices array. */
	virtual void *getData() = 0;

	//! Get amount of indices in this meshbuffer.
	/** \return Number of indices in this buffer. */
	virtual u32 getCount() const = 0;

	//! get the current hardware mapping hint
	virtual E_HARDWARE_MAPPING getHardwareMappingHint() const = 0;

	//! set the hardware mapping hint, for driver
	virtual void setHardwareMappingHint(E_HARDWARE_MAPPING newMappingHint) = 0;

	//! flags the meshbuffer as changed, reloads hardware buffers
	virtual void setDirty() = 0;

	//! Get the currently used ID for identification of changes.
	/** This shouldn't be used for anything outside the VideoDriver. */
	virtual u32 getChangedID() const = 0;

	//! Used by the VideoDriver to remember the buffer link.
	virtual void setHWBuffer(void *ptr) const = 0;
	virtual void *getHWBuffer() const = 0;

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
};

} // end namespace scene
