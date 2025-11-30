#pragma once

#include "IReferenceCounted.h"
#include "irrTypes.h"
#include "EHardwareBufferFlags.h"
#include <cstddef>

namespace scene
{

struct IHWBuffer : public virtual IReferenceCounted {
	/// Size of one element in bytes
	virtual size_t getElementSize() const = 0;
	/// Number of elements in the buffer
	virtual u32 getCount() const = 0;
	/// Pointer to the buffer data
	virtual const void *getData() const = 0;

	/// Get the currently used ID for identification of changes. Should be used only by driver.
	u32 getChangedID() const { return ChangedID; }

	/// Marks the buffer as changed, so that hardware buffers are reloaded
	void setDirty()
	{
		++ChangedID;
		// TODO
		/* if constexpr (DEBUG) {
			if (MappingHint == EHM_STATIC && HWBuffer) {
				char buf[100];
				snprintf_irr(buf, sizeof(buf), "HWBuffer @ %p modified, but it has a static hint", this);
				os::Printer::log(buf, ELL_WARNING);
			}
		} */
	}

	//! hardware mapping hint
	E_HARDWARE_MAPPING MappingHint = EHM_NEVER;
	//! link back to driver specific buffer info
	mutable void *Link = nullptr;

private:

	u32 ChangedID = 1;
	static constexpr bool DEBUG = false;
	static constexpr u32 MinVertexCountForVBO = 50;
};

} // end namespace scene
