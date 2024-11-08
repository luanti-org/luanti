// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"

/// Direction in the 6D format. g_27dirs contains corresponding vectors.
/// Here P means Positive, N stands for Negative.
enum Direction6D {
// 0
	D6D_ZP,
	D6D_YP,
	D6D_XP,
	D6D_ZN,
	D6D_YN,
	D6D_XN,
// 6
	D6D_XN_YP,
	D6D_XP_YP,
	D6D_YP_ZP,
	D6D_YP_ZN,
	D6D_XN_ZP,
	D6D_XP_ZP,
	D6D_XN_ZN,
	D6D_XP_ZN,
	D6D_XN_YN,
	D6D_XP_YN,
	D6D_YN_ZP,
	D6D_YN_ZN,
// 18
	D6D_XN_YP_ZP,
	D6D_XP_YP_ZP,
	D6D_XN_YP_ZN,
	D6D_XP_YP_ZN,
	D6D_XN_YN_ZP,
	D6D_XP_YN_ZP,
	D6D_XN_YN_ZN,
	D6D_XP_YN_ZN,
// 26
	D6D,

// aliases
	D6D_BACK   = D6D_ZP,
	D6D_TOP    = D6D_YP,
	D6D_RIGHT  = D6D_XP,
	D6D_FRONT  = D6D_ZN,
	D6D_BOTTOM = D6D_YN,
	D6D_LEFT   = D6D_XN,
};

/// Direction in the wallmounted format.
/// P is Positive, N is Negative.
enum DirectionWallmounted {
	// The 6 wallmounted directions
	DWM_YP,
	DWM_YN,
	DWM_XP,
	DWM_XN,
	DWM_ZP,
	DWM_ZN,
	// There are 6 wallmounted directions, but 8 possible states (3 bits).
	// So we have 2 additional states, which drawtypes might use for
	// special ("S") behavior.
	DWM_S1,
	DWM_S2,
	DWM_COUNT
};

extern const v3s16 g_6dirs[6];

extern const v3s16 g_7dirs[7];

extern const v3s16 g_26dirs[26];

// 26th is (0,0,0)
extern const v3s16 g_27dirs[27];

extern const u8 wallmounted_to_facedir[DWM_COUNT];

extern const v3s16 wallmounted_dirs[DWM_COUNT];

extern const v3s16 facedir_dirs[32];

extern const v3s16 fourdir_dirs[4];
