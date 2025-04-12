// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 Minetest core developers & community

#include "guid.h"
#include "serverenvironment.h"
#include <server/serveractiveobject.h>
#include <string>

class MockServerActiveObject : public ServerActiveObject
{
public:
	MockServerActiveObject(ServerEnvironment *env = nullptr, v3f p = v3f()) :
		ServerActiveObject(env, p)
	{
		if (env)
			m_guid = env->getGUIDGenerator().next().hex();
	}

	virtual ActiveObjectType getType() const { return ACTIVEOBJECT_TYPE_TEST; }
	virtual bool getCollisionBox(aabb3f *toset) const { return false; }
	virtual bool getSelectionBox(aabb3f *toset) const { return false; }
	virtual bool collideWithObjects() const { return false; }
	virtual const std::string& getGUID()
	{
		assert(!m_guid.empty());
		return m_guid;
	}

private:
	std::string m_guid;
};
