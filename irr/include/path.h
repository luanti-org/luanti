// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine" and the "irrXML" project.
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrString.h"

namespace io
{

//! Type used for all file system related strings.
/** This type will transparently handle different file system encodings.
	NOTE: For historical reasons the tool-functions using io::path are all in coreutil.h
*/
typedef core::string<fschar_t> path;

// Type only exists for historcal reasons, paths are always char now.
static_assert(sizeof(fschar_t) == sizeof(char));

//! Used in places where we identify objects by a filename, but don't actually work with the real filename
/** Irrlicht is internally not case-sensitive when it comes to names.
	Also this class is a first step towards support for correctly serializing renamed objects.
*/
struct SNamedPath
{
	//! Constructor
	SNamedPath() {}

	//! Constructor
	SNamedPath(const path &p) :
			Path(p), InternalName(PathToName(p))
	{
	}

	//! Is smaller comparator
	bool operator<(const SNamedPath &other) const
	{
		return InternalName < other.InternalName;
	}

	//! Set the path.
	void setPath(const path &p)
	{
		Path = p;
		InternalName = PathToName(p);
	}

	//! Get the path.
	const path &getPath() const
	{
		return Path;
	};

	//! Get the name which is used to identify the file.
	//! This string is similar to the names and filenames used before Irrlicht 1.7
	const path &getInternalName() const
	{
		return InternalName;
	}

	//! Implicit cast to io::path
	operator core::stringc() const
	{
		return core::stringc(getPath());
	}

protected:
	// convert the given path string to a name string.
	path PathToName(const path &p) const
	{
		path name(p);
		name.replace('\\', '/');
		name.make_lower();
		return name;
	}

private:
	path Path;
	path InternalName;
};

} // io
