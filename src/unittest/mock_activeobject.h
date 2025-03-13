// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#include <activeobject.h>

class MockActiveObject : public ActiveObject
{
public:
	MockActiveObject(u16 id) : ActiveObject(id) {}

	virtual ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_TEST; }
	virtual bool getCollisionBox(aabb3f *toset) const { return false; }
	virtual bool getSelectionBox(aabb3f *toset) const { return false; }
	virtual bool collideWithObjects() const { return false; }
};
