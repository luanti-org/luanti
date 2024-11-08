// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 Vitaliy Lobachevskiy

#include "mesh_compare.h"
#include <algorithm>
#include <map>
#include <stdexcept>

static std::vector<Triangle> expandMesh(const std::vector<video::S3DVertex> &vertices, const std::vector<u16> &indices)
{
	const int n_indices = indices.size();
	const int n_triangles = n_indices / 3;
	if (n_indices % 3)
		throw std::invalid_argument("got fractional number of triangles");

	std::vector<Triangle> ret(n_triangles);
	for (int i_triangle = 0; i_triangle < n_triangles; i_triangle++) {
		ret.at(i_triangle) = {
			vertices.at(indices.at(3 * i_triangle)),
			vertices.at(indices.at(3 * i_triangle + 1)),
			vertices.at(indices.at(3 * i_triangle + 2)),
		};
	}

	return ret;
}

/// Sorts triangle vertices, keeping winding order.
static Triangle sortTriangle(Triangle t)
{
	if (t[0] < t[1] && t[0] < t[2]) return {t[0], t[1], t[2]};
	if (t[1] < t[2] && t[1] < t[0]) return {t[1], t[2], t[0]};
	if (t[2] < t[0] && t[2] < t[1]) return {t[2], t[0], t[1]};
	throw std::invalid_argument("got bad triangle");
}

static std::vector<Triangle> canonicalizeMesh(const std::vector<video::S3DVertex> &vertices, const std::vector<u16> &indices)
{
	std::vector<Triangle> mesh = expandMesh(vertices, indices);
	for (auto &triangle: mesh)
		triangle = sortTriangle(triangle);
	std::sort(std::begin(mesh), std::end(mesh));
	return mesh;
}

bool checkMeshEqual(const std::vector<video::S3DVertex> &vertices, const std::vector<u16> &indices, const std::vector<Triangle> &expected)
{
	auto actual = canonicalizeMesh(vertices, indices);
	return actual == expected;
}

bool checkMeshEqual(const std::vector<video::S3DVertex> &vertices, const std::vector<u16> &indices, const std::vector<Quad> &expected)
{
	using QuadRefCount = std::array<int, 4>;
	struct QuadRef {
		unsigned quad_id;
		int quad_part;
	};

	std::vector<QuadRefCount> refs(expected.size());
	std::map<Triangle, QuadRef> tris;
	for (unsigned k = 0; k < expected.size(); k++) {
		auto &&quad = expected[k];
		// There are 2 ways to split a quad into two triangles. So for each quad,
		// the mesh must contain either triangles 0 and 1, or triangles 2 and 3,
		// from the following list. No more, no less.
		tris.insert({sortTriangle({quad[0], quad[1], quad[2]}), {k, 0}});
		tris.insert({sortTriangle({quad[0], quad[2], quad[3]}), {k, 1}});
		tris.insert({sortTriangle({quad[0], quad[1], quad[3]}), {k, 2}});
		tris.insert({sortTriangle({quad[1], quad[2], quad[3]}), {k, 3}});
	}

	auto actual = canonicalizeMesh(vertices, indices);
	for (auto &&tri: actual) {
		auto itri = tris.find(tri);
		if (itri == tris.end())
			return false;
		refs[itri->second.quad_id][itri->second.quad_part] += 1;
	}

	for (unsigned k = 0; k < expected.size(); k++) {
		if (refs[k] != QuadRefCount{1, 1, 0, 0} && refs[k] != QuadRefCount{0, 0, 1, 1})
			return false;
	}

	return true;
}
