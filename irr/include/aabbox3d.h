// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrMath.h"
#include "plane3d.h"
#include "line3d.h"

namespace core
{

//! Axis aligned bounding box in 3d dimensional space.
/** Has some useful methods used with occlusion culling or clipping.
 */
template <class T>
class aabbox3d
{
public:
	constexpr aabbox3d() = delete;
	//! Constructor with min edge and max edge.
	constexpr aabbox3d(const vector3d<T> &min, const vector3d<T> &max) :
			MinEdge(min), MaxEdge(max) {}
	//! Constructor with only one point.
	constexpr aabbox3d(const vector3d<T> &init) :
			MinEdge(init), MaxEdge(init) {}
	//! Constructor with min edge and max edge as single values, not vectors.
	constexpr aabbox3d(T minx, T miny, T minz, T maxx, T maxy, T maxz) :
			MinEdge(minx, miny, minz), MaxEdge(maxx, maxy, maxz) {}

	// operators
	//! Equality operator
	/** \param other box to compare with.
	\return True if both boxes are equal, else false. */
	constexpr inline bool operator==(const aabbox3d<T> &other) const
	{
		return (MinEdge == other.MinEdge && other.MaxEdge == MaxEdge);
	}
	//! Inequality operator
	/** \param other box to compare with.
	\return True if both boxes are different, else false. */
	constexpr inline bool operator!=(const aabbox3d<T> &other) const
	{
		return !(MinEdge == other.MinEdge && other.MaxEdge == MaxEdge);
	}

	// functions

	//! Resets the bounding box to a one-point box.
	/** \param x X coord of the point.
	\param y Y coord of the point.
	\param z Z coord of the point. */
	void reset(T x, T y, T z)
	{
		MaxEdge.set(x, y, z);
		MinEdge = MaxEdge;
	}

	//! Resets the bounding box.
	/** \param initValue New box to set this one to. */
	void reset(const aabbox3d<T> &initValue)
	{
		*this = initValue;
	}

	//! Resets the bounding box to a one-point box.
	/** \param initValue New point. */
	void reset(const vector3d<T> &initValue)
	{
		MaxEdge = initValue;
		MinEdge = initValue;
	}

	//! Adds a point to the bounding box
	/** The box grows bigger, if point was outside of the box.
	\param p: Point to add into the box. */
	void addInternalPoint(const vector3d<T> &p)
	{
		addInternalPoint(p.X, p.Y, p.Z);
	}

	//! Adds another bounding box
	/** The box grows bigger, if the new box was outside of the box.
	\param b: Other bounding box to add into this box. */
	void addInternalBox(const aabbox3d<T> &b)
	{
		addInternalPoint(b.MaxEdge);
		addInternalPoint(b.MinEdge);
	}

	//! Adds a point to the bounding box
	/** The box grows bigger, if point is outside of the box.
	\param x X coordinate of the point to add to this box.
	\param y Y coordinate of the point to add to this box.
	\param z Z coordinate of the point to add to this box. */
	void addInternalPoint(T x, T y, T z)
	{
		if (x > MaxEdge.X)
			MaxEdge.X = x;
		if (y > MaxEdge.Y)
			MaxEdge.Y = y;
		if (z > MaxEdge.Z)
			MaxEdge.Z = z;

		if (x < MinEdge.X)
			MinEdge.X = x;
		if (y < MinEdge.Y)
			MinEdge.Y = y;
		if (z < MinEdge.Z)
			MinEdge.Z = z;
	}

	//! Get center of the bounding box
	/** \return Center of the bounding box. */
	vector3d<T> getCenter() const
	{
		return (MinEdge + MaxEdge) / 2;
	}

	//! Get extent of the box (maximal distance of two points in the box)
	/** \return Extent of the bounding box. */
	vector3d<T> getExtent() const
	{
		return MaxEdge - MinEdge;
	}

	//! Get radius of the bounding sphere
	/** \return Radius of the bounding sphere. */
	T getRadius() const
	{
		const T radius = getExtent().getLength() / 2;
		return radius;
	}

	//! Check if the box is empty.
	/** This means that there is no space between the min and max edge.
	\return True if box is empty, else false. */
	bool isEmpty() const
	{
		return MinEdge.equals(MaxEdge);
	}

	//! Get the volume enclosed by the box in cubed units
	T getVolume() const
	{
		const vector3d<T> e = getExtent();
		return e.X * e.Y * e.Z;
	}

	//! Get the surface area of the box in squared units
	T getArea() const
	{
		const vector3d<T> e = getExtent();
		return 2 * (e.X * e.Y + e.X * e.Z + e.Y * e.Z);
	}

