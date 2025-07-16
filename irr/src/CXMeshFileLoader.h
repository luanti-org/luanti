// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "IMeshLoader.h"
#include "irrString.h"
#include "SkinnedMesh.h"

namespace io
{
class IReadFile;
} // end namespace io
namespace scene
{

//! Meshloader capable of loading x meshes.
class CXMeshFileLoader : public IMeshLoader
{
public:
	//! Constructor
	CXMeshFileLoader(scene::ISceneManager *smgr);

	//! returns true if the file maybe is able to be loaded by this class
	//! based on the file extension (e.g. ".cob")
	bool isALoadableFileExtension(const io::path &filename) const override;

	//! creates/loads an animated mesh from the file.
	//! \return Pointer to the created mesh. Returns 0 if loading failed.
	//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
	//! See IReferenceCounted::drop() for more information.
	IAnimatedMesh *createMesh(io::IReadFile *file) override;

	struct SXMesh
	{
		SXMesh() :
				MaxSkinWeightsPerVertex(0), MaxSkinWeightsPerFace(0), BoneCount(0), AttachedJointID(-1), HasSkinning(false), HasVertexColors(false) {}
		// this mesh contains triangulated texture data.
		// because in an .x file, faces can be made of more than 3
		// vertices, the indices data structure is triangulated during the
		// loading process. The IndexCountPerFace array is filled during
		// this triangulation process and stores how much indices belong to
		// every face. This data structure can be ignored, because all data
		// in this structure is triangulated.

		core::stringc Name;

		u32 MaxSkinWeightsPerVertex;
		u32 MaxSkinWeightsPerFace;
		u32 BoneCount;

		core::array<u16> IndexCountPerFace; // default 3, but could be more

		core::array<scene::SSkinMeshBuffer *> Buffers;

		core::array<video::S3DVertex> Vertices;
		core::array<core::vector2df> TCoords2;

		core::array<u32> Indices;

		core::array<u32> FaceMaterialIndices; // index of material for each face

		core::array<video::SMaterial> Materials; // material array

		core::array<u32> WeightJoint;
		core::array<u32> WeightNum;

		s32 AttachedJointID;

		bool HasSkinning;
		bool HasVertexColors;
	};

private:
	bool load(io::IReadFile *file);

	bool readFileIntoMemory(io::IReadFile *file);

	bool parseFile();

	bool parseDataObject();

	bool parseDataObjectTemplate();

	bool parseDataObjectFrame(SkinnedMesh::SJoint *parent);

	bool parseDataObjectTransformationMatrix(core::matrix4 &mat);

	bool parseDataObjectMesh(SXMesh &mesh);

	bool parseDataObjectSkinWeights(SXMesh &mesh);

	bool parseDataObjectSkinMeshHeader(SXMesh &mesh);

	bool parseDataObjectMeshNormals(SXMesh &mesh);

	bool parseDataObjectMeshTextureCoords(SXMesh &mesh);

	bool parseDataObjectMeshVertexColors(SXMesh &mesh);

	bool parseDataObjectMeshMaterialList(SXMesh &mesh);

	bool parseDataObjectAnimationSet();

	bool parseDataObjectAnimationTicksPerSecond();

	bool parseDataObjectAnimation();

	bool parseDataObjectAnimationKey(SkinnedMesh::SJoint *joint);

	bool parseDataObjectTextureFilename(core::stringc &texturename);

	bool parseUnknownDataObject();

	//! places pointer to next begin of a token, and ignores comments
	void findNextNoneWhiteSpace();

	//! places pointer to next begin of a token, which must be a number,
	// and ignores comments
	void findNextNoneWhiteSpaceNumber();

	//! returns next parseable token. Returns empty string if no token there
	core::stringc getNextToken();

	//! reads header of dataobject including the opening brace.
	//! returns false if error happened, and writes name of object
	//! if there is one
	bool readHeadOfDataObject(core::stringc *outname = 0);

	//! checks for closing curly brace, returns false if not there
	bool checkForClosingBrace();

	//! checks for one following semicolons, returns false if not there
	bool checkForOneFollowingSemicolons();

	//! checks for two following semicolons, returns false if they are not there
	bool checkForTwoFollowingSemicolons();

	//! reads a x file style string
	bool getNextTokenAsString(core::stringc &out);

	void readUntilEndOfLine();

	u16 readBinWord();
	u32 readBinDWord();
	u32 readInt();
	f32 readFloat();
	bool readVector2(core::vector2df &vec);
	bool readVector3(core::vector3df &vec);
	bool readMatrix(core::matrix4 &mat);
	bool readRGB(video::SColor &color);
	bool readRGBA(video::SColor &color);

	SkinnedMeshBuilder *AnimatedMesh;

	c8 *Buffer;
	const c8 *P;
	c8 *End;
	// counter for number arrays in binary format
	u32 BinaryNumCount;
	u32 Line;
	io::path FilePath;

	bool ErrorState;

	SkinnedMesh::SJoint *CurFrame;

	core::array<SXMesh *> Meshes;

	u32 MajorVersion;
	u32 MinorVersion;
	bool BinaryFormat;
	c8 FloatSize;
};

} // end namespace scene
