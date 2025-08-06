// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CLimitReadFile.h"
#include "irrMath.h"
#include "irrString.h"

namespace io
{

CLimitReadFile::CLimitReadFile(IReadFile *alreadyOpenedFile, long pos,
		long areaSize, const io::path &name) :
		Filename(name),
		AreaStart(0), AreaEnd(0), Pos(0),
		File(alreadyOpenedFile)
{
	if (File) {
		File->grab();
		AreaStart = pos;
		AreaEnd = AreaStart + areaSize;
	}
}

CLimitReadFile::~CLimitReadFile()
{
	if (File)
		File->drop();
}

//! returns how much was read
size_t CLimitReadFile::read(void *buffer, size_t sizeToRead)
{
	if (0 == File)
		return 0;

#if 1
	long r = AreaStart + Pos;
	long toRead = core::min_(AreaEnd, r + (long)sizeToRead) - core::max_(AreaStart, r);
	if (toRead < 0)
		return 0;
	File->seek(r);
	r = (long)File->read(buffer, toRead);
	Pos += r;
	return r;
#else
	const long pos = File->getPos();

	if (pos >= AreaEnd)
		return 0;

	if (pos + (long)sizeToRead >= AreaEnd)
		sizeToRead = AreaEnd - pos;

	return File->read(buffer, sizeToRead);
#endif
}

//! changes position in file, returns true if successful
bool CLimitReadFile::seek(long finalPos, bool relativeMovement)
{
#if 1
	Pos = core::s32_clamp(finalPos + (relativeMovement ? Pos : 0), 0, AreaEnd - AreaStart);
	return true;
#else
	const long pos = File->getPos();

	if (relativeMovement) {
		if (pos + finalPos > AreaEnd)
			finalPos = AreaEnd - pos;
	} else {
		finalPos += AreaStart;
		if (finalPos > AreaEnd)
			return false;
	}

	return File->seek(finalPos, relativeMovement);
#endif
}

//! returns size of file
long CLimitReadFile::getSize() const
{
	return AreaEnd - AreaStart;
}

//! returns where in the file we are.
long CLimitReadFile::getPos() const
{
#if 1
	return Pos;
#else
	return File->getPos() - AreaStart;
#endif
}

//! returns name of file
const io::path &CLimitReadFile::getFileName() const
{
	return Filename;
}

IReadFile *createLimitReadFile(const io::path &fileName, IReadFile *alreadyOpenedFile, long pos, long areaSize)
{
	return new CLimitReadFile(alreadyOpenedFile, pos, areaSize, fileName);
}

} // end namespace io
