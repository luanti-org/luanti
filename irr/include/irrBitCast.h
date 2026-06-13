// Copyright (C) 2026 Lars Müller
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

// This belongs in irrTypes.h next to IRR_DOWN_CAST,
// but that would pollute all include chains with <cstring> and <type_traits> which is no bueno

#pragma once

#include <cstring>
#include <type_traits>

// TODO in C++ 20 we'll be able to replace this with bit_cast
template <typename T, typename U>
T irrBitCast(const U &x)
{
	T res;
	static_assert(sizeof(T) == sizeof(U));
	static_assert(std::is_trivially_copyable_v<T>);
	static_assert(std::is_trivially_copyable_v<U>);
	std::memcpy(&res, &x, sizeof(U));
	return res;
}
