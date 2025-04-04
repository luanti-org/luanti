// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include <log.h>
#include "mapblock.h"
#include "profiler.h"
#include "activeobjectmgr.h"

namespace server
{

ActiveObjectMgr::~ActiveObjectMgr()
{
	if (!m_active_objects.empty()) {
		warningstream << "server::ActiveObjectMgr::~ActiveObjectMgr(): not cleared."
				<< std::endl;
		clear();
	}
}

void ActiveObjectMgr::clearIf(const std::function<bool(ServerActiveObject *, u16)> &cb)
{
	for (auto &it : m_active_objects.iter()) {
		if (!it.second)
			continue;
		if (cb(it.second.get(), it.first)) {
			// Remove reference from m_active_objects
			removeObject(it.first);
		}
	}
}

void ActiveObjectMgr::step(
		float dtime, const std::function<void(ServerActiveObject *)> &f)
{
	size_t count = 0;

	for (auto &ao_it : m_active_objects.iter()) {
		if (!ao_it.second)
			continue;
		count++;
		f(ao_it.second.get());
	}

	g_profiler->avg("ActiveObjectMgr: SAO count [#]", count);
}

bool ActiveObjectMgr::registerObject(std::unique_ptr<ServerActiveObject> obj)
{
	assert(obj); // Pre-condition
	if (obj->getId() == 0) {
		u16 new_id = getFreeId();
		if (new_id == 0) {
			errorstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
					<< "no free id available" << std::endl;
			return false;
		}
		obj->setId(new_id);
	} else {
		verbosestream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "supplied with id " << obj->getId() << std::endl;
	}

	if (!isFreeId(obj->getId())) {
		errorstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "id is not free (" << obj->getId() << ")" << std::endl;
		return false;
	}

	const v3f pos = obj->getBasePosition();
	if (objectpos_over_limit(pos)) {
		warningstream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
				<< "object position (" << pos.X << "," << pos.Y << "," << pos.Z
				<< ") outside maximum range" << std::endl;
		return false;
	}

	auto obj_id = obj->getId();
	m_active_objects.put(obj_id, std::move(obj));
	m_spatial_index.insert(pos.toArray(), obj_id);

	auto new_size = m_active_objects.size();
	verbosestream << "Server::ActiveObjectMgr::addActiveObjectRaw(): "
			<< "Added id=" << obj_id << "; there are now ";
	if (new_size == decltype(m_active_objects)::unknown)
		verbosestream << "???";
	else
		verbosestream << new_size;
	verbosestream << " active objects." << std::endl;
	return true;
}

void ActiveObjectMgr::removeObject(u16 id)
{
	verbosestream << "Server::ActiveObjectMgr::removeObject(): "
			<< "id=" << id << std::endl;

	// this will take the object out of the map and then destruct it
	bool ok = m_active_objects.remove(id);
	if (!ok) {
		infostream << "Server::ActiveObjectMgr::removeObject(): "
				<< "id=" << id << " not found" << std::endl;
	} else {
		m_spatial_index.remove(id);
	}
}

void ActiveObjectMgr::invalidateActiveObjectObserverCaches()
{
	for (auto &active_object : m_active_objects.iter()) {
		ServerActiveObject *obj = active_object.second.get();
		if (!obj)
			continue;
		obj->invalidateEffectiveObservers();
	}
}

void ActiveObjectMgr::updateObjectPos(u16 id, v3f pos)
{
	// HACK defensively only update if we already know the object,
	// otherwise we're still waiting to be inserted into the index
	// (or have already been removed).
	if (m_active_objects.get(id))
		m_spatial_index.update(pos.toArray(), id);
}

void ActiveObjectMgr::getObjectsInsideRadius(v3f pos, float radius,
		std::vector<ServerActiveObject *> &result,
		std::function<bool(ServerActiveObject *obj)> include_obj_cb)
{
	float r_squared = radius * radius;
	m_spatial_index.rangeQuery((pos - v3f(radius)).toArray(), (pos + v3f(radius)).toArray(), [&](auto objPos, u16 id) {
		if (v3f(objPos).getDistanceFromSQ(pos) > r_squared)
			return;

		auto obj = m_active_objects.get(id).get();
		if (!obj)
			return;
		if (!include_obj_cb || include_obj_cb(obj))
			result.push_back(obj);
	});
}

void ActiveObjectMgr::getObjectsInArea(const aabb3f &box,
		std::vector<ServerActiveObject *> &result,
		std::function<bool(ServerActiveObject *obj)> include_obj_cb)
{
	m_spatial_index.rangeQuery(box.MinEdge.toArray(), box.MaxEdge.toArray(), [&](auto _, u16 id) {
		auto obj = m_active_objects.get(id).get();
		if (!obj)
			return;
		if (!include_obj_cb || include_obj_cb(obj))
			result.push_back(obj);
	});
}

void ActiveObjectMgr::getAddedActiveObjectsAroundPos(
		v3f player_pos, const std::string &player_name,
		f32 radius, f32 player_radius,
		const std::set<u16> &current_objects,
		std::vector<u16> &added_objects)
{
	/*
		Go through the object list,
		- discard removed/deactivated objects,
		- discard objects that are too far away,
		- discard objects that are found in current_objects,
		- discard objects that are not observed by the player.
		- add remaining objects to added_objects
	*/
	for (auto &ao_it : m_active_objects.iter()) {
		u16 id = ao_it.first;

		// Get object
		ServerActiveObject *object = ao_it.second.get();
		if (!object)
			continue;

		if (object->isGone())
			continue;

		f32 distance_f = object->getBasePosition().getDistanceFrom(player_pos);
		if (object->getType() == ACTIVEOBJECT_TYPE_PLAYER) {
			// Discard if too far
			if (distance_f > player_radius && player_radius != 0)
				continue;
		} else if (distance_f > radius)
			continue;

		if (!object->isEffectivelyObservedBy(player_name))
			continue;

		// Discard if already on current_objects
		auto n = current_objects.find(id);
		if (n != current_objects.end())
			continue;
		// Add to added_objects
		added_objects.push_back(id);
	}
}

} // namespace server
