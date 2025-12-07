// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "filesys.h"
#include "util/string.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <atomic>
#include <memory>
#include "log.h"
#include "config.h"
#include "porting.h"
#if CHECK_CLIENT_BUILD()
#include "irr_ptr.h"
#include <IFileArchive.h>
#include <IFileSystem.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <io.h>
#include <direct.h>
#else
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef __linux__
#include <sys/ioctl.h>
#ifndef FICLONE
#define FICLONE _IOW(0x94, 9, int)
#endif
#endif

// Error from last OS call as string
#ifdef _WIN32
#define LAST_OS_ERROR() porting::ConvertError(GetLastError())
#else
#define LAST_OS_ERROR() strerror(errno)
#endif

namespace fs
{

#ifdef _WIN32

/***********
 * Windows *
 ***********/

std::vector<DirListNode> GetDirListing(const std::string &pathstring)
{
	std::vector<DirListNode> listing;

	WIN32_FIND_DATA FindFileData{};
	DWORD dwError;

	std::string dirSpec = pathstring + "\\*";

	// Find the first file in the directory.
	HANDLE hFind = FindFirstFile(dirSpec.c_str(), &FindFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		dwError = GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND) {
			errorstream << "GetDirListing: FindFirstFile error code "
					<< dwError << std::endl;
		}
	} else {
		// NOTE:
		// Be very sure to not include '..' in the results, it will
		// result in an epic failure when deleting stuff.

		// do-while because the first file is already in the struct
		do {
			DirListNode node;
			node.name = FindFileData.cFileName;
			node.dir = FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			if(node.name != "." && node.name != "..")
				listing.push_back(std::move(node));
		} while (FindNextFile(hFind, &FindFileData) != 0);

		dwError = GetLastError();
		FindClose(hFind);
		if (dwError != ERROR_NO_MORE_FILES) {
			errorstream << "GetDirListing: FindNextFile error code "
					<< dwError << std::endl;
			listing.clear();
			return listing;
		}
	}
	return listing;
}

bool CreateDir(const std::string &path)
{
	bool r = CreateDirectory(path.c_str(), nullptr);
	if (r || GetLastError() == ERROR_ALREADY_EXISTS)
		return true;
	return false;
}

