// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "database.h"
#include "irrlichttypes.h"


/****************
 * Black magic! *
 ****************
 * The position hashing is very messed up.
 * It's a lot more complicated than it looks.
 */

static inline s16 unsigned_to_signed(u16 i, u16 max_positive)
{
	if (i < max_positive) {
		return i;
	}

	return i - (max_positive * 2);
}


// Modulo of a negative number does not work consistently in C
static inline s64 pythonmodulo(s64 i, s16 mod)
{
	if (i >= 0) {
		return i % mod;
	}
	return mod - ((-i) % mod);
}


s64 MapDatabase::getBlockAsInteger(const v3s16 &pos)
{
	return (u64) pos.Z * 0x1000000 +
		(u64) pos.Y * 0x1000 +
		(u64) pos.X;
}


v3s16 MapDatabase::getIntegerAsBlock(s64 i)
{
	v3s16 pos;
	pos.X = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	i = (i - pos.X) / 4096;
	pos.Y = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	i = (i - pos.Y) / 4096;
	pos.Z = unsigned_to_signed(pythonmodulo(i, 4096), 2048);
	return pos;
}

