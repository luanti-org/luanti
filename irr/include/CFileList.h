// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IFileList.h"
#include "irrString.h"
#include "irrArray.h"

namespace io
{

//! An entry in a list of files, can be a folder or a file.
struct SFileListEntry
{
	//! The name of the file
	/** If this is a file or folder in the virtual filesystem and the archive
	was created with the ignoreCase flag then the file name will be lower case. */
	io::path Name;

	//! The name of the file including the path
	/** If this is a file or folder in the virtual filesystem and the archive was
	created with the ignoreDirs flag then it will be the same as Name. */
	io::path FullName;

	//! The size of the file in bytes
	u32 Size;

	//! The ID of the file in an archive
	/** This is used to link the FileList entry to extra info held about this
	file in an archive, which can hold things like data offset and CRC. */
	u32 ID;

	//! FileOffset inside an archive
	u32 Offset;

	//! True if this is a folder, false if not.
	bool IsDirectory;

	//! The == operator is provided so that CFileList can slowly search the list!
	bool operator==(const struct SFileListEntry &other) const
	{
		if (IsDirectory != other.IsDirectory)
			return false;

		return FullName.equals_ignore_case(other.FullName);
	}

	//! The < operator is provided so that CFileList can sort and quickly search the list.
	bool operator<(const struct SFileListEntry &other) const
	{
		if (IsDirectory != other.IsDirectory)
			return IsDirectory;

		return FullName.lower_ignore_case(other.FullName);
	}
};

//! Implementation of a file list
class CFileList : public IFileList
{
public:
	// CFileList methods

	//! Constructor
	/** \param path The path of this file archive */
	CFileList(const io::path &path, bool ignoreCase, bool ignorePaths);

	//! Destructor
	virtual ~CFileList();

	//! Add as a file or folder to the list
	/** \param fullPath The file name including path, up to the root of the file list.
	\param isDirectory True if this is a directory rather than a file.
	\param offset The offset where the file is stored in an archive
	\param size The size of the file in bytes.
	\param id The ID of the file in the archive which owns it */
	u32 addItem(const io::path &fullPath, u32 offset, u32 size, bool isDirectory, u32 id = 0) override;

	//! Sorts the file list. You should call this after adding any items to the file list
	void sort() override;

	//! Returns the amount of files in the filelist.
	u32 getFileCount() const override;

	//! Gets the name of a file in the list, based on an index.
	const io::path &getFileName(u32 index) const override;

	//! Gets the full name of a file in the list, path included, based on an index.
	const io::path &getFullFileName(u32 index) const override;

	//! Returns the ID of a file in the file list, based on an index.
	u32 getID(u32 index) const override;

	//! Returns true if the file is a directory
	bool isDirectory(u32 index) const override;

	//! Returns the size of a file
	u32 getFileSize(u32 index) const override;

	//! Returns the offset of a file
	u32 getFileOffset(u32 index) const override;

	//! Searches for a file or folder within the list, returns the index
	s32 findFile(const io::path &filename, bool isFolder) const override;

	//! Returns the base path of the file list
	const io::path &getPath() const override;

protected:
	//! Ignore paths when adding or searching for files
	bool IgnorePaths;

	//! Ignore case when adding or searching for files
	bool IgnoreCase;

	//! Path to the file list
	io::path Path;

	//! List of files
	core::array<SFileListEntry> Files;
};

} // end namespace io
