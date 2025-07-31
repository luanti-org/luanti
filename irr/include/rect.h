// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include "dimension2d.h"
#include "position2d.h"

namespace core
{

//! Rectangle template.
/** Mostly used by 2D GUI elements and for 2D drawing methods.
It has 2 positions instead of position and dimension and a fast
method for collision detection with other rectangles and points.

Coordinates are (0,0) for top-left corner, and increasing to the right
and to the bottom.
*/
template <class T>
class rect
{
public:
	//! Default constructor creating empty rectangle at (0,0)
	constexpr rect() :
			UpperLeftCorner(0, 0), LowerRightCorner(0, 0) {}

	//! Constructor with two corners
	constexpr rect(T x, T y, T x2, T y2) :
			UpperLeftCorner(x, y), LowerRightCorner(x2, y2) {}

	//! Constructor with two corners
	constexpr rect(const position2d<T> &upperLeft, const position2d<T> &lowerRight) :
			UpperLeftCorner(upperLeft), LowerRightCorner(lowerRight) {}

	//! Constructor with upper left corner and dimension
	template <class U>
	constexpr rect(const position2d<T> &pos, const dimension2d<U> &size) :
			UpperLeftCorner(pos), LowerRightCorner(pos.X + size.Width, pos.Y + size.Height)
	{
	}

	//! Constructor with upper left at 0,0 and lower right using dimension
	template <class U>
	explicit constexpr rect(const dimension2d<U> &size) :
			UpperLeftCorner(0, 0), LowerRightCorner(size.Width, size.Height)
	{
	}

	//! move right by given numbers
	rect<T> operator+(const position2d<T> &pos) const
	{
		rect<T> ret(*this);
		return ret += pos;
	}

	//! move right by given numbers
	rect<T> &operator+=(const position2d<T> &pos)
	{
		UpperLeftCorner += pos;
		LowerRightCorner += pos;
		return *this;
	}

	//! move left by given numbers
	rect<T> operator-(const position2d<T> &pos) const
	{
		rect<T> ret(*this);
		return ret -= pos;
	}

	//! move left by given numbers
	rect<T> &operator-=(const position2d<T> &pos)
	{
		UpperLeftCorner -= pos;
		LowerRightCorner -= pos;
		return *this;
	}

	//! equality operator
	constexpr bool operator==(const rect<T> &other) const
	{
		return (UpperLeftCorner == other.UpperLeftCorner &&
				LowerRightCorner == other.LowerRightCorner);
	}

	//! inequality operator
	constexpr bool operator!=(const rect<T> &other) const
	{
		return (UpperLeftCorner != other.UpperLeftCorner ||
				LowerRightCorner != other.LowerRightCorner);
	}

	//! compares size of rectangles
	bool operator<(const rect<T> &other) const
	{
		return getArea() < other.getArea();
	}

	//! Returns size of rectangle
	T getArea() const
	{
		return getWidth() * getHeight();
	}

	//! Returns if a 2d point is within this rectangle.
	/** \param pos Position to test if it lies within this rectangle.
	\return True if the position is within the rectangle, false if not. */
	bool isPointInside(const position2d<T> &pos) const
	{
		return (UpperLeftCorner.X <= pos.X &&
				UpperLeftCorner.Y <= pos.Y &&
				LowerRightCorner.X >= pos.X &&
				LowerRightCorner.Y >= pos.Y);
	}

	//! Check if the rectangle collides with another rectangle.
	/** \param other Rectangle to test collision with
	\return True if the rectangles collide. */
	bool isRectCollided(const rect<T> &other) const
	{
		return (LowerRightCorner.Y > other.UpperLeftCorner.Y &&
				UpperLeftCorner.Y < other.LowerRightCorner.Y &&
				LowerRightCorner.X > other.UpperLeftCorner.X &&
				UpperLeftCorner.X < other.LowerRightCorner.X);
	}

