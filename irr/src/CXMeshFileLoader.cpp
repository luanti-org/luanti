// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CXMeshFileLoader.h"
#include "SkinnedMesh.h"
#include "Transform.h"
#include "os.h"

#include "fast_atof.h"
#include "coreutil.h"
#include "ISceneManager.h"
#include "IVideoDriver.h"
#include "IReadFile.h"

#ifdef _DEBUG
#define _XREADER_DEBUG
#endif
// #define BETTER_MESHBUFFER_SPLITTING_FOR_X

#define SET_ERR_AND_RETURN() \
	do {                     \
		ErrorState = true;   \
		return false;        \
	} while (0)

namespace scene
{

//! Constructor
CXMeshFileLoader::CXMeshFileLoader(scene::ISceneManager *smgr) :
		AnimatedMesh(0), Buffer(0), P(0), End(0), BinaryNumCount(0), Line(0), ErrorState(false),
		CurFrame(0), MajorVersion(0), MinorVersion(0), BinaryFormat(false), FloatSize(0)
{}

//! returns true if the file maybe is able to be loaded by this class
//! based on the file extension (e.g. ".bsp")
bool CXMeshFileLoader::isALoadableFileExtension(const io::path &filename) const
{
	return core::hasFileExtension(filename, "x");
}

//! creates/loads an animated mesh from the file.
//! \return Pointer to the created mesh. Returns 0 if loading failed.
//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
//! See IReferenceCounted::drop() for more information.
IAnimatedMesh *CXMeshFileLoader::createMesh(io::IReadFile *file)
{
	if (!file)
		return 0;

#ifdef _XREADER_DEBUG
	u32 time = os::Timer::getRealTime();
#endif

	AnimatedMesh = new SkinnedMeshBuilder(SkinnedMesh::SourceFormat::X);

	SkinnedMesh *res = nullptr;
	if (load(file)) {
		res = AnimatedMesh->finalize();
	} else {
		AnimatedMesh->drop();
		AnimatedMesh = 0;
	}
#ifdef _XREADER_DEBUG
	time = os::Timer::getRealTime() - time;
	core::stringc tmpString = "Time to load ";
	tmpString += BinaryFormat ? "binary" : "ascii";
	tmpString += " X file: ";
	tmpString += time;
	tmpString += "ms";
	os::Printer::log(tmpString.c_str());
#endif
	// Clear up

	MajorVersion = 0;
	MinorVersion = 0;
	BinaryFormat = 0;
	BinaryNumCount = 0;
	FloatSize = 0;
	P = 0;
	End = 0;
	CurFrame = 0;

	delete[] Buffer;
	Buffer = 0;

	for (u32 i = 0; i < Meshes.size(); ++i)
		delete Meshes[i];
	Meshes.clear();

	return res;
}

bool CXMeshFileLoader::load(io::IReadFile *file)
{
	if (!readFileIntoMemory(file))
		return false;

	if (!parseFile())
		return false;

	for (u32 n = 0; n < Meshes.size(); ++n) {
		SXMesh *mesh = Meshes[n];

		// default material if nothing loaded
		if (!mesh->Materials.size()) {
			mesh->Materials.push_back(video::SMaterial());
		}

		u32 i;

		mesh->Buffers.reallocate(mesh->Materials.size());
#ifndef BETTER_MESHBUFFER_SPLITTING_FOR_X
		const u32 bufferOffset = AnimatedMesh->getMeshBufferCount();
#endif
		for (i = 0; i < mesh->Materials.size(); ++i) {
			mesh->Buffers.push_back(AnimatedMesh->addMeshBuffer());
			mesh->Buffers.getLast()->Material = mesh->Materials[i];

			if (!mesh->HasSkinning) {
				// Set up rigid animation
				if (mesh->AttachedJointID != -1) {
					AnimatedMesh->getAllJoints()[mesh->AttachedJointID]->AttachedMeshes.push_back(AnimatedMesh->getMeshBufferCount() - 1);
				}
			}
		}

		if (!mesh->FaceMaterialIndices.size()) {
			mesh->FaceMaterialIndices.set_used(mesh->Indices.size() / 3);
			for (i = 0; i < mesh->FaceMaterialIndices.size(); ++i)
				mesh->FaceMaterialIndices[i] = 0;
		}

		if (!mesh->HasVertexColors) {
			for (u32 j = 0; j < mesh->FaceMaterialIndices.size(); ++j) {
				for (u32 id = j * 3 + 0; id <= j * 3 + 2; ++id) {
					mesh->Vertices[mesh->Indices[id]].Color = 0xff777777;
				}
			}
		}

#ifdef BETTER_MESHBUFFER_SPLITTING_FOR_X
		{
			// the same vertex can be used in many different meshbuffers, but it's slow to work out

			core::array<core::array<u32>> verticesLinkIndex;
			verticesLinkIndex.reallocate(mesh->Vertices.size());
			core::array<core::array<u16>> verticesLinkBuffer;
			verticesLinkBuffer.reallocate(mesh->Vertices.size());

			for (i = 0; i < mesh->Vertices.size(); ++i) {
				verticesLinkIndex.push_back(core::array<u32>());
				verticesLinkBuffer.push_back(core::array<u16>());
			}

			for (i = 0; i < mesh->FaceMaterialIndices.size(); ++i) {
				for (u32 id = i * 3 + 0; id <= i * 3 + 2; ++id) {
					core::array<u16> &Array = verticesLinkBuffer[mesh->Indices[id]];
					bool found = false;

					for (u32 j = 0; j < Array.size(); ++j) {
						if (Array[j] == mesh->FaceMaterialIndices[i]) {
							found = true;
							break;
						}
					}

					if (!found)
						Array.push_back(mesh->FaceMaterialIndices[i]);
				}
			}

			for (i = 0; i < verticesLinkBuffer.size(); ++i) {
				if (!verticesLinkBuffer[i].size())
					verticesLinkBuffer[i].push_back(0);
			}

			for (i = 0; i < mesh->Vertices.size(); ++i) {
				core::array<u16> &Array = verticesLinkBuffer[i];
				verticesLinkIndex[i].reallocate(Array.size());
				for (u32 j = 0; j < Array.size(); ++j) {
					scene::SSkinMeshBuffer *buffer = mesh->Buffers[Array[j]];
					verticesLinkIndex[i].push_back(buffer->Vertices_Standard.size());
					buffer->Vertices_Standard.push_back(mesh->Vertices[i]);
				}
			}

			for (i = 0; i < mesh->FaceMaterialIndices.size(); ++i) {
				scene::SSkinMeshBuffer *buffer = mesh->Buffers[mesh->FaceMaterialIndices[i]];

				for (u32 id = i * 3 + 0; id <= i * 3 + 2; ++id) {
					core::array<u16> &Array = verticesLinkBuffer[mesh->Indices[id]];

					for (u32 j = 0; j < Array.size(); ++j) {
						if (Array[j] == mesh->FaceMaterialIndices[i])
							buffer->Indices.push_back(verticesLinkIndex[mesh->Indices[id]][j]);
					}
				}
			}

			for (u32 j = 0; j < mesh->WeightJoint.size(); ++j) {
				SkinnedMesh::SJoint *joint = AnimatedMesh->getAllJoints()[mesh->WeightJoint[j]];
				SkinnedMesh::SWeight &weight = joint->Weights[mesh->WeightNum[j]];

				u32 id = weight.vertex_id;

				if (id >= verticesLinkIndex.size()) {
					os::Printer::log("X loader: Weight id out of range", ELL_WARNING);
					id = 0;
					weight.strength = 0.f;
				}

				if (verticesLinkBuffer[id].size() == 1) {
					weight.vertex_id = verticesLinkIndex[id][0];
					weight.buffer_id = verticesLinkBuffer[id][0];
				} else if (verticesLinkBuffer[id].size() != 0) {
					for (u32 k = 1; k < verticesLinkBuffer[id].size(); ++k) {
						SkinnedMesh::SWeight *WeightClone = AnimatedMesh->addWeight(joint);
						WeightClone->strength = weight.strength;
						WeightClone->vertex_id = verticesLinkIndex[id][k];
						WeightClone->buffer_id = verticesLinkBuffer[id][k];
					}
				}
			}
		}
#else
		{
			core::array<u32> verticesLinkIndex;
			core::array<s16> verticesLinkBuffer;
			verticesLinkBuffer.set_used(mesh->Vertices.size());

			// init with 0
			for (i = 0; i < mesh->Vertices.size(); ++i) {
				// watch out for vertices which are not part of the mesh
				// they will keep the -1 and can lead to out-of-bounds access
				verticesLinkBuffer[i] = -1;
			}

			bool warned = false;
			// store meshbuffer number per vertex
			for (i = 0; i < mesh->FaceMaterialIndices.size(); ++i) {
				for (u32 id = i * 3 + 0; id <= i * 3 + 2; ++id) {
					if ((verticesLinkBuffer[mesh->Indices[id]] != -1) && (verticesLinkBuffer[mesh->Indices[id]] != (s16)mesh->FaceMaterialIndices[i])) {
						if (!warned) {
							os::Printer::log("X loader", "Duplicated vertex, animation might be corrupted.", ELL_WARNING);
							warned = true;
						}
						const u32 tmp = mesh->Vertices.size();
						mesh->Vertices.push_back(mesh->Vertices[mesh->Indices[id]]);
						mesh->Indices[id] = tmp;
						verticesLinkBuffer.set_used(mesh->Vertices.size());
					}
					verticesLinkBuffer[mesh->Indices[id]] = mesh->FaceMaterialIndices[i];
				}
			}

			if (mesh->FaceMaterialIndices.size() != 0) {
				// store vertices in buffers and remember relation in verticesLinkIndex
				u32 *vCountArray = new u32[mesh->Buffers.size()];
				memset(vCountArray, 0, mesh->Buffers.size() * sizeof(u32));
				// count vertices in each buffer and reallocate
				for (i = 0; i < mesh->Vertices.size(); ++i) {
					if (verticesLinkBuffer[i] != -1)
						++vCountArray[verticesLinkBuffer[i]];
				}
				if (mesh->TCoords2.size()) {
					for (i = 0; i != mesh->Buffers.size(); ++i) {
						mesh->Buffers[i]->Vertices_2TCoords->Data.reserve(vCountArray[i]);
						mesh->Buffers[i]->VertexType = video::EVT_2TCOORDS;
					}
				} else {
					for (i = 0; i != mesh->Buffers.size(); ++i)
						mesh->Buffers[i]->Vertices_Standard->Data.reserve(vCountArray[i]);
				}

				verticesLinkIndex.set_used(mesh->Vertices.size());
				// actually store vertices
				for (i = 0; i < mesh->Vertices.size(); ++i) {
					// if a vertex is missing for some reason, just skip it
					if (verticesLinkBuffer[i] == -1)
						continue;
					scene::SSkinMeshBuffer *buffer = mesh->Buffers[verticesLinkBuffer[i]];

					if (mesh->TCoords2.size()) {
						verticesLinkIndex[i] = buffer->Vertices_2TCoords->getCount();
						buffer->Vertices_2TCoords->Data.emplace_back(mesh->Vertices[i]);
						// We have a problem with correct tcoord2 handling here
						// crash fixed for now by checking the values
						buffer->Vertices_2TCoords->Data.back().TCoords2 = (i < mesh->TCoords2.size()) ? mesh->TCoords2[i] : mesh->Vertices[i].TCoords;
					} else {
						verticesLinkIndex[i] = buffer->Vertices_Standard->getCount();
						buffer->Vertices_Standard->Data.push_back(mesh->Vertices[i]);
					}
				}

				// count indices per buffer and reallocate
				memset(vCountArray, 0, mesh->Buffers.size() * sizeof(u32));
				for (i = 0; i < mesh->FaceMaterialIndices.size(); ++i)
					++vCountArray[mesh->FaceMaterialIndices[i]];
				for (i = 0; i != mesh->Buffers.size(); ++i)
					mesh->Buffers[i]->Indices->Data.reserve(vCountArray[i]);
				delete[] vCountArray;
				// create indices per buffer
				for (i = 0; i < mesh->FaceMaterialIndices.size(); ++i) {
					scene::SSkinMeshBuffer *buffer = mesh->Buffers[mesh->FaceMaterialIndices[i]];
					for (u32 id = i * 3 + 0; id != i * 3 + 3; ++id) {
						buffer->Indices->Data.push_back(verticesLinkIndex[mesh->Indices[id]]);
					}
				}
			}

			for (u32 j = 0; j < mesh->WeightJoint.size(); ++j) {
				SkinnedMesh::SWeight &weight = (AnimatedMesh->getAllJoints()[mesh->WeightJoint[j]]->Weights[mesh->WeightNum[j]]);

				u32 id = weight.vertex_id;

				if (id >= verticesLinkIndex.size()) {
					os::Printer::log("X loader: Weight id out of range", ELL_WARNING);
					id = 0;
					weight.strength = 0.f;
				}

				weight.vertex_id = verticesLinkIndex[id];
				weight.buffer_id = verticesLinkBuffer[id] + bufferOffset;
			}
		}
#endif
	}

	return true;
}

//! Reads file into memory
bool CXMeshFileLoader::readFileIntoMemory(io::IReadFile *file)
{
	const long size = file->getSize();
	if (size < 12) {
		os::Printer::log("X File is too small.", ELL_WARNING);
		return false;
	}

	Buffer = new c8[size + 1];
	Buffer[size] = 0x0; // null-terminate

	//! read all into memory
	if (file->read(Buffer, size) != static_cast<size_t>(size)) {
		os::Printer::log("Could not read from x file.", ELL_WARNING);
		return false;
	}

	Line = 1;
	End = Buffer + size;

	//! check header "xof "
	if (strncmp(Buffer, "xof ", 4) != 0) {
		os::Printer::log("Not an x file, wrong header.", ELL_WARNING);
		return false;
	}

	//! read minor and major version, e.g. 0302 or 0303
	c8 tmp[3];
	tmp[0] = Buffer[4];
	tmp[1] = Buffer[5];
	tmp[2] = 0x0;
	MajorVersion = core::strtoul10(tmp);

	tmp[0] = Buffer[6];
	tmp[1] = Buffer[7];
	MinorVersion = core::strtoul10(tmp);

	//! read format
	if (strncmp(&Buffer[8], "txt ", 4) == 0)
		BinaryFormat = false;
	else if (strncmp(&Buffer[8], "bin ", 4) == 0)
		BinaryFormat = true;
	else {
		os::Printer::log("Only uncompressed x files currently supported.", ELL_WARNING);
		return false;
	}
	BinaryNumCount = 0;

	//! read float size
	if (strncmp(&Buffer[12], "0032", 4) == 0)
		FloatSize = 4;
	else if (strncmp(&Buffer[12], "0064", 4) == 0)
		FloatSize = 8;
	else {
		os::Printer::log("Float size not supported.", ELL_WARNING);
		return false;
	}

	P = &Buffer[16];

	readUntilEndOfLine();

	return true;
}

//! Parses the file
bool CXMeshFileLoader::parseFile()
{
	while (parseDataObject()) {
		// loop
	}

	return !ErrorState;
}

//! Parses the next Data object in the file
bool CXMeshFileLoader::parseDataObject()
{
	core::stringc objectName = getNextToken();

	if (objectName.size() == 0)
		return false;

		// parse specific object
#ifdef _XREADER_DEBUG
	os::Printer::log("debug DataObject", objectName.c_str(), ELL_DEBUG);
#endif

	if (objectName == "template")
		return parseDataObjectTemplate();
	else if (objectName == "Frame") {
		return parseDataObjectFrame(0);
	} else if (objectName == "Mesh") {
		// some meshes have no frames at all
		// CurFrame = AnimatedMesh->addJoint(0);

		SXMesh *mesh = new SXMesh;

		// mesh->Buffer=AnimatedMesh->addMeshBuffer();
		Meshes.push_back(mesh);

		return parseDataObjectMesh(*mesh);
	} else if (objectName == "AnimationSet") {
		return parseDataObjectAnimationSet();
	} else if (objectName == "AnimTicksPerSecond") {
		return parseDataObjectAnimationTicksPerSecond();
	} else if (objectName == "Material") {
		return parseUnknownDataObject();
	} else if (objectName == "}") {
		os::Printer::log("} found in dataObject", ELL_WARNING);
		return true;
	}

	os::Printer::log("Unknown data object in animation of .x file", objectName.c_str(), ELL_WARNING);

	return parseUnknownDataObject();
}

bool CXMeshFileLoader::parseDataObjectTemplate()
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading template", ELL_DEBUG);
#endif