bool PathExists(const std::string &path)
{
	return (GetFileAttributes(path.c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool IsPathAbsolute(const std::string &path)
{
	return !PathIsRelative(path.c_str());
}

bool IsDir(const std::string &path)
{
	DWORD attr = GetFileAttributes(path.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES &&
			(attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool IsFile(const std::string &path)
{
	DWORD attr = GetFileAttributes(path.c_str());
	return (attr != INVALID_FILE_ATTRIBUTES &&
			!(attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool IsExecutable(const std::string &path)
{
	DWORD type;
	return GetBinaryType(path.c_str(), &type) != 0;
}

bool RecursiveDelete(const std::string &path)
{
	assert(IsPathAbsolute(path));
	if (!PathExists(path))
		return true;

	bool is_file = !IsDir(path);
	infostream << "Recursively deleting " << (is_file ? "file" : "directory")
		<< " \"" << path << "\"" << std::endl;
	if (is_file) {
		if (!DeleteFile(path.c_str())) {
			errorstream << "RecursiveDelete: Failed to delete file \""
					<< path << "\": " << LAST_OS_ERROR() << std::endl;
			return false;
		}
		return true;
	}
	std::vector<DirListNode> content = GetDirListing(path);
	for (const auto &n : content) {
		std::string fullpath = path + DIR_DELIM + n.name;
		if (!RecursiveDelete(fullpath)) {
			errorstream << "RecursiveDelete: Failed to recurse to \""
					<< fullpath << "\"" << std::endl;
			return false;
		}
	}
	if (!RemoveDirectory(path.c_str())) {
		errorstream << "RecursiveDelete: Failed to delete directory \""
					<< path << "\": " << LAST_OS_ERROR() << std::endl;
		return false;
	}
	return true;
}

bool DeleteSingleFileOrEmptyDirectory(const std::string &path, bool log_error)
{
	if (!IsDir(path)) {
		bool ok = DeleteFile(path.c_str()) != 0;
		if (!ok && log_error)
			errorstream << "DeleteFile failed: " << LAST_OS_ERROR() << std::endl;
	}
	bool ok = RemoveDirectory(path.c_str()) != 0;
	if (!ok && log_error)
		errorstream << "RemoveDirectory failed: " << LAST_OS_ERROR() << std::endl;
	return ok;
}

std::string TempPath()
{
	DWORD bufsize = GetTempPath(0, nullptr);
	if (bufsize == 0) {
		errorstream << "GetTempPath failed: " << LAST_OS_ERROR() << std::endl;
		return "";
	}
	std::string buf;
	buf.resize(bufsize);
	DWORD len = GetTempPath(bufsize, &buf[0]);
	if (len == 0 || len > bufsize) {
		errorstream << "GetTempPath failed: " << LAST_OS_ERROR() << std::endl;
		return "";
	}
	buf.resize(len);
	return buf;
}

std::string CreateTempFile()
{
	std::string path = TempPath() + DIR_DELIM "MT_XXXXXX";
	_mktemp_s(&path[0], path.size() + 1); // modifies path
	HANDLE file = CreateFile(path.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return "";
	CloseHandle(file);
	return path;
}

std::string CreateTempDir()
{
	std::string path = TempPath() + DIR_DELIM "MT_XXXXXX";
	_mktemp_s(&path[0], path.size() + 1); // modifies path
	// will error if it already exists
	if (!CreateDirectory(path.c_str(), nullptr))
		return "";
	return path;
}

bool CopyFileContents(const std::string &source, const std::string &target)
{
	BOOL ok = CopyFileEx(source.c_str(), target.c_str(), nullptr, nullptr,
		nullptr, COPY_FILE_ALLOW_DECRYPTED_DESTINATION);
	if (!ok) {
		errorstream << "copying " << source << " to " << target
			<< " failed: " << LAST_OS_ERROR() << std::endl;
		return false;
	}

	// docs: "File attributes for the existing file are copied to the new file."
	// This is not our intention so get rid of unwanted attributes:
	DWORD attr = GetFileAttributes(target.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
		errorstream << target << ": file disappeared after copy" << std::endl;
		return false;
	}
	attr &= ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN);
	SetFileAttributes(target.c_str(), attr);

	tracestream << "copied " << source << " to " << target
		<< " using CopyFileEx" << std::endl;
	return true;
}

#else

/*********
 * POSIX *
 *********/

std::vector<DirListNode> GetDirListing(const std::string &pathstring)
{
	std::vector<DirListNode> listing;

	DIR *dp;
	if((dp = opendir(pathstring.c_str())) == nullptr) {
		return listing;
	}

	struct dirent *dirp;
	while ((dirp = readdir(dp)) != nullptr) {
		// NOTE:
		// Be very sure to not include '..' in the results, it will
		// result in an epic failure when deleting stuff.
		if(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
			continue;

		DirListNode node;
		node.name = dirp->d_name;

		int isdir = -1; // -1 means unknown

		/*
			POSIX doesn't define d_type member of struct dirent and
			certain filesystems on glibc/Linux will only return
			DT_UNKNOWN for the d_type member.

			Also we don't know whether symlinks are directories or not.
		*/
#ifdef _DIRENT_HAVE_D_TYPE
		if(dirp->d_type != DT_UNKNOWN && dirp->d_type != DT_LNK)
			isdir = (dirp->d_type == DT_DIR);
#endif /* _DIRENT_HAVE_D_TYPE */

		/*
			Was d_type DT_UNKNOWN, DT_LNK or nonexistent?
			If so, try stat().
		*/
		if(isdir == -1) {
			struct stat statbuf{};
			if (stat((pathstring + DIR_DELIM + node.name).c_str(), &statbuf))
				continue;
			isdir = ((statbuf.st_mode & S_IFDIR) == S_IFDIR);
		}
		node.dir = isdir;

		listing.push_back(std::move(node));
	}
	closedir(dp);

	return listing;
}

bool CreateDir(const std::string &path)
{
	int r = mkdir(path.c_str(), 0775);
	if (r == 0) {
		return true;
	}

	// If already exists, return true
	if (errno == EEXIST)
		return true;
	return false;

}

bool PathExists(const std::string &path)
{
	return access(path.c_str(), F_OK) == 0;
}

bool IsPathAbsolute(const std::string &path)
{
	return path[0] == '/';
}

bool IsDir(const std::string &path)
{
	struct stat statbuf{};
	if (stat(path.c_str(), &statbuf))
		return false; // Actually error; but certainly not a directory
	return ((statbuf.st_mode & S_IFDIR) == S_IFDIR);
}

bool IsFile(const std::string &path)
{
	struct stat statbuf{};
	if (stat(path.c_str(), &statbuf))
		return false;
#ifdef S_IFSOCK
	// sockets cannot be opened in any way, so they are not files.
	if ((statbuf.st_mode & S_IFSOCK) == S_IFSOCK)
		return false;
#endif
	return ((statbuf.st_mode & S_IFDIR) != S_IFDIR);
}

bool IsExecutable(const std::string &path)
{
	return access(path.c_str(), X_OK) == 0;
}

bool RecursiveDelete(const std::string &path)
{
	/*
		Execute the 'rm' command directly, by fork() and execve()
	*/

	infostream << "Removing \"" << path << "\"" << std::endl;

	assert(IsPathAbsolute(path));

	const pid_t child_pid = fork();

	if (child_pid == -1) {
		errorstream << "fork errno: " << errno << ": " << strerror(errno)
			<< std::endl;
		return false;
	}

	if (child_pid == 0) {
		// Child
		std::array<const char*, 4> argv = {
			"rm",
			"-rf",
			path.c_str(),
			nullptr
		};

		execvp(argv[0], const_cast<char**>(argv.data()));

		// note: use cerr because our logging won't flush in forked process
		std::cerr << "exec errno: " << errno << ": " << strerror(errno)
			<< std::endl;
		_exit(1);
	} else {
		// Parent
		int status;
		pid_t tpid;
		do
			tpid = waitpid(child_pid, &status, 0);
		while (tpid != child_pid);
		return WIFEXITED(status) && WEXITSTATUS(status) == 0;
	}
}

bool DeleteSingleFileOrEmptyDirectory(const std::string &path, bool log_error)
{
	if (IsDir(path)) {
		bool did = rmdir(path.c_str()) == 0;
		if (!did && log_error)
			errorstream << "rmdir error: " << LAST_OS_ERROR() << std::endl;
		return did;
	}

	bool did = unlink(path.c_str()) == 0;
	if (!did && log_error)
		errorstream << "unlink error: " << LAST_OS_ERROR() << std::endl;
	return did;
}

std::string TempPath()
{
#ifdef __ANDROID__
	return porting::path_cache;
#else
	const char *env_tmpdir = getenv("TMPDIR");
	if (env_tmpdir && env_tmpdir[0] == DIR_DELIM_CHAR)
		return env_tmpdir;
#ifdef P_tmpdir
	return P_tmpdir;
#else
	return DIR_DELIM "tmp";
#endif
#endif
}

std::string CreateTempFile()
{
	std::string path = TempPath() + DIR_DELIM "MT_XXXXXX";
	int fd = mkstemp(&path[0]); // modifies path
	if (fd == -1)
		return "";
	close(fd);
	return path;
}

std::string CreateTempDir()
{
	std::string path = TempPath() + DIR_DELIM "MT_XXXXXX";
	auto r = mkdtemp(&path[0]); // modifies path
	if (!r)
		return "";
	return path;
}

namespace {
	struct FileDeleter {
		void operator()(FILE *stream) {
			fclose(stream);
		}
	};

	typedef std::unique_ptr<FILE, FileDeleter> FileUniquePtr;
}

bool CopyFileContents(const std::string &source, const std::string &target)
{
	FileUniquePtr sourcefile, targetfile;

#ifdef __linux__
	// Try to clone using Copy-on-Write (CoW). This is instant but supported
	// only by some filesystems.

	int srcfd, tgtfd;
	srcfd = open(source.c_str(), O_RDONLY);
	if (srcfd == -1) {
		errorstream << source << ": can't open for reading: "
			<< strerror(errno) << std::endl;
		return false;
	}
	tgtfd = open(target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0664);
	if (tgtfd == -1) {
		errorstream << target << ": can't open for writing: "
			<< strerror(errno) << std::endl;
		close(srcfd);
		return false;
	}

	if (ioctl(tgtfd, FICLONE, srcfd) == 0) {
		tracestream << "copied " << source << " to " << target
			<< " using FICLONE" << std::endl;
		close(srcfd);
		close(tgtfd);
		return true;
	}

	// fallback to normal copy, but no need to reopen the files
	sourcefile.reset(fdopen(srcfd, "rb"));
	targetfile.reset(fdopen(tgtfd, "wb"));
#else
	sourcefile.reset(fopen(source.c_str(), "rb"));
	targetfile.reset(fopen(target.c_str(), "wb"));
#endif
	if (!sourcefile) {
		errorstream << source << ": can't open for reading: "
			<< strerror(errno) << std::endl;
		return false;
	}
	if (!targetfile) {
		errorstream << target << ": can't open for writing: "
			<< strerror(errno) << std::endl;
		return false;
	}

	size_t total = 0;
	bool done = false;
	char readbuffer[BUFSIZ];
	while (!done) {
		size_t readbytes = fread(readbuffer, 1,
				sizeof(readbuffer), sourcefile.get());
		total += readbytes;
		if (ferror(sourcefile.get())) {
			errorstream << source << ": IO error: "
				<< strerror(errno) << std::endl;
			return false;
		}
		if (readbytes > 0)
			fwrite(readbuffer, 1, readbytes, targetfile.get());
		if (feof(sourcefile.get())) {
			// flush destination file to catch write errors (e.g. disk full)
			fflush(targetfile.get());
			done = true;
		}
		if (ferror(targetfile.get())) {
			errorstream << target << ": IO error: "
					<< strerror(errno) << std::endl;
			return false;
		}
	}
	tracestream << "copied " << total << " bytes from "
		<< source << " to " << target << std::endl;

	return true;
}

#endif

/****************************
 * portable implementations *
 ****************************/

void GetRecursiveDirs(std::vector<std::string> &dirs, const std::string &dir)
{
	constexpr std::string_view chars_to_ignore = "_.";
	if (dir.empty() || !IsDir(dir))
		return;
	dirs.push_back(dir);
	fs::GetRecursiveSubPaths(dir, dirs, false, chars_to_ignore);
}

std::vector<std::string> GetRecursiveDirs(const std::string &dir)
{
	std::vector<std::string> result;
	GetRecursiveDirs(result, dir);
	return result;
}

void GetRecursiveSubPaths(const std::string &path,
		std::vector<std::string> &dst,
		bool list_files,
		std::string_view ignore)
{
	std::vector<DirListNode> content = GetDirListing(path);
	for (const auto &n : content) {
		std::string fullpath = path + DIR_DELIM + n.name;
		if (ignore.find(n.name[0]) != std::string_view::npos)
			continue;
		if (list_files || n.dir)
			dst.push_back(fullpath);
		// Note: this is probably vulnerable to a symlink infinite loop trap
		if (n.dir)
			GetRecursiveSubPaths(fullpath, dst, list_files, ignore);
	}
}

bool CreateAllDirs(const std::string &path)
{
	std::vector<std::string> tocreate;
	std::string basepath = path, removed;
	while (!PathExists(basepath)) {
		tocreate.push_back(basepath);
		basepath = RemoveLastPathComponent(basepath, &removed);
		if (removed.empty())
			break;
	}
	for (auto it = tocreate.rbegin(); it != tocreate.rend(); ++it)
		if(!CreateDir(*it))
			return false;
	return true;
}

bool CopyDir(const std::string &source, const std::string &target)
{
	if(PathExists(source)){
		if(!PathExists(target)){
			fs::CreateAllDirs(target);
		}
		bool retval = true;
		std::vector<DirListNode> content = fs::GetDirListing(source);

		for (const auto &dln : content) {
			std::string sourcechild = source + DIR_DELIM + dln.name;
			std::string targetchild = target + DIR_DELIM + dln.name;
			if(dln.dir){
				if(!fs::CopyDir(sourcechild, targetchild)){
					retval = false;
				}
			}
			else {
				if(!fs::CopyFileContents(sourcechild, targetchild)){
					retval = false;
				}
			}
		}
		return retval;
	}

	return false;
}

bool MoveDir(const std::string &source, const std::string &target)
{
	infostream << "Moving \"" << source << "\" to \"" << target << "\"" << std::endl;

	// If target exists as empty folder delete, otherwise error
	if (fs::PathExists(target)) {
		if (rmdir(target.c_str()) != 0) {
			errorstream << "MoveDir: target \"" << target
				<< "\" exists as file or non-empty folder" << std::endl;
			return false;
		}
	}

	// Try renaming first which is instant
	if (fs::Rename(source, target))
		return true;

	infostream << "MoveDir: rename not possible, will copy instead" << std::endl;
	bool retval = fs::CopyDir(source, target);
	if (retval)
		retval &= fs::RecursiveDelete(source);
	return retval;
}

bool PathStartsWith(const std::string &path, const std::string &prefix)
{
	if (prefix.empty())
		return path.empty();
	size_t pathsize = path.size();
	size_t pathpos = 0;
	size_t prefixsize = prefix.size();
	size_t prefixpos = 0;
	for(;;){
		// Test if current characters at path and prefix are delimiter OR EOS
		bool delim1 = pathpos == pathsize
			|| IsDirDelimiter(path[pathpos]);
		bool delim2 = prefixpos == prefixsize
			|| IsDirDelimiter(prefix[prefixpos]);

		// Return false if it's delimiter/EOS in one path but not in the other
		if(delim1 != delim2)
			return false;

		if(delim1){
			// Skip consequent delimiters in path, in prefix
			while(pathpos < pathsize &&
					IsDirDelimiter(path[pathpos]))
				++pathpos;
			while(prefixpos < prefixsize &&
					IsDirDelimiter(prefix[prefixpos]))
				++prefixpos;
			// Return true if prefix has ended (at delimiter/EOS)
			if(prefixpos == prefixsize)
				return true;
			// Return false if path has ended (at delimiter/EOS)
			// while prefix did not.
			if(pathpos == pathsize)
				return false;
		}
		else{
			// Skip pairwise-equal characters in path and prefix until
			// delimiter/EOS in path or prefix.
			// Return false if differing characters are met.
			size_t len = 0;
			do{
				char pathchar = path[pathpos+len];
				char prefixchar = prefix[prefixpos+len];
				if(FILESYS_CASE_INSENSITIVE){
					pathchar = my_tolower(pathchar);
					prefixchar = my_tolower(prefixchar);
				}
				if(pathchar != prefixchar)
					return false;
				++len;
			} while(pathpos+len < pathsize
					&& !IsDirDelimiter(path[pathpos+len])
					&& prefixpos+len < prefixsize
					&& !IsDirDelimiter(
						prefix[prefixpos+len]));
			pathpos += len;
			prefixpos += len;
		}
	}
}

std::string RemoveLastPathComponent(const std::string &path,
		std::string *removed, int count)
{
	if(removed)
		removed->clear();

	size_t remaining = path.size();

	for (int i = 0; i < count; ++i) {
		// strip a dir delimiter, unless the path is empty
		// because "" and "/" are not the same
		// FIXME: same problem probably exists on win32 with "C:"
		while (remaining > 1 && IsDirDelimiter(path[remaining-1]))
			remaining--;
		// strip a path component
		size_t component_end = remaining;
		while (remaining != 0 && !IsDirDelimiter(path[remaining-1]))
			remaining--;
		size_t component_start = remaining;
		// strip another delimiter
		while (remaining > 1 && IsDirDelimiter(path[remaining-1]))
			remaining--;
		if (component_start == component_end)
			break; // could not remove anything
		if (removed) {
			std::string component = path.substr(component_start,
					component_end - component_start);
			if (i) {
				removed->insert(0, DIR_DELIM);
				removed->insert(0, component);
			} else {
				*removed = std::move(component);
			}
		}
	}
	return path.substr(0, remaining);
}

std::string RemoveRelativePathComponents(std::string path)
{
	size_t pos = path.size();
	size_t dotdot_count = 0;
	while (pos != 0) {
		size_t component_with_delim_end = pos;
		// skip a dir delimiter
		while (pos != 0 && IsDirDelimiter(path[pos-1]))
			pos--;
		// strip a path component
		size_t component_end = pos;
		while (pos != 0 && !IsDirDelimiter(path[pos-1]))
			pos--;
		size_t component_start = pos;

		std::string component = path.substr(component_start,
				component_end - component_start);
		bool remove_this_component = false;
		if (component == ".") {
			remove_this_component = true;
		} else if (component == "..") {
			remove_this_component = true;
			dotdot_count += 1;
		} else if (dotdot_count != 0) {
			remove_this_component = true;
			dotdot_count -= 1;
		}

		if (remove_this_component) {
			while (pos != 0 && IsDirDelimiter(path[pos-1]))
				pos--;
			if (component_start == 0) {
				// We need to remove the delimiter too
				path = path.substr(component_with_delim_end, std::string::npos);
			} else {
				path = path.substr(0, pos) + DIR_DELIM +
					path.substr(component_with_delim_end, std::string::npos);
			}
			if (pos > 0)
				pos++;
		}
	}

	if (dotdot_count > 0)
		return "";

	// remove trailing dir delimiters
	pos = path.size();
	while (pos != 0 && IsDirDelimiter(path[pos-1]))
		pos--;
	return path.substr(0, pos);
}

std::string AbsolutePath(const std::string &path)
{
#ifdef _WIN32
	// handle behavior differences on windows
	if (path.empty())
		return "";
	else if (!PathExists(path))
		return "";
	char *abs_path = _fullpath(NULL, path.c_str(), MAX_PATH);
#else
	char *abs_path = realpath(path.c_str(), NULL);
#endif
	if (!abs_path)
		return "";
	std::string abs_path_str(abs_path);
	free(abs_path);
	return abs_path_str;
}

std::string AbsolutePathPartial(const std::string &path)
{
	if (path.empty())
		return "";
	// Try to determine absolute path
	std::string abs_path = fs::AbsolutePath(path);
	if (!abs_path.empty())
		return abs_path;
	// Remove components until it works
	std::string cur_path = path;
	std::string removed;
	while (abs_path.empty() && !cur_path.empty()) {
		std::string component;
		cur_path = RemoveLastPathComponent(cur_path, &component);
		removed = component + (removed.empty() ? "" : DIR_DELIM + removed);
		abs_path = AbsolutePath(cur_path);
	}
	// If we had a relative path that does not exist, it needs to be joined with cwd
	if (cur_path.empty() && !IsPathAbsolute(path))
		abs_path = AbsolutePath(".");
	// or there's an error
	if (abs_path.empty())
		return "";
	// Put them back together and resolve the remaining relative components
	if (!removed.empty())
		abs_path.append(DIR_DELIM).append(removed);
	return RemoveRelativePathComponents(abs_path);
}

const char *GetFilenameFromPath(const char *path)
{
	const char *filename = strrchr(path, DIR_DELIM_CHAR);
	// Consistent with IsDirDelimiter this function handles '/' too
	if constexpr (DIR_DELIM_CHAR != '/') {
		const char *tmp = strrchr(path, '/');
		if (tmp && tmp > filename)
			filename = tmp;
	}
	return filename ? filename + 1 : path;
}

/// @return short identifier unique across all current processes *and* threads
static std::string get_unique()
{
	static std::atomic<u16> g_counter;
	int pid;
#ifdef _WIN32
	pid = GetCurrentProcessId();
#else
	pid = getpid();
#endif
	return itos(pid) + "-" + itos(g_counter.fetch_add(1));
}

bool safeWriteToFile(const std::string &path, std::string_view content)
{
	// Create it in the same directory
	std::string tmp_file;
	if (auto dir = RemoveLastPathComponent(path); !dir.empty()) {
		tmp_file = dir + DIR_DELIM;
	}
	tmp_file += ".~mt" + get_unique();

	// Write data to a temporary file
	std::string write_error;

	// We've observed behavior suggesting that std::ofstream::flush does not actually
	// flush to disk, so we use the native APIs here for explicit control.
#ifdef _WIN32
	HANDLE handle = CreateFile(tmp_file.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		errorstream << "Failed to open \"" << tmp_file << "\": " << LAST_OS_ERROR() << std::endl;
		return false;
	}
	DWORD bytes_written;
	if (!WriteFile(handle, content.data(), content.size(), &bytes_written, nullptr))
		write_error = LAST_OS_ERROR();
	else if (!FlushFileBuffers(handle))
		write_error = LAST_OS_ERROR();
	else
		assert(bytes_written == content.size());
	CloseHandle(handle);
#else
	int fd = open(tmp_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0664);
	if (fd == -1) {
		errorstream << "Failed to open \"" << tmp_file << "\": " << LAST_OS_ERROR() << std::endl;
		return false;
	}
	size_t written = 0;
	while (written < content.size()) {
		ssize_t r = write(fd, &content[written], content.size() - written);
		if (r <= 0) {
			write_error = LAST_OS_ERROR();
			break;
		}
		written += r;
	}
	// Flushes file data and inode to disk. fdatasync should be faster if available.
#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
	if (fdatasync(fd) != 0)
		write_error = LAST_OS_ERROR();
#else
	if (fsync(fd) != 0)
		write_error = LAST_OS_ERROR();
#endif
	close(fd);
#endif

	if (!write_error.empty()) {
		errorstream << "Failed to write file: " << write_error << std::endl;
		remove(tmp_file.c_str());
		return false;
	}

	std::string rename_error;

	// Move the finished temporary file over the real file
#ifdef _WIN32
	// When creating the file, it can cause Windows Search indexer, virus scanners and other apps
	// to query the file. This can make the move file call below fail.
	// We retry up to 5 times, with a 1ms sleep between, before we consider the whole operation failed
	for (int attempt = 0; attempt < 5; attempt++) {
		auto ok = MoveFileEx(tmp_file.c_str(), path.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
		if (ok) {
			rename_error.clear();
			break;
		}
		rename_error = LAST_OS_ERROR();
		sleep_ms(1);
	}
#else
	// On POSIX compliant systems rename() is specified to be able to swap the
	// file in place of the destination file, making this a truly error-proof
	// transaction.
	if (rename(tmp_file.c_str(), path.c_str()) != 0)
		rename_error = LAST_OS_ERROR();
#endif

	if (!rename_error.empty()) {
		errorstream << "Failed to overwrite \"" << path << "\": " << rename_error << std::endl;
		remove(tmp_file.c_str());
		return false;
	}

	return true;
}

#if CHECK_CLIENT_BUILD()
bool extractZipFile(io::IFileSystem *fs, const char *filename, const std::string &destination)
{
	// Be careful here not to touch the global file hierarchy in Irrlicht
	// since this function needs to be thread-safe!

	io::IArchiveLoader *zip_loader = nullptr;
	for (u32 i = 0; i < fs->getArchiveLoaderCount(); i++) {
		if (fs->getArchiveLoader(i)->isALoadableFileFormat(io::EFAT_ZIP)) {
			zip_loader = fs->getArchiveLoader(i);
			break;
		}
	}
	if (!zip_loader) {
		warningstream << "fs::extractZipFile(): Irrlicht said it doesn't support ZIPs." << std::endl;
		return false;
	}

	irr_ptr<io::IFileArchive> opened_zip(zip_loader->createArchive(filename, false, false));
	if (!opened_zip)
		return false;
	const io::IFileList* files_in_zip = opened_zip->getFileList();

	for (u32 i = 0; i < files_in_zip->getFileCount(); i++) {
		if (files_in_zip->isDirectory(i))
			continue; // ignore, we create dirs as necessary

		const auto &filename = files_in_zip->getFullFileName(i);
		std::string fullpath = destination + DIR_DELIM;
		fullpath += filename.c_str();

		fullpath = fs::RemoveRelativePathComponents(fullpath);
		if (!fs::PathStartsWith(fullpath, destination)) {
			warningstream << "fs::extractZipFile(): refusing to extract file \""
				<< filename.c_str() << "\"" << std::endl;
			continue;
		}

		std::string fullpath_dir = fs::RemoveLastPathComponent(fullpath);

		if (!fs::PathExists(fullpath_dir) && !fs::CreateAllDirs(fullpath_dir))
			return false;

		irr_ptr<io::IReadFile> toread(opened_zip->createAndOpenFile(i));

		auto os = open_ofstream(fullpath.c_str(), true);
		if (!os.good())
			return false;

		char buffer[4096];
		long total_read = 0;

		while (total_read < toread->getSize()) {
			long bytes_read = toread->read(buffer, sizeof(buffer));
			bool error = true;
			if (bytes_read != 0) {
				os.write(buffer, bytes_read);
				error = os.fail();
			}
			if (error) {
				os.close();
				remove(fullpath.c_str());
				return false;
			}
			total_read += bytes_read;
		}
	}

	return true;
}
#endif

bool ReadFile(const std::string &path, std::string &out, bool log_error)
{
	auto is = open_ifstream(path.c_str(), log_error, std::ios::ate);
	if (!is.good())
		return false;

	auto size = is.tellg();
	out.resize(size);
	is.seekg(0);
	is.read(&out[0], size);

	return !is.fail();
}

bool Rename(const std::string &from, const std::string &to)
{
	return rename(from.c_str(), to.c_str()) == 0;
}

bool OpenStream(std::filebuf &stream, const char *filename,
	std::ios::openmode mode, bool log_error, bool log_warn)
{
	assert((mode & std::ios::in) || (mode & std::ios::out));
	assert(!stream.is_open());
	// C++ dropped the ball hard for file opening error handling, there's not even
	// an implementation-defined mechanism for returning any kind of error code or message.
	if (!stream.open(filename, mode)) {
		if (log_warn || log_error) {
			std::string msg = LAST_OS_ERROR();
			(log_error ? errorstream : warningstream)
				<< "Failed to open \"" << filename << "\": " << msg << std::endl;
		}
		return false;
	}
	return true;
}

} // namespace fs
