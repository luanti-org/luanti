// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "vector3d.h"
#include "vector2d.h"
#include "SColor.h"

namespace video
{

//! Enumeration for all vertex types there are.
enum E_VERTEX_TYPE
{
	//! Standard vertex type used by the Irrlicht engine, video::S3DVertex.
	EVT_STANDARD = 0,

	//! Vertex with two texture coordinates, video::S3DVertex2TCoords.
	/** Usually used for geometry with lightmaps or other special materials. */
	EVT_2TCOORDS,

	//! Vertex with a tangent and binormal vector, video::S3DVertexTangents.
	/** Usually used for tangent space normal mapping.
		Usually tangent and binormal get send to shaders as texture coordinate sets 1 and 2.
	*/
	EVT_TANGENTS
};

//! Array holding the built in vertex type names
const char *const sBuiltInVertexTypeNames[] = {
		"standard",
		"2tcoords",
		"tangents",
		0,
	};

//! standard vertex used by the Irrlicht engine.
struct S3DVertex
{
	//! default constructor
	constexpr S3DVertex() :
			Color(0xffffffff) {}

	//! constructor
	constexpr S3DVertex(f32 x, f32 y, f32 z, f32 nx, f32 ny, f32 nz, SColor c, f32 tu, f32 tv) :
			Pos(x, y, z), Normal(nx, ny, nz), Color(c), TCoords(tu, tv) {}

	//! constructor
	constexpr S3DVertex(const core::vector3df &pos, const core::vector3df &normal,
			SColor color, const core::vector2df &tcoords) :
			Pos(pos),
			Normal(normal), Color(color), TCoords(tcoords) {}

	//! Position
	core::vector3df Pos;

	//! Normal vector
	core::vector3df Normal;

	//! Color
	SColor Color;

	//! Texture coordinates
	core::vector2df TCoords;

	constexpr bool operator==(const S3DVertex &other) const
	{
		return ((Pos == other.Pos) && (Normal == other.Normal) &&
				(Color == other.Color) && (TCoords == other.TCoords));
	}

	constexpr bool operator!=(const S3DVertex &other) const
	{
		return ((Pos != other.Pos) || (Normal != other.Normal) ||
				(Color != other.Color) || (TCoords != other.TCoords));
	}

	constexpr bool operator<(const S3DVertex &other) const
	{
		return ((Pos < other.Pos) ||
				((Pos == other.Pos) && (Normal < other.Normal)) ||
				((Pos == other.Pos) && (Normal == other.Normal) && (Color < other.Color)) ||
				((Pos == other.Pos) && (Normal == other.Normal) && (Color == other.Color) && (TCoords < other.TCoords)));
	}

	//! Get type of the class
	static E_VERTEX_TYPE getType()
	{
		return EVT_STANDARD;
	}

	//\param d d=0 returns other, d=1 returns this, values between interpolate.
	S3DVertex getInterpolated(const S3DVertex &other, f32 d)
	{
		d = core::clamp(d, 0.0f, 1.0f);
		return S3DVertex(Pos.getInterpolated(other.Pos, d),
				Normal.getInterpolated(other.Normal, d),
				Color.getInterpolated(other.Color, d),
				TCoords.getInterpolated(other.TCoords, d));
	}
};

//! Vertex with two texture coordinates.
/** Usually used for geometry with lightmaps
or other special materials.
*/
struct S3DVertex2TCoords : public S3DVertex
{
	//! default constructor
	constexpr S3DVertex2TCoords() :
			S3DVertex() {}

	//! constructor with two different texture coords, but no normal
	constexpr S3DVertex2TCoords(f32 x, f32 y, f32 z, SColor c, f32 tu, f32 tv, f32 tu2, f32 tv2) :
			S3DVertex(x, y, z, 0.0f, 0.0f, 0.0f, c, tu, tv), TCoords2(tu2, tv2) {}

	//! constructor with two different texture coords, but no normal
	constexpr S3DVertex2TCoords(const core::vector3df &pos, SColor color,
			const core::vector2df &tcoords, const core::vector2df &tcoords2) :
			S3DVertex(pos, core::vector3df(), color, tcoords),
			TCoords2(tcoords2) {}

	//! constructor with all values
	constexpr S3DVertex2TCoords(const core::vector3df &pos, const core::vector3df &normal, const SColor &color,
			const core::vector2df &tcoords, const core::vector2df &tcoords2) :
			S3DVertex(pos, normal, color, tcoords),
			TCoords2(tcoords2) {}

	//! constructor with all values
	constexpr S3DVertex2TCoords(f32 x, f32 y, f32 z, f32 nx, f32 ny, f32 nz,
			SColor c, f32 tu, f32 tv, f32 tu2, f32 tv2) :
			S3DVertex(x, y, z, nx, ny, nz, c, tu, tv),
			TCoords2(tu2, tv2) {}

	//! constructor with the same texture coords and normal
	constexpr S3DVertex2TCoords(f32 x, f32 y, f32 z, f32 nx, f32 ny, f32 nz,
			SColor c, f32 tu, f32 tv) :
			S3DVertex(x, y, z, nx, ny, nz, c, tu, tv),
			TCoords2(tu, tv) {}

	//! constructor with the same texture coords and normal
	constexpr S3DVertex2TCoords(const core::vector3df &pos, const core::vector3df &normal,
			SColor color, const core::vector2df &tcoords) :
			S3DVertex(pos, normal, color, tcoords),
			TCoords2(tcoords) {}

