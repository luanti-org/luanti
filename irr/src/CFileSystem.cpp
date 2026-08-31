// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CFileSystem.h"
#include "CFileList.h"
#include "coreutil.h"

#if defined(_IRR_WINDOWS_API_)
#include <direct.h> // for _chdir, _getcwd
#include <io.h>     // for _access, _tfindfirst
#include <stdlib.h> // for _fullpath, _MAX_PATH
#include <tchar.h>  // for _tfinddata_t
#elif (defined(_IRR_POSIX_API_) || defined(_IRR_OSX_PLATFORM_) || defined(_IRR_ANDROID_PLATFORM_))
#include <cstdlib>  // for realpath
#include <cstring>  // for strcmp
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h> // for access, chdir, getcwd
#endif

namespace io
{

io::path CFileSystem::getCurrentWorkingDirectory()
{
	io::path directory;

#if defined(_IRR_WINDOWS_API_)
	fschar_t tmp[_MAX_PATH];
	_getcwd(tmp, _MAX_PATH);
	directory = tmp;
	directory.replace('\\', '/');
#elif (defined(_IRR_POSIX_API_) || defined(_IRR_OSX_PLATFORM_))
	// getting the CWD is rather complex as we do not know the size
	// so try it until the call was successful
	// Note that neither the first nor the second parameter may be 0 according to POSIX
	u32 pathSize = 256;
	char *tmpPath = new char[pathSize];
	while ((pathSize < (1 << 16)) && !(getcwd(tmpPath, pathSize))) {
		delete[] tmpPath;
		pathSize *= 2;
		tmpPath = new char[pathSize];
	}
	if (tmpPath) {
		directory = tmpPath;
		delete[] tmpPath;
	}
#endif

	directory.validate();
	return directory;
}

bool CFileSystem::changeWorkingDirectoryTo(const io::path &newDirectory)
{
#if defined(_MSC_VER)
	return (_chdir(newDirectory.c_str()) == 0);
#else
	return (chdir(newDirectory.c_str()) == 0);
#endif
}

io::path CFileSystem::getAbsolutePath(const io::path &filename)
{
	if (filename.empty())
		return filename;
#if defined(_IRR_WINDOWS_API_)
	fschar_t *p = 0;
	fschar_t fpath[_MAX_PATH];
	p = _fullpath(fpath, filename.c_str(), _MAX_PATH);
	core::stringc tmp(p);
	tmp.replace('\\', '/');
	return tmp;
#elif (defined(_IRR_POSIX_API_) || defined(_IRR_OSX_PLATFORM_))
	c8 *p = 0;
	c8 fpath[4096];
	fpath[0] = 0;
	p = realpath(filename.c_str(), fpath);
	if (!p) {
		// content in fpath is unclear at this point
		if (!fpath[0]) { // seems like fpath wasn't altered, use our best guess
			io::path tmp(filename);
			return flattenFilename(tmp);
		} else
			return io::path(fpath);
	}
	if (filename[filename.size() - 1] == '/')
		return io::path(p) + _IRR_TEXT("/");
	else
		return io::path(p);
#else
	return io::path(filename);
#endif
}

io::path &CFileSystem::flattenFilename(io::path &directory, const io::path &root)
{
	directory.replace('\\', '/');
	if (directory.lastChar() != '/')
		directory.append('/');

	io::path dir;
	io::path subdir;

	s32 lastpos = 0;
	s32 pos = 0;
	bool lastWasRealDir = false;

	while ((pos = directory.findNext('/', lastpos)) >= 0) {
		subdir = directory.subString(lastpos, pos - lastpos + 1);

		if (subdir == _IRR_TEXT("../")) {
			if (lastWasRealDir) {
				core::deletePathFromPath(dir, 2);
				lastWasRealDir = (dir.size() != 0);
			} else {
				dir.append(subdir);
				lastWasRealDir = false;
			}
		} else if (subdir == _IRR_TEXT("/")) {
			dir = root;
		} else if (subdir != _IRR_TEXT("./")) {
			dir.append(subdir);
			lastWasRealDir = true;
		}

		lastpos = pos + 1;
	}
	directory = dir;
	return directory;
}

IFileList *CFileSystem::createFileList(const io::path &path)
{
	io::CFileList *r = 0;
	io::path Path = path;
	Path.replace('\\', '/');
	if (!Path.empty() && Path.lastChar() != '/')
		Path.append('/');

#if defined(_IRR_WINDOWS_API_)

	r = new io::CFileList(Path, true, false);

	// intptr_t is optional but supported by MinGW since 2007 or earlier.
	intptr_t hFile;
	struct _tfinddata_t c_file;
	if ((hFile = _tfindfirst(_T("*"), &c_file)) != (intptr_t)(-1L)) {
		do {
			r->addItem(Path + c_file.name, 0, c_file.size, (_A_SUBDIR & c_file.attrib) != 0, 0);
		} while (_tfindnext(hFile, &c_file) == 0);

		_findclose(hFile);
	}

#endif

#if (defined(_IRR_POSIX_API_) || defined(_IRR_OSX_PLATFORM_))

	r = new io::CFileList(Path, false, false);

	r->addItem(Path + _IRR_TEXT(".."), 0, 0, true, 0);

	//! We use the POSIX compliant methods instead of scandir
	DIR *dirHandle = opendir(Path.c_str());
	if (dirHandle) {
		struct dirent *dirEntry;
		while ((dirEntry = readdir(dirHandle))) {
			u32 size = 0;
			bool isDirectory = false;

			if ((strcmp(dirEntry->d_name, ".") == 0) ||
					(strcmp(dirEntry->d_name, "..") == 0)) {
				continue;
			}
			struct stat buf;
			if (stat(dirEntry->d_name, &buf) == 0) {
				size = buf.st_size;
				isDirectory = S_ISDIR(buf.st_mode);
			}
#if !defined(_IRR_SOLARIS_PLATFORM_) && !defined(__CYGWIN__) && !defined(__HAIKU__)
			// only available on some systems
			else {
				isDirectory = dirEntry->d_type == DT_DIR;
			}
#endif

			r->addItem(Path + dirEntry->d_name, 0, size, isDirectory, 0);
		}
		closedir(dirHandle);
	}
#endif

	if (r)
		r->sort();
	return r;
}

bool CFileSystem::existFile(const io::path &filename)
{
#if defined(_MSC_VER)
	return (_access(filename.c_str(), 0) != -1);
#elif defined(F_OK)
	return (access(filename.c_str(), F_OK) != -1);
#else
	return (access(filename.c_str(), 0) != -1);
#endif
}

} // end namespace io
