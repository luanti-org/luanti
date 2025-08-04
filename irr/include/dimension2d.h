// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include "irrMath.h" // for core::equals()

namespace core
{
template <class T>
class vector2d;

//! Specifies a 2 dimensional size.
template <class T>
class dimension2d
{
public:
	//! Default constructor for empty dimension
	constexpr dimension2d() :
			Width(0), Height(0) {}
	//! Constructor with width and height
	constexpr dimension2d(const T &width, const T &height) :
			Width(width), Height(height) {}

	dimension2d(const vector2d<T> &other); // Defined in vector2d.h

	//! Use this constructor only where you are sure that the conversion is valid.
	template <class U>
	explicit constexpr dimension2d(const dimension2d<U> &other) :
			Width((T)other.Width), Height((T)other.Height)
	{
	}

	template <class U>
	dimension2d<T> &operator=(const dimension2d<U> &other)
	{
		Width = (T)other.Width;
		Height = (T)other.Height;
		return *this;
	}

	//! Equality operator
	bool operator==(const dimension2d<T> &other) const
	{
		return core::equals(Width, other.Width) &&
			   core::equals(Height, other.Height);
	}

	//! Inequality operator
	bool operator!=(const dimension2d<T> &other) const
	{
		return !(*this == other);
	}

	bool operator==(const vector2d<T> &other) const; // Defined in vector2d.h

	bool operator!=(const vector2d<T> &other) const
	{
		return !(*this == other);
	}

	//! Set to new values
	dimension2d<T> &set(const T &width, const T &height)
	{
		Width = width;
		Height = height;
		return *this;
	}

	//! Divide width and height by scalar
	dimension2d<T> &operator/=(const T &scale)
	{
		Width /= scale;
		Height /= scale;
		return *this;
	}

	//! Divide width and height by scalar
	dimension2d<T> operator/(const T &scale) const
	{
		return dimension2d<T>(Width / scale, Height / scale);
	}

	//! Multiply width and height by scalar
	dimension2d<T> &operator*=(const T &scale)
	{
		Width *= scale;
		Height *= scale;
		return *this;
	}

	//! Multiply width and height by scalar
	dimension2d<T> operator*(const T &scale) const
	{
		return dimension2d<T>(Width * scale, Height * scale);
	}

	//! Add another dimension to this one.
	dimension2d<T> &operator+=(const dimension2d<T> &other)
	{
		Width += other.Width;
		Height += other.Height;
		return *this;
	}

	//! Add two dimensions
	dimension2d<T> operator+(const dimension2d<T> &other) const
	{
		return dimension2d<T>(Width + other.Width, Height + other.Height);
	}

	//! Subtract a dimension from this one
	dimension2d<T> &operator-=(const dimension2d<T> &other)
	{
		Width -= other.Width;
		Height -= other.Height;
		return *this;
	}

	//! Subtract one dimension from another
	dimension2d<T> operator-(const dimension2d<T> &other) const
	{
		return dimension2d<T>(Width - other.Width, Height - other.Height);
	}

	//! Get area
	T getArea() const
	{
		return Width * Height;
	}

	//! Get the optimal size according to some properties
	/** This is a function often used for texture dimension
	calculations. The function returns the next larger or
	smaller dimension which is a power-of-two dimension
	(2^n,2^m) and/or square (Width=Height).
	\param requirePowerOfTwo Forces the result to use only
	powers of two as values.
	\param requireSquare Makes width==height in the result
	\param larger Choose whether the result is larger or
	smaller than the current dimension. If one dimension
	need not be changed it is kept with any value of larger.
	\param maxValue Maximum texturesize. if value > 0 size is
	clamped to maxValue
	\return The optimal dimension under the given
	constraints. */
	dimension2d<T> getOptimalSize(
			bool requirePowerOfTwo = true,
			bool requireSquare = false,
			bool larger = true,
			u32 maxValue = 0) const
	{
		u32 i = 1;
		u32 j = 1;
		if (requirePowerOfTwo) {
			while (i < (u32)Width)
				i <<= 1;
			if (!larger && i != 1 && i != (u32)Width)
				i >>= 1;
			while (j < (u32)Height)
				j <<= 1;
			if (!larger && j != 1 && j != (u32)Height)
				j >>= 1;
		} else {
			i = (u32)Width;
			j = (u32)Height;
		}

		if (requireSquare) {
			if ((larger && (i > j)) || (!larger && (i < j)))
				j = i;
			else
				i = j;
		}

		if (maxValue > 0 && i > maxValue)
			i = maxValue;

		if (maxValue > 0 && j > maxValue)
			j = maxValue;

		return dimension2d<T>((T)i, (T)j);
	}

	//! Get the interpolated dimension
	/** \param other Other dimension to interpolate with.
	\param d Value between 0.0f and 1.0f. d=0 returns other, d=1 returns this, values between interpolate.
	\return Interpolated dimension. */
	dimension2d<T> getInterpolated(const dimension2d<T> &other, f32 d) const
	{
		f32 inv = (1.0f - d);
		return dimension2d<T>((T)(other.Width * inv + Width * d), (T)(other.Height * inv + Height * d));
	}

	//! Width of the dimension.
	T Width;
	//! Height of the dimension.
	T Height;
};

//! Typedef for an f32 dimension.
typedef dimension2d<f32> dimension2df;
//! Typedef for an unsigned integer dimension.
typedef dimension2d<u32> dimension2du;

//! Typedef for an integer dimension.
/** There are few cases where negative dimensions make sense. Please consider using
	dimension2du instead. */
typedef dimension2d<s32> dimension2di;

} // end namespace core
