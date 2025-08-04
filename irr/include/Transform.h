#pragma once

#include "irrMath.h"
#include <matrix4.h>
#include <vector3d.h>
#include <quaternion.h>

namespace core
{

struct Transform {
	vector3df translation;
	quaternion rotation;
	vector3df scale{1};

	Transform interpolate(Transform to, f32 time) const
	{
		core::quaternion interpolated_rotation;
		interpolated_rotation.slerp(rotation, to.rotation, time);
		return {
			to.translation.getInterpolated(translation, time),
			interpolated_rotation,
			to.scale.getInterpolated(scale, time),
		};
	}

	matrix4 buildMatrix() const
	{
		matrix4 T;
		T.setTranslation(translation);
		matrix4 R;
		rotation.getMatrix_transposed(R);
		matrix4 S;
		S.setScale(scale);
		return T * R * S;
	}
};

} // end namespace core
