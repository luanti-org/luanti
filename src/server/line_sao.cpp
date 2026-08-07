// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#include "line_sao.h"
#include "constants.h"
#include "exceptions.h"
#include "serverenvironment.h"
#include "util/enum_string.h"
#include "util/serialize.h"

const EnumString es_LineAlphaMode[] = {
	{(int)LineAlphaMode::LINE_ALPHA_OPAQUE, "opaque"},
	{(int)LineAlphaMode::LINE_ALPHA_BLEND, "blend"},
	{(int)LineAlphaMode::LINE_ALPHA_ADD, "add"},
	{0, nullptr},
};

const EnumString es_LineShape[] = {
	{(int)LineShape::LINE_SHAPE_LINE, "line"},
	{(int)LineShape::LINE_SHAPE_TUBE, "tube"},
	{0, nullptr},
};

namespace
{

v3f firstPointPos(const std::vector<LinePoint> &points)
{
	return points.empty() ? v3f() : points[0].pos;
}

// Full point format, including attachment metadata. Used only for persistence
void serializeStaticPoints(std::ostream &os, const std::vector<LinePoint> &points)
{
	writeU16(os, points.size());
	for (const LinePoint &point : points) {
		writeU8(os, !point.attached_guid.empty());
		if (!point.attached_guid.empty())
			os << serializeString16(point.attached_guid);
		writeV3F32(os, point.pos);
	}
}

void deserializeStaticPoints(std::istream &is, std::vector<LinePoint> &points)
{
	u16 num_points = readU16(is);
	points.resize(num_points);
	for (LinePoint &point : points) {
		bool attached = readU8(is);
		if (attached)
			point.attached_guid = deSerializeString16(is);
		point.pos = readV3F32(is);
	}
}

// Resolved-position-only point format - no attachment metadata.
void serializeResolvedPoints(std::ostream &os, const std::vector<LinePoint> &points)
{
	writeU16(os, points.size());
	for (const LinePoint &point : points)
		writeV3F32(os, point.pos);
}

void serializeProperties(std::ostream &os, const LineProperties &properties)
{
	writeARGB8(os, properties.color);
	writeU16(os, properties.colors.size());
	for (video::SColor color : properties.colors)
		writeARGB8(os, color);
	writeU8(os, (u8)properties.alpha_mode);
	writeU8(os, properties.lit);
	writeF32(os, properties.width);
	writeU8(os, (u8)properties.shape);
}

void deserializeProperties(std::istream &is, LineProperties &properties)
{
	properties.color = readARGB8(is);
	u16 num_colors = readU16(is);
	properties.colors.resize(num_colors);
	for (video::SColor &color : properties.colors)
		color = readARGB8(is);
	u8 raw_alpha_mode = readU8(is);
	// fall back to opaque for an out-of-range byte rather than casting blindly
	properties.alpha_mode = raw_alpha_mode <= (u8)LineAlphaMode::LINE_ALPHA_ADD ?
			(LineAlphaMode)raw_alpha_mode : LineAlphaMode::LINE_ALPHA_OPAQUE;
	properties.lit = readU8(is);
	properties.width = readF32(is);
	u8 raw_shape = readU8(is);
	// fall back to "line" for an out-of-range byte rather than casting blindly
	properties.shape = raw_shape <= (u8)LineShape::LINE_SHAPE_TUBE ?
			(LineShape)raw_shape : LineShape::LINE_SHAPE_LINE;
}

} // namespace

LineSAO::LineSAO(ServerEnvironment *env, v3f pos, const std::string &data) :
		ServerActiveObject(env, pos)
{
	std::istringstream is(data, std::ios::binary);
	u8 version = readU8(is);
	if (version != 2)
		throw SerializationError("unsupported LineSAO version");

	m_guid.deSerialize(is);
	deserializeStaticPoints(is, m_points);
	deserializeProperties(is, m_properties);

	updateBasePositionFromFirstPoint();
}

LineSAO::LineSAO(ServerEnvironment *env, std::vector<LinePoint> points,
		const LineProperties &properties) :
		ServerActiveObject(env, firstPointPos(points)),
		m_points(std::move(points)),
		m_properties(properties),
		m_guid(env->getGUIDGenerator().next())
{
}

void LineSAO::addedToEnvironment(u32 dtime_s)
{
	resolvePoints();
}

void LineSAO::step(float dtime, bool send_recommended)
{
	resolvePoints();
	updateBasePositionFromFirstPoint();

	m_last_sent_position_timer += dtime;

	if (!send_recommended)
		return;

	if (m_points.size() != m_last_sent_point_positions.size()) {
		sendPoints(false);
		return;
	}

	// Same throttling method as LuaEntitySAO - the longer it's been since the last send,
	// the smaller a movement is allowed to be before it's still worth sending
	float minchange = 0.2f * BS;
	if (m_last_sent_position_timer > 1.0f)
		minchange = 0.01f * BS;
	else if (m_last_sent_position_timer > 0.2f)
		minchange = 0.05f * BS;

	for (size_t i = 0; i < m_points.size(); i++) {
		if (m_points[i].pos.getDistanceFrom(m_last_sent_point_positions[i]) > minchange) {
			sendPoints(false);
			break;
		}
	}
}

void LineSAO::getStaticData(std::string *result) const
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, 2);
	m_guid.serialize(os);
	serializeStaticPoints(os, m_points);
	serializeProperties(os, m_properties);

	*result = os.str();
}

std::string LineSAO::getClientInitializationData(u16 protocol_version)
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, 1); // version
	serializeResolvedPoints(os, m_points);
	serializeProperties(os, m_properties);
	return os.str();
}

void LineSAO::setPoints(std::vector<LinePoint> points)
{
	m_points = std::move(points);
	resolvePoints();
	updateBasePositionFromFirstPoint();
	sendPoints();
}

void LineSAO::setProperties(const LineProperties &properties)
{
	m_properties = properties;
	sendProperties();
}

void LineSAO::resolvePoints()
{
	for (LinePoint &point : m_points) {
		if (point.attached_guid.empty())
			continue;

		if (ServerActiveObject *target =
				m_env->getActiveObjectByGUID(point.attached_guid)) {
			point.attached_id = target->getId();
			point.pos = target->getBasePosition();
		} else {
			// Target not currently active keep the point at its last known pos
			point.attached_id = 0;
		}
	}
}

void LineSAO::updateBasePositionFromFirstPoint()
{
	if (!m_points.empty())
		setBasePosition(m_points[0].pos);
}

void LineSAO::sendPoints(bool reliable)
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, (u8)LineCommand::LINE_CMD_SET_POINTS);
	serializeResolvedPoints(os, m_points);
	m_messages_out.emplace(getId(), reliable, os.str());

	m_last_sent_point_positions.resize(m_points.size());
	for (size_t i = 0; i < m_points.size(); i++)
		m_last_sent_point_positions[i] = m_points[i].pos;
	m_last_sent_position_timer = 0.0f;
}

void LineSAO::sendProperties()
{
	std::ostringstream os(std::ios::binary);
	writeU8(os, (u8)LineCommand::LINE_CMD_SET_PROPERTIES);
	serializeProperties(os, m_properties);
	m_messages_out.emplace(getId(), true, os.str());
}
