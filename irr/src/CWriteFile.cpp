// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CWriteFile.h"
#include <cstdio>

namespace io
{

CWriteFile::CWriteFile(const io::path &fileName, bool append) :
		Filename(fileName), FileSize(0)
{
	openFile(append);
}

CWriteFile::~CWriteFile()
{
	if (File)
		fclose(File);
}

//! returns if file is open
inline bool CWriteFile::isOpen() const
{
	return File != 0;
}

//! returns how much was read
size_t CWriteFile::write(const void *buffer, size_t sizeToWrite)
{
	if (!isOpen())
		return 0;

	return fwrite(buffer, 1, sizeToWrite, File);
}

//! changes position in file, returns true if successful
//! if relativeMovement==true, the pos is changed relative to current pos,
//! otherwise from begin of file
bool CWriteFile::seek(long finalPos, bool relativeMovement)
{
	if (!isOpen())
		return false;

	return fseek(File, finalPos, relativeMovement ? SEEK_CUR : SEEK_SET) == 0;
}

//! returns where in the file we are.
long CWriteFile::getPos() const
{
	return ftell(File);
}

//! opens the file
void CWriteFile::openFile(bool append)
{
	if (Filename.size() == 0) {
		File = 0;
		return;
	}

	File = fopen(Filename.c_str(), append ? "ab" : "wb");

	if (File) {
		// get FileSize

		fseek(File, 0, SEEK_END);
		FileSize = ftell(File);
		fseek(File, 0, SEEK_SET);
	}
}

//! returns name of file
const io::path &CWriteFile::getFileName() const
{
	return Filename;
}

//! Flush the content of the buffer in the file
bool CWriteFile::flush()
{
	if (!isOpen())
		return false;

	return fflush(File) == 0; // 0 indicates success, otherwise EOF and errno is set
}

IWriteFile *CWriteFile::createWriteFile(const io::path &fileName, bool append)
{
	CWriteFile *file = new CWriteFile(fileName, append);
	if (file->isOpen())
		return file;

	file->drop();
	return 0;
}

} // end namespace io
