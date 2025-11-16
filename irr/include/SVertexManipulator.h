// Copyright (C) 2009-2012 Christian Stehno
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "vector3d.h"

namespace scene
{


//! Interface for vertex manipulators.
/** You should derive your manipulator from this class if it shall be called for every vertex, getting as parameter just the vertex.
 */
struct IVertexManipulator
{
};

//! Vertex manipulator which scales the position of the vertex
class SVertexPositionScaleManipulator : public IVertexManipulator
{
public:
	SVertexPositionScaleManipulator(const core::vector3df &factor) :
			Factor(factor) {}
	template <typename VType>
	void operator()(VType &vertex) const
	{
		vertex.Pos *= Factor;
	}

private:
	core::vector3df Factor;
};

} // end namespace scene
