// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include <cmath>
#include <log.h>
#include "profiler.h"
#include "activeobjectmgr.h"

namespace client
{

ActiveObjectMgr::~ActiveObjectMgr()
{
	if (!m_active_objects.empty()) {
		warningstream << "client::ActiveObjectMgr::~ActiveObjectMgr(): not cleared."
				<< std::endl;
		clear();
	}
}

void ActiveObjectMgr::step(
		float dtime, const std::function<void(ClientActiveObject *)> &f)
{
	size_t count = 0;
	for (auto &ao_it : m_active_objects.iter()) {
		if (!ao_it.second)
			continue;
		count++;
		f(ao_it.second.get());
	}
	g_profiler->avg("ActiveObjectMgr: CAO count [#]", count);
}

bool ActiveObjectMgr::registerObject(std::unique_ptr<ClientActiveObject> obj)
{
	assert(obj); // Pre-condition
	if (obj->getId() == 0) {
		u16 new_id = getFreeId();
		if (new_id == 0) {
			infostream << "Client::ActiveObjectMgr::registerObject(): "
					<< "no free id available" << std::endl;

			return false;
		}
		obj->setId(new_id);
	}

	if (!isFreeId(obj->getId())) {
		infostream << "Client::ActiveObjectMgr::registerObject(): "
				<< "id is not free (" << obj->getId() << ")" << std::endl;
		return false;
	}
	infostream << "Client::ActiveObjectMgr::registerObject(): "
			<< "added (id=" << obj->getId() << ")" << std::endl;
	m_active_objects.put(obj->getId(), std::move(obj));
	return true;
}

void ActiveObjectMgr::removeObject(u16 id)
{
	verbosestream << "Client::ActiveObjectMgr::removeObject(): "
			<< "id=" << id << std::endl;

	std::unique_ptr<ClientActiveObject> obj = m_active_objects.take(id);
	if (!obj) {
		infostream << "Client::ActiveObjectMgr::removeObject(): "
				<< "id=" << id << " not found" << std::endl;
		return;
	}

	obj->removeFromScene(true);
}

void ActiveObjectMgr::getActiveObjects(const v3f &origin, f32 max_d,
		std::vector<DistanceSortedActiveObject> &dest)
{
	f32 max_d2 = max_d * max_d;
	for (auto &ao_it : m_active_objects.iter()) {
		ClientActiveObject *obj = ao_it.second.get();
		if (!obj)
			continue;

		f32 d2 = (obj->getPosition() - origin).getLengthSQ();

		if (d2 > max_d2)
			continue;

		dest.emplace_back(obj, d2);
	}
}

std::vector<DistanceSortedActiveObject> ActiveObjectMgr::getActiveSelectableObjects(const core::line3d<f32> &shootline)
{
	std::vector<DistanceSortedActiveObject> dest;
	f32 max_d = shootline.getLength();
	v3f dir = shootline.getVector().normalize();

	for (auto &ao_it : m_active_objects.iter()) {
		ClientActiveObject *obj = ao_it.second.get();
		if (!obj)
			continue;

		aabb3f selection_box{{0.0f, 0.0f, 0.0f}};
		if (!obj->getSelectionBox(&selection_box))
			continue;

		v3f obj_center = obj->getPosition() + selection_box.getCenter();
		f32 obj_radius_sq = selection_box.getExtent().getLengthSQ() / 4;

		v3f c = obj_center - shootline.start;
		f32 a = dir.dotProduct(c);           // project c onto dir
		f32 b_sq = c.getLengthSQ() - a * a;  // distance from shootline to obj_center, squared

		if (b_sq > obj_radius_sq)
			continue;

		// backward- and far-plane
		f32 obj_radius = std::sqrt(obj_radius_sq);
		if (a < -obj_radius || a > max_d + obj_radius)
			continue;

		dest.emplace_back(obj, a);
	}
	return dest;
}

} // namespace client
