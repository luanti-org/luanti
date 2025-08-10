// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

namespace scene
{

//! Enumeration for all primitive types there are.
enum E_PRIMITIVE_TYPE
{
	//! All vertices are non-connected points.
	EPT_POINTS = 0,

	//! All vertices form a single connected line.
	EPT_LINE_STRIP,

	//! Just as LINE_STRIP, but the last and the first vertex is also connected.
	EPT_LINE_LOOP,

	//! Every two vertices are connected creating n/2 lines.
	EPT_LINES,

	//! After the first two vertices each vertex defines a new triangle.
	//! Always the two last and the new one form a new triangle.
	EPT_TRIANGLE_STRIP,

	//! After the first two vertices each vertex defines a new triangle.
	//! All around the common first vertex.
	EPT_TRIANGLE_FAN,

	//! Explicitly set all vertices for each triangle.
	EPT_TRIANGLES,

	//! The single vertices are expanded to quad billboards on the GPU.
	EPT_POINT_SPRITES
};

} // end namespace scene
