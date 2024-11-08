// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include <vector>

class Map;
class IGameDef;
class Environment;
class ActiveObject;

enum CollisionType
{
	COLLISION_NODE,
	COLLISION_OBJECT,
};

enum CollisionAxis
{
	COLLISION_AXIS_NONE = -1,
	COLLISION_AXIS_X,
	COLLISION_AXIS_Y,
	COLLISION_AXIS_Z,
};

struct CollisionInfo
{
	CollisionInfo() = default;

	CollisionType type = COLLISION_NODE;
	CollisionAxis axis = COLLISION_AXIS_NONE;
	v3s16 node_p = v3s16(-32768,-32768,-32768); // COLLISION_NODE
	ActiveObject *object = nullptr; // COLLISION_OBJECT
	v3f new_pos;
	v3f old_speed;
	v3f new_speed;
	int plane = -1;
};

struct collisionMoveResult
{
	collisionMoveResult() = default;

	bool touching_ground = false;
	bool collides = false;
	bool standing_on_object = false;
	std::vector<CollisionInfo> collisions;
};

/// @brief Moves using a single iteration; speed should not exceed pos_max_d/dtime
/// @param self (optional) ActiveObject to ignore in the collision detection.
collisionMoveResult collisionMoveSimple(Environment *env,IGameDef *gamedef,
		f32 pos_max_d, const aabb3f &box_0,
		f32 stepheight, f32 dtime,
		v3f *pos_f, v3f *speed_f,
		v3f accel_f, ActiveObject *self=NULL,
		bool collide_with_objects=true);

/// @brief A simpler version of "collisionMoveSimple" that only checks whether
///        a collision occurs at the given position.
/// @param self (optional) ActiveObject to ignore in the collision detection.
/// @returns `true` when `box_0` truly intersects with a node or object.
///          Touching faces are not counted as intersection.
bool collision_check_intersection(Environment *env, IGameDef *gamedef,
		const aabb3f &box_0, const v3f &pos_f, ActiveObject *self = nullptr,
		bool collide_with_objects = true);

// Helper function:
// Checks for collision of a moving aabbox with a static aabbox
// Returns -1 if no collision, 0 if X collision, 1 if Y collision, 2 if Z collision
// dtime receives time until first collision, invalid if -1 is returned
CollisionAxis axisAlignedCollision(
		const aabb3f &staticbox, const aabb3f &movingbox,
		v3f speed, f32 *dtime);

// Helper function:
// Checks if moving the movingbox up by the given distance would hit a ceiling.
bool wouldCollideWithCeiling(
		const std::vector<aabb3f> &staticboxes,
		const aabb3f &movingbox,
		f32 y_increase, f32 d);
