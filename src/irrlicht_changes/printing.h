// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Vitaliy Lobachevskiy

#pragma once
#include <ostream>
#include <vector2d.h>
#include <vector3d.h>

namespace core {

	template <class T>
	std::ostream &operator<< (std::ostream &os, vector2d<T> vec)
	{
		return os << "(" << vec.X << "," << vec.Y << ")";
	}

	template <class T>
	std::ostream &operator<< (std::ostream &os, vector3d<T> vec)
	{
		return os << "(" << vec.X << "," << vec.Y << "," << vec.Z << ")";
	}

}
