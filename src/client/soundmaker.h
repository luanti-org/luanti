// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti contributors

#pragma once

#include "sound.h"
#include "nodedef.h"
#include "event_manager.h"

class NodeDugEvent : public MtEvent
{
public:
	v3s16 p;
	MapNode n;

	NodeDugEvent(v3s16 p, MapNode n):
		p(p),
		n(n)
	{}
	Type getType() const { return NODE_DUG; }
};

class SoundMaker
{
	ISoundManager *m_sound;
	const NodeDefManager *m_ndef;

public:
	bool makes_footstep_sound = true;
	float m_player_step_timer = 0.0f;
	float m_player_jump_timer = 0.0f;

	SoundSpec m_player_step_sound;
	SoundSpec m_player_leftpunch_sound;
	// Second sound made on left punch, currently used for item 'use' sound
	SoundSpec m_player_leftpunch_sound2;
	SoundSpec m_player_rightpunch_sound;

	SoundMaker(ISoundManager *sound, const NodeDefManager *ndef) :
		m_sound(sound), m_ndef(ndef) {}

	void playPlayerStep()
	{
		if (m_player_step_timer <= 0 && m_player_step_sound.exists()) {
			m_player_step_timer = 0.03;
			if (makes_footstep_sound)
				m_sound->playSound(0, m_player_step_sound);
		}
	}

	void playPlayerJump()
	{
		if (m_player_jump_timer <= 0.0f) {
			m_player_jump_timer = 0.2f;
			m_sound->playSound(0, SoundSpec("player_jump", 0.5f));
		}
	}

	static void viewBobbingStep(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerStep();
	}

	static void playerRegainGround(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerStep();
	}

	static void playerJump(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->playPlayerJump();
	}

	static void cameraPunchLeft(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, sm->m_player_leftpunch_sound);
		sm->m_sound->playSound(0, sm->m_player_leftpunch_sound2);
	}

	static void cameraPunchRight(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, sm->m_player_rightpunch_sound);
	}

	static void nodeDug(MtEvent *e, void *data);

	static void playerDamage(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, SoundSpec("player_damage", 0.5));
	}

	static void playerFallingDamage(MtEvent *e, void *data)
	{
		SoundMaker *sm = (SoundMaker *)data;
		sm->m_sound->playSound(0, SoundSpec("player_falling_damage", 0.5));
	}

	void registerReceiver(MtEventManager *mgr)
	{
		mgr->reg(MtEvent::VIEW_BOBBING_STEP, viewBobbingStep, this);
		mgr->reg(MtEvent::PLAYER_REGAIN_GROUND, playerRegainGround, this);
		mgr->reg(MtEvent::PLAYER_JUMP, playerJump, this);
		mgr->reg(MtEvent::CAMERA_PUNCH_LEFT, cameraPunchLeft, this);
		mgr->reg(MtEvent::CAMERA_PUNCH_RIGHT, cameraPunchRight, this);
		mgr->reg(MtEvent::NODE_DUG, nodeDug, this);
		mgr->reg(MtEvent::PLAYER_DAMAGE, playerDamage, this);
		mgr->reg(MtEvent::PLAYER_FALLING_DAMAGE, playerFallingDamage, this);
	}

	void step(float dtime)
	{
		m_player_step_timer -= dtime;
		m_player_jump_timer -= dtime;
	}
};
