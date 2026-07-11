// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#pragma once

#include "line_common.h"
#include "serveractiveobject.h"
#include "util/guid.h"
#include <vector>

struct EnumString;

// One endpoint of a LineSAO - a fixed pos, or one that follows another
// object, persisted by GUID so it survives reloads.
struct LinePoint
{
	v3f pos;
	std::string attached_guid; // empty if this point is not attached

	// Derived from attached_guid at runtime, not serialized directly.
	ServerActiveObject::object_t attached_id = 0;
};

extern const EnumString es_LineAlphaMode[];

struct LineProperties
{
	video::SColor color = video::SColor(0xFFFFFFFF);
	std::vector<video::SColor> colors; // one per point, overrides `color` if non-empty
	LineAlphaMode alpha_mode = LineAlphaMode::LINE_ALPHA_OPAQUE;
};

// A lightweight, persistent, server-side polyline with no callbacks or Lua state.
class LineSAO : public ServerActiveObject
{
public:
	LineSAO() = delete;
	// Used by the environment to load a stored line
	LineSAO(ServerEnvironment *env, v3f pos, const std::string &data);
	// Used by the Lua API
	LineSAO(ServerEnvironment *env, std::vector<LinePoint> points,
			const LineProperties &properties);

	ActiveObjectType getType() const override { return ACTIVEOBJECT_TYPE_LINE; }

	void addedToEnvironment(u32 dtime_s) override;
	void step(float dtime, bool send_recommended) override;

	bool isStaticAllowed() const override { return true; }
	void getStaticData(std::string *result) const override;
	std::string getClientInitializationData(u16 protocol_version) override;

	std::string getGUID() const override { return m_guid.base64(); }

	bool getCollisionBox(aabb3f *toset) const override { return false; }
	bool getSelectionBox(aabb3f *toset) const override { return false; }
	bool collideWithObjects() const override { return false; }

	// line-specific
	const std::vector<LinePoint> &getPoints() const { return m_points; }
	void setPoints(std::vector<LinePoint> points);
	const LineProperties &getProperties() const { return m_properties; }
	void setProperties(const LineProperties &properties);

private:
	// Resolves attached_guid to attached_id and refreshes pos for attached points.
	void resolvePoints();
	// Base position always tracks the first point.
	void updateBasePositionFromFirstPoint();

	// Sends point positions; passive per-step resends are unreliable, explicit setPoints() calls are reliable.
	void sendPoints(bool reliable = true);
	// Queues a LINE_CMD_SET_PROPERTIES message.
	void sendProperties();

	std::vector<LinePoint> m_points;
	LineProperties m_properties;
	MyGUID m_guid;

	// Throttle state for passive per-step resends - but setPoints() bypasses throttling and sends immediately.
	std::vector<v3f> m_last_sent_point_positions;
	float m_last_sent_position_timer = 0.0f;
};
