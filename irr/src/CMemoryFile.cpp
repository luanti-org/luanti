// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CMemoryFile.h"
#include "irrString.h"

namespace io
{

CMemoryReadFile::CMemoryReadFile(const void *memory, long len, const io::path &fileName, bool d) :
		Buffer(memory), Len(len), Pos(0), Filename(fileName), deleteMemoryWhenDropped(d)
{}

CMemoryReadFile::~CMemoryReadFile()
{
	if (deleteMemoryWhenDropped)
		delete[] (c8 *)Buffer;
}

//! returns how much was read
size_t CMemoryReadFile::read(void *buffer, size_t sizeToRead)
{
	long amount = static_cast<long>(sizeToRead);
	if (Pos + amount > Len)
		amount -= Pos + amount - Len;

	if (amount <= 0)
		return 0;

	c8 *p = (c8 *)Buffer;
	memcpy(buffer, p + Pos, amount);

	Pos += amount;

	return static_cast<size_t>(amount);
}

//! changes position in file, returns true if successful
//! if relativeMovement==true, the pos is changed relative to current pos,
//! otherwise from begin of file
bool CMemoryReadFile::seek(long finalPos, bool relativeMovement)
{
	if (relativeMovement) {
		if (Pos + finalPos < 0 || Pos + finalPos > Len)
			return false;

		Pos += finalPos;
	} else {
		if (finalPos < 0 || finalPos > Len)
			return false;

		Pos = finalPos;
	}

	return true;
}

//! returns size of file
long CMemoryReadFile::getSize() const
{
	return Len;
}

//! returns where in the file we are.
long CMemoryReadFile::getPos() const
{
	return Pos;
}

//! returns name of file
const io::path &CMemoryReadFile::getFileName() const
{
	return Filename;
}

CMemoryWriteFile::CMemoryWriteFile(void *memory, long len, const io::path &fileName, bool d) :
		Buffer(memory), Len(len), Pos(0), Filename(fileName), deleteMemoryWhenDropped(d)
{}

CMemoryWriteFile::~CMemoryWriteFile()
{
	if (deleteMemoryWhenDropped)
		delete[] (c8 *)Buffer;
}

//! returns how much was written
size_t CMemoryWriteFile::write(const void *buffer, size_t sizeToWrite)
{
	long amount = (long)sizeToWrite;
	if (Pos + amount > Len)
		amount -= Pos + amount - Len;

	if (amount <= 0)
		return 0;

	c8 *p = (c8 *)Buffer;
	memcpy(p + Pos, buffer, amount);

	Pos += amount;

	return (size_t)amount;
}

//! changes position in file, returns true if successful
//! if relativeMovement==true, the pos is changed relative to current pos,
//! otherwise from begin of file
bool CMemoryWriteFile::seek(long finalPos, bool relativeMovement)
{
	if (relativeMovement) {
		if (Pos + finalPos < 0 || Pos + finalPos > Len)
			return false;

		Pos += finalPos;
	} else {
		if (finalPos < 0 || finalPos > Len)
			return false;

		Pos = finalPos;
	}

	return true;
}

//! returns where in the file we are.
long CMemoryWriteFile::getPos() const
{
	return Pos;
}

//! returns name of file
const io::path &CMemoryWriteFile::getFileName() const
{
	return Filename;
}

bool CMemoryWriteFile::flush()
{
	return true; // no buffering, so nothing to do
}

IReadFile *createMemoryReadFile(const void *memory, long size, const io::path &fileName, bool deleteMemoryWhenDropped)
{
	CMemoryReadFile *file = new CMemoryReadFile(memory, size, fileName, deleteMemoryWhenDropped);
	return file;
}

IWriteFile *createMemoryWriteFile(void *memory, long size, const io::path &fileName, bool deleteMemoryWhenDropped)
{
	CMemoryWriteFile *file = new CMemoryWriteFile(memory, size, fileName, deleteMemoryWhenDropped);
	return file;
}

} // end namespace io
