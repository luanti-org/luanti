// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#pragma once

#include "constants.h"
#include "light_common.h"
#include "serveractiveobject.h"
#include "util/guid.h"
#include <SColor.h>

// Distinguishes the "attach to this GUID" constructor from the
// "raw serialized blob" constructor, otherwise both would take a plain
// std::string and be ambiguous
struct LightAttachment
{
	std::string guid; // empty means free-floating
};

struct LightProperties
{
	video::SColor color = video::SColor(0xFFFFFFFF);
	float range = 8.0f * BS;
	float falloff = 2.0f;
};

// A lightweight, persistent, server-side point light with no callbacks or Lua state
// Just a position, optionally by GUID so attachment survives reload
class LightSAO : public ServerActiveObject
{
public:
	LightSAO() = delete;
	// Used by the environment to load a stored light
	LightSAO(ServerEnvironment *env, v3f pos, const std::string &data);
	// Used by the Lua API
	LightSAO(ServerEnvironment *env, v3f pos, LightAttachment attachment,
			const LightProperties &properties);

	ActiveObjectType getType() const override { return ACTIVEOBJECT_TYPE_LIGHT; }

	void addedToEnvironment(u32 dtime_s) override;
	void step(float dtime, bool send_recommended) override;

	bool isStaticAllowed() const override { return true; }
	void getStaticData(std::string *result) const override;
	std::string getClientInitializationData(u16 protocol_version) override;

	std::string getGUID() const override { return m_guid.base64(); }

	bool getCollisionBox(aabb3f *toset) const override { return false; }
	bool getSelectionBox(aabb3f *toset) const override { return false; }
	bool collideWithObjects() const override { return false; }

	// Detaches if attached, and moves to a free-floating position.
	void setPos(v3f pos);
	void setAttachedGUID(std::string attached_guid);
	object_t getAttachedId() const { return m_attached_id; }
	const LightProperties &getProperties() const { return m_properties; }
	void setProperties(const LightProperties &properties);

private:
	// How often to retry the linear GUID scan while unresolved,
	// and how long to keep retrying before giving up and removing it.
	static constexpr float ATTACHMENT_RESCAN_INTERVAL = 1.0f;
	static constexpr float ATTACHMENT_TIMEOUT = 2.0f;

	// Resolves m_attached_guid to m_attached_id and refreshes pos if attached.
	// force bypasses the rescan throttle, for the initial resolve attempt.
	void resolveAttachment(float dtime, bool force = false);

	// Sends the attached object's id or the free-floating position
	// The client then tracks that object's own CAO position itself.
	void sendState(bool reliable = true);
	// Queues a LIGHT_CMD_SET_PROPERTIES message.
	void sendProperties();

	std::string m_attached_guid; // empty if free-floating
	ServerActiveObject::object_t m_attached_id = 0;
	MyGUID m_guid;
	LightProperties m_properties;

	// What was last sent to clients, to know when a resend is warranted.
	ServerActiveObject::object_t m_last_sent_attached_id = 0;
	v3f m_last_sent_pos;
	float m_last_sent_position_timer = 0.0f;

	// How long the target has been continuously unresolved
	// and how long since the last GUID scan attempt
	float m_unresolved_time = 0.0f;
	float m_time_since_last_scan = 0.0f;
};
