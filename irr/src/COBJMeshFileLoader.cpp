// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "COBJMeshFileLoader.h"
#include "IMeshManipulator.h"
#include "IVideoDriver.h"
#include "SMesh.h"
#include "CMeshBuffer.h"
#include "IReadFile.h"
#include "fast_atof.h"
#include "coreutil.h"
#include "os.h"

namespace scene
{

#ifdef _DEBUG
#define _IRR_DEBUG_OBJ_LOADER_
#endif

//! Constructor
COBJMeshFileLoader::COBJMeshFileLoader(scene::ISceneManager *smgr) :
		SceneManager(smgr)
{}

//! destructor
COBJMeshFileLoader::~COBJMeshFileLoader()
{
}

//! returns true if the file maybe is able to be loaded by this class
//! based on the file extension (e.g. ".bsp")
bool COBJMeshFileLoader::isALoadableFileExtension(const io::path &filename) const
{
	return core::hasFileExtension(filename, "obj");
}

//! creates/loads an animated mesh from the file.
//! \return Pointer to the created mesh. Returns 0 if loading failed.
//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
//! See IReferenceCounted::drop() for more information.
IAnimatedMesh *COBJMeshFileLoader::createMesh(io::IReadFile *file)
{
	if (!file)
		return 0;

	const long filesize = file->getSize();
	if (!filesize)
		return 0;

	const u32 WORD_BUFFER_LENGTH = 512;

	core::array<core::vector3df> vertexBuffer(1000);
	core::array<core::vector3df> normalsBuffer(1000);
	core::array<core::vector2df> textureCoordBuffer(1000);

	SObjMtl *currMtl = new SObjMtl();
	Materials.push_back(currMtl);
	u32 smoothingGroup = 0;

	const io::path fullName = file->getFileName();

	c8 *buf = new c8[filesize + 1]; // plus null-terminator
	memset(buf, 0, filesize + 1);
	file->read((void *)buf, filesize);
	const c8 *const bufEnd = buf + filesize;

	// Process obj information
	const c8 *bufPtr = buf;
	core::stringc grpName, mtlName;
	bool mtlChanged = false;
	[[maybe_unused]] u32 lineNr = 1; // only counts non-empty lines, still useful in debugging to locate errors
	core::array<int> faceCorners;
	faceCorners.reallocate(32); // should be large enough
	const core::stringc TAG_OFF = "off";
	u32 degeneratedFaces = 0;

	while (bufPtr != bufEnd) {
		switch (bufPtr[0]) {
		case 'm': // mtllib (material)
			break; // not supported

		case 'v': // v, vn, vt
			switch (bufPtr[1]) {
			case ' ': // vertex
			{
				core::vector3df vec;
				bufPtr = readVec3(bufPtr, vec, bufEnd);
				vertexBuffer.push_back(vec);
			} break;

			case 'n': // normal
			{
				core::vector3df vec;
				bufPtr = readVec3(bufPtr, vec, bufEnd);
				normalsBuffer.push_back(vec);
			} break;

			case 't': // texcoord
			{
				core::vector2df vec;
				bufPtr = readUV(bufPtr, vec, bufEnd);
				textureCoordBuffer.push_back(vec);
			} break;
			}
			break;

		case 'g': // group name
		{
			c8 grp[WORD_BUFFER_LENGTH];
			bufPtr = goAndCopyNextWord(grp, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
#ifdef _IRR_DEBUG_OBJ_LOADER_
			os::Printer::log("Loaded group start", grp, ELL_DEBUG);
#endif
			grpName = ('\0' != grp[0]) ? grp : "default";
			mtlChanged = true;
		} break;

		case 's': // smoothing can be a group or off (equiv. to 0)
		{
			c8 smooth[WORD_BUFFER_LENGTH];
			bufPtr = goAndCopyNextWord(smooth, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
#ifdef _IRR_DEBUG_OBJ_LOADER_
			os::Printer::log("Loaded smoothing group start", smooth, ELL_DEBUG);
#endif
			if (TAG_OFF == smooth)
				smoothingGroup = 0;
			else
				smoothingGroup = core::strtoul10(smooth);

			(void)smoothingGroup; // disable unused variable warnings
		} break;

		case 'u': // usemtl
			// get name of material
			{
				c8 matName[WORD_BUFFER_LENGTH];
				bufPtr = goAndCopyNextWord(matName, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
#ifdef _IRR_DEBUG_OBJ_LOADER_
				os::Printer::log("Loaded material start", matName, ELL_DEBUG);
#endif
				mtlName = matName;
				mtlChanged = true;
			}
			break;

		case 'f': // face
		{
			c8 vertexWord[WORD_BUFFER_LENGTH]; // for retrieving vertex data
			video::S3DVertex v;
			// Assign vertex color from currently active material's diffuse color
			if (mtlChanged) {
				// retrieve the material
				SObjMtl *useMtl = findMtl(mtlName, grpName);
				// only change material if we found it
				if (useMtl)
					currMtl = useMtl;
				mtlChanged = false;
			}
			if (currMtl)
				v.Color = video::SColorf(0.8f, 0.8f, 0.8f, 1.0f).toSColor();

			// get all vertices data in this face (current line of obj file)
			const core::stringc wordBuffer = copyLine(bufPtr, bufEnd);
			const c8 *linePtr = wordBuffer.c_str();
			const c8 *const endPtr = linePtr + wordBuffer.size();

			faceCorners.set_used(0); // fast clear

			// read in all vertices
			auto &Vertices = currMtl->Meshbuffer->Vertices->Data;
			linePtr = goNextWord(linePtr, endPtr);
			while (0 != linePtr[0]) {
				// Array to communicate with retrieveVertexIndices()
				// sends the buffer sizes and gets the actual indices
				// if index not set returns -1
				s32 Idx[3];
				Idx[0] = Idx[1] = Idx[2] = -1;

				// read in next vertex's data
				u32 wlength = copyWord(vertexWord, linePtr, WORD_BUFFER_LENGTH, endPtr);
				// this function will also convert obj's 1-based index to c++'s 0-based index
				retrieveVertexIndices(vertexWord, Idx, vertexWord + wlength + 1, vertexBuffer.size(), textureCoordBuffer.size(), normalsBuffer.size());
				if (Idx[0] >= 0 && Idx[0] < (s32)vertexBuffer.size())
					v.Pos = vertexBuffer[Idx[0]];
				else {
					os::Printer::log("Invalid vertex index in this line", wordBuffer.c_str(), ELL_ERROR);
					delete[] buf;
					cleanUp();
					return 0;
				}
				if (Idx[1] >= 0 && Idx[1] < (s32)textureCoordBuffer.size())
					v.TCoords = textureCoordBuffer[Idx[1]];
				else
					v.TCoords.set(0.0f, 0.0f);
				if (Idx[2] >= 0 && Idx[2] < (s32)normalsBuffer.size())
					v.Normal = normalsBuffer[Idx[2]];
				else {
					v.Normal.set(0.0f, 0.0f, 0.0f);
					currMtl->RecalculateNormals = true;
				}

				int vertLocation;
				auto n = currMtl->VertMap.find(v);
				if (n != currMtl->VertMap.end()) {
					vertLocation = n->second;
				} else {
					Vertices.push_back(v);
					vertLocation = Vertices.size() - 1;
					currMtl->VertMap.emplace(v, vertLocation);
				}

				faceCorners.push_back(vertLocation);

				// go to next vertex
				linePtr = goNextWord(linePtr, endPtr);
			}

			if (faceCorners.size() < 3) {
				os::Printer::log("Too few vertices in this line", wordBuffer.c_str(), ELL_ERROR);
				delete[] buf;
				cleanUp();
				return 0;
			}

			// triangulate the face
			auto &Indices = currMtl->Meshbuffer->Indices->Data;
			const int c = faceCorners[0];
			for (u32 i = 1; i < faceCorners.size() - 1; ++i) {
				// Add a triangle
				const int a = faceCorners[i + 1];
				const int b = faceCorners[i];
				if (a != b && a != c && b != c) { // ignore degenerated faces. We can get them when we merge vertices above in the VertMap.
					Indices.push_back(a);
					Indices.push_back(b);
					Indices.push_back(c);
				} else {
					++degeneratedFaces;
				}
			}
		} break;

		case '#': // comment
		default:
			break;
		} // end switch(bufPtr[0])
		// eat up rest of line
		bufPtr = goNextLine(bufPtr, bufEnd);
		++lineNr;
	} // end while(bufPtr && (bufPtr-buf<filesize))

	if (degeneratedFaces > 0) {
		core::stringc log(degeneratedFaces);
		log += " degenerated faces removed in ";
		log += core::stringc(fullName);
		os::Printer::log(log.c_str(), ELL_INFORMATION);
	}

	SMesh *mesh = new SMesh();

	// Combine all the groups (meshbuffers) into the mesh
	for (u32 m = 0; m < Materials.size(); ++m) {
		if (Materials[m]->Meshbuffer->getIndexCount() > 0) {
			Materials[m]->Meshbuffer->recalculateBoundingBox();
			if (Materials[m]->RecalculateNormals)
				SceneManager->getMeshManipulator()->recalculateNormals(Materials[m]->Meshbuffer);
			mesh->addMeshBuffer(Materials[m]->Meshbuffer);
		}
	}

	// Clean up the allocate obj file contents
	delete[] buf;
	// more cleaning up
	cleanUp();

	// Nothing in the mesh
	if (mesh->getMeshBufferCount() == 0) {
		mesh->drop();
		return nullptr;
	}

	mesh->recalculateBoundingBox();
	return mesh;
}

//! Read RGB color
const c8 *COBJMeshFileLoader::readColor(const c8 *bufPtr, video::SColor &color, const c8 *const bufEnd)
{
	const u32 COLOR_BUFFER_LENGTH = 16;
	c8 colStr[COLOR_BUFFER_LENGTH];

	bufPtr = goAndCopyNextWord(colStr, bufPtr, COLOR_BUFFER_LENGTH, bufEnd);
	color.setRed((u32)core::round32(core::fast_atof(colStr) * 255.f));
	bufPtr = goAndCopyNextWord(colStr, bufPtr, COLOR_BUFFER_LENGTH, bufEnd);
	color.setGreen((u32)core::round32(core::fast_atof(colStr) * 255.f));
	bufPtr = goAndCopyNextWord(colStr, bufPtr, COLOR_BUFFER_LENGTH, bufEnd);
	color.setBlue((u32)core::round32(core::fast_atof(colStr) * 255.f));
	return bufPtr;
}

//! Read 3d vector of floats
const c8 *COBJMeshFileLoader::readVec3(const c8 *bufPtr, core::vector3df &vec, const c8 *const bufEnd)
{
	const u32 WORD_BUFFER_LENGTH = 256;
	c8 wordBuffer[WORD_BUFFER_LENGTH];

	bufPtr = goAndCopyNextWord(wordBuffer, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
	vec.X = -core::fast_atof(wordBuffer); // change handedness
	bufPtr = goAndCopyNextWord(wordBuffer, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
	vec.Y = core::fast_atof(wordBuffer);
	bufPtr = goAndCopyNextWord(wordBuffer, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
	vec.Z = core::fast_atof(wordBuffer);
	return bufPtr;
}

//! Read 2d vector of floats
const c8 *COBJMeshFileLoader::readUV(const c8 *bufPtr, core::vector2df &vec, const c8 *const bufEnd)
{
	const u32 WORD_BUFFER_LENGTH = 256;
	c8 wordBuffer[WORD_BUFFER_LENGTH];

	bufPtr = goAndCopyNextWord(wordBuffer, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
	vec.X = core::fast_atof(wordBuffer);
	bufPtr = goAndCopyNextWord(wordBuffer, bufPtr, WORD_BUFFER_LENGTH, bufEnd);
	vec.Y = 1 - core::fast_atof(wordBuffer); // change handedness
	return bufPtr;
}

//! Read boolean value represented as 'on' or 'off'
const c8 *COBJMeshFileLoader::readBool(const c8 *bufPtr, bool &tf, const c8 *const bufEnd)
{
	const u32 BUFFER_LENGTH = 8;
	c8 tfStr[BUFFER_LENGTH];

	bufPtr = goAndCopyNextWord(tfStr, bufPtr, BUFFER_LENGTH, bufEnd);
	tf = strcmp(tfStr, "off") != 0;
	return bufPtr;
}

COBJMeshFileLoader::SObjMtl *COBJMeshFileLoader::findMtl(const core::stringc &mtlName, const core::stringc &grpName)
{
	COBJMeshFileLoader::SObjMtl *defMaterial = 0;
	// search existing Materials for best match
	// exact match does return immediately, only name match means a new group
	for (u32 i = 0; i < Materials.size(); ++i) {
		if (Materials[i]->Name == mtlName) {
			if (Materials[i]->Group == grpName)
				return Materials[i];
			else
				defMaterial = Materials[i];
		}
	}
	// we found a partial match
	if (defMaterial) {
		Materials.push_back(new SObjMtl(*defMaterial));
		Materials.getLast()->Group = grpName;
		return Materials.getLast();
	}
	// we found a new group for a non-existent material
	else if (grpName.size()) {
		Materials.push_back(new SObjMtl(*Materials[0]));
		Materials.getLast()->Group = grpName;
		return Materials.getLast();
	}
	return 0;
}

//! skip space characters and stop on first non-space
const c8 *COBJMeshFileLoader::goFirstWord(const c8 *buf, const c8 *const bufEnd, bool acrossNewlines)
{
	// skip space characters
	if (acrossNewlines)
		while ((buf != bufEnd) && core::isspace(*buf))
			++buf;
	else
		while ((buf != bufEnd) && core::isspace(*buf) && (*buf != '\n'))
			++buf;

	return buf;
}

//! skip current word and stop at beginning of next one
const c8 *COBJMeshFileLoader::goNextWord(const c8 *buf, const c8 *const bufEnd, bool acrossNewlines)
{
	// skip current word
	while ((buf != bufEnd) && !core::isspace(*buf))
		++buf;

	return goFirstWord(buf, bufEnd, acrossNewlines);
}

//! Read until line break is reached and stop at the next non-space character
const c8 *COBJMeshFileLoader::goNextLine(const c8 *buf, const c8 *const bufEnd)
{
	// look for newline characters
	while (buf != bufEnd) {
		// found it, so leave
		if (*buf == '\n' || *buf == '\r')
			break;
		++buf;
	}
	return goFirstWord(buf, bufEnd);
}

u32 COBJMeshFileLoader::copyWord(c8 *outBuf, const c8 *const inBuf, u32 outBufLength, const c8 *const bufEnd)
{
	if (!outBufLength)
		return 0;
	if (!inBuf) {
		*outBuf = 0;
		return 0;
	}

	u32 i = 0;
	while (inBuf[i]) {
		if (core::isspace(inBuf[i]) || &(inBuf[i]) == bufEnd)
			break;
		++i;
	}

	u32 length = core::min_(i, outBufLength - 1);
	for (u32 j = 0; j < length; ++j)
		outBuf[j] = inBuf[j];

	outBuf[length] = 0;
	return length;
}

core::stringc COBJMeshFileLoader::copyLine(const c8 *inBuf, const c8 *bufEnd)
{
	if (!inBuf)
		return core::stringc();

	const c8 *ptr = inBuf;
	while (ptr < bufEnd) {
		if (*ptr == '\n' || *ptr == '\r')
			break;
		++ptr;
	}
	// we must avoid the +1 in case the array is used up
	return core::stringc(inBuf, (u32)(ptr - inBuf + ((ptr < bufEnd) ? 1 : 0)));
}

const c8 *COBJMeshFileLoader::goAndCopyNextWord(c8 *outBuf, const c8 *inBuf, u32 outBufLength, const c8 *bufEnd)
{
	inBuf = goNextWord(inBuf, bufEnd, false);
	copyWord(outBuf, inBuf, outBufLength, bufEnd);
	return inBuf;
}

bool COBJMeshFileLoader::retrieveVertexIndices(c8 *vertexData, s32 *idx, const c8 *bufEnd, u32 vbsize, u32 vtsize, u32 vnsize)
{
	const u32 BUFFER_LENGTH = 16;
	c8 word[BUFFER_LENGTH];
	const c8 *p = goFirstWord(vertexData, bufEnd);
	u32 idxType = 0; // 0 = posIdx, 1 = texcoordIdx, 2 = normalIdx

	u32 i = 0;
	while (p != bufEnd) {
		if (i >= BUFFER_LENGTH) {
			return false;
		}
		if ((core::isdigit(*p)) || (*p == '-')) {
			// build up the number
			word[i++] = *p;
		} else if (*p == '/' || *p == ' ' || *p == '\0') {
			// number is completed. Convert and store it
			word[i] = '\0';
			// if no number was found index will become 0 and later on -1 by decrement
			idx[idxType] = core::strtol10(word);
			if (idx[idxType] < 0) {
				switch (idxType) {
				case 0:
					idx[idxType] += vbsize;
					break;
				case 1:
					idx[idxType] += vtsize;
					break;
				case 2:
					idx[idxType] += vnsize;
					break;
				}
			} else
				idx[idxType] -= 1;

			// reset the word
			word[0] = '\0';
			i = 0;

			// go to the next kind of index type
			if (*p == '/') {
				if (++idxType > 2) {
					// error checking, shouldn't reach here unless file is wrong
					idxType = 0;
				}
			} else {
				// set all missing values to disable (=-1)
				while (++idxType < 3)
					idx[idxType] = -1;
				++p;
				break; // while
			}
		}

		// go to the next char
		++p;
	}

	return true;
}

void COBJMeshFileLoader::cleanUp()
{
	for (u32 i = 0; i < Materials.size(); ++i) {
		Materials[i]->Meshbuffer->drop();
		delete Materials[i];
	}

	Materials.clear();
}

} // end namespace scene
