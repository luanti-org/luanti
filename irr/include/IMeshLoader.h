// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IReferenceCounted.h"
#include "path.h"

namespace io
{
class IReadFile;
} // end namespace io
namespace scene
{
class IAnimatedMesh;

//! Class which is able to load an animated mesh from a file.
/** If you want Irrlicht be able to load meshes of
currently unsupported file formats (e.g. .cob), then implement
this and add your new Meshloader with
ISceneManager::addExternalMeshLoader() to the engine. */
class IMeshLoader : public virtual IReferenceCounted
{
public:
	//! Constructor
	IMeshLoader() {}

	//! Destructor
	virtual ~IMeshLoader() {}

	//! Returns true if the file might be loaded by this class.
	/** This decision should be based on the file extension (e.g. ".cob")
	only.
	\param filename Name of the file to test.
	\return True if the file might be loaded by this class. */
	virtual bool isALoadableFileExtension(const io::path &filename) const = 0;

	//! Creates/loads an animated mesh from the file.
	/** \param file File handler to load the file from.
	\return Pointer to the created mesh. Returns 0 if loading failed.
	If you no longer need the mesh, you should call IAnimatedMesh::drop().
	See IReferenceCounted::drop() for more information. */
	virtual IAnimatedMesh *createMesh(io::IReadFile *file) = 0;
};

} // end namespace scene
