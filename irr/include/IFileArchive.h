// Copyright (C) 2002-2012 Nikolaus Gebhardt/ Thomas Alten
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReadFile.h"
#include "IFileList.h"

namespace io
{

//! FileSystemType: which filesystem should be used for e.g. browsing
enum EFileSystemType
{
	FILESYSTEM_NATIVE = 0, // Native OS FileSystem
	FILESYSTEM_VIRTUAL     // Virtual FileSystem
};

//! Contains the different types of archives
enum E_FILE_ARCHIVE_TYPE
{
	//! A PKZIP archive
	EFAT_ZIP = MAKE_IRR_ID('Z', 'I', 'P', 0),

	//! The type of this archive is unknown
	EFAT_UNKNOWN = MAKE_IRR_ID('u', 'n', 'k', 'n')
};

//! The FileArchive manages archives and provides access to files inside them.
class IFileArchive : public virtual IReferenceCounted
{
public:
	//! Opens a file based on its name
	/** Creates and returns a new IReadFile for a file in the archive.
	\param filename The file to open
	\return Returns A pointer to the created file on success,
	or 0 on failure. */
	virtual IReadFile *createAndOpenFile(const path &filename) = 0;

	//! Opens a file based on its position in the file list.
	/** Creates and returns
	\param index The zero based index of the file.
	\return Returns a pointer to the created file on success, or 0 on failure. */
	virtual IReadFile *createAndOpenFile(u32 index) = 0;

	//! Returns the complete file tree
	/** \return Returns the complete directory tree for the archive,
	including all files and folders */
	virtual const IFileList *getFileList() const = 0;

	//! get the archive type
	virtual E_FILE_ARCHIVE_TYPE getType() const { return EFAT_UNKNOWN; }

	//! return the name (id) of the file Archive
	virtual const io::path &getArchiveName() const = 0;

	//! Add a directory in the archive and all it's files to the FileList
	/** Only needed for file-archives which have no information about their own
	directory structure. In that case the user must add directories manually.
	Currently this is necessary for archives of type EFAT_ANDROID_ASSET.
	The root-path itself is already set by the engine.
	If directories are not added manually opening files might still work,
	but checks if file exists will fail.
	*/
	virtual void addDirectoryToFileList(const io::path &filename) {}
};

//! Class which is able to create an archive from a file.
/** If you want the Irrlicht Engine be able to load archives of
currently unsupported file formats (e.g .wad), then implement
this and add your new Archive loader with
IFileSystem::addArchiveLoader() to the engine. */
class IArchiveLoader : public virtual IReferenceCounted
{
public:
	//! Check if the file might be loaded by this class
	/** Check based on the file extension (e.g. ".zip")
	\param filename Name of file to check.
	\return True if file seems to be loadable. */
	virtual bool isALoadableFileFormat(const path &filename) const = 0;

	//! Check if the file might be loaded by this class
	/** This check may look into the file.
	\param file File handle to check.
	\return True if file seems to be loadable. */
	virtual bool isALoadableFileFormat(io::IReadFile *file) const = 0;

	//! Check to see if the loader can create archives of this type.
	/** Check based on the archive type.
	\param fileType The archive type to check.
	\return True if the archive loader supports this type, false if not */
	virtual bool isALoadableFileFormat(E_FILE_ARCHIVE_TYPE fileType) const = 0;

	//! Creates an archive from the filename
	/** \param filename File to use.
	\param ignoreCase Searching is performed without regarding the case
	\param ignorePaths Files are searched for without checking for the directories
	\return Pointer to newly created archive, or 0 upon error. */
	virtual IFileArchive *createArchive(const path &filename, bool ignoreCase, bool ignorePaths) const = 0;

	//! Creates an archive from the file
	/** \param file File handle to use.
	\param ignoreCase Searching is performed without regarding the case
	\param ignorePaths Files are searched for without checking for the directories
	\return Pointer to newly created archive, or 0 upon error. */
	virtual IFileArchive *createArchive(io::IReadFile *file, bool ignoreCase, bool ignorePaths) const = 0;
};

} // end namespace io
