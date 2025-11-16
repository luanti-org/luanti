// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "EReadFileType.h"
#include "path.h"

namespace io
{

//! Interface providing read access to a file.
class IReadFile : public virtual IReferenceCounted
{
public:
	//! Reads an amount of bytes from the file.
	/** \param buffer Pointer to buffer where read bytes are written to.
	\param sizeToRead Amount of bytes to read from the file.
	\return How many bytes were read. */
	virtual size_t read(void *buffer, size_t sizeToRead) = 0;

	//! Changes position in file
	/** \param finalPos Destination position in the file.
	\param relativeMovement If set to true, the position in the file is
	changed relative to current position. Otherwise the position is changed
	from beginning of file.
	\return True if successful, otherwise false. */
	virtual bool seek(long finalPos, bool relativeMovement = false) = 0;

	//! Get size of file.
	/** \return Size of the file in bytes. */
	virtual long getSize() const = 0;

	//! Get the current position in the file.
	/** \return Current position in the file in bytes on success or -1L on failure. */
	virtual long getPos() const = 0;

	//! Get name of file.
	/** \return File name as zero terminated character string. */
	virtual const io::path &getFileName() const = 0;

	//! Get the type of the class implementing this interface
	virtual EREAD_FILE_TYPE getType() const
	{
		return EFIT_UNKNOWN;
	}
};

//! Internal function, please do not use.
IReadFile *createLimitReadFile(const io::path &fileName, IReadFile *alreadyOpenedFile, long pos, long areaSize);

} // end namespace io
