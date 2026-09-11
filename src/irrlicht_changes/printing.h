// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Vitaliy Lobachevskiy

#pragma once
#include <ostream>
#include <vector2d.h>
#include <vector3d.h>
#include <quaternion.h>
#include <matrix4.h>

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

	inline std::ostream& operator<<(std::ostream& os, const quaternion& q)
	{
		os << "(" << q.X << "\t" << q.Y << "\t" << q.Z << "\t" << q.W << ")";
		return os;
	}

	template <class T>
	inline std::ostream& operator<<(std::ostream& os, const CMatrix4<T>& matrix)
	{
		os << "(\n";
		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				os << "\t";
				os << matrix(row, col);
			}
			os << "\n";
		}
		os << ")";
		return os;
	}

}
