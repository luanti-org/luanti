// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "path.h"

namespace io
{
class IWriteFile;
} // end namespace io

namespace video
{
class IImage;

//! Interface for writing software image data.
class IImageWriter : public IReferenceCounted
{
public:
	//! Check if this writer can write a file with the given extension
	/** \param filename Name of the file to check.
	\return True if file extension specifies a writable type. */
	virtual bool isAWriteableFileExtension(const io::path &filename) const = 0;

	//! Write image to file
	/** \param file File handle to write to.
	\param image Image to write into file.
	\param param Writer specific parameter, influencing e.g. quality.
	\return True if image was successfully written. */
	virtual bool writeImage(io::IWriteFile *file, IImage *image, u32 param = 0) const = 0;
};

} // namespace video
