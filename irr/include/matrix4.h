// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <cstring> // memset, memcpy
#include "irrMath.h"
#include "vector3d.h"
#include "vector2d.h"
#include "plane3d.h"
#include "aabbox3d.h"
#include "rect.h"
#include <cassert>

namespace core
{

//! 4x4 matrix. Mostly used as transformation matrix for 3d calculations.
/** Conventions: Matrices are considered to be in row-major order.
 * Multiplication of a matrix A with a row vector v is the premultiplication vA.
 * Translations are thus in the 4th row.
 * The matrix product AB yields a matrix C such that vC = (vB)A:
 * B is applied first, then A.
 */
template <class T>
class CMatrix4
{
public:
	//! Constructor Flags
	enum eConstructor
	{
		EM4CONST_NOTHING = 0,
		EM4CONST_COPY,
		EM4CONST_IDENTITY,
		EM4CONST_TRANSPOSED,
		EM4CONST_INVERSE,
		EM4CONST_INVERSE_TRANSPOSED
	};

	//! Default constructor
	/** \param constructor Choose the initialization style */
	CMatrix4(eConstructor constructor = EM4CONST_IDENTITY);

	//! Constructor with value initialization
	constexpr CMatrix4(const T &r0c0, const T &r0c1, const T &r0c2, const T &r0c3,
			const T &r1c0, const T &r1c1, const T &r1c2, const T &r1c3,
			const T &r2c0, const T &r2c1, const T &r2c2, const T &r2c3,
			const T &r3c0, const T &r3c1, const T &r3c2, const T &r3c3)
	{
		M[0] = r0c0;
		M[1] = r0c1;
		M[2] = r0c2;
		M[3] = r0c3;
		M[4] = r1c0;
		M[5] = r1c1;
		M[6] = r1c2;
		M[7] = r1c3;
		M[8] = r2c0;
		M[9] = r2c1;
		M[10] = r2c2;
		M[11] = r2c3;
		M[12] = r3c0;
		M[13] = r3c1;
		M[14] = r3c2;
		M[15] = r3c3;
	}

	//! Copy constructor
	/** \param other Other matrix to copy from
	\param constructor Choose the initialization style */
	CMatrix4(const CMatrix4<T> &other, eConstructor constructor = EM4CONST_COPY);

	//! Simple operator for directly accessing every element of the matrix.
	T &operator()(const s32 row, const s32 col)
	{
		return M[row * 4 + col];
	}

	//! Simple operator for directly accessing every element of the matrix.
	const T &operator()(const s32 row, const s32 col) const { return M[row * 4 + col]; }

	//! Simple operator for linearly accessing every element of the matrix.
	T &operator[](u32 index)
	{
		return M[index];
	}

	//! Simple operator for linearly accessing every element of the matrix.
	const T &operator[](u32 index) const { return M[index]; }

	//! Sets this matrix equal to the other matrix.
	CMatrix4<T> &operator=(const CMatrix4<T> &other) = default;

	//! Sets all elements of this matrix to the value.
	inline CMatrix4<T> &operator=(const T &scalar);

	//! Returns pointer to internal array
	const T *pointer() const { return M; }
	T *pointer()
	{
		return M;
	}

	//! Returns true if other matrix is equal to this matrix.
	constexpr bool operator==(const CMatrix4<T> &other) const
	{
		for (s32 i = 0; i < 16; ++i)
			if (M[i] != other.M[i])
				return false;

		return true;
	}

	//! Returns true if other matrix is not equal to this matrix.
	constexpr bool operator!=(const CMatrix4<T> &other) const
	{
		return !(*this == other);
	}

	//! Add another matrix.
	CMatrix4<T> operator+(const CMatrix4<T> &other) const;

	//! Add another matrix.
	CMatrix4<T> &operator+=(const CMatrix4<T> &other);

	//! Subtract another matrix.
	CMatrix4<T> operator-(const CMatrix4<T> &other) const;

	//! Subtract another matrix.
	CMatrix4<T> &operator-=(const CMatrix4<T> &other);

	//! set this matrix to the product of two matrices
	/** Calculate b*a */
	inline CMatrix4<T> &setbyproduct(const CMatrix4<T> &other_a, const CMatrix4<T> &other_b);

	//! Set this matrix to the product of two matrices
	/** Calculate b*a, no optimization used,
	use it if you know you never have an identity matrix */
	CMatrix4<T> &setbyproduct_nocheck(const CMatrix4<T> &other_a, const CMatrix4<T> &other_b);

	//! Multiply by another matrix.
	/** Calculate other*this */
	CMatrix4<T> operator*(const CMatrix4<T> &other) const;

	//! Multiply by another matrix.
	/** Like calling: (*this) = (*this) * other
	 */
	CMatrix4<T> &operator*=(const CMatrix4<T> &other);

	//! Multiply by scalar.
	CMatrix4<T> operator*(const T &scalar) const;

	//! Multiply by scalar.
	CMatrix4<T> &operator*=(const T &scalar);

	//! Set matrix to identity.
	inline CMatrix4<T> &makeIdentity();

	//! Returns true if the matrix is the identity matrix
	inline bool isIdentity() const;

	//! Set the translation of the current matrix. Will erase any previous values.
	CMatrix4<T> &setTranslation(const vector3d<T> &translation);

	//! Gets the current translation
	vector3d<T> getTranslation() const;

	//! Make a rotation matrix from Euler angles. The 4th row and column are unmodified.
	//! NOTE: Rotation order is (extrinsic) X-Y-Z.
	//! This means that vectors are first rotated around the X,
	//! then the (unrotated) Y, and finally the (unrotated) Z axis.
	//! NOTE: The rotation is done as per the right-hand rule.
	//! See test_irr_matrix4.cpp if you're still unsure about the conventions used here.
	inline CMatrix4<T> &setRotationRadians(const vector3d<T> &rotation);

	//! Same as `setRotationRadians`, but uses degrees.
	CMatrix4<T> &setRotationDegrees(const vector3d<T> &rotation);

	//! Get the rotation, as set by setRotation() when you already know the scale used to create the matrix
	/**
	NOTE: No scale value can be 0 or the result is undefined.
	NOTE: It does not necessarily return the *same* Euler angles as those set by setRotationDegrees(),
		but the rotation will be equivalent,  i.e. will have the same result when used to rotate a vector or node.
	NOTE: It will (usually) give wrong results when further transformations have been added in the matrix (like shear).
	WARNING: There have been troubles with this function over the years and we may still have missed some corner cases.
		It's generally safer to keep the rotation and scale you used to create the matrix around and work with those.
	*/
	vector3d<T> getRotationRadians(const vector3d<T> &scale) const;

	//! Returns the rotation, as set by setRotation().
	/** NOTE: You will have the same end-rotation as used in setRotation, but it might not use the same axis values.
		NOTE: This only works correctly for TRS matrix products where S is a positive, component-wise scaling (see setScale).
		NOTE: It does not necessarily return the *same* Euler angles as those set by setRotationDegrees(),
		but the rotation will be equivalent, i.e. will have the same result when used to rotate a vector or node.
	*/
	vector3d<T> getRotationRadians() const;

	//! Same as getRotationRadians, but returns degrees.
	vector3d<T> getRotationDegrees() const;

	//! Make a rotation matrix from angle and axis, assuming left handed rotation.
	/** The 4th row and column are unmodified. */
	inline CMatrix4<T> &setRotationAxisRadians(const T &angle, const vector3d<T> &axis);

	//! Set Scale
	CMatrix4<T> &setScale(const vector3d<T> &scale);

	//! Set Scale
	CMatrix4<T> &setScale(const T scale) { return setScale(vector3d<T>(scale, scale, scale)); }

	//! Get Scale
	vector3d<T> getScale() const;

	//! Translate a vector by the inverse of the translation part of this matrix.
	void inverseTranslateVect(vector3df &vect) const;

	//! Scale a vector, then rotate by the inverse of the rotation part of this matrix.
	[[nodiscard]] vector3d<T> scaleThenInvRotVect(const vector3d<T> &vect) const;

	//! Rotate and scale a vector. Applies both rotation & scale part of the matrix.
	[[nodiscard]] vector3d<T> rotateAndScaleVect(const vector3d<T> &vect) const;

	//! Transforms the vector by this matrix
	/** This operation is performed as if the vector was 4d with the 4th component = 1 */
	[[nodiscard]] vector3d<T> transformVect(const vector3d<T> &v) const;

