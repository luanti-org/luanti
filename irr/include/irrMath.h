// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include <cmath>
#include <cfloat>
#include <cstdlib> // for abs() etc.
#include <climits> // For INT_MAX / UINT_MAX
#include <type_traits>

namespace core
{

//! Rounding error constant often used when comparing f32 values.

constexpr f32 ROUNDING_ERROR_f32 = 0.000001f;
constexpr f64 ROUNDING_ERROR_f64 = 0.00000001;

#ifdef PI // make sure we don't collide with a define
#undef PI
#endif
//! Constant for PI.
constexpr f32 PI = static_cast<f32>(M_PI);

#ifdef PI64 // make sure we don't collide with a define
#undef PI64
#endif
//! Constant for 64bit PI.
constexpr f64 PI64 = M_PI;

//! 32bit Constant for converting from degrees to radians
constexpr f32 DEGTORAD = PI / 180.0f;

//! 32bit constant for converting from radians to degrees (formally known as GRAD_PI)
constexpr f32 RADTODEG = 180.0f / PI;

//! 64bit constant for converting from degrees to radians (formally known as GRAD_PI2)
constexpr f64 DEGTORAD64 = PI64 / 180.0;

//! 64bit constant for converting from radians to degrees
constexpr f64 RADTODEG64 = 180.0 / PI64;

//! Utility function to convert a radian value to degrees
/** Provided as it can be clearer to write radToDeg(X) than RADTODEG * X
\param radians The radians value to convert to degrees.
*/
inline constexpr f32 radToDeg(f32 radians)
{
	return RADTODEG * radians;
}

//! Utility function to convert a radian value to degrees
/** Provided as it can be clearer to write radToDeg(X) than RADTODEG * X
\param radians The radians value to convert to degrees.
*/
inline constexpr f64 radToDeg(f64 radians)
{
	return RADTODEG64 * radians;
}

//! Utility function to convert a degrees value to radians
/** Provided as it can be clearer to write degToRad(X) than DEGTORAD * X
\param degrees The degrees value to convert to radians.
*/
inline constexpr f32 degToRad(f32 degrees)
{
	return DEGTORAD * degrees;
}

//! Utility function to convert a degrees value to radians
/** Provided as it can be clearer to write degToRad(X) than DEGTORAD * X
\param degrees The degrees value to convert to radians.
*/
inline constexpr f64 degToRad(f64 degrees)
{
	return DEGTORAD64 * degrees;
}

//! returns minimum of two values.
template <class T>
inline const T &min_(const T &a, const T &b)
{
	return a < b ? a : b;
}

//! returns minimum of three values.
template <class T>
inline const T &min_(const T &a, const T &b, const T &c)
{
	return a < b ? min_(a, c) : min_(b, c);
}

//! returns maximum of two values.
template <class T>
inline const T &max_(const T &a, const T &b)
{
	return a < b ? b : a;
}

//! returns maximum of three values.
template <class T>
inline const T &max_(const T &a, const T &b, const T &c)
{
	return a < b ? max_(b, c) : max_(a, c);
}

//! returns abs of two values.
template <class T>
inline T abs_(const T &a)
{
	return std::abs(a);
}

//! returns linear interpolation of a and b with ratio t
//! \return: a if t==0, b if t==1, and the linear interpolation else
template <class T>
inline T lerp(const T &a, const T &b, const f32 t)
{
	return (T)(a * (1.f - t)) + (b * t);
}

//! clamps a value between low and high
template <class T>
inline const T clamp(const T &value, const T &low, const T &high)
{
	return min_(max_(value, low), high);
}

template <class T>
inline T roundingError();

template <>
inline f32 roundingError()
{
	return ROUNDING_ERROR_f32;
}

template <>
inline f64 roundingError()
{
	return ROUNDING_ERROR_f64;
}

template <class T>
inline T relativeErrorFactor()
{
	return 1;
}

template <>
inline f32 relativeErrorFactor()
{
	return 4;
}

template <>
inline f64 relativeErrorFactor()
{
	return 8;
}

//! returns if a equals b, for types without rounding errors
template <class T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
inline bool equals(const T a, const T b)
{
	return a == b;
}

//! returns if a equals b, taking possible rounding errors into account
template <class T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
inline bool equals(const T a, const T b, const T tolerance = roundingError<T>())
{
	return std::abs(a - b) <= tolerance;
}

//! returns if a equals b, taking relative error in form of factor
//! this particular function does not involve any division.
template <class T>
inline bool equalsRelative(const T a, const T b, const T factor = relativeErrorFactor<T>())
{
	// https://eagergames.wordpress.com/2017/04/01/fast-parallel-lines-and-vectors-test/

	const T maxi = max_(a, b);
	const T mini = min_(a, b);
	const T maxMagnitude = max_(maxi, -mini);

	return (maxMagnitude * factor + maxi) == (maxMagnitude * factor + mini); // MAD Wise
}

union FloatIntUnion32
{
	FloatIntUnion32(float f1 = 0.0f) :
			f(f1) {}
	// Portable sign-extraction
	bool sign() const { return (i >> 31) != 0; }

