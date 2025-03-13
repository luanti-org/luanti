// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "clientobject.h"
#include "debug.h"
#include "porting.h"

/*
	ClientActiveObject
*/

ClientActiveObject::ClientActiveObject(u16 id, Client *client,
		ClientEnvironment *env):
	ActiveObject(id),
	m_client(client),
	m_env(env)
{
}

ClientActiveObject::~ClientActiveObject()
{
	removeFromScene(true);
}

std::unique_ptr<ClientActiveObject> ClientActiveObject::create(ActiveObjectType type,
		Client *client, ClientEnvironment *env)
{
	// Find factory function
	auto n = m_types.find(type);
	if (n == m_types.end()) {
		// If factory is not found, just return.
		warningstream << "ClientActiveObject: No factory for type="
				<< (int)type << std::endl;
		return nullptr;
	}

	Factory f = n->second;
	std::unique_ptr<ClientActiveObject> object = (*f)(client, env);
	return object;
}

void ClientActiveObject::registerType(u16 type, Factory f)
{
	auto n = m_types.find(type);
	if (n != m_types.end())
		return;
	m_types[type] = f;
}


