// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMeshCache.h"
#include "irrArray.h"

namespace scene
{
class CMeshCache : public IMeshCache
{
public:
	virtual ~CMeshCache();

	//! Adds a mesh to the internal list of loaded meshes.
	/** Usually, ISceneManager::getMesh() is called to load a mesh from file.
	That method searches the list of loaded meshes if a mesh has already been loaded and
	returns a pointer to if	it is in that list and already in memory. Otherwise it loads
	the mesh. With IMeshCache::addMesh(), it is possible to pretend that a mesh already
	has been loaded. This method can be used for example by mesh loaders who need to
	load more than one mesh with one call. They can add additional meshes with this
	method to the scene manager. The COLLADA loader for example uses this method.
	\param filename: Filename of the mesh. When called ISceneManager::getMesh() with this
	parameter, the method will return the mesh parameter given with this method.
	\param mesh: Pointer to a mesh which will now be referenced by this name. */
	void addMesh(const io::path &filename, IAnimatedMesh *mesh) override;

	//! Removes a mesh from the cache.
	/** After loading a mesh with getMesh(), the mesh can be removed from the cache
	using this method, freeing a lot of memory. */
	void removeMesh(const IMesh *const mesh) override;

	//! Returns amount of loaded meshes in the cache.
	/** You can load new meshes into the cache using getMesh() and addMesh().
	If you ever need to access the internal mesh cache, you can do this using
	removeMesh(), getMeshNumber(), getMeshByIndex() and getMeshFilename() */
	u32 getMeshCount() const override;

	//! Returns current index number of the mesh, and -1 if it is not in the cache.
	s32 getMeshIndex(const IMesh *const mesh) const override;

	//! Returns a mesh based on its index number.
	/** \param index: Index of the mesh, number between 0 and getMeshCount()-1.
	Note that this number is only valid until a new mesh is loaded or removed *
	\return Returns pointer to the mesh or 0 if there is none with this number. */
	IAnimatedMesh *getMeshByIndex(u32 index) override;

	//! Returns a mesh based on its name.
	/** \param name Name of the mesh. Usually a filename.
	\return Pointer to the mesh or 0 if there is none with this number. */
	IAnimatedMesh *getMeshByName(const io::path &name) override;

	//! Get the name of a loaded mesh, based on its index.
	/** \param index: Index of the mesh, number between 0 and getMeshCount()-1.
	\return The name if mesh was found and has a name, else	the path is empty. */
	const io::SNamedPath &getMeshName(u32 index) const override;

	//! Get the name of a loaded mesh, if there is any.
	/** \param mesh Pointer to mesh to query.
	\return The name if mesh was found and has a name, else	the path is empty. */
	const io::SNamedPath &getMeshName(const IMesh *const mesh) const override;

	//! Renames a loaded mesh.
	/** Note that renaming meshes might change the ordering of the
	meshes, and so the index of the meshes as returned by
	getMeshIndex() or taken by some methods will change.
	\param index The index of the mesh in the cache.
	\param name New name for the mesh.
	\return True if mesh was renamed. */
	bool renameMesh(u32 index, const io::path &name) override;

	//! Renames a loaded mesh.
	/** Note that renaming meshes might change the ordering of the
	meshes, and so the index of the meshes as returned by
	getMeshIndex() or taken by some methods will change.
	\param mesh Mesh to be renamed.
	\param name New name for the mesh.
	\return True if mesh was renamed. */
	bool renameMesh(const IMesh *const mesh, const io::path &name) override;

	//! returns if a mesh already was loaded
	bool isMeshLoaded(const io::path &name) override;

	//! Clears the whole mesh cache, removing all meshes.
	void clear() override;

	//! Clears all meshes that are held in the mesh cache but not used anywhere else.
	void clearUnusedMeshes() override;

protected:
	struct MeshEntry
	{
		MeshEntry(const io::path &name) :
				NamedPath(name)
		{
		}
		io::SNamedPath NamedPath;
		IAnimatedMesh *Mesh;

		bool operator<(const MeshEntry &other) const
		{
			return (NamedPath < other.NamedPath);
		}
	};

	//! loaded meshes
	core::array<MeshEntry> Meshes;
};

} // end namespace scene
