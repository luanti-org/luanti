#pragma once

#include "irrMath.h"
#include <matrix4.h>
#include <variant>
#include <vector3d.h>
#include <quaternion.h>

namespace irr
{
namespace core
{

struct Transform {
	vector3df translation;
	quaternion rotation;
	vector3df scale{1};

	// Tries to decompose the matrix, if there is one.
	static Transform decompose(const core::matrix4 &mat)
	{
		auto scale = mat.getScale();
		return {
			mat.getTranslation(),
			quaternion(mat.getRotationDegrees(scale) * DEGTORAD),
			scale,
		};
	}

	matrix4 buildMatrix() const
	{
		matrix4 T;
		T.setTranslation(translation);
		matrix4 R;
		// TODO this is sussy. probably shouldn't be doing this.
		rotation.getMatrix_transposed(R);
		matrix4 S;
		S.setScale(scale);
		return T * R * S;
	}
};

} // end namespace core
} // end namespace irr