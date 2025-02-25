// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 juhdanad, Daniel Juhasz <juhdanad@gmail.com>

#include "raycast.h"
#include "irr_v3d.h"
#include "irr_aabb3d.h"
#include <quaternion.h>
#include "constants.h"

bool RaycastSort::operator() (const PointedThing &pt1,
	const PointedThing &pt2) const
{
	// "nothing" cannot be sorted
	assert(pt1.type != POINTEDTHING_NOTHING);
	assert(pt2.type != POINTEDTHING_NOTHING);
	f32 pt1_distSq = pt1.distanceSq;

	// Add some bonus when one of them is an object
	if (pt1.type != pt2.type) {
		if (pt1.type == POINTEDTHING_OBJECT)
			pt1_distSq -= BS * BS;
		else if (pt2.type == POINTEDTHING_OBJECT)
			pt1_distSq += BS * BS;
	}

	// returns false if pt1 is nearer than pt2
	if (pt1_distSq < pt2.distanceSq) {
		return false;
	}

	if (pt1_distSq == pt2.distanceSq) {
		// Sort them to allow only one order
		if (pt1.type == POINTEDTHING_OBJECT)
			return (pt2.type == POINTEDTHING_OBJECT
				&& pt1.object_id < pt2.object_id);

		return (pt2.type == POINTEDTHING_OBJECT
				|| pt1.node_undersurface < pt2.node_undersurface);
	}
	return true;
}


RaycastState::RaycastState(const core::line3d<f32> &shootline,
	bool objects_pointable, bool liquids_pointable,
	const std::optional<Pointabilities> &pointabilities) :
	m_shootline(shootline),
	m_iterator(shootline.start / BS, shootline.getVector() / BS),
	m_previous_node(m_iterator.m_current_node_pos),
	m_objects_pointable(objects_pointable),
	m_liquids_pointable(liquids_pointable),
	m_pointabilities(pointabilities)
{
}


bool boxLineCollision(const aabb3f &box, const v3f start,
	const v3f dir, v3f *collision_point, v3f *collision_normal)
{
	if (box.isPointInside(start)) {
		*collision_point = start;
		collision_normal->set(0, 0, 0);
		return true;
	}
	float m = 0;

	// Test X collision
	if (dir.X != 0) {
		if (dir.X > 0)
			m = (box.MinEdge.X - start.X) / dir.X;
		else
			m = (box.MaxEdge.X - start.X) / dir.X;

		if (m >= 0 && m <= 1) {
			*collision_point = start + dir * m;
			if ((collision_point->Y >= box.MinEdge.Y)
					&& (collision_point->Y <= box.MaxEdge.Y)
					&& (collision_point->Z >= box.MinEdge.Z)
					&& (collision_point->Z <= box.MaxEdge.Z)) {
				collision_normal->set((dir.X > 0) ? -1 : 1, 0, 0);
				return true;
			}
		}
	}

	// Test Y collision
	if (dir.Y != 0) {
		if (dir.Y > 0)
			m = (box.MinEdge.Y - start.Y) / dir.Y;
		else
			m = (box.MaxEdge.Y - start.Y) / dir.Y;

		if (m >= 0 && m <= 1) {
			*collision_point = start + dir * m;
			if ((collision_point->X >= box.MinEdge.X)
					&& (collision_point->X <= box.MaxEdge.X)
					&& (collision_point->Z >= box.MinEdge.Z)
					&& (collision_point->Z <= box.MaxEdge.Z)) {
				collision_normal->set(0, (dir.Y > 0) ? -1 : 1, 0);
				return true;
			}
		}
	}

	// Test Z collision
	if (dir.Z != 0) {
		if (dir.Z > 0)
			m = (box.MinEdge.Z - start.Z) / dir.Z;
		else
			m = (box.MaxEdge.Z - start.Z) / dir.Z;

		if (m >= 0 && m <= 1) {
			*collision_point = start + dir * m;
			if ((collision_point->X >= box.MinEdge.X)
					&& (collision_point->X <= box.MaxEdge.X)
					&& (collision_point->Y >= box.MinEdge.Y)
					&& (collision_point->Y <= box.MaxEdge.Y)) {
				collision_normal->set(0, 0, (dir.Z > 0) ? -1 : 1);
				return true;
			}
		}
	}
	return false;
}

bool boxLineCollision(const aabb3f &box, v3f rotation_radians,
	v3f start, v3f dir,
	v3f *collision_point, v3f *collision_normal, v3f *raw_collision_normal)
{
	// Inversely transform the ray rather than rotating the box faces;
	// this allows us to continue using a simple ray - AABB intersection
	core::quaternion rot(rotation_radians);
	rot.makeInverse();

	bool collision = boxLineCollision(box, rot * start, rot * dir, collision_point, collision_normal);
	if (!collision)
		return collision;

	// Transform the results back
	rot.makeInverse();
	*collision_point = rot * *collision_point;
	*raw_collision_normal = *collision_normal;
	*collision_normal = rot * *collision_normal;
	return collision;
}
