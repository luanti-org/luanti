// Copyright Michael Zeilfelder
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReadFile.h"

namespace io
{

//! Interface providing read access to a memory read file.
class IMemoryReadFile : public IReadFile
{
public:
	//! Get direct access to internal buffer of memory block used as file.
	/** It's usually better to use the IReadFile functions to access
	the file content. But as that buffer exist over the full life-time
	of a CMemoryReadFile, it's sometimes nice to avoid the additional
	data-copy which read() needs.
	*/
	virtual const void *getBuffer() const = 0;
};
} // end namespace io
