// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "clientobject.h"
#include "debug.h"

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

std::unordered_map<u16, ClientActiveObject::Factory> &ClientActiveObject::getTypes()
{
	static std::unordered_map<u16, Factory> types;
	return types;
}

std::unique_ptr<ClientActiveObject> ClientActiveObject::create(ActiveObjectType type,
		Client *client, ClientEnvironment *env)
{
	// Find factory function
	auto &types = getTypes();
	auto n = types.find(type);
	if (n == types.end()) {
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
	auto &types = getTypes();
	auto n = types.find(type);
	if (n != types.end())
		return;
	types[type] = f;
}


