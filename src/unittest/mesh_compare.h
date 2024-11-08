// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Vitaliy Lobachevskiy

#pragma once
#include <array>
#include <vector>
#include <irrlichttypes.h>
#include <S3DVertex.h>

/// Represents a triangle as three vertices.
/// “Smallest” (according to <) vertex is expected to be first, others should follow in the counter-clockwise order.
using Triangle = std::array<video::S3DVertex, 3>;

/// Represents a quad as four vertices.
/// Vertices should be in the counter-clockwise order.
using Quad = std::array<video::S3DVertex, 4>;

/// Compare two meshes for equality.
/// @param vertices Vertices of the first mesh. Order doesn’t matter.
/// @param indices Indices of the first mesh. Triangle order doesn’t matter. Vertex order in a triangle only matters for winding.
/// @param expected The second mesh, in an expanded form. Must be sorted.
/// @returns Whether the two meshes are equal.
[[nodiscard]] bool checkMeshEqual(const std::vector<video::S3DVertex> &vertices, const std::vector<u16> &indices, const std::vector<Triangle> &expected);

/// Compare two meshes for equality.
/// @param vertices Vertices of the first mesh. Order doesn’t matter.
/// @param indices Indices of the first mesh. Triangle order doesn’t matter. Vertex order in a triangle only matters for winding.
/// @param expected The second mesh, in a quad form.
/// @returns Whether the two meshes are equal.
/// @note There are two ways to split a quad into 2 triangles; either is allowed.
[[nodiscard]] bool checkMeshEqual(const std::vector<video::S3DVertex> &vertices, const std::vector<u16> &indices, const std::vector<Quad> &expected);