	//! Transforms the vector by this matrix
	/** This operation is performed as if the vector was 4d with the 4th component = 1 */
	void transformVect(vector3d<T> &vect) const {
		const vector3d<T> &v = vect;
		vect = transformVect(v);
	}

	//! Transforms input vector by this matrix and stores result in output vector
	/** This operation is performed as if the vector was 4d with the 4th component = 1 */
	void transformVect(vector3d<T> &out, const vector3d<T> &in) const {
		out = transformVect(in);
	}

	//! An alternate transform vector method, writing into an array of 4 floats
	/** This operation is performed as if the vector was 4d with the 4th component =1.
		NOTE: out[3] will be written to (4th vector component)*/
	void transformVect(T *out, const vector3df &in) const;

	//! An alternate transform vector method, reading from and writing to an array of 3 floats
	/** This operation is performed as if the vector was 4d with the 4th component =1
		NOTE: out[3] will be written to (4th vector component)*/
	void transformVec3(T *out, const T *in) const;

	//! An alternate transform vector method, reading from and writing to an array of 4 floats
	void transformVec4(T *out, const T *in) const;

	//! Translate a vector by the translation part of this matrix.
	/** This operation is performed as if the vector was 4d with the 4th component =1 */
	void translateVect(vector3df &vect) const;

	//! Transforms a plane by this matrix
	void transformPlane(plane3d<f32> &plane) const;

	//! Transforms a plane by this matrix
	void transformPlane(const plane3d<f32> &in, plane3d<f32> &out) const;

	//! Transforms a axis aligned bounding box
	void transformBoxEx(aabbox3d<f32> &box) const;

	//! Multiplies this matrix by a 1x4 matrix
	void multiplyWith1x4Matrix(T *matrix) const;

	//! Calculates inverse of matrix. Slow.
	/** \return Returns false if there is no inverse matrix.*/
	bool makeInverse();

	//! Inverts a primitive matrix which only contains a translation and a rotation
	/** \param out: where result matrix is written to. */
	bool getInversePrimitive(CMatrix4<T> &out) const;

	//! Gets the inverse matrix of this one
	/** \param out: where result matrix is written to.
	\return Returns false if there is no inverse matrix. */
	bool getInverse(CMatrix4<T> &out) const;

	//! Builds a right-handed perspective projection matrix based on a field of view
	//\param zClipFromZero: Clipping of z can be projected from 0 to w when true (D3D style) and from -w to w when false (OGL style).
	CMatrix4<T> &buildProjectionMatrixPerspectiveFovRH(f32 fieldOfViewRadians, f32 aspectRatio, f32 zNear, f32 zFar, bool zClipFromZero = true);

	//! Builds a left-handed perspective projection matrix based on a field of view
	CMatrix4<T> &buildProjectionMatrixPerspectiveFovLH(f32 fieldOfViewRadians, f32 aspectRatio, f32 zNear, f32 zFar, bool zClipFromZero = true);

	//! Builds a left-handed perspective projection matrix based on a field of view, with far plane at infinity
	CMatrix4<T> &buildProjectionMatrixPerspectiveFovInfinityLH(f32 fieldOfViewRadians, f32 aspectRatio, f32 zNear, f32 epsilon = 0);

	//! Builds a right-handed perspective projection matrix.
	CMatrix4<T> &buildProjectionMatrixPerspectiveRH(f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero = true);

	//! Builds a left-handed perspective projection matrix.
	CMatrix4<T> &buildProjectionMatrixPerspectiveLH(f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero = true);

	//! Builds a left-handed orthogonal projection matrix.
	//\param zClipFromZero: Clipping of z can be projected from 0 to 1 when true (D3D style) and from -1 to 1 when false (OGL style).
	CMatrix4<T> &buildProjectionMatrixOrthoLH(f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero = true);

	//! Builds a right-handed orthogonal projection matrix.
	CMatrix4<T> &buildProjectionMatrixOrthoRH(f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero = true);

	//! Builds a left-handed look-at matrix.
	//! NOTE: upVector must not be collinear to the postion-to-target vector
	CMatrix4<T> &buildCameraLookAtMatrixLH(
			const vector3df &position,
			const vector3df &target,
			const vector3df &upVector);

	//! Builds a right-handed look-at matrix.
	//! NOTE: upVector must not be collinear to the postion-to-target vector
	CMatrix4<T> &buildCameraLookAtMatrixRH(
			const vector3df &position,
			const vector3df &target,
			const vector3df &upVector);

	//! Builds a matrix that flattens geometry into a plane.
	/** \param light: light source
	\param plane: plane into which the geometry if flattened into
	\param point: value between 0 and 1, describing the light source.
	If this is 1, it is a point light, if it is 0, it is a directional light. */
	CMatrix4<T> &buildShadowMatrix(const vector3df &light, plane3df plane, f32 point = 1.0f);

	//! Builds a matrix which transforms a normalized Device Coordinate to Device Coordinates.
	/** Used to scale <-1,-1><1,1> to viewport, for example from <-1,-1> <1,1> to the viewport <0,0><0,640> */
	CMatrix4<T> &buildNDCToDCMatrix(const rect<s32> &area, f32 zScale);

	//! Creates a new matrix as interpolated matrix from two other ones.
	/** \param b: other matrix to interpolate with
	\param time: Must be a value between 0 and 1. */
	CMatrix4<T> interpolate(const CMatrix4<T> &b, f32 time) const;

	//! Gets transposed matrix
	CMatrix4<T> getTransposed() const;

	//! Gets transposed matrix
	inline void getTransposed(CMatrix4<T> &dest) const;

	//! Builds a matrix that rotates from one vector to another
	/** \param from: vector to rotate from
	\param to: vector to rotate to
	 */
	CMatrix4<T> &buildRotateFromTo(const vector3df &from, const vector3df &to);

	//! Builds a combined matrix which translates to a center before rotation and translates from origin afterwards
	/** \param center Position to rotate around
	\param translate Translation applied after the rotation
	 */
	void setRotationCenter(const vector3df &center, const vector3df &translate);

	//! Builds a matrix which rotates a source vector to a look vector over an arbitrary axis
	/** \param camPos: viewer position in world coo
	\param center: object position in world-coo and rotation pivot
	\param translation: object final translation from center
	\param axis: axis to rotate about
	\param from: source vector to rotate from
	 */
	void buildAxisAlignedBillboard(const vector3df &camPos,
			const vector3df &center,
			const vector3df &translation,
			const vector3df &axis,
			const vector3df &from);

	/*
		construct 2D Texture transformations
		rotate about center, scale, and transform.
	*/
	//! Set to a texture transformation matrix with the given parameters.
	CMatrix4<T> &buildTextureTransform(f32 rotateRad,
			const vector2df &rotatecenter,
			const vector2df &translate,
			const vector2df &scale);

	//! Set texture transformation rotation
	/** Rotate about z axis, recenter at (0.5,0.5).
	Doesn't clear other elements than those affected
	\param radAngle Angle in radians
	\return Altered matrix */
	CMatrix4<T> &setTextureRotationCenter(f32 radAngle);

	//! Set texture transformation translation
	/** Doesn't clear other elements than those affected.
	\param x Offset on x axis
	\param y Offset on y axis
	\return Altered matrix */
	CMatrix4<T> &setTextureTranslate(f32 x, f32 y);

	//! Get texture transformation translation
	/** \param x returns offset on x axis
	\param y returns offset on y axis */
	void getTextureTranslate(f32 &x, f32 &y) const;

	//! Set texture transformation translation, using a transposed representation
	/** Doesn't clear other elements than those affected.
	\param x Offset on x axis
	\param y Offset on y axis
	\return Altered matrix */
	CMatrix4<T> &setTextureTranslateTransposed(f32 x, f32 y);

	//! Set texture transformation scale
	/** Doesn't clear other elements than those affected.
	\param sx Scale factor on x axis
	\param sy Scale factor on y axis
	\return Altered matrix. */
	CMatrix4<T> &setTextureScale(f32 sx, f32 sy);

	//! Get texture transformation scale
	/** \param sx Returns x axis scale factor
	\param sy Returns y axis scale factor */
	void getTextureScale(f32 &sx, f32 &sy) const;

