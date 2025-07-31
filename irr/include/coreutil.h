// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "path.h"

namespace core
{

/*! \file coreutil.h
	\brief File containing useful basic utility functions
*/

// ----------- some basic quite often used string functions -----------------

//! search if a filename has a proper extension
inline s32 isFileExtension(const io::path &filename, const io::path &ext0,
		const io::path &ext1, const io::path &ext2)
{
	s32 extPos = filename.findLast('.');
	if (extPos < 0)
		return 0;

	extPos += 1;
	if (filename.equals_substring_ignore_case(ext0, extPos))
		return 1;
	if (filename.equals_substring_ignore_case(ext1, extPos))
		return 2;
	if (filename.equals_substring_ignore_case(ext2, extPos))
		return 3;
	return 0;
}

//! search if a filename has a proper extension
inline bool hasFileExtension(const io::path &filename, const io::path &ext0,
		const io::path &ext1 = "", const io::path &ext2 = "")
{
	return isFileExtension(filename, ext0, ext1, ext2) > 0;
}

//! cut the filename extension from a source file path and store it in a dest file path
inline io::path &cutFilenameExtension(io::path &dest, const io::path &source)
{
	s32 endPos = source.findLast('.');
	dest = source.subString(0, endPos < 0 ? source.size() : endPos);
	return dest;
}

//! get the filename extension from a file path
inline io::path &getFileNameExtension(io::path &dest, const io::path &source)
{
	s32 endPos = source.findLast('.');
	if (endPos < 0)
		dest = "";
	else
		dest = source.subString(endPos, source.size());
	return dest;
}

//! delete path from filename
inline io::path deletePathFromFilename(const io::path &filename)
{
	// delete path from filename
	const fschar_t *s = filename.c_str();
	const fschar_t *p = s + filename.size();

	// search for path separator or beginning
	while (*p != '/' && *p != '\\' && p != s)
		p--;

	if (p != s)
		++p;

	return p;
}

//! trim paths
inline io::path &deletePathFromPath(io::path &filename, s32 pathCount)
{
	// delete path from filename
	s32 i = filename.size();

	// search for path separator or beginning
	while (i >= 0) {
		if (filename[i] == '/' || filename[i] == '\\') {
			if (--pathCount <= 0)
				break;
		}
		--i;
	}

	if (i > 0) {
		filename[i + 1] = 0;
		filename.validate();
	} else
		filename = "";
	return filename;
}

//! looks if file is in the same directory of path. returns offset of directory.
//! 0 means in same directory. 1 means file is direct child of path
inline s32 isInSameDirectory(const io::path &path, const io::path &file)
{
	if (path.size() && !path.equalsn(file, path.size()))
		return -1;

	s32 subA = 0;
	s32 subB = 0;
	s32 pos = 0;
	while ((pos = path.findNext('/', pos)) >= 0) {
		subA += 1;
		pos += 1;
	}

	pos = 0;
	while ((pos = file.findNext('/', pos)) >= 0) {
		subB += 1;
		pos += 1;
	}

	return subB - subA;
}

//! splits a path into components
static inline void splitFilename(const io::path &name, io::path *path = 0,
		io::path *filename = 0, io::path *extension = 0, bool make_lower = false)
{
	s32 i = name.size();
	s32 extpos = i;

	// search for path separator or beginning
	while (i >= 0) {
		if (name[i] == '.') {
			extpos = i;
			if (extension)
				*extension = name.subString(extpos + 1, name.size() - (extpos + 1), make_lower);
		} else if (name[i] == '/' || name[i] == '\\') {
			if (filename)
				*filename = name.subString(i + 1, extpos - (i + 1), make_lower);
			if (path) {
				*path = name.subString(0, i + 1, make_lower);
				path->replace('\\', '/');
			}
			return;
		}
		i -= 1;
	}
	if (filename)
		*filename = name.subString(0, extpos, make_lower);
}

//! create a filename from components
static inline io::path mergeFilename(const io::path &path, const io::path &filename, const io::path &extension = "")
{
	io::path result(path);

	if (!result.empty()) {
		fschar_t last = result.lastChar();
		if (last != _IRR_TEXT('/') && last != _IRR_TEXT('\\'))
			result += _IRR_TEXT('/');
	}
	if (!filename.empty())
		result += filename;
	if (!extension.empty()) {
		if (!result.empty() && extension[0] != _IRR_TEXT('.'))
			result += _IRR_TEXT('.');
		result += extension;
	}

	return result;
}

//! some standard function ( to remove dependencies )
inline bool isdigit(s32 c)
{
	return c >= '0' && c <= '9';
}
inline bool isspace(s32 c)
{
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}
inline bool isupper(s32 c)
{
	return c >= 'A' && c <= 'Z';
}

} // end namespace core
