// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <vector>
#include "IReadFile.h"
#include "irrString.h"
#include "IFileSystem.h"
#include "CFileList.h"

namespace io
{
// the fields crc-32, compressed size and uncompressed size are set to
// zero in the local header
static constexpr s16 ZIP_INFO_IN_DATA_DESCRIPTOR = 0x0008;

// byte-align structures
#include "irrpack.h"

struct SZIPFileDataDescriptor
{
	u32 CRC32;
	u32 CompressedSize;
	u32 UncompressedSize;
} PACK_STRUCT;

struct SZIPFileHeader
{
	u32 Sig; // 'PK0304' little endian (0x04034b50)
	s16 VersionToExtract;
	s16 GeneralBitFlag;
	s16 CompressionMethod;
	s16 LastModFileTime;
	s16 LastModFileDate;
	SZIPFileDataDescriptor DataDescriptor;
	s16 FilenameLength;
	s16 ExtraFieldLength;
	// filename (variable size)
	// extra field (variable size )
} PACK_STRUCT;

struct SZIPFileCentralDirFileHeader
{
	u32 Sig; // 'PK0102' (0x02014b50)
	u16 VersionMadeBy;
	u16 VersionToExtract;
	u16 GeneralBitFlag;
	u16 CompressionMethod;
	u16 LastModFileTime;
	u16 LastModFileDate;
	u32 CRC32;
	u32 CompressedSize;
	u32 UncompressedSize;
	u16 FilenameLength;
	u16 ExtraFieldLength;
	u16 FileCommentLength;
	u16 DiskNumberStart;
	u16 InternalFileAttributes;
	u32 ExternalFileAttributes;
	u32 RelativeOffsetOfLocalHeader;

	// filename (variable size)
	// extra field (variable size)
	// file comment (variable size)

} PACK_STRUCT;

struct SZIPFileCentralDirEnd
{
	u32 Sig;           // 'PK0506' end_of central dir signature			// (0x06054b50)
	u16 NumberDisk;    // number of this disk
	u16 NumberStart;   // number of the disk with the start of the central directory
	u16 TotalDisk;     // total number of entries in the central dir on this disk
	u16 TotalEntries;  // total number of entries in the central dir
	u32 Size;          // size of the central directory
	u32 Offset;        // offset of start of centraldirectory with respect to the starting disk number
	u16 CommentLength; // zipfile comment length
					   // zipfile comment (variable size)
} PACK_STRUCT;

// Default alignment
#include "irrunpack.h"

//! Contains extended info about zip files in the archive
struct SZipFileEntry
{
	//! Position of data in the archive file
	s32 Offset;

	//! The header for this file containing compression info etc
	SZIPFileHeader header;
};

//! Archiveloader capable of loading ZIP Archives
class CArchiveLoaderZIP : public IArchiveLoader
{
public:
	//! Constructor
	CArchiveLoaderZIP(io::IFileSystem *fs);

	//! returns true if the file maybe is able to be loaded by this class
	//! based on the file extension (e.g. ".zip")
	bool isALoadableFileFormat(const io::path &filename) const override;

	//! Check if the file might be loaded by this class
	/** Check might look into the file.
	\param file File handle to check.
	\return True if file seems to be loadable. */
	bool isALoadableFileFormat(io::IReadFile *file) const override;

	//! Check to see if the loader can create archives of this type.
	/** Check based on the archive type.
	\param fileType The archive type to check.
	\return True if the archile loader supports this type, false if not */
	bool isALoadableFileFormat(E_FILE_ARCHIVE_TYPE fileType) const override;

	//! Creates an archive from the filename
	/** \param file File handle to check.
	\return Pointer to newly created archive, or 0 upon error. */
	IFileArchive *createArchive(const io::path &filename, bool ignoreCase, bool ignorePaths) const override;

	//! creates/loads an archive from the file.
	//! \return Pointer to the created archive. Returns 0 if loading failed.
	io::IFileArchive *createArchive(io::IReadFile *file, bool ignoreCase, bool ignorePaths) const override;

private:
	io::IFileSystem *FileSystem;
};

/*!
	Zip file Reader written April 2002 by N.Gebhardt.
*/
class CZipReader : public virtual IFileArchive, virtual CFileList
{
public:
	//! constructor
	CZipReader(IFileSystem *fs, IReadFile *file, bool ignoreCase, bool ignorePaths);

	//! destructor
	virtual ~CZipReader();

	//! opens a file by file name
	IReadFile *createAndOpenFile(const io::path &filename) override;

	//! opens a file by index
	IReadFile *createAndOpenFile(u32 index) override;

	//! returns the list of files
	const IFileList *getFileList() const override;

	//! get the archive type
	E_FILE_ARCHIVE_TYPE getType() const override;

	//! return the id of the file Archive
	const io::path &getArchiveName() const override { return Path; }

protected:
	//! reads the next file header from a ZIP file, returns false if there are no more headers.
	/* if ignoreGPBits is set, the item will be read despite missing
	file information. This is used when reading items from the central
	directory. */
	bool scanZipHeader(bool ignoreGPBits = false);

	bool scanCentralDirectoryHeader();

	io::IFileSystem *FileSystem;
	IReadFile *File;

	// holds extended info about files
	std::vector<SZipFileEntry> FileInfo;
};

} // end namespace io
