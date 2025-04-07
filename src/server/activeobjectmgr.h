// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#pragma once

#include <functional>
#include <vector>
#include "../activeobjectmgr.h"
#include "serveractiveobject.h"
#include "util/k_d_tree.h"

namespace server
{
class ActiveObjectMgr final : public ::ActiveObjectMgr<ServerActiveObject>
{
public:
	~ActiveObjectMgr() override;

	// If cb returns true, the obj will be deleted
	void clearIf(const std::function<bool(ServerActiveObject *, u16)> &cb);
	void step(float dtime,
			const std::function<void(ServerActiveObject *)> &f) override;
	bool registerObject(std::unique_ptr<ServerActiveObject> obj) override;
	void removeObject(u16 id) override;

	void invalidateActiveObjectObserverCaches();

	void updateObjectPos(u16 id, v3f pos);

	void getObjectsInsideRadius(v3f pos, float radius,
			std::vector<ServerActiveObject *> &result,
			std::function<bool(ServerActiveObject *obj)> include_obj_cb);
	void getObjectsInArea(const aabb3f &box,
			std::vector<ServerActiveObject *> &result,
			std::function<bool(ServerActiveObject *obj)> include_obj_cb);
	void getAddedActiveObjectsAroundPos(
			v3f player_pos, const std::string &player_name,
			f32 radius, f32 player_radius,
			const std::set<u16> &current_objects,
			std::vector<u16> &added_objects);

private:
	k_d_tree::DynamicKdTrees<3, f32, u16> m_spatial_index;
};
} // namespace server
