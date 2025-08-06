// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"

namespace gui
{
enum EGUI_ALIGNMENT
{
	//! Aligned to parent's top or left side (default)
	EGUIA_UPPERLEFT = 0,
	//! Aligned to parent's bottom or right side
	EGUIA_LOWERRIGHT,
	//! Aligned to the center of parent
	EGUIA_CENTER,
	//! Stretched to fit parent
	EGUIA_SCALE
};

//! Names for alignments
const c8 *const GUIAlignmentNames[] = {
		"upperLeft",
		"lowerRight",
		"center",
		"scale",
		0,
	};

} // namespace gui