	//! Set texture transformation scale, and recenter at (0.5,0.5)
	/** Doesn't clear other elements than those affected.
	\param sx Scale factor on x axis
	\param sy Scale factor on y axis
	\return Altered matrix. */
	CMatrix4<T> &setTextureScaleCenter(f32 sx, f32 sy);

	//! Sets all matrix data members at once
	CMatrix4<T> &setM(const T *data);

	//! Compare two matrices using the equal method
	bool equals(const CMatrix4<T> &other, const T tolerance = (T)ROUNDING_ERROR_f64) const;

private:
	template <bool degrees>
	vector3d<T> getRotation(const vector3d<T> &scale) const;

	//! Matrix data, stored in row-major order
	T M[16];
};

// Default constructor
template <class T>
inline CMatrix4<T>::CMatrix4(eConstructor constructor)
{
	switch (constructor) {
	case EM4CONST_NOTHING:
	case EM4CONST_COPY:
		break;
	case EM4CONST_IDENTITY:
	case EM4CONST_INVERSE:
	default:
		makeIdentity();
		break;
	}
}

// Copy constructor
template <class T>
inline CMatrix4<T>::CMatrix4(const CMatrix4<T> &other, eConstructor constructor)
{
	switch (constructor) {
	case EM4CONST_IDENTITY:
		makeIdentity();
		break;
	case EM4CONST_NOTHING:
		break;
	case EM4CONST_COPY:
		*this = other;
		break;
	case EM4CONST_TRANSPOSED:
		other.getTransposed(*this);
		break;
	case EM4CONST_INVERSE:
		if (!other.getInverse(*this))
			memset(M, 0, 16 * sizeof(T));
		break;
	case EM4CONST_INVERSE_TRANSPOSED:
		if (!other.getInverse(*this))
			memset(M, 0, 16 * sizeof(T));
		else
			*this = getTransposed();
		break;
	}
}

//! Add another matrix.
template <class T>
inline CMatrix4<T> CMatrix4<T>::operator+(const CMatrix4<T> &other) const
{
	CMatrix4<T> temp(EM4CONST_NOTHING);

	temp[0] = M[0] + other[0];
	temp[1] = M[1] + other[1];
	temp[2] = M[2] + other[2];
	temp[3] = M[3] + other[3];
	temp[4] = M[4] + other[4];
	temp[5] = M[5] + other[5];
	temp[6] = M[6] + other[6];
	temp[7] = M[7] + other[7];
	temp[8] = M[8] + other[8];
	temp[9] = M[9] + other[9];
	temp[10] = M[10] + other[10];
	temp[11] = M[11] + other[11];
	temp[12] = M[12] + other[12];
	temp[13] = M[13] + other[13];
	temp[14] = M[14] + other[14];
	temp[15] = M[15] + other[15];

	return temp;
}

//! Add another matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::operator+=(const CMatrix4<T> &other)
{
	M[0] += other[0];
	M[1] += other[1];
	M[2] += other[2];
	M[3] += other[3];
	M[4] += other[4];
	M[5] += other[5];
	M[6] += other[6];
	M[7] += other[7];
	M[8] += other[8];
	M[9] += other[9];
	M[10] += other[10];
	M[11] += other[11];
	M[12] += other[12];
	M[13] += other[13];
	M[14] += other[14];
	M[15] += other[15];

	return *this;
}

//! Subtract another matrix.
template <class T>
inline CMatrix4<T> CMatrix4<T>::operator-(const CMatrix4<T> &other) const
{
	CMatrix4<T> temp(EM4CONST_NOTHING);

	temp[0] = M[0] - other[0];
	temp[1] = M[1] - other[1];
	temp[2] = M[2] - other[2];
	temp[3] = M[3] - other[3];
	temp[4] = M[4] - other[4];
	temp[5] = M[5] - other[5];
	temp[6] = M[6] - other[6];
	temp[7] = M[7] - other[7];
	temp[8] = M[8] - other[8];
	temp[9] = M[9] - other[9];
	temp[10] = M[10] - other[10];
	temp[11] = M[11] - other[11];
	temp[12] = M[12] - other[12];
	temp[13] = M[13] - other[13];
	temp[14] = M[14] - other[14];
	temp[15] = M[15] - other[15];

	return temp;
}

//! Subtract another matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::operator-=(const CMatrix4<T> &other)
{
	M[0] -= other[0];
	M[1] -= other[1];
	M[2] -= other[2];
	M[3] -= other[3];
	M[4] -= other[4];
	M[5] -= other[5];
	M[6] -= other[6];
	M[7] -= other[7];
	M[8] -= other[8];
	M[9] -= other[9];
	M[10] -= other[10];
	M[11] -= other[11];
	M[12] -= other[12];
	M[13] -= other[13];
	M[14] -= other[14];
	M[15] -= other[15];

	return *this;
}

//! Multiply by scalar.
template <class T>
inline CMatrix4<T> CMatrix4<T>::operator*(const T &scalar) const
{
	CMatrix4<T> temp(EM4CONST_NOTHING);

	temp[0] = M[0] * scalar;
	temp[1] = M[1] * scalar;
	temp[2] = M[2] * scalar;
	temp[3] = M[3] * scalar;
	temp[4] = M[4] * scalar;
	temp[5] = M[5] * scalar;
	temp[6] = M[6] * scalar;
	temp[7] = M[7] * scalar;
	temp[8] = M[8] * scalar;
	temp[9] = M[9] * scalar;
	temp[10] = M[10] * scalar;
	temp[11] = M[11] * scalar;
	temp[12] = M[12] * scalar;
	temp[13] = M[13] * scalar;
	temp[14] = M[14] * scalar;
	temp[15] = M[15] * scalar;

	return temp;
}

//! Multiply by scalar.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::operator*=(const T &scalar)
{
	M[0] *= scalar;
	M[1] *= scalar;
	M[2] *= scalar;
	M[3] *= scalar;
	M[4] *= scalar;
	M[5] *= scalar;
	M[6] *= scalar;
	M[7] *= scalar;
	M[8] *= scalar;
	M[9] *= scalar;
	M[10] *= scalar;
	M[11] *= scalar;
	M[12] *= scalar;
	M[13] *= scalar;
	M[14] *= scalar;
	M[15] *= scalar;

	return *this;
}

//! Multiply by another matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::operator*=(const CMatrix4<T> &other)
{
	CMatrix4<T> temp(*this);
	return setbyproduct_nocheck(temp, other);
}

//! multiply by another matrix
// set this matrix to the product of two other matrices
// goal is to reduce stack use and copy
template <class T>
inline CMatrix4<T> &CMatrix4<T>::setbyproduct_nocheck(const CMatrix4<T> &other_a, const CMatrix4<T> &other_b)
{
	const T *m1 = other_a.M;
	const T *m2 = other_b.M;

	M[0] = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2] + m1[12] * m2[3];
	M[1] = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2] + m1[13] * m2[3];
	M[2] = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2] + m1[14] * m2[3];
	M[3] = m1[3] * m2[0] + m1[7] * m2[1] + m1[11] * m2[2] + m1[15] * m2[3];

	M[4] = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6] + m1[12] * m2[7];
	M[5] = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6] + m1[13] * m2[7];
	M[6] = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6] + m1[14] * m2[7];
	M[7] = m1[3] * m2[4] + m1[7] * m2[5] + m1[11] * m2[6] + m1[15] * m2[7];

	M[8] = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10] + m1[12] * m2[11];
	M[9] = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10] + m1[13] * m2[11];
	M[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10] + m1[14] * m2[11];
	M[11] = m1[3] * m2[8] + m1[7] * m2[9] + m1[11] * m2[10] + m1[15] * m2[11];

	M[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12] * m2[15];
	M[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13] * m2[15];
	M[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14] * m2[15];
	M[15] = m1[3] * m2[12] + m1[7] * m2[13] + m1[11] * m2[14] + m1[15] * m2[15];
	return *this;
}

//! multiply by another matrix
// set this matrix to the product of two other matrices
// goal is to reduce stack use and copy
template <class T>
inline CMatrix4<T> &CMatrix4<T>::setbyproduct(const CMatrix4<T> &other_a, const CMatrix4<T> &other_b)
{
	return setbyproduct_nocheck(other_a, other_b);
}

