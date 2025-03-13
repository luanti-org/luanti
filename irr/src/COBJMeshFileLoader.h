// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <map>
#include "IMeshLoader.h"
#include "ISceneManager.h"
#include "irrString.h"
#include "SMeshBuffer.h"

namespace irr
{
namespace scene
{

//! Meshloader capable of loading obj meshes.
class COBJMeshFileLoader : public IMeshLoader
{
public:
	//! Constructor
	COBJMeshFileLoader(scene::ISceneManager *smgr);

	//! destructor
	virtual ~COBJMeshFileLoader();

	//! returns true if the file maybe is able to be loaded by this class
	//! based on the file extension (e.g. ".obj")
	bool isALoadableFileExtension(const io::path &filename) const override;

	//! creates/loads an animated mesh from the file.
	//! \return Pointer to the created mesh. Returns 0 if loading failed.
	//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
	//! See IReferenceCounted::drop() for more information.
	IAnimatedMesh *createMesh(io::IReadFile *file) override;

private:
	struct SObjMtl
	{
		SObjMtl() :
				Meshbuffer(0), Bumpiness(1.0f), Illumination(0),
				RecalculateNormals(false)
		{
			Meshbuffer = new SMeshBuffer();
		}

		SObjMtl(const SObjMtl &o) :
				Name(o.Name), Group(o.Group),
				Bumpiness(o.Bumpiness), Illumination(o.Illumination),
				RecalculateNormals(false)
		{
			Meshbuffer = new SMeshBuffer();
			Meshbuffer->Material = o.Meshbuffer->Material;
		}

		std::map<video::S3DVertex, int> VertMap;
		scene::SMeshBuffer *Meshbuffer;
		core::stringc Name;
		core::stringc Group;
		f32 Bumpiness;
		c8 Illumination;
		bool RecalculateNormals;
	};

	// returns a pointer to the first printable character available in the buffer
	const c8 *goFirstWord(const c8 *buf, const c8 *const bufEnd, bool acrossNewlines = true);
	// returns a pointer to the first printable character after the first non-printable
	const c8 *goNextWord(const c8 *buf, const c8 *const bufEnd, bool acrossNewlines = true);
	// returns a pointer to the next printable character after the first line break
	const c8 *goNextLine(const c8 *buf, const c8 *const bufEnd);
	// copies the current word from the inBuf to the outBuf
	u32 copyWord(c8 *outBuf, const c8 *inBuf, u32 outBufLength, const c8 *const pBufEnd);
	// copies the current line from the inBuf to the outBuf
	core::stringc copyLine(const c8 *inBuf, const c8 *const bufEnd);

	// combination of goNextWord followed by copyWord
	const c8 *goAndCopyNextWord(c8 *outBuf, const c8 *inBuf, u32 outBufLength, const c8 *const pBufEnd);

	//! Find and return the material with the given name
	SObjMtl *findMtl(const core::stringc &mtlName, const core::stringc &grpName);

	//! Read RGB color
	const c8 *readColor(const c8 *bufPtr, video::SColor &color, const c8 *const pBufEnd);
	//! Read 3d vector of floats
	const c8 *readVec3(const c8 *bufPtr, core::vector3df &vec, const c8 *const pBufEnd);
	//! Read 2d vector of floats
	const c8 *readUV(const c8 *bufPtr, core::vector2df &vec, const c8 *const pBufEnd);
	//! Read boolean value represented as 'on' or 'off'
	const c8 *readBool(const c8 *bufPtr, bool &tf, const c8 *const bufEnd);

	// reads and convert to integer the vertex indices in a line of obj file's face statement
	// -1 for the index if it doesn't exist
	// indices are changed to 0-based index instead of 1-based from the obj file
	bool retrieveVertexIndices(c8 *vertexData, s32 *idx, const c8 *bufEnd, u32 vbsize, u32 vtsize, u32 vnsize);

	void cleanUp();

	scene::ISceneManager *SceneManager;

	core::array<SObjMtl *> Materials;
};

} // end namespace scene
} // end namespace irr
