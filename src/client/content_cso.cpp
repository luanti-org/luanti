// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "content_cso.h"
#include <IBillboardSceneNode.h>
#include <ISceneManager.h>
#include "client/texturesource.h"
#include "clientenvironment.h"
#include "client.h"
#include "map.h"
#include "nodedef.h"

class SmokePuffCSO: public ClientSimpleObject
{
	float m_age = 0.0f;
	scene::IBillboardSceneNode *m_spritenode = nullptr;
public:
	SmokePuffCSO(scene::ISceneManager *smgr,
			ClientEnvironment *env, const v3f &pos, const v2f &size)
	{
		infostream<<"SmokePuffCSO: constructing"<<std::endl;
		m_spritenode = smgr->addBillboardSceneNode(
				NULL, v2f(1,1), pos, -1);
		video::ITexture *tex = env->getGameDef()->tsrc()->getTextureForMesh("smoke_puff.png");
		m_spritenode->forEachMaterial([tex] (auto &mat) {
			mat.TextureLayers[0].Texture = tex;
			mat.TextureLayers[0].MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
			mat.TextureLayers[0].MagFilter = video::ETMAGF_NEAREST;
			mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			mat.FogEnable = true;
		});
		m_spritenode->setColor(video::SColor(255,0,0,0));
		m_spritenode->setVisible(true);
		m_spritenode->setSize(size);
		/* Update brightness */
		u8 light;
		bool pos_ok;
		MapNode n = env->getMap().getNode(floatToInt(pos, BS), &pos_ok);
		light = pos_ok ? decode_light(n.getLightBlend(env->getDayNightRatio(),
							env->getGameDef()->ndef()->getLightingFlags(n)))
						: 64;
		video::SColor color(255,light,light,light);
		m_spritenode->setColor(color);
	}
	virtual ~SmokePuffCSO()
	{
		infostream<<"SmokePuffCSO: destructing"<<std::endl;
		m_spritenode->remove();
	}
	void step(float dtime)
	{
		m_age += dtime;
		if(m_age > 1.0){
			m_to_be_removed = true;
		}
	}
};

ClientSimpleObject* createSmokePuff(scene::ISceneManager *smgr,
		ClientEnvironment *env, v3f pos, v2f size)
{
	return new SmokePuffCSO(smgr, env, pos, size);
}

