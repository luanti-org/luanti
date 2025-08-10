// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"
#include "catch_amalgamated.hpp"
#include "irrMath.h"
#include "matrix4.h"
#include "irr_v3d.h"
#include "quaternion.h"
#include <functional>

// Irrlicht provides three different representations of rotations:
// - Euler angles in radians (or degrees, but that doesn't matter much);
// - Quaternions;
// - Rotation matrices.
// These tests ensure that converting between these representations is rotation-preserving.

using matrix4 = core::matrix4;
using quaternion = core::quaternion;

// Despite the internal usage of doubles, matrix4::setRotationRadians
// simply incurs component-wise errors of the order 1e-3.
const f32 tolerance = 1e-2f;

static bool matrix_equals(const matrix4 &mat, const matrix4 &mat2)
{
	return mat.equals(mat2, tolerance);
}

static bool euler_angles_equiv(v3f rad, v3f rad2)
{
	matrix4 mat, mat2;
	mat.setRotationRadians(rad);
	mat2.setRotationRadians(rad2);
	return matrix_equals(mat, mat2);
}

static void test_euler_angles_rad(const std::function<void(v3f)> &test_euler_radians)
{
	Catch::Generators::RandomFloatingGenerator<f32> gen(0.0f, 2 * core::PI, Catch::getSeed());
	auto random_angle = [&gen]() {
		f32 f = gen.get();
		gen.next();
		return f;
	};
	for (int i = 0; i < 1000; ++i)
		test_euler_radians(v3f{random_angle(), random_angle(), random_angle()});
	for (int i = 0; i < 4; i++)
	for (int j = 0; j < 4; j++)
	for (int k = 0; k < 4; k++) {
		v3f rad = core::PI / 4.0f * v3f(i, j, k);
		test_euler_radians(rad);
		// Test very slightly nudged, "almost-perfect" rotations to make sure
		// that the conversions are relatively stable at extremal points
		for (int l = 0; l < 10; ++l) {
			v3f jitter = v3f{random_angle(), random_angle(), random_angle()} * 0.001f;
			test_euler_radians(rad + jitter);
		}
	}
}

TEST_CASE("rotations") {

SECTION("euler-to-quaternion conversion") {
	test_euler_angles_rad([](v3f rad) {
		core::matrix4 rot, rot_quat;
		rot.setRotationRadians(rad);
		quaternion q(rad);
		q.getMatrix(rot_quat);
		// Check equivalence of the rotations via matrices
		CHECK(matrix_equals(rot, rot_quat));
	});
}

// Now that we've already tested the conversion to quaternions,
// this essentially primarily tests the quaternion to euler conversion
SECTION("quaternion-euler roundtrip") {
	test_euler_angles_rad([](v3f rad) {
		quaternion q(rad);
		v3f rad2;
		q.toEuler(rad2);
		CHECK(euler_angles_equiv(rad, rad2));
	});
}

SECTION("matrix-quaternion roundtrip") {
	test_euler_angles_rad([](v3f rad) {
		matrix4 mat;
		mat.setRotationRadians(rad);
		quaternion q(mat);
		matrix4 mat2;
		q.getMatrix(mat2);
		CHECK(matrix_equals(mat, mat2));
	});
}

SECTION("matrix-euler roundtrip") {
	test_euler_angles_rad([](v3f rad) {
		matrix4 mat, mat2;
		mat.setRotationRadians(rad);
		v3f rad2 = mat.getRotationRadians();
		mat2.setRotationRadians(rad2);
		CHECK(matrix_equals(mat, mat2));
	});
}

}