	//! Stores all 8 edges of the box into an array
	/** \param edges: Pointer to array of 8 edges. */
	void getEdges(vector3d<T> *edges) const
	{
		const core::vector3d<T> middle = getCenter();
		const core::vector3d<T> diag = middle - MaxEdge;

		/*
		Edges are stored in this way:
		Hey, am I an ascii artist, or what? :) niko.
			   /3--------/7
			  / |       / |
			 /  |      /  |
			1---------5   |
			|  /2- - -|- -6
			| /       |  /
			|/        | /
			0---------4/
		*/

		edges[0].set(middle.X + diag.X, middle.Y + diag.Y, middle.Z + diag.Z);
		edges[1].set(middle.X + diag.X, middle.Y - diag.Y, middle.Z + diag.Z);
		edges[2].set(middle.X + diag.X, middle.Y + diag.Y, middle.Z - diag.Z);
		edges[3].set(middle.X + diag.X, middle.Y - diag.Y, middle.Z - diag.Z);
		edges[4].set(middle.X - diag.X, middle.Y + diag.Y, middle.Z + diag.Z);
		edges[5].set(middle.X - diag.X, middle.Y - diag.Y, middle.Z + diag.Z);
		edges[6].set(middle.X - diag.X, middle.Y + diag.Y, middle.Z - diag.Z);
		edges[7].set(middle.X - diag.X, middle.Y - diag.Y, middle.Z - diag.Z);
	}

	//! Repairs the box.
	/** Necessary if for example MinEdge and MaxEdge are swapped. */
	void repair()
	{
		T t;

		if (MinEdge.X > MaxEdge.X) {
			t = MinEdge.X;
			MinEdge.X = MaxEdge.X;
			MaxEdge.X = t;
		}
		if (MinEdge.Y > MaxEdge.Y) {
			t = MinEdge.Y;
			MinEdge.Y = MaxEdge.Y;
			MaxEdge.Y = t;
		}
		if (MinEdge.Z > MaxEdge.Z) {
			t = MinEdge.Z;
			MinEdge.Z = MaxEdge.Z;
			MaxEdge.Z = t;
		}
	}

	// Check if MaxEdge > MinEdge
	bool isValid() const
	{
		if (MinEdge.X > MaxEdge.X)
			return false;
		if (MinEdge.Y > MaxEdge.Y)
			return false;
		if (MinEdge.Z > MaxEdge.Z)
			return false;

		return true;
	}

	//! Calculates a new interpolated bounding box.
	/** d=0 returns other, d=1 returns this, all other values blend between
	the two boxes.
	\param other Other box to interpolate between
	\param d Value between 0.0f and 1.0f.
	\return Interpolated box. */
	aabbox3d<T> getInterpolated(const aabbox3d<T> &other, f32 d) const
	{
		f32 inv = 1.0f - d;
		return aabbox3d<T>((other.MinEdge * inv) + (MinEdge * d),
				(other.MaxEdge * inv) + (MaxEdge * d));
	}

	//! Determines if a point is within this box.
	/** Border is included (IS part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not */
	bool isPointInside(const vector3d<T> &p) const
	{
		return (p.X >= MinEdge.X && p.X <= MaxEdge.X &&
				p.Y >= MinEdge.Y && p.Y <= MaxEdge.Y &&
				p.Z >= MinEdge.Z && p.Z <= MaxEdge.Z);
	}

	//! Determines if a point is within this box and not its borders.
	/** Border is excluded (NOT part of the box)!
	\param p: Point to check.
	\return True if the point is within the box and false if not. */
	bool isPointTotalInside(const vector3d<T> &p) const
	{
		return (p.X > MinEdge.X && p.X < MaxEdge.X &&
				p.Y > MinEdge.Y && p.Y < MaxEdge.Y &&
				p.Z > MinEdge.Z && p.Z < MaxEdge.Z);
	}

	//! Check if this box is completely inside the 'other' box.
	/** \param other: Other box to check against.
	\return True if this box is completely inside the other box,
	otherwise false. */
	bool isFullInside(const aabbox3d<T> &other) const
	{
		return (MinEdge.X >= other.MinEdge.X && MinEdge.Y >= other.MinEdge.Y && MinEdge.Z >= other.MinEdge.Z &&
				MaxEdge.X <= other.MaxEdge.X && MaxEdge.Y <= other.MaxEdge.Y && MaxEdge.Z <= other.MaxEdge.Z);
	}

