// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

namespace scene
{

//! An enumeration for all types of debug data for built-in scene nodes (flags)
enum E_DEBUG_SCENE_TYPE
{
	//! No Debug Data ( Default )
	EDS_OFF = 0,

	//! Show Bounding Boxes of SceneNode
	EDS_BBOX = 1,

	//! Show Vertex Normals
	EDS_NORMALS = 2,

	//! Shows Skeleton/Tags
	EDS_SKELETON = 4,

	//! Overlays Mesh Wireframe
	EDS_MESH_WIRE_OVERLAY = 8,

	//! Show Bounding Boxes of all MeshBuffers
	EDS_BBOX_BUFFERS = 32,

	//! EDS_BBOX | EDS_BBOX_BUFFERS
	EDS_BBOX_ALL = EDS_BBOX | EDS_BBOX_BUFFERS,

	//! Show all debug infos
	EDS_FULL = 0xffff
};

} // end namespace scene
