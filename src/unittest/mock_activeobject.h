// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#include "activeobject.h"

class MockActiveObject : public ActiveObject
{
public:
	MockActiveObject(u16 id) : ActiveObject(id) {}

	virtual ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_TEST; }
	virtual u32 getCollisionGroup() const { return 0; }
	virtual u32 getCollisionMask() const { return 0; }
	virtual bool getCollisionBox(aabb3f *toset) const { return false; }
	virtual bool getSelectionBox(aabb3f *toset) const { return false; }
};
