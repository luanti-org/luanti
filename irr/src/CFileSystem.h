// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReadFile.h"
#include "IFileList.h"
#include "irrString.h"

namespace io
{

/*!
	Filesystem utility functions.
*/
class CFileSystem
{
public:
	//! Returns the string of the current working directory
	static io::path getCurrentWorkingDirectory();

	//! Changes the current Working Directory to the string given.
	//! The string is operating system dependent. Under Windows it will look
	//! like this: "drive:\directory\sudirectory\"
	static bool changeWorkingDirectoryTo(const io::path &newDirectory);

	//! Converts a relative path to an absolute (unique) path, resolving symbolic links
	static io::path getAbsolutePath(const io::path &filename);

	//! flatten a path and file name for example: "/you/me/../." becomes "/you"
	static io::path &flattenFilename(io::path &directory, const io::path &root = _IRR_TEXT("/"));

	//! Creates a list of files and directories in the given directory
	static IFileList *createFileList(const io::path &path);

	//! determines if a file exists and would be able to be opened.
	static bool existFile(const io::path &filename);
};

} // end namespace io
