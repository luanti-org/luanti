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
		matrix4 trs;
		rotation.getMatrix_transposed(trs);
		auto scale_row = [&trs](int col, f32 scale) {
			int i = 4 * col;
			trs[i] *= scale;
			trs[i + 1] *= scale;
			trs[i + 2] *= scale;
		};
		scale_row(0, scale.X);
		scale_row(1, scale.Y);
		scale_row(2, scale.Z);
		trs.setTranslation(translation);
		return trs;
	}
};

} // end namespace core
