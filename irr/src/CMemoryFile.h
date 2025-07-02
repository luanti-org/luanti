// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMemoryReadFile.h"
#include "IWriteFile.h"
#include "irrString.h"

namespace io
{

/*!
	Class for reading from memory.
*/
class CMemoryReadFile final : public IMemoryReadFile
{
public:
	//! Constructor
	CMemoryReadFile(const void *memory, long len, const io::path &fileName, bool deleteMemoryWhenDropped);

	//! Destructor
	virtual ~CMemoryReadFile();

	//! returns how much was read
	size_t read(void *buffer, size_t sizeToRead) override;

	//! changes position in file, returns true if successful
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
		return ERFT_MEMORY_READ_FILE;
	}

	//! Get direct access to internal buffer
	const void *getBuffer() const override
	{
		return Buffer;
	}

private:
	const void *Buffer;
	long Len;
	long Pos;
	io::path Filename;
	bool deleteMemoryWhenDropped;
};

/*!
	Class for writing to memory.
*/
class CMemoryWriteFile : public IWriteFile
{
public:
	//! Constructor
	CMemoryWriteFile(void *memory, long len, const io::path &fileName, bool deleteMemoryWhenDropped);

	//! Destructor
	virtual ~CMemoryWriteFile();

	//! returns how much was written
	size_t write(const void *buffer, size_t sizeToWrite) override;

	//! changes position in file, returns true if successful
	bool seek(long finalPos, bool relativeMovement = false) override;

	//! returns where in the file we are.
	long getPos() const override;

	//! returns name of file
	const io::path &getFileName() const override;

	bool flush() override;

private:
	void *Buffer;
	long Len;
	long Pos;
	io::path Filename;
	bool deleteMemoryWhenDropped;
};

} // end namespace io