	s32 i;
	f32 f;
};

//! We compare the difference in ULP's (spacing between floating-point numbers, aka ULP=1 means there exists no float between).
//\result true when numbers have a ULP <= maxUlpDiff AND have the same sign.
inline bool equalsByUlp(f32 a, f32 b, int maxUlpDiff)
{
	// Based on the ideas and code from Bruce Dawson on
	// http://www.altdevblogaday.com/2012/02/22/comparing-floating-point-numbers-2012-edition/
	// When floats are interpreted as integers the two nearest possible float numbers differ just
	// by one integer number. Also works the other way round, an integer of 1 interpreted as float
	// is for example the smallest possible float number.

	const FloatIntUnion32 fa(a);
	const FloatIntUnion32 fb(b);

	// Different signs, we could maybe get difference to 0, but so close to 0 using epsilons is better.
	if (fa.sign() != fb.sign()) {
		// Check for equality to make sure +0==-0
		if (fa.i == fb.i)
			return true;
		return false;
	}

	// Find the difference in ULPs.
	const int ulpsDiff = abs_(fa.i - fb.i);
	if (ulpsDiff <= maxUlpDiff)
		return true;

	return false;
}

//! returns if a equals zero, taking rounding errors into account
inline bool iszero(const f64 a, const f64 tolerance = ROUNDING_ERROR_f64)
{
	return fabs(a) <= tolerance;
}

//! returns if a equals zero, taking rounding errors into account
inline bool iszero(const f32 a, const f32 tolerance = ROUNDING_ERROR_f32)
{
	return fabsf(a) <= tolerance;
}

//! returns if a equals not zero, taking rounding errors into account
inline bool isnotzero(const f32 a, const f32 tolerance = ROUNDING_ERROR_f32)
{
	return fabsf(a) > tolerance;
}

//! returns if a equals zero, taking rounding errors into account
inline bool iszero(const s32 a, const s32 tolerance = 0)
{
	return (a & 0x7ffffff) <= tolerance;
}

//! returns if a equals zero, taking rounding errors into account
inline bool iszero(const u32 a, const u32 tolerance = 0)
{
	return a <= tolerance;
}

//! returns if a equals zero, taking rounding errors into account
inline bool iszero(const s64 a, const s64 tolerance = 0)
{
	return abs_(a) <= tolerance;
}

inline s32 s32_min(s32 a, s32 b)
{
	return min_(a, b);
}

inline s32 s32_max(s32 a, s32 b)
{
	return max_(a, b);
}

inline s32 s32_clamp(s32 value, s32 low, s32 high)
{
	return clamp(value, low, high);
}

// integer log2 of an integer. returning 0 if denormal
inline s32 u32_log2(u32 in)
{
	s32 ret = 0;
	while (in > 1) {
		in >>= 1;
		ret++;
	}
	return ret;
}

/*
	float IEEE-754 bit representation

	0      0x00000000
	1.0    0x3f800000
	0.5    0x3f000000
	3      0x40400000
	+inf   0x7f800000
	-inf   0xff800000
	+NaN   0x7fc00000 or 0x7ff00000
	in general: number = (sign ? -1:1) * 2^(exponent) * 1.(mantissa bits)
*/

typedef union
{
	u32 u;
	s32 s;
	f32 f;
} inttofloat;

#define F32_AS_S32(f) (*((s32 *)&(f)))
#define F32_AS_U32(f) (*((u32 *)&(f)))
#define F32_AS_U32_POINTER(f) (((u32 *)&(f)))

#define F32_VALUE_0 0x00000000
#define F32_VALUE_1 0x3f800000

//! code is taken from IceFPU
//! Integer representation of a floating-point value.
inline u32 IR(f32 x)
{
	inttofloat tmp;
	tmp.f = x;
	return tmp.u;
}

//! Floating-point representation of an integer value.
inline f32 FR(u32 x)
{
	inttofloat tmp;
	tmp.u = x;
	return tmp.f;
}
inline f32 FR(s32 x)
{
	inttofloat tmp;
	tmp.s = x;
	return tmp.f;
}

#define F32_LOWER_0(n) ((n) < 0.0f)
#define F32_LOWER_EQUAL_0(n) ((n) <= 0.0f)
#define F32_GREATER_0(n) ((n) > 0.0f)
#define F32_GREATER_EQUAL_0(n) ((n) >= 0.0f)
#define F32_EQUAL_1(n) ((n) == 1.0f)
#define F32_EQUAL_0(n) ((n) == 0.0f)
#define F32_A_GREATER_B(a, b) ((a) > (b))

#ifndef REALINLINE
#ifdef _MSC_VER
#define REALINLINE __forceinline
#else
#define REALINLINE inline
#endif
#endif

// NOTE: This is not as exact as the c99/c++11 round function, especially at high numbers starting with 8388609
//       (only low number which seems to go wrong is 0.49999997 which is rounded to 1)
//      Also negative 0.5 is rounded up not down unlike with the standard function (p.E. input -0.5 will be 0 and not -1)
inline f32 round_(f32 x)
{
	return floorf(x + 0.5f);
}

// calculate: sqrt ( x )
REALINLINE f32 squareroot(const f32 f)
{
	return sqrtf(f);
}

// calculate: sqrt ( x )
REALINLINE f64 squareroot(const f64 f)
{
	return sqrt(f);
}

// calculate: sqrt ( x )
REALINLINE s32 squareroot(const s32 f)
{
	return static_cast<s32>(squareroot(static_cast<f32>(f)));
}

// calculate: sqrt ( x )
REALINLINE s64 squareroot(const s64 f)
{
	return static_cast<s64>(squareroot(static_cast<f64>(f)));
}

// calculate: 1 / sqrt ( x )
REALINLINE f64 reciprocal_squareroot(const f64 x)
{
	return 1.0 / sqrt(x);
}

// calculate: 1 / sqrtf ( x )
REALINLINE f32 reciprocal_squareroot(const f32 f)
{
	return 1.f / sqrtf(f);
}

// calculate: 1 / sqrtf( x )
REALINLINE s32 reciprocal_squareroot(const s32 x)
{
	return static_cast<s32>(reciprocal_squareroot(static_cast<f32>(x)));
}

// calculate: 1 / x
REALINLINE f32 reciprocal(const f32 f)
{
	return 1.f / f;
}

// calculate: 1 / x
REALINLINE f64 reciprocal(const f64 f)
{
	return 1.0 / f;
}

// calculate: 1 / x, low precision allowed
REALINLINE f32 reciprocal_approxim(const f32 f)
{
	return 1.f / f;
}

REALINLINE s32 floor32(f32 x)
{
	return (s32)floorf(x);
}

REALINLINE s32 ceil32(f32 x)
{
	return (s32)ceilf(x);
}

// NOTE: Please check round_ documentation about some inaccuracies in this compared to standard library round function.
REALINLINE s32 round32(f32 x)
{
	return (s32)round_(x);
}

inline f32 f32_max3(const f32 a, const f32 b, const f32 c)
{
	return a > b ? (a > c ? a : c) : (b > c ? b : c);
}

inline f32 f32_min3(const f32 a, const f32 b, const f32 c)
{
	return a < b ? (a < c ? a : c) : (b < c ? b : c);
}

inline f32 fract(f32 x)
{
	return x - floorf(x);
}

} // end namespace core

using core::FR;
using core::IR;
