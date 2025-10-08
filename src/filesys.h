// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "config.h"
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <fstream>

#ifdef _WIN32
#define DIR_DELIM "\\"
#define DIR_DELIM_CHAR '\\'
#define FILESYS_CASE_INSENSITIVE true
#define PATH_DELIM ";"
#else
#define DIR_DELIM "/"
#define DIR_DELIM_CHAR '/'
#define FILESYS_CASE_INSENSITIVE false
#define PATH_DELIM ":"
#endif

namespace io {
class IFileSystem;
}

namespace fs
{

struct DirListNode
{
	std::string name;
	bool dir;
};

[[nodiscard]]
std::vector<DirListNode> GetDirListing(const std::string &path);

// Returns true if already exists
bool CreateDir(const std::string &path);

[[nodiscard]] bool PathExists(const std::string &path);

[[nodiscard]] bool IsPathAbsolute(const std::string &path);

[[nodiscard]] bool IsDir(const std::string &path);

[[nodiscard]] bool IsExecutable(const std::string &path);

[[nodiscard]] bool IsFile(const std::string &path);

[[nodiscard]] inline bool IsDirDelimiter(char c)
{
	return c == '/' || c == DIR_DELIM_CHAR;
}

// Only pass full paths to this one. returns true on success.
bool RecursiveDelete(const std::string &path);

bool DeleteSingleFileOrEmptyDirectory(const std::string &path);

/// Returns path to temp directory.
/// You probably don't want to use this directly, see `CreateTempFile` or `CreateTempDir`.
/// @return path or "" on error
[[nodiscard]] std::string TempPath();

/// Returns path to securely-created temporary file (will already exist when this function returns).
/// @return path or "" on error
[[nodiscard]] std::string CreateTempFile();

/// Returns path to securely-created temporary directory (will already exist when this function returns).
/// @return path or "" on error
[[nodiscard]] std::string CreateTempDir();

/* Returns a list of subdirectories, including the path itself, but excluding
       hidden directories (whose names start with . or _)
*/
void GetRecursiveDirs(std::vector<std::string> &dirs, const std::string &dir);
[[nodiscard]]
std::vector<std::string> GetRecursiveDirs(const std::string &dir);

/* Multiplatform */

/* The path itself not included, returns a list of all subpaths.
   dst - vector that contains all the subpaths.
   list files - include files in the list of subpaths.
   ignore - paths that start with these charcters will not be listed.
*/
void GetRecursiveSubPaths(const std::string &path,
		  std::vector<std::string> &dst,
		  bool list_files,
		  const std::set<char> &ignore = {});

// Only pass full paths to this one. True on success.
bool RecursiveDeleteContent(const std::string &path);

// Create all directories on the given path that don't already exist.
bool CreateAllDirs(const std::string &path);

// Copy a regular file
bool CopyFileContents(const std::string &source, const std::string &target);

// Copy directory and all subdirectories
// Omits files and subdirectories that start with a period
bool CopyDir(const std::string &source, const std::string &target);

// Move directory and all subdirectories
// Behavior with files/subdirs that start with a period is undefined
bool MoveDir(const std::string &source, const std::string &target);

// Check if one path is prefix of another
// For example, "/tmp" is a prefix of "/tmp" and "/tmp/file" but not "/tmp2"
// Ignores case differences and '/' vs. '\\' on Windows
bool PathStartsWith(const std::string &path, const std::string &prefix);

// Remove last path component and the dir delimiter before and/or after it,
// returns "" if there is only one path component.
// removed: If non-NULL, receives the removed component(s).
// count: Number of components to remove
std::string RemoveLastPathComponent(const std::string &path,
		std::string *removed = NULL, int count = 1);

// Remove "." and ".." path components and for every ".." removed, remove
// the last normal path component before it. Unlike AbsolutePath,
// this does not resolve symlinks and check for existence of directories.
std::string RemoveRelativePathComponents(std::string path);

// Returns the absolute path for the passed path, with "." and ".." path
// components and symlinks removed.  Returns "" on error.
[[nodiscard]]
std::string AbsolutePath(const std::string &path);

// This is a combination of RemoveRelativePathComponents() and AbsolutePath()
// It will resolve symlinks for the leading path components that exist and
// still remove "." and ".." in the rest of the path.
// Returns "" on error.
[[nodiscard]]
std::string AbsolutePathPartial(const std::string &path);

// Returns the filename from a path or the entire path if no directory
// delimiter is found.
[[nodiscard]]
const char *GetFilenameFromPath(const char *path);

// Replace the content of a file on disk in a way that is safe from
// corruption/truncation on a crash.
// logs and returns false on error
bool safeWriteToFile(const std::string &path, std::string_view content);

#if IS_CLIENT_BUILD
bool extractZipFile(io::IFileSystem *fs, const char *filename, const std::string &destination);
#endif

bool ReadFile(const std::string &path, std::string &out, bool log_error = false);

bool Rename(const std::string &from, const std::string &to);

/**
 * Open a file buffer with error handling, commonly used with `std::fstream.rdbuf()`.
 *
 * @param stream stream references, must not already be open
 * @param filename filename to open
 * @param mode mode bits (used as-is)
 * @param log_error log failure to errorstream?
 * @param log_warn log failure to warningstream?
 * @return true if success
*/
bool OpenStream(std::filebuf &stream, const char *filename,
	std::ios::openmode mode, bool log_error, bool log_warn);

} // namespace fs

// outside of namespace for convenience:

/**
 * Helper function to open an output file stream (= writing).
 *
 * For compatibility with fopen() binary mode and truncate are always set.
 * Use `fs::OpenStream` for more control.
 * @param name file name
 * @param log if true, failure to open the file will be logged to errorstream
 * @param mode additional mode bits (e.g. std::ios::app)
 * @return file stream, will be !good in case of error
*/
[[nodiscard]]
inline std::ofstream open_ofstream(const char *name, bool log,
	std::ios::openmode mode = std::ios::openmode())
{
	mode |= std::ios::out | std::ios::binary;
	if (!(mode & std::ios::app))
		mode |= std::ios::trunc;
	std::ofstream ofs;
	if (!fs::OpenStream(*ofs.rdbuf(), name, mode, log, false))
		ofs.setstate(std::ios::failbit);
	return ofs;
}

/**
 * Helper function to open an input file stream (= reading).
 *
 * For compatibility with fopen() binary mode is always set.
 * Use `fs::OpenStream` for more control.
 * @param name file name
 * @param log if true, failure to open the file will be logged to errorstream
 * @param mode additional mode bits (e.g. std::ios::ate)
 * @return file stream, will be !good in case of error
*/
[[nodiscard]]
inline std::ifstream open_ifstream(const char *name, bool log,
	std::ios::openmode mode = std::ios::openmode())
{
	mode |= std::ios::in | std::ios::binary;
	std::ifstream ifs;
	if (!fs::OpenStream(*ifs.rdbuf(), name, mode, log, false))
		ifs.setstate(std::ios::failbit);
	return ifs;
}
