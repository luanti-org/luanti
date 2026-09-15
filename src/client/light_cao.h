// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Zenon Seth <Zenon.Seth@gmail.com>

#pragma once

#include "client/clientobject.h"
#include "constants.h"
#include "light_common.h"
#include <SColor.h>

/*
	Client-side counterpart of LightSAO (src/server/light_sao.h).
	Has no scene node - its only job is to register/update/remove itself in
	the client's DynamicLightManager, keyed by its own object id. When
	attached, the server only tells us which object id to follow once; we
	then read that object's own already smoothly-interpolated CAO position
	every step, rather than have the server resend positions continuously.
*/
class LightCAO : public ClientActiveObject
{
public:
	LightCAO(Client *client, ClientEnvironment *env);
	~LightCAO() override;

	ActiveObjectType getType() const override { return ACTIVEOBJECT_TYPE_LIGHT; }
	static std::unique_ptr<ClientActiveObject> create(Client *client, ClientEnvironment *env);

	void addToScene(ITextureSource *tsrc, scene::ISceneManager *smgr) override {}
	void removeFromScene(bool permanent) override;
	void initialize(const std::string &data) override;
	void processMessage(const std::string &data) override;

	void step(float dtime, ClientEnvironment *env) override;

	bool getCollisionBox(aabb3f *toset) const override { return false; }
	bool getSelectionBox(aabb3f *toset) const override { return false; }
	bool collideWithObjects() const override { return false; }

private:
	struct Properties
	{
		video::SColor color = video::SColor(0xFFFFFFFF);
		float range = 8.0f * BS;
		float falloff = 2.0f;
	};

	void deserializeState(std::istream &is);
	void deserializeProperties(std::istream &is);
	void updateManager();

	// Either m_attached_id is nonzero, meaning follow that object's live
	// position, or it's 0 and m_pos is the free-floating position last sent.
	object_t m_attached_id = 0;
	v3f m_pos;
	bool m_have_pos = false;
	Properties m_properties;
	bool m_registered = false;
};