	//! Clips this rectangle with another one.
	/** \param other Rectangle to clip with */
	void clipAgainst(const rect<T> &other)
	{
		if (other.LowerRightCorner.X < LowerRightCorner.X)
			LowerRightCorner.X = other.LowerRightCorner.X;
		if (other.LowerRightCorner.Y < LowerRightCorner.Y)
			LowerRightCorner.Y = other.LowerRightCorner.Y;

		if (other.UpperLeftCorner.X > LowerRightCorner.X)
			LowerRightCorner.X = other.UpperLeftCorner.X;
		if (other.UpperLeftCorner.Y > LowerRightCorner.Y)
			LowerRightCorner.Y = other.UpperLeftCorner.Y;

		if (other.LowerRightCorner.X < UpperLeftCorner.X)
			UpperLeftCorner.X = other.LowerRightCorner.X;
		if (other.LowerRightCorner.Y < UpperLeftCorner.Y)
			UpperLeftCorner.Y = other.LowerRightCorner.Y;

		if (other.UpperLeftCorner.X > UpperLeftCorner.X)
			UpperLeftCorner.X = other.UpperLeftCorner.X;
		if (other.UpperLeftCorner.Y > UpperLeftCorner.Y)
			UpperLeftCorner.Y = other.UpperLeftCorner.Y;
	}

	//! Moves this rectangle to fit inside another one.
	/** \return True on success, false if not possible */
	bool constrainTo(const rect<T> &other)
	{
		if (other.getWidth() < getWidth() || other.getHeight() < getHeight())
			return false;

		T diff = other.LowerRightCorner.X - LowerRightCorner.X;
		if (diff < 0) {
			LowerRightCorner.X += diff;
			UpperLeftCorner.X += diff;
		}

		diff = other.LowerRightCorner.Y - LowerRightCorner.Y;
		if (diff < 0) {
			LowerRightCorner.Y += diff;
			UpperLeftCorner.Y += diff;
		}

		diff = UpperLeftCorner.X - other.UpperLeftCorner.X;
		if (diff < 0) {
			UpperLeftCorner.X -= diff;
			LowerRightCorner.X -= diff;
		}

		diff = UpperLeftCorner.Y - other.UpperLeftCorner.Y;
		if (diff < 0) {
			UpperLeftCorner.Y -= diff;
			LowerRightCorner.Y -= diff;
		}

		return true;
	}

	//! Get width of rectangle.
	T getWidth() const
	{
		return LowerRightCorner.X - UpperLeftCorner.X;
	}

	//! Get height of rectangle.
	T getHeight() const
	{
		return LowerRightCorner.Y - UpperLeftCorner.Y;
	}

	//! If the lower right corner of the rect is smaller then the upper left, the points are swapped.
	void repair()
	{
		if (LowerRightCorner.X < UpperLeftCorner.X) {
			T t = LowerRightCorner.X;
			LowerRightCorner.X = UpperLeftCorner.X;
			UpperLeftCorner.X = t;
		}

		if (LowerRightCorner.Y < UpperLeftCorner.Y) {
			T t = LowerRightCorner.Y;
			LowerRightCorner.Y = UpperLeftCorner.Y;
			UpperLeftCorner.Y = t;
		}
	}

	//! Returns if the rect is valid to draw.
	/** It would be invalid if the UpperLeftCorner is lower or more
	right than the LowerRightCorner. */
	bool isValid() const
	{
		return ((LowerRightCorner.X >= UpperLeftCorner.X) &&
				(LowerRightCorner.Y >= UpperLeftCorner.Y));
	}

	//! Get the center of the rectangle
	position2d<T> getCenter() const
	{
		return position2d<T>(
				(UpperLeftCorner.X + LowerRightCorner.X) / 2,
				(UpperLeftCorner.Y + LowerRightCorner.Y) / 2);
	}

	//! Get the dimensions of the rectangle
	dimension2d<T> getSize() const
	{
		return dimension2d<T>(getWidth(), getHeight());
	}

	//! Adds a point to the rectangle
	/** Causes the rectangle to grow bigger if point is outside of
	the box
	\param p Point to add to the box. */
	void addInternalPoint(const position2d<T> &p)
	{
		addInternalPoint(p.X, p.Y);
	}

	//! Adds a point to the bounding rectangle
	/** Causes the rectangle to grow bigger if point is outside of
	the box
	\param x X-Coordinate of the point to add to this box.
	\param y Y-Coordinate of the point to add to this box. */
	void addInternalPoint(T x, T y)
	{
		if (x > LowerRightCorner.X)
			LowerRightCorner.X = x;
		if (y > LowerRightCorner.Y)
			LowerRightCorner.Y = y;

		if (x < UpperLeftCorner.X)
			UpperLeftCorner.X = x;
		if (y < UpperLeftCorner.Y)
			UpperLeftCorner.Y = y;
	}

	//! Upper left corner
	position2d<T> UpperLeftCorner;
	//! Lower right corner
	position2d<T> LowerRightCorner;
};

//! Rectangle with float values
typedef rect<f32> rectf;
//! Rectangle with int values
typedef rect<s32> recti;

} // end namespace core
