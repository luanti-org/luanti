// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "database.h"
#include "irrlichttypes.h"


/****************
 * The position encoding is a bit messed up because negative
 * values were not taken into account.
 * But this also maps 0,0,0 to 0, which is nice, and we mostly
 * need forward encoding in Luanti.
 */
s64 MapDatabase::getBlockAsInteger(const v3s16 &pos)
{
	return ((s64) pos.Z << 24) + ((s64) pos.Y << 12) + pos.X;
}


v3s16 MapDatabase::getIntegerAsBlock(s64 i)
{
	// Offset so that all negative coordinates become non-negative
	i = i + 0x800800800;
	// Which is now easier to decode using simple bit masks:
	return { (s16)( (i        & 0xFFF) - 0x800),
	         (s16)(((i >> 12) & 0xFFF) - 0x800),
	         (s16)(((i >> 24) & 0xFFF) - 0x800) };
}