	//! Returns the intersection of this box with another, if possible.
	aabbox3d<T> intersect(const aabbox3d<T> &other) const
	{
		aabbox3d<T> out;

		if (!intersectsWithBox(other))
			return out;

		out.MaxEdge.X = min_(MaxEdge.X, other.MaxEdge.X);
		out.MaxEdge.Y = min_(MaxEdge.Y, other.MaxEdge.Y);
		out.MaxEdge.Z = min_(MaxEdge.Z, other.MaxEdge.Z);

		out.MinEdge.X = max_(MinEdge.X, other.MinEdge.X);
		out.MinEdge.Y = max_(MinEdge.Y, other.MinEdge.Y);
		out.MinEdge.Z = max_(MinEdge.Z, other.MinEdge.Z);

		return out;
	}

	//! Determines if the axis-aligned box intersects with another axis-aligned box.
	/** \param other: Other box to check a intersection with.
	\return True if there is an intersection with the other box,
	otherwise false. */
	bool intersectsWithBox(const aabbox3d<T> &other) const
	{
		return (MinEdge.X <= other.MaxEdge.X && MinEdge.Y <= other.MaxEdge.Y && MinEdge.Z <= other.MaxEdge.Z &&
				MaxEdge.X >= other.MinEdge.X && MaxEdge.Y >= other.MinEdge.Y && MaxEdge.Z >= other.MinEdge.Z);
	}

	//! Tests if the box intersects with a line
	/** \param line: Line to test intersection with.
	\return True if there is an intersection , else false. */
	bool intersectsWithLine(const line3d<T> &line) const
	{
		return intersectsWithLine(line.getMiddle(), line.getVector().normalize(),
				(T)(line.getLength() * 0.5));
	}

	//! Tests if the box intersects with a line
	/** \param linemiddle Center of the line.
	\param linevect Vector of the line.
	\param halflength Half length of the line.
	\return True if there is an intersection, else false. */
	bool intersectsWithLine(const vector3d<T> &linemiddle,
			const vector3d<T> &linevect, T halflength) const
	{
		const vector3d<T> e = getExtent() * (T)0.5;
		const vector3d<T> t = getCenter() - linemiddle;

		if ((fabs(t.X) > e.X + halflength * fabs(linevect.X)) ||
				(fabs(t.Y) > e.Y + halflength * fabs(linevect.Y)) ||
				(fabs(t.Z) > e.Z + halflength * fabs(linevect.Z)))
			return false;

		T r = e.Y * (T)fabs(linevect.Z) + e.Z * (T)fabs(linevect.Y);
		if (fabs(t.Y * linevect.Z - t.Z * linevect.Y) > r)
			return false;

		r = e.X * (T)fabs(linevect.Z) + e.Z * (T)fabs(linevect.X);
		if (fabs(t.Z * linevect.X - t.X * linevect.Z) > r)
			return false;

		r = e.X * (T)fabs(linevect.Y) + e.Y * (T)fabs(linevect.X);
		if (fabs(t.X * linevect.Y - t.Y * linevect.X) > r)
			return false;

		return true;
	}

	//! Classifies a relation with a plane.
	/** \param plane Plane to classify relation to.
	\return Returns ISREL3D_FRONT if the box is in front of the plane,
	ISREL3D_BACK if the box is behind the plane, and
	ISREL3D_CLIPPED if it is on both sides of the plane. */
	EIntersectionRelation3D classifyPlaneRelation(const plane3d<T> &plane) const
	{
		vector3d<T> nearPoint(MaxEdge);
		vector3d<T> farPoint(MinEdge);

		if (plane.Normal.X > (T)0) {
			nearPoint.X = MinEdge.X;
			farPoint.X = MaxEdge.X;
		}

		if (plane.Normal.Y > (T)0) {
			nearPoint.Y = MinEdge.Y;
			farPoint.Y = MaxEdge.Y;
		}

		if (plane.Normal.Z > (T)0) {
			nearPoint.Z = MinEdge.Z;
			farPoint.Z = MaxEdge.Z;
		}

		if (plane.Normal.dotProduct(nearPoint) + plane.D > (T)0)
			return ISREL3D_FRONT;

		if (plane.Normal.dotProduct(farPoint) + plane.D > (T)0)
			return ISREL3D_CLIPPED;

		return ISREL3D_BACK;
	}

	//! The near edge
	vector3d<T> MinEdge;

	//! The far edge
	vector3d<T> MaxEdge;
};

//! Typedef for a f32 3d bounding box.
typedef aabbox3d<f32> aabbox3df;
//! Typedef for an integer 3d bounding box.
typedef aabbox3d<s32> aabbox3di;

} // end namespace core
