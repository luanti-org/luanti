// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#define ARRLEN(x) (sizeof(x) / sizeof((x)[0]))

#define MYMIN(a, b) ((a) < (b) ? (a) : (b))

#define MYMAX(a, b) ((a) > (b) ? (a) : (b))

// Requires <algorithm>
#define CONTAINS(c, v) (std::find((c).begin(), (c).end(), (v)) != (c).end())

// Requires <algorithm>
#define SORT_AND_UNIQUE(c) do { \
	std::sort((c).begin(), (c).end()); \
	(c).erase(std::unique((c).begin(), (c).end()), (c).end()); \
	} while (0)

// To disable copy constructors and assignment operations for some class
// 'Foobar', add the macro DISABLE_CLASS_COPY(Foobar) in the class definition.
// Note this also disables copying for any classes derived from 'Foobar' as well
// as classes having a 'Foobar' member.
#define DISABLE_CLASS_COPY(C)        \
	C(const C &) = delete;           \
	C &operator=(const C &) = delete;

// If you have used DISABLE_CLASS_COPY with a class but still want to permit moving
// use this macro to add the default move constructors back.
#define ALLOW_CLASS_MOVE(C)      \
	C(C &&other) = default;      \
	C &operator=(C &&) = default;

