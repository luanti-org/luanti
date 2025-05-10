#include <matrix4.h>
#include "irr_v3d.h"

/// @note This is not consistent with Irrlicht's setRotationRadians
void setPitchYawRollRad(core::matrix4 &m, v3f rot);

/// @note This is not consistent with Irrlicht's getRotationRadians
v3f getPitchYawRollRad(const core::matrix4 &m);