//! multiply by another matrix
template <class T>
inline CMatrix4<T> CMatrix4<T>::operator*(const CMatrix4<T> &m2) const
{
	CMatrix4<T> m3(EM4CONST_NOTHING);

	const T *m1 = M;

	m3[0] = m1[0] * m2[0] + m1[4] * m2[1] + m1[8] * m2[2] + m1[12] * m2[3];
	m3[1] = m1[1] * m2[0] + m1[5] * m2[1] + m1[9] * m2[2] + m1[13] * m2[3];
	m3[2] = m1[2] * m2[0] + m1[6] * m2[1] + m1[10] * m2[2] + m1[14] * m2[3];
	m3[3] = m1[3] * m2[0] + m1[7] * m2[1] + m1[11] * m2[2] + m1[15] * m2[3];

	m3[4] = m1[0] * m2[4] + m1[4] * m2[5] + m1[8] * m2[6] + m1[12] * m2[7];
	m3[5] = m1[1] * m2[4] + m1[5] * m2[5] + m1[9] * m2[6] + m1[13] * m2[7];
	m3[6] = m1[2] * m2[4] + m1[6] * m2[5] + m1[10] * m2[6] + m1[14] * m2[7];
	m3[7] = m1[3] * m2[4] + m1[7] * m2[5] + m1[11] * m2[6] + m1[15] * m2[7];

	m3[8] = m1[0] * m2[8] + m1[4] * m2[9] + m1[8] * m2[10] + m1[12] * m2[11];
	m3[9] = m1[1] * m2[8] + m1[5] * m2[9] + m1[9] * m2[10] + m1[13] * m2[11];
	m3[10] = m1[2] * m2[8] + m1[6] * m2[9] + m1[10] * m2[10] + m1[14] * m2[11];
	m3[11] = m1[3] * m2[8] + m1[7] * m2[9] + m1[11] * m2[10] + m1[15] * m2[11];

	m3[12] = m1[0] * m2[12] + m1[4] * m2[13] + m1[8] * m2[14] + m1[12] * m2[15];
	m3[13] = m1[1] * m2[12] + m1[5] * m2[13] + m1[9] * m2[14] + m1[13] * m2[15];
	m3[14] = m1[2] * m2[12] + m1[6] * m2[13] + m1[10] * m2[14] + m1[14] * m2[15];
	m3[15] = m1[3] * m2[12] + m1[7] * m2[13] + m1[11] * m2[14] + m1[15] * m2[15];
	return m3;
}

