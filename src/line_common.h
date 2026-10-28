// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#pragma once

#include "irrlichttypes.h"

/*
	Shared between LineSAO and LineCAO - both sides serialize/deserialize
	these as raw bytes, so they must have the same values on both ends
*/

// Prefixed to avoid clashing with the OPAQUE/TRANSPARENT macros defines on windows
enum class LineAlphaMode : u8
{
	LINE_ALPHA_OPAQUE,
	LINE_ALPHA_BLEND,
	LINE_ALPHA_ADD,
};

enum class LineShape : u8
{
	LINE_SHAPE_LINE,
	LINE_SHAPE_TUBE,
};

// Wire commands for LineSAO/LineCAO's own private message protocol
enum class LineCommand : u8
{
	LINE_CMD_SET_POINTS,
	LINE_CMD_SET_PROPERTIES,
};
