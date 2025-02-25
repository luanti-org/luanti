// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "database.h"
#include "irrlichttypes.h"


/****************
 * The position encoding is a bit messed up because negative
 * values were not taken into account properly.
 */
s64 MapDatabase::getBlockAsInteger(const v3s16 &pos)
{
	return (u64) pos.Z * 0x1000000 +
		(u64) pos.Y * 0x1000 +
		(u64) pos.X;
}


v3s16 MapDatabase::getIntegerAsBlock(s64 i)
{
	// Missing offset so that all negative coordinates become non-negative
	i = i + 0x800800800;
	pos.X = (i & 0xFFF) - 0x800;
	pos.Y = ((i >> 12) & 0xFFF) - 0x800;
	pos.Z = ((i >> 24) & 0xFFF) - 0x800;
	return pos;
}
