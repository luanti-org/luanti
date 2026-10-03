// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#include "light_cao.h"
#include "client.h"
#include "client/clientenvironment.h"
#include "client/dynamiclight.h"
#include "constants.h"
#include "util/serialize.h"
#include <sstream>

LightCAO::LightCAO(Client *client, ClientEnvironment *env) :
		ClientActiveObject(0, client, env)
{
	// Runs during global static initialization for the file-scope
	// proto_LightCAO below, where client is nullptr
	if (!client)
		ClientActiveObject::registerType(getType(), create);
}

LightCAO::~LightCAO()
{
	removeFromScene(true);
}

std::unique_ptr<ClientActiveObject> LightCAO::create(Client *client, ClientEnvironment *env)
{
	return std::make_unique<LightCAO>(client, env);
}

void LightCAO::removeFromScene(bool permanent)
{
	if (!m_registered)
		return;
	m_client->getDynamicLightManager()->remove(getId());
	m_registered = false;
}

void LightCAO::step(float dtime, ClientEnvironment *env)
{
	// Re-read every frame: while attached, the target CAO's own position is
	// updated/interpolated elsewhere, so this is what keeps us following it
	// smoothly instead of only moving on the throttled server resends.
	if (m_registered)
		updateManager();
}

void LightCAO::initialize(const std::string &data)
{
	std::istringstream is(data, std::ios::binary);
	u8 version = readU8(is);
	if (version != 1)
		return;
	deserializeState(is);
	deserializeProperties(is);
	updateManager();
}

void LightCAO::processMessage(const std::string &data)
{
	std::istringstream is(data, std::ios::binary);
	auto cmd = (LightCommand)readU8(is);
	switch (cmd) {
	case LightCommand::LIGHT_CMD_SET_STATE:
		deserializeState(is);
		break;
	case LightCommand::LIGHT_CMD_SET_PROPERTIES:
		deserializeProperties(is);
		break;
	default:
		return;
	}
	updateManager();
}

void LightCAO::deserializeState(std::istream &is)
{
	bool attached = readU8(is);
	if (attached) {
		m_attached_id = readU16(is);
	} else {
		m_attached_id = 0;
		m_pos = readV3F32(is);
	}
}

void LightCAO::deserializeProperties(std::istream &is)
{
	m_properties.color = readARGB8(is);
	m_properties.range = readF32(is);
	m_properties.falloff = readF32(is);
}

void LightCAO::updateManager()
{
	if (m_attached_id != 0) {
		if (ClientActiveObject *target = m_env->getActiveObject(m_attached_id)) {
			m_pos = target->getPosition();
			m_have_pos = true;
		}
	} else {
		m_have_pos = true;
	}

	if (!m_have_pos)
		return;

	m_registered = true;
	m_client->getDynamicLightManager()->addOrUpdate(getId(), m_pos, m_properties.range,
			video::SColorf(m_properties.color), m_properties.falloff);
}

// Prototype: registers ACTIVEOBJECT_TYPE_LIGHT's factory at static-init time.
static LightCAO proto_LightCAO(nullptr, nullptr);
