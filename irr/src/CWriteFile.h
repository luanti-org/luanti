// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <cstdio>
#include "IWriteFile.h"
#include "irrString.h"

namespace io
{

/*!
	Class for writing a real file to disk.
*/
class CWriteFile : public IWriteFile
{
public:
	CWriteFile(const io::path &fileName, bool append);

	virtual ~CWriteFile();

	//! Reads an amount of bytes from the file.
	size_t write(const void *buffer, size_t sizeToWrite) override;

	//! Changes position in file, returns true if successful.
	bool seek(long finalPos, bool relativeMovement = false) override;

	//! Returns the current position in the file.
	long getPos() const override;

	//! Returns name of file.
	const io::path &getFileName() const override;

	//! Flush the content of the buffer in the file
	bool flush() override;

	//! returns if file is open
	bool isOpen() const;

	//! creator method
	static IWriteFile *createWriteFile(const io::path &fileName, bool append);

private:
	//! opens the file
	void openFile(bool append);

	io::path Filename;
	FILE *File;
	long FileSize;
};

} // end namespace io
