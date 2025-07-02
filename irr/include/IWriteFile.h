// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "path.h"

namespace io
{

//! Interface providing write access to a file.
class IWriteFile : public virtual IReferenceCounted
{
public:
	//! Writes an amount of bytes to the file.
	/** \param buffer Pointer to buffer of bytes to write.
	\param sizeToWrite Amount of bytes to write to the file.
	\return How much bytes were written. */
	virtual size_t write(const void *buffer, size_t sizeToWrite) = 0;

	//! Changes position in file
	/** \param finalPos Destination position in the file.
	\param relativeMovement If set to true, the position in the file is
	changed relative to current position. Otherwise the position is changed
	from begin of file.
	\return True if successful, otherwise false. */
	virtual bool seek(long finalPos, bool relativeMovement = false) = 0;

	//! Get the current position in the file.
	/** \return Current position in the file in bytes on success or -1L on failure */
	virtual long getPos() const = 0;

	//! Get name of file.
	/** \return File name as zero terminated character string. */
	virtual const path &getFileName() const = 0;

	//! Flush the content of the buffer in the file
	/** \return True if successful, otherwise false. */
	virtual bool flush() = 0;
};

} // end namespace io
