// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "activeobject.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>


class ClientEnvironment;
class ITextureSource;
class Client;
struct ItemStack;

namespace scene
{
	class IAnimatedMeshSceneNode;
	class ISceneNode;
	class ISceneManager;
}

class ClientActiveObject : public ActiveObject
{
public:
	ClientActiveObject(u16 id, Client *client, ClientEnvironment *env);
	virtual ~ClientActiveObject();

	virtual void addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr) = 0;
	virtual void removeFromScene(bool permanent) {}

	virtual void updateLight(u32 day_night_ratio) {}

	virtual bool getCollisionBox(aabb3f *toset) const { return false; }
	virtual bool getSelectionBox(aabb3f *toset) const { return false; }
	virtual bool collideWithObjects() const { return false; }
	virtual const v3f getPosition() const { return v3f(0.0f); } // in BS-space
	virtual const v3f getVelocity() const { return v3f(0.0f); } // in BS-space
	virtual scene::ISceneNode *getSceneNode() const
	{ return NULL; }
	virtual scene::IAnimatedMeshSceneNode *getAnimatedMeshSceneNode() const
	{ return NULL; }
	virtual bool isLocalPlayer() const { return false; }

	virtual ClientActiveObject *getParent() const { return nullptr; };
	virtual const std::unordered_set<object_t> &getAttachmentChildIds() const
	{ static std::unordered_set<object_t> rv; return rv; }
	virtual void updateAttachments() {};

	virtual bool doShowSelectionBox() { return true; }

	// Step object in time
	virtual void step(float dtime, ClientEnvironment *env) {}

	// Process a message sent by the server side object
	virtual void processMessage(const std::string &data) {}

	virtual std::string infoText() { return ""; }
	virtual std::string debugInfoText() { return ""; }

	/*
		This takes the return value of
		ServerActiveObject::getClientInitializationData
	*/
	virtual void initialize(const std::string &data) {}

	// Create a certain type of ClientActiveObject
	static std::unique_ptr<ClientActiveObject> create(ActiveObjectType type,
			Client *client, ClientEnvironment *env);

	// If returns true, punch will not be sent to the server
	virtual bool directReportPunch(v3f dir, const ItemStack *punchitem,
		const ItemStack *hand_item, float time_from_last_punch = 1000000) { return false; }

protected:
	// Used for creating objects based on type
	typedef std::unique_ptr<ClientActiveObject> (*Factory)(Client *client, ClientEnvironment *env);
	static void registerType(u16 type, Factory f);
	Client *m_client;
	ClientEnvironment *m_env;
private:
	// Used for creating objects based on type
	static std::unordered_map<u16, Factory> m_types;
};

class DistanceSortedActiveObject
{
public:
	ClientActiveObject *obj;

	DistanceSortedActiveObject(ClientActiveObject *a_obj, f32 a_d)
	{
		obj = a_obj;
		d = a_d;
	}

	bool operator < (const DistanceSortedActiveObject &other) const
	{
		return d < other.d;
	}

private:
	f32 d;
};