	//! constructor from S3DVertex
	constexpr S3DVertex2TCoords(const S3DVertex &o) :
			S3DVertex(o) {}

	//! Second set of texture coordinates
	core::vector2df TCoords2;

	//! Equality operator
	constexpr bool operator==(const S3DVertex2TCoords &other) const
	{
		return ((static_cast<S3DVertex>(*this) == static_cast<const S3DVertex &>(other)) &&
				(TCoords2 == other.TCoords2));
	}

	//! Inequality operator
	constexpr bool operator!=(const S3DVertex2TCoords &other) const
	{
		return ((static_cast<S3DVertex>(*this) != static_cast<const S3DVertex &>(other)) ||
				(TCoords2 != other.TCoords2));
	}

	constexpr bool operator<(const S3DVertex2TCoords &other) const
	{
		return ((static_cast<S3DVertex>(*this) < other) ||
				((static_cast<S3DVertex>(*this) == static_cast<const S3DVertex &>(other)) && (TCoords2 < other.TCoords2)));
	}

	static E_VERTEX_TYPE getType()
	{
		return EVT_2TCOORDS;
	}

	//\param d d=0 returns other, d=1 returns this, values between interpolate.
	S3DVertex2TCoords getInterpolated(const S3DVertex2TCoords &other, f32 d)
	{
		d = core::clamp(d, 0.0f, 1.0f);
		return S3DVertex2TCoords(Pos.getInterpolated(other.Pos, d),
				Normal.getInterpolated(other.Normal, d),
				Color.getInterpolated(other.Color, d),
				TCoords.getInterpolated(other.TCoords, d),
				TCoords2.getInterpolated(other.TCoords2, d));
	}
};

//! Vertex with a tangent and binormal vector.
/** Usually used for tangent space normal mapping.
	Usually tangent and binormal get send to shaders as texture coordinate sets 1 and 2.
*/
struct S3DVertexTangents : public S3DVertex
{
	//! default constructor
	S3DVertexTangents() :
			S3DVertex() {}

	//! constructor
	constexpr S3DVertexTangents(f32 x, f32 y, f32 z, f32 nx = 0.0f, f32 ny = 0.0f, f32 nz = 0.0f,
			SColor c = 0xFFFFFFFF, f32 tu = 0.0f, f32 tv = 0.0f,
			f32 tanx = 0.0f, f32 tany = 0.0f, f32 tanz = 0.0f,
			f32 bx = 0.0f, f32 by = 0.0f, f32 bz = 0.0f) :
			S3DVertex(x, y, z, nx, ny, nz, c, tu, tv),
			Tangent(tanx, tany, tanz), Binormal(bx, by, bz) {}

	//! constructor
	constexpr S3DVertexTangents(const core::vector3df &pos, SColor c,
			const core::vector2df &tcoords) :
			S3DVertex(pos, core::vector3df(), c, tcoords) {}

	//! constructor
	constexpr S3DVertexTangents(const core::vector3df &pos,
			const core::vector3df &normal, SColor c,
			const core::vector2df &tcoords,
			const core::vector3df &tangent = core::vector3df(),
			const core::vector3df &binormal = core::vector3df()) :
			S3DVertex(pos, normal, c, tcoords),
			Tangent(tangent), Binormal(binormal) {}

	//! constructor from S3DVertex
	constexpr S3DVertexTangents(const S3DVertex &o) :
			S3DVertex(o) {}

	//! Tangent vector along the x-axis of the texture
	core::vector3df Tangent;

	//! Binormal vector (tangent x normal)
	core::vector3df Binormal;

	constexpr bool operator==(const S3DVertexTangents &other) const
	{
		return ((static_cast<S3DVertex>(*this) == static_cast<const S3DVertex &>(other)) &&
				(Tangent == other.Tangent) &&
				(Binormal == other.Binormal));
	}

	constexpr bool operator!=(const S3DVertexTangents &other) const
	{
		return ((static_cast<S3DVertex>(*this) != static_cast<const S3DVertex &>(other)) ||
				(Tangent != other.Tangent) ||
				(Binormal != other.Binormal));
	}

	constexpr bool operator<(const S3DVertexTangents &other) const
	{
		return ((static_cast<S3DVertex>(*this) < other) ||
				((static_cast<S3DVertex>(*this) == static_cast<const S3DVertex &>(other)) && (Tangent < other.Tangent)) ||
				((static_cast<S3DVertex>(*this) == static_cast<const S3DVertex &>(other)) && (Tangent == other.Tangent) && (Binormal < other.Binormal)));
	}

	static E_VERTEX_TYPE getType()
	{
		return EVT_TANGENTS;
	}

	S3DVertexTangents getInterpolated(const S3DVertexTangents &other, f32 d)
	{
		d = core::clamp(d, 0.0f, 1.0f);
		return S3DVertexTangents(Pos.getInterpolated(other.Pos, d),
				Normal.getInterpolated(other.Normal, d),
				Color.getInterpolated(other.Color, d),
				TCoords.getInterpolated(other.TCoords, d),
				Tangent.getInterpolated(other.Tangent, d),
				Binormal.getInterpolated(other.Binormal, d));
	}
};

inline u32 getVertexPitchFromType(E_VERTEX_TYPE vertexType)
{
	switch (vertexType) {
	case video::EVT_2TCOORDS:
		return sizeof(video::S3DVertex2TCoords);
	case video::EVT_TANGENTS:
		return sizeof(video::S3DVertexTangents);
	default:
		return sizeof(video::S3DVertex);
	}
}

} // end namespace video