template <class T>
inline vector3d<T> CMatrix4<T>::getTranslation() const
{
	return vector3d<T>(M[12], M[13], M[14]);
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setTranslation(const vector3d<T> &translation)
{
	M[12] = translation.X;
	M[13] = translation.Y;
	M[14] = translation.Z;
	return *this;
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setScale(const vector3d<T> &scale)
{
	M[0] = scale.X;
	M[5] = scale.Y;
	M[10] = scale.Z;
	return *this;
}

//! Returns the absolute values of the scales of the 3x3 submatrix.
/**
Note: You only get back original values if the matrix only set the scale.
Otherwise the result is a scale you can use to normalize the matrix axes,
but it's usually no longer what you did set with setScale.
*/
template <class T>
inline vector3d<T> CMatrix4<T>::getScale() const
{
	auto row_vector_length = [this](int col) {
		int i = 4 * col;
		return sqrtf(M[i] * M[i] + M[i+1] * M[i+1] + M[i+2] * M[i+2]);
	};
	return {
		row_vector_length(0),
		row_vector_length(1),
		row_vector_length(2),
	};
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setRotationDegrees(const vector3d<T> &rotation)
{
	return setRotationRadians(rotation * DEGTORAD);
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setRotationRadians(const vector3d<T> &rotation)
{
	const f64 cPitch = cos(rotation.X);
	const f64 sPitch = sin(rotation.X);
	const f64 cYaw = cos(rotation.Y);
	const f64 sYaw = sin(rotation.Y);
	const f64 cRoll = cos(rotation.Z);
	const f64 sRoll = sin(rotation.Z);

	M[0] = (T)(cYaw * cRoll);
	M[1] = (T)(cYaw * sRoll);
	M[2] = (T)(-sYaw);

	const f64 sPitch_sYaw = sPitch * sYaw;
	const f64 cPitch_sYaw = cPitch * sYaw;

	M[4] = (T)(sPitch_sYaw * cRoll - cPitch * sRoll);
	M[5] = (T)(sPitch_sYaw * sRoll + cPitch * cRoll);
	M[6] = (T)(sPitch * cYaw);

	M[8] = (T)(cPitch_sYaw * cRoll + sPitch * sRoll);
	M[9] = (T)(cPitch_sYaw * sRoll - sPitch * cRoll);
	M[10] = (T)(cPitch * cYaw);
	return *this;
}

template <class T>
template <bool degrees>
inline vector3d<T> CMatrix4<T>::getRotation(const vector3d<T> &scale_) const
{
	// Based on code by Chev
	const CMatrix4<T> &mat = *this;
	const vector3d<f64> scale(iszero(scale_.X) ? FLT_MAX : scale_.X, iszero(scale_.Y) ? FLT_MAX : scale_.Y, iszero(scale_.Z) ? FLT_MAX : scale_.Z);
	const vector3d<f64> invScale(reciprocal(scale.X), reciprocal(scale.Y), reciprocal(scale.Z));

	f64 a = clamp(mat[2] * invScale.X, -1.0, 1.0);
	f64 Y = -asin(a);

	f64 rotx, roty, X, Z;

	if (!core::equals(std::abs(a), 1.0)) {
		// abs(a) = abs(sin(Y)) = 1 <=> cos(Y) = 0
		rotx = mat[10] * invScale.Z;
		roty = mat[6] * invScale.Y;
		X = atan2(roty, rotx);
		rotx = mat[0] * invScale.X;
		roty = mat[1] * invScale.X;
		Z = atan2(roty, rotx);
	} else {
		X = 0.0;
		rotx = mat[5];
		roty = -mat[4];
		Z = atan2(roty, rotx);
	}

	if (degrees) {
		X *= core::RADTODEG64;
		Y *= core::RADTODEG64;
		Z *= core::RADTODEG64;
	}

	return vector3d<T>((T)X, (T)Y, (T)Z);
}

template <class T>
inline vector3d<T> CMatrix4<T>::getRotationRadians(const vector3d<T> &scale) const
{
	return getRotation<false>(scale);
}

template <class T>
inline vector3d<T> CMatrix4<T>::getRotationRadians() const
{
	return getRotationRadians(getScale());
}

template <class T>
inline vector3d<T> CMatrix4<T>::getRotationDegrees() const
{
	return getRotation<true>(getScale());
}

//! Sets matrix to rotation matrix defined by axis and angle, assuming LH rotation
template <class T>
inline CMatrix4<T> &CMatrix4<T>::setRotationAxisRadians(const T &angle, const vector3d<T> &axis)
{
	const f64 c = cos(angle);
	const f64 s = sin(angle);
	const f64 t = 1.0 - c;

	const f64 tx = t * axis.X;
	const f64 ty = t * axis.Y;
	const f64 tz = t * axis.Z;

	const f64 sx = s * axis.X;
	const f64 sy = s * axis.Y;
	const f64 sz = s * axis.Z;

	M[0] = (T)(tx * axis.X + c);
	M[1] = (T)(tx * axis.Y + sz);
	M[2] = (T)(tx * axis.Z - sy);

	M[4] = (T)(ty * axis.X - sz);
	M[5] = (T)(ty * axis.Y + c);
	M[6] = (T)(ty * axis.Z + sx);

	M[8] = (T)(tz * axis.X + sy);
	M[9] = (T)(tz * axis.Y - sx);
	M[10] = (T)(tz * axis.Z + c);

	return *this;
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::makeIdentity()
{
	memset(M, 0, 16 * sizeof(T));
	M[0] = M[5] = M[10] = M[15] = (T)1;
	return *this;
}

/*
	check identity with epsilon
	solve floating range problems..
*/
template <class T>
inline bool CMatrix4<T>::isIdentity() const
{
	if (!core::equals(M[12], (T)0) || !core::equals(M[13], (T)0) || !core::equals(M[14], (T)0) || !core::equals(M[15], (T)1))
		return false;

	if (!core::equals(M[0], (T)1) || !core::equals(M[1], (T)0) || !core::equals(M[2], (T)0) || !core::equals(M[3], (T)0))
		return false;

	if (!core::equals(M[4], (T)0) || !core::equals(M[5], (T)1) || !core::equals(M[6], (T)0) || !core::equals(M[7], (T)0))
		return false;

	if (!core::equals(M[8], (T)0) || !core::equals(M[9], (T)0) || !core::equals(M[10], (T)1) || !core::equals(M[11], (T)0))
		return false;
/*
		if (!core::equals( M[ 0], (T)1 ) ||
			!core::equals( M[ 5], (T)1 ) ||
			!core::equals( M[10], (T)1 ) ||
			!core::equals( M[15], (T)1 ))
			return false;

		for (s32 i=0; i<4; ++i)
			for (s32 j=0; j<4; ++j)
				if ((j != i) && (!iszero((*this)(i,j))))
					return false;
*/
	return true;
}

template <class T>
inline vector3d<T> CMatrix4<T>::rotateAndScaleVect(const vector3d<T> &v) const
{
	return {
		v.X * M[0] + v.Y * M[4] + v.Z * M[8],
		v.X * M[1] + v.Y * M[5] + v.Z * M[9],
		v.X * M[2] + v.Y * M[6] + v.Z * M[10]
	};
}

template <class T>
inline vector3d<T> CMatrix4<T>::scaleThenInvRotVect(const vector3d<T> &v) const
{
	return {
		v.X * M[0] + v.Y * M[1] + v.Z * M[2],
		v.X * M[4] + v.Y * M[5] + v.Z * M[6],
		v.X * M[8] + v.Y * M[9] + v.Z * M[10]
	};
}

template <class T>
inline vector3d<T> CMatrix4<T>::transformVect(const vector3d<T> &v) const
{
	return {
		v.X * M[0] + v.Y * M[4] + v.Z * M[8] + M[12],
		v.X * M[1] + v.Y * M[5] + v.Z * M[9] + M[13],
		v.X * M[2] + v.Y * M[6] + v.Z * M[10] + M[14],
	};
}

template <class T>
inline void CMatrix4<T>::transformVect(T *out, const vector3df &in) const
{
	out[0] = in.X * M[0] + in.Y * M[4] + in.Z * M[8] + M[12];
	out[1] = in.X * M[1] + in.Y * M[5] + in.Z * M[9] + M[13];
	out[2] = in.X * M[2] + in.Y * M[6] + in.Z * M[10] + M[14];
	out[3] = in.X * M[3] + in.Y * M[7] + in.Z * M[11] + M[15];
}

template <class T>
inline void CMatrix4<T>::transformVec3(T *out, const T *in) const
{
	out[0] = in[0] * M[0] + in[1] * M[4] + in[2] * M[8] + M[12];
	out[1] = in[0] * M[1] + in[1] * M[5] + in[2] * M[9] + M[13];
	out[2] = in[0] * M[2] + in[1] * M[6] + in[2] * M[10] + M[14];
}

template <class T>
inline void CMatrix4<T>::transformVec4(T *out, const T *in) const
{
	out[0] = in[0] * M[0] + in[1] * M[4] + in[2] * M[8] + in[3] * M[12];
	out[1] = in[0] * M[1] + in[1] * M[5] + in[2] * M[9] + in[3] * M[13];
	out[2] = in[0] * M[2] + in[1] * M[6] + in[2] * M[10] + in[3] * M[14];
	out[3] = in[0] * M[3] + in[1] * M[7] + in[2] * M[11] + in[3] * M[15];
}

//! Transforms a plane by this matrix
template <class T>
inline void CMatrix4<T>::transformPlane(plane3d<f32> &plane) const
{
	vector3df member;
	// Transform the plane member point, i.e. rotate, translate and scale it.
	transformVect(member, plane.getMemberPoint());

	// Transform the normal by the transposed inverse of the matrix
	CMatrix4<T> transposedInverse(*this, EM4CONST_INVERSE_TRANSPOSED);
	vector3df normal = transposedInverse.rotateAndScaleVect(plane.Normal);
	plane.setPlane(member, normal.normalize());
}

//! Transforms a plane by this matrix
template <class T>
inline void CMatrix4<T>::transformPlane(const plane3d<f32> &in, plane3d<f32> &out) const
{
	out = in;
	transformPlane(out);
}

//! Transforms a axis aligned bounding box more accurately than transformBox()
template <class T>
inline void CMatrix4<T>::transformBoxEx(aabbox3d<f32> &box) const
{
	const f32 Amin[3] = {box.MinEdge.X, box.MinEdge.Y, box.MinEdge.Z};
	const f32 Amax[3] = {box.MaxEdge.X, box.MaxEdge.Y, box.MaxEdge.Z};

	f32 Bmin[3];
	f32 Bmax[3];

	Bmin[0] = Bmax[0] = M[12];
	Bmin[1] = Bmax[1] = M[13];
	Bmin[2] = Bmax[2] = M[14];

	const CMatrix4<T> &m = *this;

	for (u32 i = 0; i < 3; ++i) {
		for (u32 j = 0; j < 3; ++j) {
			const f32 a = m(j, i) * Amin[j];
			const f32 b = m(j, i) * Amax[j];

			if (a < b) {
				Bmin[i] += a;
				Bmax[i] += b;
			} else {
				Bmin[i] += b;
				Bmax[i] += a;
			}
		}
	}

	box.MinEdge.X = Bmin[0];
	box.MinEdge.Y = Bmin[1];
	box.MinEdge.Z = Bmin[2];

	box.MaxEdge.X = Bmax[0];
	box.MaxEdge.Y = Bmax[1];
	box.MaxEdge.Z = Bmax[2];
}

//! Multiplies this matrix by a 1x4 matrix
template <class T>
inline void CMatrix4<T>::multiplyWith1x4Matrix(T *matrix) const
{
	/*
	0  1  2  3
	4  5  6  7
	8  9  10 11
	12 13 14 15
	*/

	T mat[4];
	mat[0] = matrix[0];
	mat[1] = matrix[1];
	mat[2] = matrix[2];
	mat[3] = matrix[3];

	matrix[0] = M[0] * mat[0] + M[4] * mat[1] + M[8] * mat[2] + M[12] * mat[3];
	matrix[1] = M[1] * mat[0] + M[5] * mat[1] + M[9] * mat[2] + M[13] * mat[3];
	matrix[2] = M[2] * mat[0] + M[6] * mat[1] + M[10] * mat[2] + M[14] * mat[3];
	matrix[3] = M[3] * mat[0] + M[7] * mat[1] + M[11] * mat[2] + M[15] * mat[3];
}

template <class T>
inline void CMatrix4<T>::inverseTranslateVect(vector3df &vect) const
{
	vect.X = vect.X - M[12];
	vect.Y = vect.Y - M[13];
	vect.Z = vect.Z - M[14];
}

template <class T>
inline void CMatrix4<T>::translateVect(vector3df &vect) const
{
	vect.X = vect.X + M[12];
	vect.Y = vect.Y + M[13];
	vect.Z = vect.Z + M[14];
}

template <class T>
inline bool CMatrix4<T>::getInverse(CMatrix4<T> &out) const
{
	/// Calculates the inverse of this Matrix
	/// The inverse is calculated using Cramers rule.
	/// If no inverse exists then 'false' is returned.

	const CMatrix4<T> &m = *this;

	f32 d = (m[0] * m[5] - m[1] * m[4]) * (m[10] * m[15] - m[11] * m[14]) -
			(m[0] * m[6] - m[2] * m[4]) * (m[9] * m[15] - m[11] * m[13]) +
			(m[0] * m[7] - m[3] * m[4]) * (m[9] * m[14] - m[10] * m[13]) +
			(m[1] * m[6] - m[2] * m[5]) * (m[8] * m[15] - m[11] * m[12]) -
			(m[1] * m[7] - m[3] * m[5]) * (m[8] * m[14] - m[10] * m[12]) +
			(m[2] * m[7] - m[3] * m[6]) * (m[8] * m[13] - m[9] * m[12]);

	if (iszero(d, FLT_MIN))
		return false;

	d = reciprocal(d);

	out[0] = d * (m[5] * (m[10] * m[15] - m[11] * m[14]) +
						 m[6] * (m[11] * m[13] - m[9] * m[15]) +
						 m[7] * (m[9] * m[14] - m[10] * m[13]));
	out[1] = d * (m[9] * (m[2] * m[15] - m[3] * m[14]) +
						 m[10] * (m[3] * m[13] - m[1] * m[15]) +
						 m[11] * (m[1] * m[14] - m[2] * m[13]));
	out[2] = d * (m[13] * (m[2] * m[7] - m[3] * m[6]) +
						 m[14] * (m[3] * m[5] - m[1] * m[7]) +
						 m[15] * (m[1] * m[6] - m[2] * m[5]));
	out[3] = d * (m[1] * (m[7] * m[10] - m[6] * m[11]) +
						 m[2] * (m[5] * m[11] - m[7] * m[9]) +
						 m[3] * (m[6] * m[9] - m[5] * m[10]));
	out[4] = d * (m[6] * (m[8] * m[15] - m[11] * m[12]) +
						 m[7] * (m[10] * m[12] - m[8] * m[14]) +
						 m[4] * (m[11] * m[14] - m[10] * m[15]));
	out[5] = d * (m[10] * (m[0] * m[15] - m[3] * m[12]) +
						 m[11] * (m[2] * m[12] - m[0] * m[14]) +
						 m[8] * (m[3] * m[14] - m[2] * m[15]));
	out[6] = d * (m[14] * (m[0] * m[7] - m[3] * m[4]) +
						 m[15] * (m[2] * m[4] - m[0] * m[6]) +
						 m[12] * (m[3] * m[6] - m[2] * m[7]));
	out[7] = d * (m[2] * (m[7] * m[8] - m[4] * m[11]) +
						 m[3] * (m[4] * m[10] - m[6] * m[8]) +
						 m[0] * (m[6] * m[11] - m[7] * m[10]));
	out[8] = d * (m[7] * (m[8] * m[13] - m[9] * m[12]) +
						 m[4] * (m[9] * m[15] - m[11] * m[13]) +
						 m[5] * (m[11] * m[12] - m[8] * m[15]));
	out[9] = d * (m[11] * (m[0] * m[13] - m[1] * m[12]) +
						 m[8] * (m[1] * m[15] - m[3] * m[13]) +
						 m[9] * (m[3] * m[12] - m[0] * m[15]));
	out[10] = d * (m[15] * (m[0] * m[5] - m[1] * m[4]) +
						  m[12] * (m[1] * m[7] - m[3] * m[5]) +
						  m[13] * (m[3] * m[4] - m[0] * m[7]));
	out[11] = d * (m[3] * (m[5] * m[8] - m[4] * m[9]) +
						  m[0] * (m[7] * m[9] - m[5] * m[11]) +
						  m[1] * (m[4] * m[11] - m[7] * m[8]));
	out[12] = d * (m[4] * (m[10] * m[13] - m[9] * m[14]) +
						  m[5] * (m[8] * m[14] - m[10] * m[12]) +
						  m[6] * (m[9] * m[12] - m[8] * m[13]));
	out[13] = d * (m[8] * (m[2] * m[13] - m[1] * m[14]) +
						  m[9] * (m[0] * m[14] - m[2] * m[12]) +
						  m[10] * (m[1] * m[12] - m[0] * m[13]));
	out[14] = d * (m[12] * (m[2] * m[5] - m[1] * m[6]) +
						  m[13] * (m[0] * m[6] - m[2] * m[4]) +
						  m[14] * (m[1] * m[4] - m[0] * m[5]));
	out[15] = d * (m[0] * (m[5] * m[10] - m[6] * m[9]) +
						  m[1] * (m[6] * m[8] - m[4] * m[10]) +
						  m[2] * (m[4] * m[9] - m[5] * m[8]));

	return true;
}

//! Inverts a primitive matrix which only contains a translation and a rotation
//! \param out: where result matrix is written to.
template <class T>
inline bool CMatrix4<T>::getInversePrimitive(CMatrix4<T> &out) const
{
	out.M[0] = M[0];
	out.M[1] = M[4];
	out.M[2] = M[8];
	out.M[3] = 0;

	out.M[4] = M[1];
	out.M[5] = M[5];
	out.M[6] = M[9];
	out.M[7] = 0;

	out.M[8] = M[2];
	out.M[9] = M[6];
	out.M[10] = M[10];
	out.M[11] = 0;

	out.M[12] = (T) - (M[12] * M[0] + M[13] * M[1] + M[14] * M[2]);
	out.M[13] = (T) - (M[12] * M[4] + M[13] * M[5] + M[14] * M[6]);
	out.M[14] = (T) - (M[12] * M[8] + M[13] * M[9] + M[14] * M[10]);
	out.M[15] = 1;

	return true;
}

/*!
 */
template <class T>
inline bool CMatrix4<T>::makeInverse()
{
	CMatrix4<T> temp(EM4CONST_NOTHING);

	if (getInverse(temp)) {
		*this = temp;
		return true;
	}

	return false;
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::operator=(const T &scalar)
{
	for (s32 i = 0; i < 16; ++i)
		M[i] = scalar;

	return *this;
}

// Builds a right-handed perspective projection matrix based on a field of view
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildProjectionMatrixPerspectiveFovRH(
		f32 fieldOfViewRadians, f32 aspectRatio, f32 zNear, f32 zFar, bool zClipFromZero)
{
	const f64 h = reciprocal(tan(fieldOfViewRadians * 0.5));
	assert(aspectRatio != 0.f); // divide by zero
	const T w = static_cast<T>(h / aspectRatio);

	assert(zNear != zFar); // divide by zero
	M[0] = w;
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (T)h;
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	// M[10]
	M[11] = -1;

	M[12] = 0;
	M[13] = 0;
	// M[14]
	M[15] = 0;

	if (zClipFromZero) { // DirectX version
		M[10] = (T)(zFar / (zNear - zFar));
		M[14] = (T)(zNear * zFar / (zNear - zFar));
	} else // OpenGL version
	{
		M[10] = (T)((zFar + zNear) / (zNear - zFar));
		M[14] = (T)(2.0f * zNear * zFar / (zNear - zFar));
	}

	return *this;
}

// Builds a left-handed perspective projection matrix based on a field of view
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildProjectionMatrixPerspectiveFovLH(
		f32 fieldOfViewRadians, f32 aspectRatio, f32 zNear, f32 zFar, bool zClipFromZero)
{
	const f64 h = reciprocal(tan(fieldOfViewRadians * 0.5));
	assert(aspectRatio != 0.f); // divide by zero
	const T w = static_cast<T>(h / aspectRatio);

	assert(zNear != zFar); // divide by zero
	M[0] = w;
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (T)h;
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	// M[10]
	M[11] = 1;

	M[12] = 0;
	M[13] = 0;
	// M[14]
	M[15] = 0;

	if (zClipFromZero) { // DirectX version
		M[10] = (T)(zFar / (zFar - zNear));
		M[14] = (T)(-zNear * zFar / (zFar - zNear));
	} else // OpenGL version
	{
		M[10] = (T)((zFar + zNear) / (zFar - zNear));
		M[14] = (T)(2.0f * zNear * zFar / (zNear - zFar));
	}

	return *this;
}

// Builds a left-handed perspective projection matrix based on a field of view, with far plane culling at infinity
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildProjectionMatrixPerspectiveFovInfinityLH(
		f32 fieldOfViewRadians, f32 aspectRatio, f32 zNear, f32 epsilon)
{
	const f64 h = reciprocal(tan(fieldOfViewRadians * 0.5));
	assert(aspectRatio != 0.f); // divide by zero
	const T w = static_cast<T>(h / aspectRatio);

	M[0] = w;
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (T)h;
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	M[10] = (T)(1.f - epsilon);
	M[11] = 1;

	M[12] = 0;
	M[13] = 0;
	M[14] = (T)(zNear * (epsilon - 1.f));
	M[15] = 0;

	return *this;
}

// Builds a left-handed orthogonal projection matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildProjectionMatrixOrthoLH(
		f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero)
{
	assert(widthOfViewVolume != 0.f);  // divide by zero
	assert(heightOfViewVolume != 0.f); // divide by zero
	assert(zNear != zFar);             // divide by zero
	M[0] = (T)(2 / widthOfViewVolume);
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (T)(2 / heightOfViewVolume);
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	// M[10]
	M[11] = 0;

	M[12] = 0;
	M[13] = 0;
	// M[14]
	M[15] = 1;

	if (zClipFromZero) {
		M[10] = (T)(1 / (zFar - zNear));
		M[14] = (T)(zNear / (zNear - zFar));
	} else {
		M[10] = (T)(2 / (zFar - zNear));
		M[14] = (T) - (zFar + zNear) / (zFar - zNear);
	}

	return *this;
}

// Builds a right-handed orthogonal projection matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildProjectionMatrixOrthoRH(
		f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero)
{
	assert(widthOfViewVolume != 0.f);  // divide by zero
	assert(heightOfViewVolume != 0.f); // divide by zero
	assert(zNear != zFar);             // divide by zero
	M[0] = (T)(2 / widthOfViewVolume);
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (T)(2 / heightOfViewVolume);
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	// M[10]
	M[11] = 0;

	M[12] = 0;
	M[13] = 0;
	// M[14]
	M[15] = 1;

	if (zClipFromZero) {
		M[10] = (T)(1 / (zNear - zFar));
		M[14] = (T)(zNear / (zNear - zFar));
	} else {
		M[10] = (T)(2 / (zNear - zFar));
		M[14] = (T) - (zFar + zNear) / (zFar - zNear);
	}

	return *this;
}

// Builds a right-handed perspective projection matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildProjectionMatrixPerspectiveRH(
		f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero)
{
	assert(widthOfViewVolume != 0.f);  // divide by zero
	assert(heightOfViewVolume != 0.f); // divide by zero
	assert(zNear != zFar);             // divide by zero
	M[0] = (T)(2 * zNear / widthOfViewVolume);
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (T)(2 * zNear / heightOfViewVolume);
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	// M[10]
	M[11] = -1;

	M[12] = 0;
	M[13] = 0;
	// M[14]
	M[15] = 0;

	if (zClipFromZero) { // DirectX version
		M[10] = (T)(zFar / (zNear - zFar));
		M[14] = (T)(zNear * zFar / (zNear - zFar));
	} else // OpenGL version
	{
		M[10] = (T)((zFar + zNear) / (zNear - zFar));
		M[14] = (T)(2.0f * zNear * zFar / (zNear - zFar));
	}

	return *this;
}

// Builds a left-handed perspective projection matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildProjectionMatrixPerspectiveLH(
		f32 widthOfViewVolume, f32 heightOfViewVolume, f32 zNear, f32 zFar, bool zClipFromZero)
{
	assert(widthOfViewVolume != 0.f);  // divide by zero
	assert(heightOfViewVolume != 0.f); // divide by zero
	assert(zNear != zFar);             // divide by zero
	M[0] = (T)(2 * zNear / widthOfViewVolume);
	M[1] = 0;
	M[2] = 0;
	M[3] = 0;

	M[4] = 0;
	M[5] = (T)(2 * zNear / heightOfViewVolume);
	M[6] = 0;
	M[7] = 0;

	M[8] = 0;
	M[9] = 0;
	// M[10]
	M[11] = 1;

	M[12] = 0;
	M[13] = 0;
	// M[14] = (T)(zNear*zFar/(zNear-zFar));
	M[15] = 0;

	if (zClipFromZero) { // DirectX version
		M[10] = (T)(zFar / (zFar - zNear));
		M[14] = (T)(zNear * zFar / (zNear - zFar));
	} else // OpenGL version
	{
		M[10] = (T)((zFar + zNear) / (zFar - zNear));
		M[14] = (T)(2.0f * zNear * zFar / (zNear - zFar));
	}

	return *this;
}

// Builds a matrix that flattens geometry into a plane.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildShadowMatrix(const vector3df &light, plane3df plane, f32 point)
{
	plane.Normal.normalize();
	const f32 d = plane.Normal.dotProduct(light);

	M[0] = (T)(-plane.Normal.X * light.X + d);
	M[1] = (T)(-plane.Normal.X * light.Y);
	M[2] = (T)(-plane.Normal.X * light.Z);
	M[3] = (T)(-plane.Normal.X * point);

	M[4] = (T)(-plane.Normal.Y * light.X);
	M[5] = (T)(-plane.Normal.Y * light.Y + d);
	M[6] = (T)(-plane.Normal.Y * light.Z);
	M[7] = (T)(-plane.Normal.Y * point);

	M[8] = (T)(-plane.Normal.Z * light.X);
	M[9] = (T)(-plane.Normal.Z * light.Y);
	M[10] = (T)(-plane.Normal.Z * light.Z + d);
	M[11] = (T)(-plane.Normal.Z * point);

	M[12] = (T)(-plane.D * light.X);
	M[13] = (T)(-plane.D * light.Y);
	M[14] = (T)(-plane.D * light.Z);
	M[15] = (T)(-plane.D * point + d);

	return *this;
}

// Builds a left-handed look-at matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildCameraLookAtMatrixLH(
		const vector3df &position,
		const vector3df &target,
		const vector3df &upVector)
{
	vector3df zaxis = target - position;
	zaxis.normalize();

	vector3df xaxis = upVector.crossProduct(zaxis);
	xaxis.normalize();

	vector3df yaxis = zaxis.crossProduct(xaxis);

	M[0] = (T)xaxis.X;
	M[1] = (T)yaxis.X;
	M[2] = (T)zaxis.X;
	M[3] = 0;

	M[4] = (T)xaxis.Y;
	M[5] = (T)yaxis.Y;
	M[6] = (T)zaxis.Y;
	M[7] = 0;

	M[8] = (T)xaxis.Z;
	M[9] = (T)yaxis.Z;
	M[10] = (T)zaxis.Z;
	M[11] = 0;

	M[12] = (T)-xaxis.dotProduct(position);
	M[13] = (T)-yaxis.dotProduct(position);
	M[14] = (T)-zaxis.dotProduct(position);
	M[15] = 1;

	return *this;
}

// Builds a right-handed look-at matrix.
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildCameraLookAtMatrixRH(
		const vector3df &position,
		const vector3df &target,
		const vector3df &upVector)
{
	vector3df zaxis = position - target;
	zaxis.normalize();

	vector3df xaxis = upVector.crossProduct(zaxis);
	xaxis.normalize();

	vector3df yaxis = zaxis.crossProduct(xaxis);

	M[0] = (T)xaxis.X;
	M[1] = (T)yaxis.X;
	M[2] = (T)zaxis.X;
	M[3] = 0;

	M[4] = (T)xaxis.Y;
	M[5] = (T)yaxis.Y;
	M[6] = (T)zaxis.Y;
	M[7] = 0;

	M[8] = (T)xaxis.Z;
	M[9] = (T)yaxis.Z;
	M[10] = (T)zaxis.Z;
	M[11] = 0;

	M[12] = (T)-xaxis.dotProduct(position);
	M[13] = (T)-yaxis.dotProduct(position);
	M[14] = (T)-zaxis.dotProduct(position);
	M[15] = 1;

	return *this;
}

// creates a new matrix as interpolated matrix from this and the passed one.
template <class T>
inline CMatrix4<T> CMatrix4<T>::interpolate(const CMatrix4<T> &b, f32 time) const
{
	CMatrix4<T> mat(EM4CONST_NOTHING);

	for (u32 i = 0; i < 16; i += 4) {
		mat.M[i + 0] = (T)(M[i + 0] + (b.M[i + 0] - M[i + 0]) * time);
		mat.M[i + 1] = (T)(M[i + 1] + (b.M[i + 1] - M[i + 1]) * time);
		mat.M[i + 2] = (T)(M[i + 2] + (b.M[i + 2] - M[i + 2]) * time);
		mat.M[i + 3] = (T)(M[i + 3] + (b.M[i + 3] - M[i + 3]) * time);
	}
	return mat;
}

// returns transposed matrix
template <class T>
inline CMatrix4<T> CMatrix4<T>::getTransposed() const
{
	CMatrix4<T> t(EM4CONST_NOTHING);
	getTransposed(t);
	return t;
}

// returns transposed matrix
template <class T>
inline void CMatrix4<T>::getTransposed(CMatrix4<T> &o) const
{
	o[0] = M[0];
	o[1] = M[4];
	o[2] = M[8];
	o[3] = M[12];

	o[4] = M[1];
	o[5] = M[5];
	o[6] = M[9];
	o[7] = M[13];

	o[8] = M[2];
	o[9] = M[6];
	o[10] = M[10];
	o[11] = M[14];

	o[12] = M[3];
	o[13] = M[7];
	o[14] = M[11];
	o[15] = M[15];
}

// used to scale <-1,-1><1,1> to viewport
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildNDCToDCMatrix(const rect<s32> &viewport, f32 zScale)
{
	const f32 scaleX = (viewport.getWidth() - 0.75f) * 0.5f;
	const f32 scaleY = -(viewport.getHeight() - 0.75f) * 0.5f;

	const f32 dx = -0.5f + ((viewport.UpperLeftCorner.X + viewport.LowerRightCorner.X) * 0.5f);
	const f32 dy = -0.5f + ((viewport.UpperLeftCorner.Y + viewport.LowerRightCorner.Y) * 0.5f);

	makeIdentity();
	M[12] = (T)dx;
	M[13] = (T)dy;
	return setScale(vector3d<T>((T)scaleX, (T)scaleY, (T)zScale));
}

//! Builds a matrix that rotates from one vector to another
/** \param from: vector to rotate from
\param to: vector to rotate to

	http://www.euclideanspace.com/maths/geometry/rotations/conversions/angleToMatrix/index.htm
 */
template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildRotateFromTo(const vector3df &from, const vector3df &to)
{
	// unit vectors
	vector3df f(from);
	vector3df t(to);
	f.normalize();
	t.normalize();

	// axis multiplication by sin
	vector3df vs(t.crossProduct(f));

	// axis of rotation
	vector3df v(vs);
	v.normalize();

	// cosine angle
	T ca = f.dotProduct(t);

	vector3df vt(v * (1 - ca));

	M[0] = vt.X * v.X + ca;
	M[5] = vt.Y * v.Y + ca;
	M[10] = vt.Z * v.Z + ca;

	vt.X *= v.Y;
	vt.Z *= v.X;
	vt.Y *= v.Z;

	M[1] = vt.X - vs.Z;
	M[2] = vt.Z + vs.Y;
	M[3] = 0;

	M[4] = vt.X + vs.Z;
	M[6] = vt.Y - vs.X;
	M[7] = 0;

	M[8] = vt.Z - vs.Y;
	M[9] = vt.Y + vs.X;
	M[11] = 0;

	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;

	return *this;
}

//! Builds a matrix which rotates a source vector to a look vector over an arbitrary axis
/** \param camPos: viewer position in world coord
\param center: object position in world-coord, rotation pivot
\param translation: object final translation from center
\param axis: axis to rotate about
\param from: source vector to rotate from
 */
template <class T>
inline void CMatrix4<T>::buildAxisAlignedBillboard(
		const vector3df &camPos,
		const vector3df &center,
		const vector3df &translation,
		const vector3df &axis,
		const vector3df &from)
{
	// axis of rotation
	vector3df up = axis;
	up.normalize();
	const vector3df forward = (camPos - center).normalize();
	const vector3df right = up.crossProduct(forward).normalize();

	// correct look vector
	const vector3df look = right.crossProduct(up);

	// rotate from to
	// axis multiplication by sin
	const vector3df vs = look.crossProduct(from);

	// cosine angle
	const f32 ca = from.dotProduct(look);

	vector3df vt(up * (1.f - ca));

	M[0] = static_cast<T>(vt.X * up.X + ca);
	M[5] = static_cast<T>(vt.Y * up.Y + ca);
	M[10] = static_cast<T>(vt.Z * up.Z + ca);

	vt.X *= up.Y;
	vt.Z *= up.X;
	vt.Y *= up.Z;

	M[1] = static_cast<T>(vt.X - vs.Z);
	M[2] = static_cast<T>(vt.Z + vs.Y);
	M[3] = 0;

	M[4] = static_cast<T>(vt.X + vs.Z);
	M[6] = static_cast<T>(vt.Y - vs.X);
	M[7] = 0;

	M[8] = static_cast<T>(vt.Z - vs.Y);
	M[9] = static_cast<T>(vt.Y + vs.X);
	M[11] = 0;

	setRotationCenter(center, translation);
}

//! Builds a combined matrix which translate to a center before rotation and translate afterward
template <class T>
inline void CMatrix4<T>::setRotationCenter(const vector3df &center, const vector3df &translation)
{
	M[12] = -M[0] * center.X - M[4] * center.Y - M[8] * center.Z + (center.X - translation.X);
	M[13] = -M[1] * center.X - M[5] * center.Y - M[9] * center.Z + (center.Y - translation.Y);
	M[14] = -M[2] * center.X - M[6] * center.Y - M[10] * center.Z + (center.Z - translation.Z);
	M[15] = (T)1.0;
}

/*!
	Generate texture coordinates as linear functions so that:
		u = Ux*x + Uy*y + Uz*z + Uw
		v = Vx*x + Vy*y + Vz*z + Vw
	The matrix M for this case is:
		Ux  Vx  0  0
		Uy  Vy  0  0
		Uz  Vz  0  0
		Uw  Vw  0  0
*/

template <class T>
inline CMatrix4<T> &CMatrix4<T>::buildTextureTransform(f32 rotateRad,
		const vector2df &rotatecenter,
		const vector2df &translate,
		const vector2df &scale)
{
	const f32 c = cosf(rotateRad);
	const f32 s = sinf(rotateRad);

	M[0] = (T)(c * scale.X);
	M[1] = (T)(s * scale.Y);
	M[2] = 0;
	M[3] = 0;

	M[4] = (T)(-s * scale.X);
	M[5] = (T)(c * scale.Y);
	M[6] = 0;
	M[7] = 0;

	M[8] = (T)(c * scale.X * rotatecenter.X + -s * rotatecenter.Y + translate.X);
	M[9] = (T)(s * scale.Y * rotatecenter.X + c * rotatecenter.Y + translate.Y);
	M[10] = 1;
	M[11] = 0;

	M[12] = 0;
	M[13] = 0;
	M[14] = 0;
	M[15] = 1;

	return *this;
}

// rotate about z axis, center ( 0.5, 0.5 )
template <class T>
inline CMatrix4<T> &CMatrix4<T>::setTextureRotationCenter(f32 rotateRad)
{
	const f32 c = cosf(rotateRad);
	const f32 s = sinf(rotateRad);
	M[0] = (T)c;
	M[1] = (T)s;

	M[4] = (T)-s;
	M[5] = (T)c;

	M[8] = (T)(0.5f * (s - c) + 0.5f);
	M[9] = (T)(-0.5f * (s + c) + 0.5f);

	return *this;
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setTextureTranslate(f32 x, f32 y)
{
	M[8] = (T)x;
	M[9] = (T)y;

	return *this;
}

template <class T>
inline void CMatrix4<T>::getTextureTranslate(f32 &x, f32 &y) const
{
	x = (f32)M[8];
	y = (f32)M[9];
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setTextureTranslateTransposed(f32 x, f32 y)
{
	M[2] = (T)x;
	M[6] = (T)y;
	return *this;
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setTextureScale(f32 sx, f32 sy)
{
	M[0] = (T)sx;
	M[5] = (T)sy;
	return *this;
}

template <class T>
inline void CMatrix4<T>::getTextureScale(f32 &sx, f32 &sy) const
{
	sx = (f32)M[0];
	sy = (f32)M[5];
}

template <class T>
inline CMatrix4<T> &CMatrix4<T>::setTextureScaleCenter(f32 sx, f32 sy)
{
	M[0] = (T)sx;
	M[5] = (T)sy;
	M[8] = (T)(0.5f - 0.5f * sx);
	M[9] = (T)(0.5f - 0.5f * sy);
	return *this;
}

// sets all matrix data members at once
template <class T>
inline CMatrix4<T> &CMatrix4<T>::setM(const T *data)
{
	memcpy(M, data, 16 * sizeof(T));
	return *this;
}

//! Compare two matrices using the equal method
template <class T>
inline bool CMatrix4<T>::equals(const CMatrix4<T> &other, const T tolerance) const
{
	for (s32 i = 0; i < 16; ++i)
		if (!core::equals(M[i], other.M[i], tolerance))
			return false;

	return true;
}

// Multiply by scalar.
template <class T>
inline CMatrix4<T> operator*(const T scalar, const CMatrix4<T> &mat)
{
	return mat * scalar;
}

//! Typedef for f32 matrix
typedef CMatrix4<f32> matrix4;

//! global const identity matrix
extern const matrix4 IdentityMatrix;

} // end namespace core
