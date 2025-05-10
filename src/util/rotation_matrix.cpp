#include "rotation_matrix.h"

void setPitchYawRollRad(core::matrix4 &m, v3f rot)
{
	f64 a1 = rot.Z, a2 = rot.X, a3 = rot.Y;
	f64 c1 = cos(a1), s1 = sin(a1);
	f64 c2 = cos(a2), s2 = sin(a2);
	f64 c3 = cos(a3), s3 = sin(a3);
	f32 *M = m.pointer();

	M[0] = s1 * s2 * s3 + c1 * c3;
	M[1] = s1 * c2;
	M[2] = s1 * s2 * c3 - c1 * s3;

	M[4] = c1 * s2 * s3 - s1 * c3;
	M[5] = c1 * c2;
	M[6] = c1 * s2 * c3 + s1 * s3;

	M[8] = c2 * s3;
	M[9] = -s2;
	M[10] = c2 * c3;
}

v3f getPitchYawRollRad(const core::matrix4 &m)
{
	const f32 *M = m.pointer();

	f64 a1 = atan2(M[1], M[5]);
	f32 c2 = std::sqrt((f64)M[10]*M[10] + (f64)M[8]*M[8]);
	f32 a2 = atan2f(-M[9], c2);
	f64 c1 = cos(a1);
	f64 s1 = sin(a1);
	f32 a3 = atan2f(s1*M[6] - c1*M[2], c1*M[0] - s1*M[4]);

	return v3f(a2, a3, a1);
}
