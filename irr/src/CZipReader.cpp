// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CZipReader.h"

#include "os.h"

#include "CFileList.h"
#include "CReadFile.h"
#include "coreutil.h"

#include <zlib.h> // use system lib

namespace io
{

// -----------------------------------------------------------------------------
// zip loader
// -----------------------------------------------------------------------------

//! Constructor
CArchiveLoaderZIP::CArchiveLoaderZIP(io::IFileSystem *fs) :
		FileSystem(fs)
{}

//! returns true if the file maybe is able to be loaded by this class
bool CArchiveLoaderZIP::isALoadableFileFormat(const io::path &filename) const
{
	return core::hasFileExtension(filename, "zip", "pk3");
}

//! Check to see if the loader can create archives of this type.
bool CArchiveLoaderZIP::isALoadableFileFormat(E_FILE_ARCHIVE_TYPE fileType) const
{
	return fileType == EFAT_ZIP;
}

//! Creates an archive from the filename
/** \param file File handle to check.
\return Pointer to newly created archive, or 0 upon error. */
IFileArchive *CArchiveLoaderZIP::createArchive(const io::path &filename, bool ignoreCase, bool ignorePaths) const
{
	IFileArchive *archive = 0;
	io::IReadFile *file = FileSystem->createAndOpenFile(filename);

	if (file) {
		archive = createArchive(file, ignoreCase, ignorePaths);
		file->drop();
	}

	return archive;
}

//! creates/loads an archive from the file.
//! \return Pointer to the created archive. Returns 0 if loading failed.
IFileArchive *CArchiveLoaderZIP::createArchive(io::IReadFile *file, bool ignoreCase, bool ignorePaths) const
{
	IFileArchive *archive = 0;
	if (file) {
		file->seek(0);

		archive = new CZipReader(FileSystem, file, ignoreCase, ignorePaths);
	}
	return archive;
}

//! Check if the file might be loaded by this class
/** Check might look into the file.
\param file File handle to check.
\return True if file seems to be loadable. */
bool CArchiveLoaderZIP::isALoadableFileFormat(io::IReadFile *file) const
{
	SZIPFileHeader header;

	file->read(&header.Sig, 4);
#ifdef __BIG_ENDIAN__
	header.Sig = os::Byteswap::byteswap(header.Sig);
#endif

	return header.Sig == 0x04034b50; // ZIP
}

// -----------------------------------------------------------------------------
// zip archive
// -----------------------------------------------------------------------------

CZipReader::CZipReader(IFileSystem *fs, IReadFile *file, bool ignoreCase, bool ignorePaths) :
		CFileList((file ? file->getFileName() : io::path("")), ignoreCase, ignorePaths), FileSystem(fs), File(file)
{
	if (File) {
		File->grab();

		// load file entries
		while (scanZipHeader()) {}

		sort();
	}
}

CZipReader::~CZipReader()
{
	if (File)
		File->drop();
}

//! get the archive type
E_FILE_ARCHIVE_TYPE CZipReader::getType() const
{
	return EFAT_ZIP;
}

const IFileList *CZipReader::getFileList() const
{
	return this;
}

//! scans for a local header, returns false if there is no more local file header.
bool CZipReader::scanZipHeader(bool ignoreGPBits)
{
	io::path ZipFileName = "";
	SZipFileEntry entry;
	entry.Offset = 0;
	memset(&entry.header, 0, sizeof(SZIPFileHeader));

	File->read(&entry.header, sizeof(SZIPFileHeader));

#ifdef __BIG_ENDIAN__
	entry.header.Sig = os::Byteswap::byteswap(entry.header.Sig);
	entry.header.VersionToExtract = os::Byteswap::byteswap(entry.header.VersionToExtract);
	entry.header.GeneralBitFlag = os::Byteswap::byteswap(entry.header.GeneralBitFlag);
	entry.header.CompressionMethod = os::Byteswap::byteswap(entry.header.CompressionMethod);
	entry.header.LastModFileTime = os::Byteswap::byteswap(entry.header.LastModFileTime);
	entry.header.LastModFileDate = os::Byteswap::byteswap(entry.header.LastModFileDate);
	entry.header.DataDescriptor.CRC32 = os::Byteswap::byteswap(entry.header.DataDescriptor.CRC32);
	entry.header.DataDescriptor.CompressedSize = os::Byteswap::byteswap(entry.header.DataDescriptor.CompressedSize);
	entry.header.DataDescriptor.UncompressedSize = os::Byteswap::byteswap(entry.header.DataDescriptor.UncompressedSize);
	entry.header.FilenameLength = os::Byteswap::byteswap(entry.header.FilenameLength);
	entry.header.ExtraFieldLength = os::Byteswap::byteswap(entry.header.ExtraFieldLength);
#endif

	if (entry.header.Sig != 0x04034b50)
		return false; // local file headers end here.

	// read filename
	{
		c8 *tmp = new c8[entry.header.FilenameLength + 2];
		File->read(tmp, entry.header.FilenameLength);
		tmp[entry.header.FilenameLength] = 0;
		ZipFileName = tmp;
		delete[] tmp;
	}

	if (entry.header.ExtraFieldLength)
		File->seek(entry.header.ExtraFieldLength, true);

	// if bit 3 was set, use CentralDirectory for setup
	if (!ignoreGPBits && entry.header.GeneralBitFlag & ZIP_INFO_IN_DATA_DESCRIPTOR) {
		SZIPFileCentralDirEnd dirEnd;
		FileInfo.clear();
		Files.clear();
		// First place where the end record could be stored
		File->seek(File->getSize() - 22);
		const char endID[] = {0x50, 0x4b, 0x05, 0x06, 0x0};
		char tmp[5] = {'\0'};
		bool found = false;
		// search for the end record ID
		while (!found && File->getPos() > 0) {
			int seek = 8;
			File->read(tmp, 4);
			switch (tmp[0]) {
			case 0x50:
				if (!strcmp(endID, tmp)) {
					seek = 4;
					found = true;
				}
				break;
			case 0x4b:
				seek = 5;
				break;
			case 0x05:
				seek = 6;
				break;
			case 0x06:
				seek = 7;
				break;
			}
			File->seek(-seek, true);
		}
		File->read(&dirEnd, sizeof(dirEnd));
#ifdef __BIG_ENDIAN__
		dirEnd.NumberDisk = os::Byteswap::byteswap(dirEnd.NumberDisk);
		dirEnd.NumberStart = os::Byteswap::byteswap(dirEnd.NumberStart);
		dirEnd.TotalDisk = os::Byteswap::byteswap(dirEnd.TotalDisk);
		dirEnd.TotalEntries = os::Byteswap::byteswap(dirEnd.TotalEntries);
		dirEnd.Size = os::Byteswap::byteswap(dirEnd.Size);
		dirEnd.Offset = os::Byteswap::byteswap(dirEnd.Offset);
		dirEnd.CommentLength = os::Byteswap::byteswap(dirEnd.CommentLength);
#endif
		FileInfo.reserve(dirEnd.TotalEntries);
		File->seek(dirEnd.Offset);
		while (scanCentralDirectoryHeader()) {
		}
		return false;
	}

	// store position in file
	entry.Offset = File->getPos();
	// move forward length of data
	File->seek(entry.header.DataDescriptor.CompressedSize, true);

#ifdef _DEBUG
// os::Debuginfo::print("added file from archive", ZipFileName.c_str());
#endif

	addItem(ZipFileName, entry.Offset, entry.header.DataDescriptor.UncompressedSize, ZipFileName.lastChar() == '/', FileInfo.size());
	FileInfo.push_back(entry);

	return true;
}

//! scans for a local header, returns false if there is no more local file header.
bool CZipReader::scanCentralDirectoryHeader()
{
	io::path ZipFileName = "";
	SZIPFileCentralDirFileHeader entry;
	File->read(&entry, sizeof(SZIPFileCentralDirFileHeader));

#ifdef __BIG_ENDIAN__
	entry.Sig = os::Byteswap::byteswap(entry.Sig);
	entry.VersionMadeBy = os::Byteswap::byteswap(entry.VersionMadeBy);
	entry.VersionToExtract = os::Byteswap::byteswap(entry.VersionToExtract);
	entry.GeneralBitFlag = os::Byteswap::byteswap(entry.GeneralBitFlag);
	entry.CompressionMethod = os::Byteswap::byteswap(entry.CompressionMethod);
	entry.LastModFileTime = os::Byteswap::byteswap(entry.LastModFileTime);
	entry.LastModFileDate = os::Byteswap::byteswap(entry.LastModFileDate);
	entry.CRC32 = os::Byteswap::byteswap(entry.CRC32);
	entry.CompressedSize = os::Byteswap::byteswap(entry.CompressedSize);
	entry.UncompressedSize = os::Byteswap::byteswap(entry.UncompressedSize);
	entry.FilenameLength = os::Byteswap::byteswap(entry.FilenameLength);
	entry.ExtraFieldLength = os::Byteswap::byteswap(entry.ExtraFieldLength);
	entry.FileCommentLength = os::Byteswap::byteswap(entry.FileCommentLength);
	entry.DiskNumberStart = os::Byteswap::byteswap(entry.DiskNumberStart);
	entry.InternalFileAttributes = os::Byteswap::byteswap(entry.InternalFileAttributes);
	entry.ExternalFileAttributes = os::Byteswap::byteswap(entry.ExternalFileAttributes);
	entry.RelativeOffsetOfLocalHeader = os::Byteswap::byteswap(entry.RelativeOffsetOfLocalHeader);
#endif

	if (entry.Sig != 0x02014b50)
		return false; // central dir headers end here.

	const long pos = File->getPos();
	File->seek(entry.RelativeOffsetOfLocalHeader);
	scanZipHeader(true);
	File->seek(pos + entry.FilenameLength + entry.ExtraFieldLength + entry.FileCommentLength);
	auto &lastInfo = FileInfo.back();
	lastInfo.header.DataDescriptor.CompressedSize = entry.CompressedSize;
	lastInfo.header.DataDescriptor.UncompressedSize = entry.UncompressedSize;
	lastInfo.header.DataDescriptor.CRC32 = entry.CRC32;
	Files.getLast().Size = entry.UncompressedSize;
	return true;
}

//! opens a file by file name
IReadFile *CZipReader::createAndOpenFile(const io::path &filename)
{
	s32 index = findFile(filename, false);

	if (index != -1)
		return createAndOpenFile(index);

	return 0;
}

//! opens a file by index
IReadFile *CZipReader::createAndOpenFile(u32 index)
{
	// Irrlicht supports 0, 8, 12, 14, 99
	// 0 - The file is stored (no compression)
	// 1 - The file is Shrunk
	// 2 - The file is Reduced with compression factor 1
	// 3 - The file is Reduced with compression factor 2
	// 4 - The file is Reduced with compression factor 3
	// 5 - The file is Reduced with compression factor 4
	// 6 - The file is Imploded
	// 7 - Reserved for Tokenizing compression algorithm
	// 8 - The file is Deflated
	// 9 - Reserved for enhanced Deflating
	// 10 - PKWARE Date Compression Library Imploding
	// 12 - bzip2 - Compression Method from libbz2, WinZip 10
	// 14 - LZMA - Compression Method, WinZip 12
	// 96 - Jpeg compression - Compression Method, WinZip 12
	// 97 - WavPack - Compression Method, WinZip 11
	// 98 - PPMd - Compression Method, WinZip 10
	// 99 - AES encryption, WinZip 9

	const SZipFileEntry &e = FileInfo[Files[index].ID];
	char buf[64];
	s16 actualCompressionMethod = e.header.CompressionMethod;
	IReadFile *decrypted = 0;
	u8 *decryptedBuf = 0;
	u32 decryptedSize = e.header.DataDescriptor.CompressedSize;
	switch (actualCompressionMethod) {
	case 0: // no compression
	{
		if (decrypted)
			return decrypted;
		else
			return createLimitReadFile(Files[index].FullName, File, e.Offset, decryptedSize);
	}
	case 8: {
		const u32 uncompressedSize = e.header.DataDescriptor.UncompressedSize;
		c8 *pBuf = new c8[uncompressedSize];
		if (!pBuf) {
			snprintf_irr(buf, 64, "Not enough memory for decompressing %s", Files[index].FullName.c_str());
			os::Printer::log(buf, ELL_ERROR);
			if (decrypted)
				decrypted->drop();
			return 0;
		}

		u8 *pcData = decryptedBuf;
		if (!pcData) {
			pcData = new u8[decryptedSize];
			if (!pcData) {
				snprintf_irr(buf, 64, "Not enough memory for decompressing %s", Files[index].FullName.c_str());
				os::Printer::log(buf, ELL_ERROR);
				delete[] pBuf;
				return 0;
			}

			// memset(pcData, 0, decryptedSize);
			File->seek(e.Offset);
			File->read(pcData, decryptedSize);
		}

		// Setup the inflate stream.
		z_stream stream;
		s32 err;

		stream.next_in = (Bytef *)pcData;
		stream.avail_in = (uInt)decryptedSize;
		stream.next_out = (Bytef *)pBuf;
		stream.avail_out = uncompressedSize;
		stream.zalloc = (alloc_func)0;
		stream.zfree = (free_func)0;

		// Perform inflation. wbits < 0 indicates no zlib header inside the data.
		err = inflateInit2(&stream, -MAX_WBITS);
		if (err == Z_OK) {
			err = inflate(&stream, Z_FINISH);
			inflateEnd(&stream);
			if (err == Z_STREAM_END)
				err = Z_OK;
			err = Z_OK;
			inflateEnd(&stream);
		}

		if (decrypted)
			decrypted->drop();
		else
			delete[] pcData;

		if (err != Z_OK) {
			snprintf_irr(buf, 64, "Error decompressing %s", Files[index].FullName.c_str());
			os::Printer::log(buf, ELL_ERROR);
			delete[] pBuf;
			return 0;
		} else
			return FileSystem->createMemoryReadFile(pBuf, uncompressedSize, Files[index].FullName, true);
	}
	case 12: {
		os::Printer::log("bzip2 decompression not supported. File cannot be read.", ELL_ERROR);
		return 0;
	}
	case 14: {
		os::Printer::log("lzma decompression not supported. File cannot be read.", ELL_ERROR);
		return 0;
	}
	case 99:
		// If we come here with an encrypted file, decryption support is missing
		os::Printer::log("Decryption support not enabled. File cannot be read.", ELL_ERROR);
		return 0;
	default:
		snprintf_irr(buf, 64, "file has unsupported compression method. %s", Files[index].FullName.c_str());
		os::Printer::log(buf, ELL_ERROR);
		return 0;
	};
}

} // end namespace io
