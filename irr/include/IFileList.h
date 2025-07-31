// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "path.h"

namespace io
{

//! Provides a list of files and folders.
/** File lists usually contain a list of all files in a given folder,
but can also contain a complete directory structure. */
class IFileList : public virtual IReferenceCounted
{
public:
	//! Get the number of files in the filelist.
	/** \return Amount of files and directories in the file list. */
	virtual u32 getFileCount() const = 0;

	//! Gets the name of a file in the list, based on an index.
	/** The path is not included in this name. Use getFullFileName for this.
	\param index is the zero based index of the file which name should
	be returned. The index must be less than the amount getFileCount() returns.
	\return File name of the file. Returns 0, if an error occurred. */
	virtual const io::path &getFileName(u32 index) const = 0;

	//! Gets the full name of a file in the list including the path, based on an index.
	/** \param index is the zero based index of the file which name should
	be returned. The index must be less than the amount getFileCount() returns.
	\return File name of the file. Returns 0 if an error occurred. */
	virtual const io::path &getFullFileName(u32 index) const = 0;

	//! Returns the size of a file in the file list, based on an index.
	/** \param index is the zero based index of the file which should be returned.
	The index must be less than the amount getFileCount() returns.
	\return The size of the file in bytes. */
	virtual u32 getFileSize(u32 index) const = 0;

	//! Returns the file offset of a file in the file list, based on an index.
	/** \param index is the zero based index of the file which should be returned.
	The index must be less than the amount getFileCount() returns.
	\return The offset of the file in bytes. */
	virtual u32 getFileOffset(u32 index) const = 0;

	//! Returns the ID of a file in the file list, based on an index.
	/** This optional ID can be used to link the file list entry to information held
	elsewhere. For example this could be an index in an IFileArchive, linking the entry
	to its data offset, uncompressed size and CRC.
	\param index is the zero based index of the file which should be returned.
	The index must be less than the amount getFileCount() returns.
	\return The ID of the file. */
	virtual u32 getID(u32 index) const = 0;

	//! Check if the file is a directory
	/** \param index The zero based index which will be checked. The index
	must be less than the amount getFileCount() returns.
	\return True if the file is a directory, else false. */
	virtual bool isDirectory(u32 index) const = 0;

	//! Searches for a file or folder in the list
	/** Searches for a file by name
	\param filename The name of the file to search for.
	\param isFolder True if you are searching for a directory path, false if you are searching for a file
	\return Returns the index of the file in the file list, or -1 if
	no matching name name was found. */
	virtual s32 findFile(const io::path &filename, bool isFolder = false) const = 0;

	//! Returns the base path of the file list
	virtual const io::path &getPath() const = 0;

	//! Add as a file or folder to the list
	/** \param fullPath The file name including path, from the root of the file list.
	\param isDirectory True if this is a directory rather than a file.
	\param offset The file offset inside an archive
	\param size The size of the file in bytes.
	\param id The ID of the file in the archive which owns it */
	virtual u32 addItem(const io::path &fullPath, u32 offset, u32 size, bool isDirectory, u32 id = 0) = 0;

	//! Sorts the file list. You should call this after adding any items to the file list
	virtual void sort() = 0;
};

} // end namespace io
