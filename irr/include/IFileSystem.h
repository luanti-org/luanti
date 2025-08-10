// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "IFileArchive.h"

namespace io
{

class IReadFile;
class IWriteFile;
class IFileList;

//! The FileSystem manages files and archives and provides access to them.
/** It manages where files are, so that modules which use the the IO do not
need to know where every file is located. A file could be in a .zip-Archive or
as file on disk, using the IFileSystem makes no difference to this. */
class IFileSystem : public virtual IReferenceCounted
{
public:
	//! Opens a file for read access.
	/** \param filename: Name of file to open.
	\return Pointer to the created file interface.
	The returned pointer should be dropped when no longer needed.
	See IReferenceCounted::drop() for more information. */
	virtual IReadFile *createAndOpenFile(const path &filename) = 0;

	//! Creates an IReadFile interface for accessing memory like a file.
	/** This allows you to use a pointer to memory where an IReadFile is requested.
	\param memory: A pointer to the start of the file in memory
	\param len: The length of the memory in bytes
	\param fileName: The name given to this file
	\param deleteMemoryWhenDropped: True if the memory should be deleted
	along with the IReadFile when it is dropped.
	\return Pointer to the created file interface.
	The returned pointer should be dropped when no longer needed.
	See IReferenceCounted::drop() for more information.
	*/
	virtual IReadFile *createMemoryReadFile(const void *memory, s32 len, const path &fileName, bool deleteMemoryWhenDropped = false) = 0;

	//! Creates an IReadFile interface for accessing files inside files.
	/** This is useful e.g. for archives.
	\param fileName: The name given to this file
	\param alreadyOpenedFile: Pointer to the enclosing file
	\param pos: Start of the file inside alreadyOpenedFile
	\param areaSize: The length of the file
	\return A pointer to the created file interface.
	The returned pointer should be dropped when no longer needed.
	See IReferenceCounted::drop() for more information.
	*/
	virtual IReadFile *createLimitReadFile(const path &fileName,
			IReadFile *alreadyOpenedFile, long pos, long areaSize) = 0;

	//! Creates an IWriteFile interface for accessing memory like a file.
	/** This allows you to use a pointer to memory where an IWriteFile is requested.
		You are responsible for allocating enough memory.
	\param memory: A pointer to the start of the file in memory (allocated by you)
	\param len: The length of the memory in bytes
	\param fileName: The name given to this file
	\param deleteMemoryWhenDropped: True if the memory should be deleted
	along with the IWriteFile when it is dropped.
	\return Pointer to the created file interface.
	The returned pointer should be dropped when no longer needed.
	See IReferenceCounted::drop() for more information.
	*/
	virtual IWriteFile *createMemoryWriteFile(void *memory, s32 len, const path &fileName, bool deleteMemoryWhenDropped = false) = 0;

	//! Opens a file for write access.
	/** \param filename: Name of file to open.
	\param append: If the file already exist, all write operations are
	appended to the file.
	\return Pointer to the created file interface. 0 is returned, if the
	file could not created or opened for writing.
	The returned pointer should be dropped when no longer needed.
	See IReferenceCounted::drop() for more information. */
	virtual IWriteFile *createAndWriteFile(const path &filename, bool append = false) = 0;

	//! Adds an external archive loader to the engine.
	/** Use this function to add support for new archive types to the
	engine, for example proprietary or encrypted file storage. */
	virtual void addArchiveLoader(IArchiveLoader *loader) = 0;

	//! Gets the number of archive loaders currently added
	virtual u32 getArchiveLoaderCount() const = 0;

	//! Retrieve the given archive loader
	/** \param index The index of the loader to retrieve. This parameter is an 0-based
	array index.
	\return A pointer to the specified loader, 0 if the index is incorrect. */
	virtual IArchiveLoader *getArchiveLoader(u32 index) const = 0;

	//! Get the current working directory.
	/** \return Current working directory as a string. */
	virtual const path &getWorkingDirectory() = 0;

	//! Changes the current working directory.
	/** \param newDirectory: A string specifying the new working directory.
	The string is operating system dependent. Under Windows it has
	the form "<drive>:\<directory>\<sudirectory>\<..>". An example would be: "C:\Windows\"
	\return True if successful, otherwise false. */
	virtual bool changeWorkingDirectoryTo(const path &newDirectory) = 0;

	//! Converts a relative path to an absolute (unique) path, resolving symbolic links if required
	/** \param filename Possibly relative file or directory name to query.
	\result Absolute filename which points to the same file. */
	virtual path getAbsolutePath(const path &filename) const = 0;

	//! Get the directory a file is located in.
	/** \param filename: The file to get the directory from.
	\return String containing the directory of the file. */
	virtual path getFileDir(const path &filename) const = 0;

	//! Get the base part of a filename, i.e. the name without the directory part.
	/** If no directory is prefixed, the full name is returned.
	\param filename: The file to get the basename from
	\param keepExtension True if filename with extension is returned otherwise everything
	after the final '.' is removed as well. */
	virtual path getFileBasename(const path &filename, bool keepExtension = true) const = 0;

	//! flatten a path and file name for example: "/you/me/../." becomes "/you"
	virtual path &flattenFilename(path &directory, const path &root = "/") const = 0;

	//! Get the relative filename, relative to the given directory
	virtual path getRelativeFilename(const path &filename, const path &directory) const = 0;

	//! Creates a list of files and directories in the current working directory and returns it.
	/** \return a Pointer to the created IFileList is returned. After the list has been used
	it has to be deleted using its IFileList::drop() method.
	See IReferenceCounted::drop() for more information. */
	virtual IFileList *createFileList() = 0;

	//! Creates an empty filelist
	/** \return a Pointer to the created IFileList is returned. After the list has been used
	it has to be deleted using its IFileList::drop() method.
	See IReferenceCounted::drop() for more information. */
	virtual IFileList *createEmptyFileList(const io::path &path, bool ignoreCase, bool ignorePaths) = 0;

	//! Set the active type of file system.
	virtual EFileSystemType setFileListSystem(EFileSystemType listType) = 0;

	//! Determines if a file exists and could be opened.
	/** \param filename is the string identifying the file which should be tested for existence.
	\return True if file exists, and false if it does not exist or an error occurred. */
	virtual bool existFile(const path &filename) const = 0;
};

} // end namespace io
