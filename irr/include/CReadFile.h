// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <cstdio>
#include "IReadFile.h"
#include "irrString.h"

namespace io
{

/*!
	Class for reading a real file from disk.
*/
class CReadFile final : public IReadFile
{
public:
	CReadFile(const io::path &fileName);

	virtual ~CReadFile();

	//! returns how much was read
	size_t read(void *buffer, size_t sizeToRead) override;

	//! changes position in file, returns true if successful
	bool seek(long finalPos, bool relativeMovement = false) override;

	//! returns size of file
	long getSize() const override;

	//! returns if file is open
	bool isOpen() const
	{
		return File != 0;
	}

	//! returns where in the file we are.
	long getPos() const override;

	//! returns name of file
	const io::path &getFileName() const override;

	//! Get the type of the class implementing this interface
	EREAD_FILE_TYPE getType() const override
	{
		return ERFT_READ_FILE;
	}

	//! create read file on disk.
	static IReadFile *createReadFile(const io::path &fileName);

private:
	//! opens the file
	void openFile();

	FILE *File;
	long FileSize;
	io::path Filename;
};

} // end namespace io
