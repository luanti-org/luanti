// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#include <server/serveractiveobject.h>

class MockServerActiveObject : public ServerActiveObject
{
public:
	MockServerActiveObject(ServerEnvironment *env = nullptr, v3f p = v3f()) :
		ServerActiveObject(env, p) {}

	virtual ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_TEST; }
	virtual bool getCollisionBox(aabb3f *toset) const { return false; }
	virtual bool getSelectionBox(aabb3f *toset) const { return false; }
	virtual bool collideWithObjects() const { return false; }
	virtual const GUId& getGuid() {return m_guid;}
private:
	GUId m_guid;
};
