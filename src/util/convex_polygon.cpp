// SPDX-FileCopyrightText: 2024 Minetest
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "convex_polygon.h"
#include <algorithm>

#ifndef SERVER

#include <IVideoDriver.h>

void ConvexPolygon::draw(video::IVideoDriver *driver, const video::SColor &color) const
{
	if (vertices.size() < 3)
		return;

	auto new_2d_vertex = [&color](const v2f &pos) -> video::S3DVertex {
		return video::S3DVertex(
				v3f(pos.X * 2.0f - 1.0f, -(pos.Y * 2.0f - 1.0f), 0.0f),
				v3f(), color, v2f());
	};

	std::vector<video::S3DVertex> s3d_vertices;
	s3d_vertices.reserve(vertices.size());
	for (const v2f &v : vertices)
		s3d_vertices.push_back(new_2d_vertex(v));

	std::vector<u16> index_list;
	index_list.reserve(vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i)
		index_list.push_back(i);

	video::SMaterial material;
	material.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
	material.ZWriteEnable = video::EZW_OFF;
	driver->setMaterial(material);

	driver->setTransform(video::ETS_PROJECTION, core::matrix4::EM4CONST_IDENTITY);
	driver->setTransform(video::ETS_VIEW, core::matrix4::EM4CONST_IDENTITY);
	driver->setTransform(video::ETS_WORLD, core::matrix4::EM4CONST_IDENTITY);

	driver->drawVertexPrimitiveList((void *)&s3d_vertices[0], (u32)s3d_vertices.size(),
			(void *)&index_list[0], (u32)index_list.size() - 2,
			video::EVT_STANDARD, // S3DVertex vertices
			scene::EPT_TRIANGLE_FAN,
			video::EIT_16BIT); // 16 bit indices
}

#endif // !def(SERVER)

f32 ConvexPolygon::area() const
{
	if (vertices.size() < 3)
		return 0.0f;
	// sum up the areas of all triangles
	f32 area = 0.0f;
	const v2f &v1 = vertices[0];
	for (size_t i = 2; i < vertices.size(); ++i) {
		const v2f &v2 = vertices[i-1];
		const v2f &v3 = vertices[i];
		// area of the triangle v1 v2 v3: 0.5 * det(d1 d2)
		// (winding order matters, for sign)
		v2f d1 = v2 - v1;
		v2f d2 = v3 - v1;
		area += 0.5f * (d1.X * d2.Y - d1.Y * d2.X);
	}
	return area;
}

ConvexPolygon ConvexPolygon::clip(v3f clip_line) const
{
	using polygon_iterator = std::vector<v2f>::const_iterator;

	// the return value
	ConvexPolygon clipped;
	auto &vertices_out = clipped.vertices;

	if (vertices.empty())
		return clipped; // emtpty

	// returns whether pos is in the not clipped half-space
	auto is_in = [&](const v2f &pos) {
		return pos.X * clip_line.X + pos.Y * clip_line.Y + clip_line.Z >= 0.0f;
	};

	auto is_out = [&](const v2f &pos) {
		return !is_in(pos);
	};

	// There are 3 cases:
	// * all vertices are clipped away
	// * no vertices are clipped away
	// * clip_line intersects the polygon at two points.
	//   There is hence one (possibly over the list end) contiguous sub-list of
	//   vertices that are all in (not clipped)
	//
	// is_in applied on the polygon vertices looks like this:
	// {in, in, in, out, out, in, in}
	//  last_in ^             ^ first_in
	//    first_out ^    ^ last_out
	//
	// We need to cut the polygon between the out-in and in-out vertex pairs.

	// find the first vertex that is in/out of the clip_line, and also the vertex
	// before
	polygon_iterator first_in, first_out;
	polygon_iterator last_out, last_in;
	if (is_in(vertices[0])) {
		first_out = std::find_if(vertices.begin() + 1, vertices.end(), is_out);
		if (first_out == vertices.end()) {
			// all are in
			clipped = {vertices};
			return clipped;
		}
		last_in = first_out - 1;
		first_in = std::find_if(first_out + 1, vertices.end(), is_in);
		last_out = first_in - 1;
		if (first_in == vertices.end())
			first_in = vertices.begin(); // we already checked that the 0th is in
	} else {
		first_in = std::find_if(vertices.begin() + 1, vertices.end(), is_in);
		if (first_in == vertices.end()) {
			// all are out
			return clipped; // empty
		}
		last_out = first_in - 1;
		first_out = std::find_if(first_in + 1, vertices.end(), is_out);
		last_in = first_out - 1;
		if (first_out == vertices.end())
			first_out = vertices.begin(); // we already checked that the 0th is out
	}

	// copy all vertices that are in
	if (first_in <= last_in) {
		vertices_out.reserve((last_in - first_in) + 1 + 2);
		vertices_out.insert(vertices_out.end(), first_in, last_in + 1);
	} else {
		vertices_out.reserve((vertices.end() - first_in) + (last_in - vertices.begin()) + 1 + 2);
		vertices_out.insert(vertices_out.end(), first_in, vertices.end());
		vertices_out.insert(vertices_out.end(), vertices.begin(), last_in + 1);
	}

	auto split_edge = [&](const v2f &p1, const v2f &p2) {
		auto homogenize = [](const v2f &p) { return v3f(p.X, p.Y, 1.0f); };
		// find a pos that is on clip_line and between p1 and p2
		// compute: meet(clip_line, join(p1, p2))
		v3f pos_homo = clip_line.crossProduct(homogenize(p1).crossProduct(homogenize(p2)));
		// dehomogenize
		return v2f(pos_homo.X / pos_homo.Z, pos_homo.Y / pos_homo.Z);
	};

	// split in-out pair
	vertices_out.push_back(split_edge(*last_in, *first_out));

	// split out-in pair
	vertices_out.push_back(split_edge(*last_out, *first_in));

	return clipped;
}
