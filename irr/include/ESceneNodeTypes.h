// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"

namespace scene
{

//! An enumeration for all types of built-in scene nodes
/** A scene node type is represented by a four character code
such as 'cube' or 'mesh' instead of simple numbers, to avoid
name clashes with external scene nodes.*/
enum ESCENE_NODE_TYPE
{
	//! of type CSceneManager (note that ISceneManager is not(!) an ISceneNode)
	ESNT_SCENE_MANAGER = MAKE_IRR_ID('s', 'm', 'n', 'g'),

	//! Mesh Scene Node
	ESNT_MESH = MAKE_IRR_ID('m', 'e', 's', 'h'),

	//! Empty Scene Node
	ESNT_EMPTY = MAKE_IRR_ID('e', 'm', 't', 'y'),

	//! Dummy Transformation Scene Node
	ESNT_DUMMY_TRANSFORMATION = MAKE_IRR_ID('d', 'm', 'm', 'y'),

	//! Camera Scene Node
	ESNT_CAMERA = MAKE_IRR_ID('c', 'a', 'm', '_'),

	//! Billboard Scene Node
	ESNT_BILLBOARD = MAKE_IRR_ID('b', 'i', 'l', 'l'),

	//! Animated Mesh Scene Node
	ESNT_ANIMATED_MESH = MAKE_IRR_ID('a', 'm', 's', 'h'),

	//! Unknown scene node
	ESNT_UNKNOWN = MAKE_IRR_ID('u', 'n', 'k', 'n'),

	//! Will match with any scene node when checking types
	ESNT_ANY = MAKE_IRR_ID('a', 'n', 'y', '_')
};

} // end namespace scene