	// parse a template data object. Currently not stored.
	core::stringc name;

	if (!readHeadOfDataObject(&name)) {
		os::Printer::log("Left delimiter in template data object missing.",
				name.c_str(), ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	// read GUID
	getNextToken();

	// read and ignore data members
	while (true) {
		core::stringc s = getNextToken();

		if (s == "}")
			break;

		if (s.size() == 0)
			return false;
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectFrame(SkinnedMesh::SJoint *Parent)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading frame", ELL_DEBUG);
#endif

	// A coordinate frame, or "frame of reference." The Frame template
	// is open and can contain any object. The Direct3D extensions (D3DX)
	// mesh-loading functions recognize Mesh, FrameTransformMatrix, and
	// Frame template instances as child objects when loading a Frame
	// instance.

	u32 JointID = 0;

	core::stringc name;

	if (!readHeadOfDataObject(&name)) {
		os::Printer::log("No opening brace in Frame found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	SkinnedMesh::SJoint *joint = 0;

	if (name.size()) {
		auto n = AnimatedMesh->getJointNumber(name.c_str());
		if (n.has_value()) {
			JointID = *n;
			joint = AnimatedMesh->getAllJoints()[JointID];
			joint->setParent(Parent);
		}
	}

	if (!joint) {
#ifdef _XREADER_DEBUG
		os::Printer::log("creating joint ", name.c_str(), ELL_DEBUG);
#endif
		joint = AnimatedMesh->addJoint(Parent);
		joint->Name = name.c_str();
		JointID = AnimatedMesh->getAllJoints().size() - 1;
	} else {
#ifdef _XREADER_DEBUG
		os::Printer::log("using joint ", name.c_str(), ELL_DEBUG);
#endif
	}

	// Now inside a frame.
	// read tokens until closing brace is reached.

	while (true) {
		core::stringc objectName = getNextToken();

#ifdef _XREADER_DEBUG
		os::Printer::log("debug DataObject in frame:", objectName.c_str(), ELL_DEBUG);
#endif

		if (objectName.size() == 0) {
			os::Printer::log("Unexpected ending found in Frame in x file.", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		} else if (objectName == "}") {
			break; // frame finished
		} else if (objectName == "Frame") {

			if (!parseDataObjectFrame(joint))
				return false;
		} else if (objectName == "FrameTransformMatrix") {
			core::matrix4 matrix;
			if (!parseDataObjectTransformationMatrix(matrix))
				return false;
			joint->transform = matrix;
		} else if (objectName == "Mesh") {
			/*
			frame.Meshes.push_back(SXMesh());
			if (!parseDataObjectMesh(frame.Meshes.getLast()))
				return false;
			*/
			SXMesh *mesh = new SXMesh;

			mesh->AttachedJointID = JointID;

			Meshes.push_back(mesh);

			if (!parseDataObjectMesh(*mesh))
				return false;
		} else {
			os::Printer::log("Unknown data object in frame in x file", objectName.c_str(), ELL_WARNING);
			if (!parseUnknownDataObject())
				return false;
		}
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectTransformationMatrix(core::matrix4 &mat)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading Transformation Matrix", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Transformation Matrix found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	readMatrix(mat);

	if (!checkForOneFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Transformation Matrix found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in Transformation Matrix found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectMesh(SXMesh &mesh)
{
	core::stringc name;

	if (!readHeadOfDataObject(&name)) {
#ifdef _XREADER_DEBUG
		os::Printer::log("CXFileReader: Reading mesh", ELL_DEBUG);
#endif
		os::Printer::log("No opening brace in Mesh found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading mesh", name.c_str(), ELL_DEBUG);
#endif

	// read vertex count
	const u32 nVertices = readInt();

	// read vertices
	mesh.Vertices.set_used(nVertices);
	for (u32 n = 0; n < nVertices; ++n) {
		readVector3(mesh.Vertices[n].Pos);
		mesh.Vertices[n].Color = 0xFFFFFFFF;
		mesh.Vertices[n].Normal = core::vector3df(0.0f);
	}

	if (!checkForTwoFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Mesh Vertex Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	// read faces
	const u32 nFaces = readInt();

	mesh.Indices.set_used(nFaces * 3);
	mesh.IndexCountPerFace.set_used(nFaces);

	core::array<u32> polygonfaces;
	u32 currentIndex = 0;

	for (u32 k = 0; k < nFaces; ++k) {
		const u32 fcnt = readInt();

		if (fcnt != 3) {
			if (fcnt < 3) {
				os::Printer::log("Invalid face count (<3) found in Mesh x file reader.", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}

			// read face indices
			polygonfaces.set_used(fcnt);
			u32 triangles = (fcnt - 2);
			mesh.Indices.set_used(mesh.Indices.size() + ((triangles - 1) * 3));
			mesh.IndexCountPerFace[k] = (u16)(triangles * 3);

			for (u32 f = 0; f < fcnt; ++f)
				polygonfaces[f] = readInt();

			for (u32 jk = 0; jk < triangles; ++jk) {
				mesh.Indices[currentIndex++] = polygonfaces[0];
				mesh.Indices[currentIndex++] = polygonfaces[jk + 1];
				mesh.Indices[currentIndex++] = polygonfaces[jk + 2];
			}

			// TODO: change face indices in material list
		} else {
			mesh.Indices[currentIndex++] = readInt();
			mesh.Indices[currentIndex++] = readInt();
			mesh.Indices[currentIndex++] = readInt();
			mesh.IndexCountPerFace[k] = 3;
		}
	}

	for (u32 j = 0; j < mesh.Indices.size(); j++) {
		if (mesh.Indices[j] >= mesh.Vertices.size()) {
			os::Printer::log("Out of range index found in Mesh x file reader.", ELL_WARNING);
			SET_ERR_AND_RETURN();
		}
	}

	if (!checkForTwoFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Mesh Face Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	// here, other data objects may follow

	while (true) {
		core::stringc objectName = getNextToken();

		if (objectName.size() == 0) {
			os::Printer::log("Unexpected ending found in Mesh in x file.", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		} else if (objectName == "}") {
			break; // mesh finished
		}

#ifdef _XREADER_DEBUG
		os::Printer::log("debug DataObject in mesh", objectName.c_str(), ELL_DEBUG);
#endif

		if (objectName == "MeshNormals") {
			if (!parseDataObjectMeshNormals(mesh))
				return false;
		} else if (objectName == "MeshTextureCoords") {
			if (!parseDataObjectMeshTextureCoords(mesh))
				return false;
		} else if (objectName == "MeshVertexColors") {
			if (!parseDataObjectMeshVertexColors(mesh))
				return false;
		} else if (objectName == "MeshMaterialList") {
			if (!parseDataObjectMeshMaterialList(mesh))
				return false;
		} else if (objectName == "VertexDuplicationIndices") {
			// we'll ignore vertex duplication indices
			// TODO: read them
			if (!parseUnknownDataObject())
				return false;
		} else if (objectName == "DeclData") {
			if (!readHeadOfDataObject()) {
				os::Printer::log("No starting brace in DeclData found.", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}
			// arbitrary vertex attributes
			// first comes the number of element definitions
			// then the vertex element type definitions
			// with format type;tesselator;semantics;usageindex
			// we want to support 2;0;6;0 == tangent
			//                    2;0;7;0 == binormal
			//                    2;0;3;0 == normal
			//                  1/2;0;5;0 == 1st uv coord
			// and              1/2;0;5;1 == 2nd uv coord
			// type==2 is 3xf32, type==1 is 2xf32
			u32 j;
			const u32 dcnt = readInt();
			u16 size = 0;
			s16 normalpos = -1;
			s16 uvpos = -1;
			s16 uv2pos = -1;
			s16 tangentpos = -1;
			s16 binormalpos = -1;
			s16 normaltype = -1;
			s16 uvtype = -1;
			s16 uv2type = -1;
			s16 tangenttype = -1;
			s16 binormaltype = -1;

			(void)tangentpos;   // disable unused variable warnings
			(void)binormalpos;  // disable unused variable warnings
			(void)tangenttype;  // disable unused variable warnings
			(void)binormaltype; // disable unused variable warnings

			for (j = 0; j < dcnt; ++j) {
				const u32 type = readInt();
				// const u32 tesselator = readInt();
				readInt();
				const u32 semantics = readInt();
				const u32 index = readInt();
				switch (semantics) {
				case 3:
					normalpos = size;
					normaltype = type;
					break;
				case 5:
					if (index == 0) {
						uvpos = size;
						uvtype = type;
					} else if (index == 1) {
						uv2pos = size;
						uv2type = type;
					}
					break;
				case 6:
					tangentpos = size;
					tangenttype = type;
					break;
				case 7:
					binormalpos = size;
					binormaltype = type;
					break;
				default:
					break;
				}
				switch (type) {
				case 0:
					size += 4;
					break;
				case 1:
					size += 8;
					break;
				case 2:
					size += 12;
					break;
				case 3:
					size += 16;
					break;
				case 4:
				case 5:
				case 6:
					size += 4;
					break;
				case 7:
					size += 8;
					break;
				case 8:
				case 9:
					size += 4;
					break;
				case 10:
					size += 8;
					break;
				case 11:
					size += 4;
					break;
				case 12:
					size += 8;
					break;
				case 13:
					size += 4;
					break;
				case 14:
					size += 4;
					break;
				case 15:
					size += 4;
					break;
				case 16:
					size += 8;
					break;
				}
			}
			const u32 datasize = readInt();
			u32 *data = new u32[datasize];
			for (j = 0; j < datasize; ++j)
				data[j] = readInt();

			if (!checkForOneFollowingSemicolons()) {
				os::Printer::log("No finishing semicolon in DeclData found.", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			}
			if (!checkForClosingBrace()) {
				os::Printer::log("No closing brace in DeclData.", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				delete[] data;
				SET_ERR_AND_RETURN();
			}
			u8 *dataptr = (u8 *)data;
			if ((uv2pos != -1) && (uv2type == 1))
				mesh.TCoords2.reallocate(mesh.Vertices.size());
			for (j = 0; j < mesh.Vertices.size(); ++j) {
				if ((normalpos != -1) && (normaltype == 2))
					mesh.Vertices[j].Normal.set(*((core::vector3df *)(dataptr + normalpos)));
				if ((uvpos != -1) && (uvtype == 1))
					mesh.Vertices[j].TCoords.set(*((core::vector2df *)(dataptr + uvpos)));
				if ((uv2pos != -1) && (uv2type == 1))
					mesh.TCoords2.push_back(*((core::vector2df *)(dataptr + uv2pos)));
				dataptr += size;
			}
			delete[] data;
		} else if (objectName == "FVFData") {
			if (!readHeadOfDataObject()) {
				os::Printer::log("No starting brace in FVFData found.", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}
			const u32 dataformat = readInt();
			const u32 datasize = readInt();
			u32 *data = new u32[datasize];
			for (u32 j = 0; j < datasize; ++j)
				data[j] = readInt();
			if (dataformat & 0x102) { // 2nd uv set
				mesh.TCoords2.reallocate(mesh.Vertices.size());
				u8 *dataptr = (u8 *)data;
				const u32 size = ((dataformat >> 8) & 0xf) * sizeof(core::vector2df);
				for (u32 j = 0; j < mesh.Vertices.size(); ++j) {
					mesh.TCoords2.push_back(*((core::vector2df *)(dataptr)));
					dataptr += size;
				}
			}
			delete[] data;
			if (!checkForOneFollowingSemicolons()) {
				os::Printer::log("No finishing semicolon in FVFData found.", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			}
			if (!checkForClosingBrace()) {
				os::Printer::log("No closing brace in FVFData found in x file", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}
		} else if (objectName == "XSkinMeshHeader") {
			if (!parseDataObjectSkinMeshHeader(mesh))
				return false;
		} else if (objectName == "SkinWeights") {
			// mesh.SkinWeights.push_back(SXSkinWeight());
			// if (!parseDataObjectSkinWeights(mesh.SkinWeights.getLast()))
			if (!parseDataObjectSkinWeights(mesh))
				return false;
		} else {
			os::Printer::log("Unknown data object in mesh in x file", objectName.c_str(), ELL_WARNING);
			if (!parseUnknownDataObject())
				return false;
		}
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectSkinWeights(SXMesh &mesh)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading mesh skin weights", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Skin Weights found in .x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	core::stringc TransformNodeName;

	if (!getNextTokenAsString(TransformNodeName)) {
		os::Printer::log("Unknown syntax while reading transform node name string in .x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	mesh.HasSkinning = true;

	auto n = AnimatedMesh->getJointNumber(TransformNodeName.c_str());
	SkinnedMesh::SJoint *joint = n.has_value() ? AnimatedMesh->getAllJoints()[*n] : nullptr;

	if (!joint) {
#ifdef _XREADER_DEBUG
		os::Printer::log("creating joint for skinning ", TransformNodeName.c_str(), ELL_DEBUG);
#endif
		n = AnimatedMesh->getAllJoints().size();
		joint = AnimatedMesh->addJoint(0);
		joint->Name = TransformNodeName.c_str();
	}

	// read vertex weights
	const u32 nWeights = readInt();

	// read vertex indices
	u32 i;

	const u32 jointStart = joint->Weights.size();
	joint->Weights.reserve(jointStart + nWeights);

	mesh.WeightJoint.reallocate(mesh.WeightJoint.size() + nWeights);
	mesh.WeightNum.reallocate(mesh.WeightNum.size() + nWeights);

	for (i = 0; i < nWeights; ++i) {
		mesh.WeightJoint.push_back(*n);
		mesh.WeightNum.push_back(joint->Weights.size()); // id of weight

		// Note: This adds a weight to joint->Weights
		SkinnedMesh::SWeight *weight = AnimatedMesh->addWeight(joint);

		weight->buffer_id = 0;
		weight->vertex_id = readInt();
	}

	// read vertex weights

	for (i = jointStart; i < jointStart + nWeights; ++i)
		joint->Weights[i].strength = readFloat();

	// read matrix offset

	// transforms the mesh vertices to the space of the bone
	// When concatenated to the bone's transform, this provides the
	// world space coordinates of the mesh as affected by the bone
	core::matrix4 MatrixOffset;
	readMatrix(MatrixOffset);
	joint->GlobalInversedMatrix = MatrixOffset;

	if (!checkForOneFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Skin Weights found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in Skin Weights found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectSkinMeshHeader(SXMesh &mesh)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading skin mesh header", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Skin Mesh header found in .x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	mesh.MaxSkinWeightsPerVertex = readInt();
	mesh.MaxSkinWeightsPerFace = readInt();
	mesh.BoneCount = readInt();

	if (!BinaryFormat)
		getNextToken(); // skip semicolon

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in skin mesh header in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectMeshNormals(SXMesh &mesh)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: reading mesh normals", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Mesh Normals found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	// read count
	const u32 nNormals = readInt();
	core::array<core::vector3df> normals;
	normals.set_used(nNormals);

	// read normals
	for (u32 i = 0; i < nNormals; ++i)
		readVector3(normals[i]);

	if (!checkForTwoFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Mesh Normals Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	core::array<u32> normalIndices;
	normalIndices.set_used(mesh.Indices.size());

	// read face normal indices
	const u32 nFNormals = readInt();
	// if (nFNormals >= mesh.IndexCountPerFace.size())
	if (0) { // this condition doesn't work for some reason
		os::Printer::log("Too many face normals found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	u32 normalidx = 0;
	core::array<u32> polygonfaces;
	for (u32 k = 0; k < nFNormals; ++k) {
		const u32 fcnt = readInt();
		u32 triangles = fcnt - 2;
		u32 indexcount = triangles * 3;

		if (indexcount != mesh.IndexCountPerFace[k]) {
			os::Printer::log("Not matching normal and face index count found in x file", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		}

		if (indexcount == 3) {
			// default, only one triangle in this face
			for (u32 h = 0; h < 3; ++h) {
				const u32 normalnum = readInt();
				mesh.Vertices[mesh.Indices[normalidx++]].Normal.set(normals[normalnum]);
			}
		} else {
			polygonfaces.set_used(fcnt);
			// multiple triangles in this face
			for (u32 h = 0; h < fcnt; ++h)
				polygonfaces[h] = readInt();

			for (u32 jk = 0; jk < triangles; ++jk) {
				mesh.Vertices[mesh.Indices[normalidx++]].Normal.set(normals[polygonfaces[0]]);
				mesh.Vertices[mesh.Indices[normalidx++]].Normal.set(normals[polygonfaces[jk + 1]]);
				mesh.Vertices[mesh.Indices[normalidx++]].Normal.set(normals[polygonfaces[jk + 2]]);
			}
		}
	}

	if (!checkForTwoFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Mesh Face Normals Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in Mesh Normals found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectMeshTextureCoords(SXMesh &mesh)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: reading mesh texture coordinates", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Mesh Texture Coordinates found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	const u32 nCoords = readInt();
	// if (nCoords >= mesh.Vertices.size())
	if (0) { // this condition doesn't work for some reason
		os::Printer::log("Too many texture coords found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	for (u32 i = 0; i < nCoords; ++i)
		readVector2(mesh.Vertices[i].TCoords);

	if (!checkForTwoFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Mesh Texture Coordinates Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in Mesh Texture Coordinates Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectMeshVertexColors(SXMesh &mesh)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: reading mesh vertex colors", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace for Mesh Vertex Colors found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	mesh.HasVertexColors = true;
	const u32 nColors = readInt();
	for (u32 i = 0; i < nColors; ++i) {
		const u32 Index = readInt();
		if (Index >= mesh.Vertices.size()) {
			os::Printer::log("index value in parseDataObjectMeshVertexColors out of bounds", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		}
		readRGBA(mesh.Vertices[Index].Color);
		checkForOneFollowingSemicolons();
	}

	if (!checkForOneFollowingSemicolons()) {
		os::Printer::log("No finishing semicolon in Mesh Vertex Colors Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
	}

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in Mesh Texture Coordinates Array found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		return false;
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectMeshMaterialList(SXMesh &mesh)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading mesh material list", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Mesh Material List found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	// read material count
	const u32 nMaterials = readInt();
	mesh.Materials.reallocate(nMaterials);

	// read non triangulated face material index count
	const u32 nFaceIndices = readInt();

	// There seems to be a compact representation of "all faces the same material"
	// being represented as 1;1;0;; which means 1 material, 1 face with first material
	// all the other faces have to obey then, so check is disabled
	// if (nFaceIndices != mesh.IndexCountPerFace.size())
	//	os::Printer::log("Index count per face not equal to face material index count in x file.", ELL_WARNING);

	// read non triangulated face indices and create triangulated ones
	mesh.FaceMaterialIndices.set_used(mesh.Indices.size() / 3);
	u32 triangulatedindex = 0;
	u32 ind = 0;
	for (u32 tfi = 0; tfi < mesh.IndexCountPerFace.size(); ++tfi) {
		if (tfi < nFaceIndices)
			ind = readInt();
		if (ind >= core::max_(nMaterials, 1U)) {
			os::Printer::log("Out of range index found in x file", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		}
		const u32 fc = mesh.IndexCountPerFace[tfi] / 3;
		for (u32 k = 0; k < fc; ++k)
			mesh.FaceMaterialIndices[triangulatedindex++] = ind;
	}

	// in version 03.02, the face indices end with two semicolons.
	// commented out version check, as version 03.03 exported from blender also has 2 semicolons
	if (!BinaryFormat) { // && MajorVersion == 3 && MinorVersion <= 2)
		if (P[0] == ';')
			++P;
	}

	// read following data objects

	while (true) {
		core::stringc objectName = getNextToken();

		if (objectName.size() == 0) {
			os::Printer::log("Unexpected ending found in Mesh Material list in .x file.", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		} else if (objectName == "}") {
			break; // material list finished
		} else if (objectName == "{") {
			// template materials now available thanks to joeWright
			objectName = getNextToken();
			mesh.Materials.push_back(video::SMaterial());
			getNextToken(); // skip }
		} else if (objectName == "Material") {
			mesh.Materials.push_back(video::SMaterial());
			if (!parseUnknownDataObject())
				return false;
		} else if (objectName == ";") {
			// ignore
		} else {
			os::Printer::log("Unknown data object in material list in x file", objectName.c_str(), ELL_WARNING);
			if (!parseUnknownDataObject())
				return false;
		}
	}
	return true;
}

bool CXMeshFileLoader::parseDataObjectAnimationSet()
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: Reading animation set", ELL_DEBUG);
#endif

	core::stringc AnimationName;

	if (!readHeadOfDataObject(&AnimationName)) {
		os::Printer::log("No opening brace in Animation Set found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}
	os::Printer::log("Reading animationset ", AnimationName, ELL_DEBUG);

	while (true) {
		core::stringc objectName = getNextToken();

		if (objectName.size() == 0) {
			os::Printer::log("Unexpected ending found in Animation set in x file.", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		} else if (objectName == "}") {
			break; // animation set finished
		} else if (objectName == "Animation") {
			if (!parseDataObjectAnimation())
				return false;
		} else {
			os::Printer::log("Unknown data object in animation set in x file", objectName.c_str(), ELL_WARNING);
			if (!parseUnknownDataObject())
				return false;
		}
	}
	return true;
}

bool CXMeshFileLoader::parseDataObjectAnimationTicksPerSecond()
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: reading AnimationTicksPerSecond", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Animation found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	const u32 ticks = readInt();

	if (!checkForOneFollowingSemicolons()) {
		os::Printer::log("No closing semicolon in AnimationTicksPerSecond in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in AnimationTicksPerSecond in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	AnimatedMesh->setAnimationSpeed(static_cast<f32>(ticks));

	return true;
}

bool CXMeshFileLoader::parseDataObjectAnimation()
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: reading animation", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Animation found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	// anim.closed = true;
	// anim.linearPositionQuality = true;
	SkinnedMesh::SJoint animationDump;

	core::stringc FrameName;

	while (true) {
		core::stringc objectName = getNextToken();

		if (objectName.size() == 0) {
			os::Printer::log("Unexpected ending found in Animation in x file.", ELL_WARNING);
			os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			SET_ERR_AND_RETURN();
		} else if (objectName == "}") {
			break; // animation finished
		} else if (objectName == "AnimationKey") {
			if (!parseDataObjectAnimationKey(&animationDump))
				return false;
		} else if (objectName == "AnimationOptions") {
			// TODO: parse options.
			if (!parseUnknownDataObject())
				return false;
		} else if (objectName == "{") {
			// read frame name
			FrameName = getNextToken();

			if (!checkForClosingBrace()) {
				os::Printer::log("Unexpected ending found in Animation in x file.", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}
		} else {
			os::Printer::log("Unknown data object in animation in x file", objectName.c_str(), ELL_WARNING);
			if (!parseUnknownDataObject())
				SET_ERR_AND_RETURN();
		}
	}

	if (FrameName.size() != 0) {
#ifdef _XREADER_DEBUG
		os::Printer::log("frame name", FrameName.c_str(), ELL_DEBUG);
#endif
		auto n = AnimatedMesh->getJointNumber(FrameName.c_str());

		SkinnedMesh::SJoint *joint;
		if (n.has_value()) {
			joint = AnimatedMesh->getAllJoints()[*n];
		} else {
#ifdef _XREADER_DEBUG
			os::Printer::log("creating joint for animation ", FrameName.c_str(), ELL_DEBUG);
#endif
			joint = AnimatedMesh->addJoint(0);
			joint->Name = FrameName.c_str();
		}

		joint->keys.append(animationDump.keys);
	} else
		os::Printer::log("joint name was never given", ELL_WARNING);

	return true;
}

bool CXMeshFileLoader::parseDataObjectAnimationKey(SkinnedMesh::SJoint *joint)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: reading animation key", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Animation Key found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	// read key type

	const u32 keyType = readInt();

	if (keyType > 4) {
		os::Printer::log("Unknown key type found in Animation Key in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	// read number of keys
	const u32 numberOfKeys = readInt();

	// eat the semicolon after the "0".  if there are keys present, readInt()
	// does this for us.  If there aren't, we need to do it explicitly
	if (numberOfKeys == 0)
		checkForOneFollowingSemicolons();

	for (u32 i = 0; i < numberOfKeys; ++i) {
		// read time
		const f32 time = (f32)readInt();

		// read keys
		switch (keyType) {
		case 0: // rotation
		{
			// read quaternions

			// read count
			if (readInt() != 4) {
				os::Printer::log("Expected 4 numbers in animation key in x file", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}

			f32 W = -readFloat();
			f32 X = -readFloat();
			f32 Y = -readFloat();
			f32 Z = -readFloat();

			if (!checkForTwoFollowingSemicolons()) {
				os::Printer::log("No finishing semicolon after quaternion animation key in x file", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			}

			core::quaternion rotation(X, Y, Z, W);
			rotation.normalize();
			AnimatedMesh->addRotationKey(joint, time, rotation);
		} break;
		case 1: // scale
		case 2: // position
		{
			// read vectors

			// read count
			if (readInt() != 3) {
				os::Printer::log("Expected 3 numbers in animation key in x file", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}

			core::vector3df vector;
			readVector3(vector);

			if (!checkForTwoFollowingSemicolons()) {
				os::Printer::log("No finishing semicolon after vector animation key in x file", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			}

			if (keyType == 2) {
				AnimatedMesh->addPositionKey(joint, time, vector);
			} else {
				AnimatedMesh->addScaleKey(joint, time, vector);
			}
		} break;
		case 3:
		case 4: {
			// read matrix

			// read count
			if (readInt() != 16) {
				os::Printer::log("Expected 16 numbers in animation key in x file", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
				SET_ERR_AND_RETURN();
			}

			// read matrix
			core::matrix4 mat(core::matrix4::EM4CONST_NOTHING);
			readMatrix(mat);

			// mat=joint->LocalMatrix*mat;

			if (!checkForOneFollowingSemicolons()) {
				os::Printer::log("No finishing semicolon after matrix animation key in x file", ELL_WARNING);
				os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
			}

			AnimatedMesh->addRotationKey(joint, time, core::quaternion(mat.getTransposed()));
			AnimatedMesh->addPositionKey(joint, time, mat.getTranslation());

			/*
							core::vector3df scale=mat.getScale();

							if (scale.X==0)
								scale.X=1;
							if (scale.Y==0)
								scale.Y=1;
							if (scale.Z==0)
								scale.Z=1;
							SkinnedMesh::SScaleKey *keyS=AnimatedMesh->addScaleKey(joint);
							keyS->frame=time;
							keyS->scale=scale;
			*/
		} break;
		} // end switch
	}

	if (!checkForOneFollowingSemicolons())
		--P;

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in animation key in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	return true;
}

bool CXMeshFileLoader::parseDataObjectTextureFilename(core::stringc &texturename)
{
#ifdef _XREADER_DEBUG
	os::Printer::log("CXFileReader: reading texture filename", ELL_DEBUG);
#endif

	if (!readHeadOfDataObject()) {
		os::Printer::log("No opening brace in Texture filename found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	if (!getNextTokenAsString(texturename)) {
		os::Printer::log("Unknown syntax while reading texture filename string in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	if (!checkForClosingBrace()) {
		os::Printer::log("No closing brace in Texture filename found in x file", ELL_WARNING);
		os::Printer::log("Line", core::stringc(Line).c_str(), ELL_WARNING);
		SET_ERR_AND_RETURN();
	}

	return true;
}

bool CXMeshFileLoader::parseUnknownDataObject()
{
	// find opening delimiter
	while (true) {
		core::stringc t = getNextToken();

		if (t.size() == 0)
			return false;

		if (t == "{")
			break;
	}

	u32 counter = 1;

	// parse until closing delimiter

	while (counter) {
		core::stringc t = getNextToken();

		if (t.size() == 0)
			return false;

		if (t == "{")
			++counter;
		else if (t == "}")
			--counter;
	}

	return true;
}

//! checks for closing curly brace, returns false if not there
bool CXMeshFileLoader::checkForClosingBrace()
{
	return (getNextToken() == "}");
}

//! checks for one following semicolon, returns false if not there
bool CXMeshFileLoader::checkForOneFollowingSemicolons()
{
	if (BinaryFormat)
		return true;

	if (getNextToken() == ";")
		return true;
	else {
		--P;
		return false;
	}
}

//! checks for two following semicolons, returns false if they are not there
bool CXMeshFileLoader::checkForTwoFollowingSemicolons()
{
	if (BinaryFormat)
		return true;

	for (u32 k = 0; k < 2; ++k) {
		if (getNextToken() != ";") {
			--P;
			return false;
		}
	}

	return true;
}

//! reads header of dataobject including the opening brace.
//! returns false if error happened, and writes name of object
//! if there is one
bool CXMeshFileLoader::readHeadOfDataObject(core::stringc *outname)
{
	core::stringc nameOrBrace = getNextToken();
	if (nameOrBrace != "{") {
		if (outname)
			(*outname) = nameOrBrace;

		if (getNextToken() != "{")
			return false;
	}

	return true;
}

//! returns next parseable token. Returns empty string if no token there
core::stringc CXMeshFileLoader::getNextToken()
{
	core::stringc s;

	// process binary-formatted file
	if (BinaryFormat) {
		// in binary mode it will only return NAME and STRING token
		// and (correctly) skip over other tokens.

		s16 tok = readBinWord();
		u32 len;

		// standalone tokens
		switch (tok) {
		case 1:
			// name token
			len = readBinDWord();
			s = core::stringc(P, len);
			P += len;
			return s;
		case 2:
			// string token
			len = readBinDWord();
			s = core::stringc(P, len);
			P += (len + 2);
			return s;
		case 3:
			// integer token
			P += 4;
			return "<integer>";
		case 5:
			// GUID token
			P += 16;
			return "<guid>";
		case 6:
			len = readBinDWord();
			P += (len * 4);
			return "<int_list>";
		case 7:
			len = readBinDWord();
			P += (len * FloatSize);
			return "<flt_list>";
		case 0x0a:
			return "{";
		case 0x0b:
			return "}";
		case 0x0c:
			return "(";
		case 0x0d:
			return ")";
		case 0x0e:
			return "[";
		case 0x0f:
			return "]";
		case 0x10:
			return "<";
		case 0x11:
			return ">";
		case 0x12:
			return ".";
		case 0x13:
			return ",";
		case 0x14:
			return ";";
		case 0x1f:
			return "template";
		case 0x28:
			return "WORD";
		case 0x29:
			return "DWORD";
		case 0x2a:
			return "FLOAT";
		case 0x2b:
			return "DOUBLE";
		case 0x2c:
			return "CHAR";
		case 0x2d:
			return "UCHAR";
		case 0x2e:
			return "SWORD";
		case 0x2f:
			return "SDWORD";
		case 0x30:
			return "void";
		case 0x31:
			return "string";
		case 0x32:
			return "unicode";
		case 0x33:
			return "cstring";
		case 0x34:
			return "array";
		}
	}
	// process text-formatted file
	else {
		findNextNoneWhiteSpace();

		if (P >= End)
			return s;

		while ((P < End) && !core::isspace(P[0])) {
			// either keep token delimiters when already holding a token, or return if first valid char
			if (P[0] == ';' || P[0] == '}' || P[0] == '{' || P[0] == ',') {
				if (!s.size()) {
					s.append(P[0]);
					++P;
				}
				break; // stop for delimiter
			}
			s.append(P[0]);
			++P;
		}
	}
	return s;
}

//! places pointer to next begin of a token, which must be a number,
// and ignores comments
void CXMeshFileLoader::findNextNoneWhiteSpaceNumber()
{
	if (BinaryFormat)
		return;

	while ((P < End) && (P[0] != '-') && (P[0] != '.') &&
			!(core::isdigit(P[0]))) {
		// check if this is a comment
		if ((P[0] == '/' && P[1] == '/') || P[0] == '#')
			readUntilEndOfLine();
		else
			++P;
	}
}

// places pointer to next begin of a token, and ignores comments
void CXMeshFileLoader::findNextNoneWhiteSpace()
{
	if (BinaryFormat)
		return;

	while (true) {
		while ((P < End) && core::isspace(P[0])) {
			if (*P == '\n')
				++Line;
			++P;
		}

		if (P >= End)
			return;

		// check if this is a comment
		if ((P[0] == '/' && P[1] == '/') ||
				P[0] == '#')
			readUntilEndOfLine();
		else
			break;
	}
}

//! reads a x file style string
bool CXMeshFileLoader::getNextTokenAsString(core::stringc &out)
{
	if (BinaryFormat) {
		out = getNextToken();
		return true;
	}
	findNextNoneWhiteSpace();

	if (P >= End)
		return false;

	if (P[0] != '"')
		return false;
	++P;

	while (P < End && P[0] != '"') {
		out.append(P[0]);
		++P;
	}

	if (P[1] != ';' || P[0] != '"')
		return false;
	P += 2;

	return true;
}

void CXMeshFileLoader::readUntilEndOfLine()
{
	if (BinaryFormat)
		return;

	while (P < End) {
		if (P[0] == '\n' || P[0] == '\r') {
			++P;
			++Line;
			return;
		}

		++P;
	}
}

u16 CXMeshFileLoader::readBinWord()
{
	if (P >= End)
		return 0;
#ifdef __BIG_ENDIAN__
	const u16 tmp = os::Byteswap::byteswap(*(u16 *)P);
#else
	const u16 tmp = *(u16 *)P;
#endif
	P += 2;
	return tmp;
}

u32 CXMeshFileLoader::readBinDWord()
{
	if (P >= End)
		return 0;
#ifdef __BIG_ENDIAN__
	const u32 tmp = os::Byteswap::byteswap(*(u32 *)P);
#else
	const u32 tmp = *(u32 *)P;
#endif
	P += 4;
	return tmp;
}

u32 CXMeshFileLoader::readInt()
{
	if (BinaryFormat) {
		if (!BinaryNumCount) {
			const u16 tmp = readBinWord(); // 0x06 or 0x03
			if (tmp == 0x06)
				BinaryNumCount = readBinDWord();
			else
				BinaryNumCount = 1; // single int
		}
		--BinaryNumCount;
		return readBinDWord();
	} else {
		findNextNoneWhiteSpaceNumber();
		return core::strtoul10(P, &P);
	}
}

f32 CXMeshFileLoader::readFloat()
{
	if (BinaryFormat) {
		if (!BinaryNumCount) {
			const u16 tmp = readBinWord(); // 0x07 or 0x42
			if (tmp == 0x07)
				BinaryNumCount = readBinDWord();
			else
				BinaryNumCount = 1; // single int
		}
		--BinaryNumCount;
		if (FloatSize == 8) {
#ifdef __BIG_ENDIAN__
			// TODO: Check if data is properly converted here
			f32 ctmp[2];
			ctmp[1] = os::Byteswap::byteswap(*(f32 *)P);
			ctmp[0] = os::Byteswap::byteswap(*(f32 *)P + 4);
			const f32 tmp = (f32)(*(f64 *)(void *)ctmp);
#else
			const f32 tmp = (f32)(*(f64 *)P);
#endif
			P += 8;
			return tmp;
		} else {
#ifdef __BIG_ENDIAN__
			const f32 tmp = os::Byteswap::byteswap(*(f32 *)P);
#else
			const f32 tmp = *(f32 *)P;
#endif
			P += 4;
			return tmp;
		}
	}
	findNextNoneWhiteSpaceNumber();
	f32 ftmp;
	P = core::fast_atof_move(P, ftmp);
	return ftmp;
}

// read 2-dimensional vector. Stops at semicolon after second value for text file format
bool CXMeshFileLoader::readVector2(core::vector2df &vec)
{
	vec.X = readFloat();
	vec.Y = readFloat();
	return true;
}

// read 3-dimensional vector. Stops at semicolon after third value for text file format
bool CXMeshFileLoader::readVector3(core::vector3df &vec)
{
	vec.X = readFloat();
	vec.Y = readFloat();
	vec.Z = readFloat();
	return true;
}

// read color without alpha value. Stops after second semicolon after blue value
bool CXMeshFileLoader::readRGB(video::SColor &color)
{
	video::SColorf tmpColor;
	tmpColor.r = readFloat();
	tmpColor.g = readFloat();
	tmpColor.b = readFloat();
	color = tmpColor.toSColor();
	return checkForOneFollowingSemicolons();
}

// read color with alpha value. Stops after second semicolon after blue value
bool CXMeshFileLoader::readRGBA(video::SColor &color)
{
	video::SColorf tmpColor;
	tmpColor.r = readFloat();
	tmpColor.g = readFloat();
	tmpColor.b = readFloat();
	tmpColor.a = readFloat();
	color = tmpColor.toSColor();
	return checkForOneFollowingSemicolons();
}

// read matrix from list of floats
bool CXMeshFileLoader::readMatrix(core::matrix4 &mat)
{
	for (u32 i = 0; i < 16; ++i)
		mat[i] = readFloat();
	return checkForOneFollowingSemicolons();
}

} // end namespace scene
