// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReadFile.h"
#include "irrString.h"


namespace io
{

/*! this is a read file, which is limited to some boundaries,
	so that it may only start from a certain file position
	and may only read until a certain file position.
	This can be useful, for example for reading uncompressed files
	in an archive (zip, tar).
!*/
class CLimitReadFile final : public IReadFile
{
public:
	CLimitReadFile(IReadFile *alreadyOpenedFile, long pos, long areaSize, const io::path &name);

	virtual ~CLimitReadFile();

	//! returns how much was read
	size_t read(void *buffer, size_t sizeToRead) override;

	//! changes position in file, returns true if successful
	//! if relativeMovement==true, the pos is changed relative to current pos,
	//! otherwise from begin of file
	bool seek(long finalPos, bool relativeMovement = false) override;

	//! returns size of file
	long getSize() const override;

	//! returns where in the file we are.
	long getPos() const override;

	//! returns name of file
	const io::path &getFileName() const override;

	//! Get the type of the class implementing this interface
	EREAD_FILE_TYPE getType() const override
	{
		return ERFT_LIMIT_READ_FILE;
	}

private:
	io::path Filename;
	long AreaStart;
	long AreaEnd;
	long Pos;
	IReadFile *File;
};

} // end namespace io
