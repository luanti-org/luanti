// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#include "light_sao.h"
#include "constants.h"
#include "exceptions.h"
#include "serverenvironment.h"
#include "util/serialize.h"

namespace
{

void serializeStaticState(std::ostream &os, v3f pos, const std::string &attached_guid)
{
	writeU8(os, !attached_guid.empty());
	if (!attached_guid.empty())
		os << serializeString16(attached_guid);
	writeV3F32(os, pos);
}

// Returns the deserialized pos; attached_guid is filled in if present.
v3f deserializeStaticState(std::istream &is, std::string &attached_guid)
{
	bool attached = readU8(is);
	if (attached)
		attached_guid = deSerializeString16(is);
	return readV3F32(is);
}

void serializeProperties(std::ostream &os, const LightProperties &properties)
{
	writeARGB8(os, properties.color);
	writeF32(os, properties.range);
	writeF32(os, properties.falloff);
}

void deserializeProperties(std::istream &is, LightProperties &properties)
{
	properties.color = readARGB8(is);
	properties.range = readF32(is);
	properties.falloff = readF32(is);
}

} // namespace

LightSAO::LightSAO(ServerEnvironment *env, v3f pos, const std::string &data) :
		ServerActiveObject(env, pos)
{
	std::istringstream is(data, std::ios::binary);
	u8 version = readU8(is);
	if (version != 1)
		throw SerializationError("unsupported LightSAO version");

	m_guid.deSerialize(is);
	setBasePosition(deserializeStaticState(is, m_attached_guid));
	deserializeProperties(is, m_properties);
}

LightSAO::LightSAO(ServerEnvironment *env, v3f pos, LightAttachment attachment,
		const LightProperties &properties) :
		ServerActiveObject(env, pos),
		m_attached_guid(std::move(attachment.guid)),
		m_guid(env->getGUIDGenerator().next()),
		m_properties(properties)
{
}

void LightSAO::addedToEnvironment(u32 dtime_s)
{
	// Force an immediate attempt but don't count the offilne gap toward the unresolved timeout
	resolveAttachment(0.0f, true);
}

void LightSAO::step(float dtime, bool send_recommended)
{
	resolveAttachment(dtime);

	m_last_sent_position_timer += dtime;

	if (!send_recommended)
		return;

	// Attachment or target changes are always worth an immediate resend
	if (m_attached_id != m_last_sent_attached_id) {
		sendState(false);
		return;
	}

	// While attached, the client is already following the target
	// no per-step position resends needed.
	if (m_attached_id != 0)
		return;

	float minchange = getPositionResendMinChange(m_last_sent_position_timer);

	if (getBasePosition().getDistanceFrom(m_last_sent_pos) > minchange)
		sendState(false);
}

void LightSAO::getStaticData(std::string *result) const
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, 1);
	m_guid.serialize(os);
	serializeStaticState(os, getBasePosition(), m_attached_guid);
	serializeProperties(os, m_properties);

	*result = os.str();
}

std::string LightSAO::getClientInitializationData(u16 protocol_version)
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, 1); // version
	writeU8(os, m_attached_id != 0);
	if (m_attached_id != 0)
		writeU16(os, m_attached_id);
	else
		writeV3F32(os, getBasePosition());
	serializeProperties(os, m_properties);
	return os.str();
}

void LightSAO::setPos(v3f pos)
{
	m_attached_guid.clear();
	m_attached_id = 0;
	setBasePosition(pos);
	sendState();
}

void LightSAO::setAttachedGUID(std::string attached_guid)
{
	m_attached_guid = std::move(attached_guid);
	resolveAttachment(0.0f, true);
	sendState();
}

void LightSAO::setProperties(const LightProperties &properties)
{
	m_properties = properties;
	sendProperties();
}

void LightSAO::resolveAttachment(float dtime, bool force)
{
	if (m_attached_guid.empty())
		return;

	// Cheap path: id from last resolve is still valid, no GUID scan needed
	if (m_attached_id != 0) {
		if (ServerActiveObject *target = m_env->getActiveObject(m_attached_id)) {
			setBasePosition(target->getBasePosition());
			m_unresolved_time = 0.0f;
			m_time_since_last_scan = 0.0f;
			return;
		}
		m_attached_id = 0;
	}

	m_unresolved_time += dtime;
	m_time_since_last_scan += dtime;
	if (!force && m_time_since_last_scan < ATTACHMENT_RESCAN_INTERVAL)
		return;
	m_time_since_last_scan = 0.0f;

	// Id lookup failed or wasn't known yet - fall back to the slow scan
	if (ServerActiveObject *target = m_env->getActiveObjectByGUID(m_attached_guid)) {
		m_attached_id = target->getId();
		setBasePosition(target->getBasePosition());
		m_unresolved_time = 0.0f;
		return;
	}

	// Still not found after retrying - give up rather than scanning forever
	if (m_unresolved_time >= ATTACHMENT_TIMEOUT)
		markForRemoval();
}

void LightSAO::sendState(bool reliable)
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, (u8)LightCommand::LIGHT_CMD_SET_STATE);
	writeU8(os, m_attached_id != 0);
	if (m_attached_id != 0)
		writeU16(os, m_attached_id);
	else
		writeV3F32(os, getBasePosition());
	m_messages_out.emplace(getId(), reliable, os.str());

	m_last_sent_attached_id = m_attached_id;
	m_last_sent_pos = getBasePosition();
	m_last_sent_position_timer = 0.0f;
}

void LightSAO::sendProperties()
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, (u8)LightCommand::LIGHT_CMD_SET_PROPERTIES);
	serializeProperties(os, m_properties);
	m_messages_out.emplace(getId(), true, os.str());
}
