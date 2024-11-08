// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 juhdanad, Daniel Juhasz <juhdanad@gmail.com>

#pragma once

#include "voxelalgorithms.h"
#include "util/pointedthing.h"

//! Sorts PointedThings based on their distance.
struct RaycastSort
{
	bool operator() (const PointedThing &pt1, const PointedThing &pt2) const;
};

//! Describes the state of a raycast.
class RaycastState
{
public:
	/*!
	 * Creates a raycast.
	 * @param objects_pointable if false, only nodes will be found
	 * @param liquids pointable if false, liquid nodes won't be found
	 */
	RaycastState(const core::line3d<f32> &shootline, bool objects_pointable,
		bool liquids_pointable, const std::optional<Pointabilities> &pointabilities);

	//! Shootline of the raycast.
	core::line3d<f32> m_shootline;
	//! Iterator to store the progress of the raycast.
	voxalgo::VoxelLineIterator m_iterator;
	//! Previous tested node during the raycast.
	v3s16 m_previous_node;

	/*!
	 * This priority queue stores the found pointed things
	 * waiting to be returned.
	 */
	std::priority_queue<PointedThing, std::vector<PointedThing>, RaycastSort> m_found;

	bool m_objects_pointable;
	bool m_liquids_pointable;
	const std::optional<Pointabilities> m_pointabilities;

	//! The code needs to search these nodes around the center node.
	core::aabbox3d<s16> m_search_range { 0, 0, 0, 0, 0, 0 };

	//! If true, the Environment will initialize this state.
	bool m_initialization_needed = true;
};

/*!
 * Checks if a line and a box intersects.
 * @param[in]  box              box to test collision
 * @param[in]  start            starting point of the line
 * @param[in]  dir              direction and length of the line
 * @param[out] collision_point  first point of the collision
 * @param[out] collision_normal normal vector at the collision, points
 * outwards of the surface. If start is in the box, zero vector.
 * @returns true if a collision point was found
 */
bool boxLineCollision(const aabb3f &box, v3f start, v3f dir,
	v3f *collision_point, v3f *collision_normal);

bool boxLineCollision(const aabb3f &box, v3f box_rotation,
	v3f start, v3f dir,
	v3f *collision_point, v3f *collision_normal, v3f *raw_collision_normal);
