// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CFileList.h"
#include "irrArray.h"
#include "coreutil.h"

#include "os.h"

namespace io
{

static const io::path emptyFileListEntry;

CFileList::CFileList(const io::path &path, bool ignoreCase, bool ignorePaths) :
		IgnorePaths(ignorePaths), IgnoreCase(ignoreCase), Path(path)
{
	Path.replace('\\', '/');
}

CFileList::~CFileList()
{
	Files.clear();
}

u32 CFileList::getFileCount() const
{
	return Files.size();
}

void CFileList::sort()
{
	Files.sort();
}

const io::path &CFileList::getFileName(u32 index) const
{
	if (index >= Files.size())
		return emptyFileListEntry;

	return Files[index].Name;
}

//! Gets the full name of a file in the list, path included, based on an index.
const io::path &CFileList::getFullFileName(u32 index) const
{
	if (index >= Files.size())
		return emptyFileListEntry;

	return Files[index].FullName;
}

//! adds a file or folder
u32 CFileList::addItem(const io::path &fullPath, u32 offset, u32 size, bool isDirectory, u32 id)
{
	SFileListEntry entry;
	entry.ID = id ? id : Files.size();
	entry.Offset = offset;
	entry.Size = size;
	entry.Name = fullPath;
	entry.Name.replace('\\', '/');
	entry.IsDirectory = isDirectory;

	// remove trailing slash
	if (entry.Name.lastChar() == '/') {
		entry.IsDirectory = true;
		entry.Name[entry.Name.size() - 1] = 0;
		entry.Name.validate();
	}

	if (IgnoreCase)
		entry.Name.make_lower();

	entry.FullName = entry.Name;

	entry.Name = core::deletePathFromFilename(entry.Name);

	if (IgnorePaths)
		entry.FullName = entry.Name;

	// os::Printer::log(Path.c_str(), entry.FullName);

	Files.push_back(entry);

	return Files.size() - 1;
}

//! Returns the ID of a file in the file list, based on an index.
u32 CFileList::getID(u32 index) const
{
	return index < Files.size() ? Files[index].ID : 0;
}

bool CFileList::isDirectory(u32 index) const
{
	bool ret = false;
	if (index < Files.size())
		ret = Files[index].IsDirectory;

	return ret;
}

//! Returns the size of a file
u32 CFileList::getFileSize(u32 index) const
{
	return index < Files.size() ? Files[index].Size : 0;
}

u32 CFileList::getFileOffset(u32 index) const
{
	return index < Files.size() ? Files[index].Offset : 0;
}

//! Searches for a file or folder within the list, returns the index
s32 CFileList::findFile(const io::path &filename, bool isDirectory = false) const
{
	SFileListEntry entry;
	// we only need FullName to be set for the search
	entry.FullName = filename;
	entry.IsDirectory = isDirectory;

	// exchange
	entry.FullName.replace('\\', '/');

	// remove trailing slash
	if (entry.FullName.lastChar() == '/') {
		entry.IsDirectory = true;
		entry.FullName[entry.FullName.size() - 1] = 0;
		entry.FullName.validate();
	}

	if (IgnoreCase)
		entry.FullName.make_lower();

	if (IgnorePaths)
		entry.FullName = core::deletePathFromFilename(entry.FullName);

	return Files.binary_search(entry);
}

//! Returns the base path of the file list
const io::path &CFileList::getPath() const
{
	return Path;
}

} // end namespace io
