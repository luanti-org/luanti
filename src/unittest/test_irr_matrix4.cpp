// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "catch.h"
#include "catch_amalgamated.hpp"
#include "irrMath.h"
#include "matrix4.h"
#include "irr_v3d.h"

using matrix4 = core::matrix4;

static bool matrix_equals(const matrix4 &a, const matrix4 &b) {
	return a.equals(b, 0.00001f);
}

constexpr v3f x{1, 0, 0};
constexpr v3f y{0, 1, 0};
constexpr v3f z{0, 0, 1};

TEST_CASE("matrix4") {

SECTION("setRotationRadians") {
	SECTION("rotation order is ZYX (matrix notation)") {
		v3f rot{1, 2, 3};
		matrix4 X, Y, Z, ZYX;
		X.setRotationRadians({rot.X, 0, 0});
		Y.setRotationRadians({0, rot.Y, 0});
		Z.setRotationRadians({0, 0, rot.Z});
		ZYX.setRotationRadians(rot);
		CHECK(!matrix_equals(X * Y * Z, ZYX));
		CHECK(!matrix_equals(X * Z * Y, ZYX));
		CHECK(!matrix_equals(Y * X * Z, ZYX));
		CHECK(!matrix_equals(Y * Z * X, ZYX));
		CHECK(!matrix_equals(Z * X * Y, ZYX));
		CHECK(matrix_equals(Z * Y * X, ZYX));
	}

	const f32 quarter_turn = core::PI / 2;

	// See https://en.wikipedia.org/wiki/Right-hand_rule#/media/File:Cartesian_coordinate_system_handedness.svg
	// for a visualization of what handedness means for rotations

	SECTION("rotation is right-handed") {
		SECTION("rotation around the X-axis is Z-up, counter-clockwise") {
			matrix4 X;
			X.setRotationRadians({quarter_turn, 0, 0});
			CHECK(X.transformVect(x).equals(x));
			CHECK(X.transformVect(y).equals(z));
			CHECK(X.transformVect(z).equals(-y));
		}

		SECTION("rotation around the Y-axis is Z-up, clockwise") {
			matrix4 Y;
			Y.setRotationRadians({0, quarter_turn, 0});
			CHECK(Y.transformVect(y).equals(y));
			CHECK(Y.transformVect(x).equals(-z));
			CHECK(Y.transformVect(z).equals(x));
		}

		SECTION("rotation around the Z-axis is Y-up, counter-clockwise") {
			matrix4 Z;
			Z.setRotationRadians({0, 0, quarter_turn});
			CHECK(Z.transformVect(z).equals(z));
			CHECK(Z.transformVect(x).equals(y));
			CHECK(Z.transformVect(y).equals(-x));
		}
	}
}

SECTION("getScale") {
	SECTION("correctly gets the length of each row of the 3x3 submatrix") {
		matrix4 A(
			1, 2, 3, 0,
			4, 5, 6, 0,
			7, 8, 9, 0,
			0, 0, 0, 1
		);
		v3f scale = A.getScale();
		CHECK(scale.equals(v3f(
			v3f(1, 2, 3).getLength(),
			v3f(4, 5, 6).getLength(),
			v3f(7, 8, 9).getLength()
		)));
	}
}

SECTION("getRotationRadians") {
	auto test_rotation_degrees = [](v3f rad, v3f scale) {
		matrix4 S;
		S.setScale(scale);
		matrix4 R;
		R.setRotationRadians(rad);
		v3f rot = (R * S).getRotationRadians();
		matrix4 B;
		B.setRotationRadians(rot);
		CHECK(matrix_equals(R, B));
	};
	SECTION("returns a rotation equivalent to the original rotation") {
		test_rotation_degrees({1.0f, 2.0f, 3.0f}, v3f(1));
		Catch::Generators::RandomFloatingGenerator<f32> gen_angle(0.0f, 2 * core::PI, Catch::getSeed());
		Catch::Generators::RandomFloatingGenerator<f32> gen_scale(0.1f, 10, Catch::getSeed());
		auto draw = [](auto gen) {
			f32 f = gen.get();
			gen.next();
			return f;
		};
		auto draw_v3f = [&](auto gen) {
			return v3f{draw(gen), draw(gen), draw(gen)};
		};
		for (int i = 0; i < 1000; ++i)
			test_rotation_degrees(draw_v3f(gen_angle), draw_v3f(gen_scale));
		for (f32 i = 0; i < 4; ++i)
		for (f32 j = 0; j < 4; ++j)
		for (f32 k = 0; k < 4; ++k) {
			v3f rad = core::PI / 4.0f * v3f(i, j, k);
			for (int l = 0; l < 100; ++l) {
				test_rotation_degrees(rad, draw_v3f(gen_scale));
			}
		}
	}
}

}
