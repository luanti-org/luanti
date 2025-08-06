// Copyright (C) 2006-2012 Luke Hoschke
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

// B3D Mesh loader
// File format designed by Mark Sibly for the Blitz3D engine and has been
// declared public domain

#pragma once

#include "IMeshLoader.h"
#include "ISceneManager.h"
#include "SkinnedMesh.h"
#include "SB3DStructs.h"
#include "IReadFile.h"

namespace scene
{

//! Meshloader for B3D format
class CB3DMeshFileLoader : public IMeshLoader
{
public:
	//! Constructor
	CB3DMeshFileLoader(scene::ISceneManager *smgr);

	//! returns true if the file maybe is able to be loaded by this class
	//! based on the file extension (e.g. ".bsp")
	bool isALoadableFileExtension(const io::path &filename) const override;

	//! creates/loads an animated mesh from the file.
	//! \return Pointer to the created mesh. Returns 0 if loading failed.
	//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
	//! See IReferenceCounted::drop() for more information.
	IAnimatedMesh *createMesh(io::IReadFile *file) override;

private:
	bool load();
	bool readChunkNODE(SkinnedMesh::SJoint *InJoint);
	bool readChunkMESH(SkinnedMesh::SJoint *InJoint);
	bool readChunkVRTS(SkinnedMesh::SJoint *InJoint);
	bool readChunkTRIS(scene::SSkinMeshBuffer *MeshBuffer, u32 MeshBufferID, s32 Vertices_Start);
	bool readChunkBONE(SkinnedMesh::SJoint *InJoint);
	bool readChunkKEYS(SkinnedMesh::SJoint *InJoint);
	bool readChunkANIM();
	bool readChunkTEXS();
	bool readChunkBRUS();

	std::string readString();
	void readFloats(f32 *vec, u32 count);

	core::array<SB3dChunk> B3dStack;

	core::array<SB3dMaterial> Materials;
	core::array<SB3dTexture> Textures;

	core::array<s32> AnimatedVertices_VertexID;

	core::array<s32> AnimatedVertices_BufferID;

	core::array<video::S3DVertex2TCoords> BaseVertices;

	SkinnedMeshBuilder *AnimatedMesh;
	io::IReadFile *B3DFile;

	// B3Ds have Vertex ID's local within the mesh I don't want this
	//  Variable needs to be class member due to recursion in calls
	u32 VerticesStart;

	bool NormalsInFile;
	bool HasVertexColors;
	bool ShowWarning;
};

} // end namespace scene
