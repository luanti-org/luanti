// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti developers

#include "sound_maker.h"

#include "mtevent.h"
#include "nodedef.h"
#include "sound.h"

void SoundMaker::playPlayerStep()
{
	if (m_player_step_timer <= 0.0f && m_player_step_sound.exists()) {
		m_player_step_timer = 0.03f;
		if (makes_footstep_sound)
			m_sound->playSound(0, m_player_step_sound);
	}
}

void SoundMaker::playPlayerJump()
{
	if (m_player_jump_timer <= 0.0f) {
		m_player_jump_timer = 0.2f;
		m_sound->playSound(0, SoundSpec("player_jump", 0.5f));
	}
}

void SoundMaker::viewBobbingStep(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	sm->playPlayerStep();
}

void SoundMaker::playerRegainGround(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	sm->playPlayerStep();
}

void SoundMaker::playerJump(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	sm->playPlayerJump();
}

void SoundMaker::cameraPunchLeft(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	sm->m_sound->playSound(0, sm->m_player_leftpunch_sound);
	sm->m_sound->playSound(0, sm->m_player_leftpunch_sound2);
}

void SoundMaker::cameraPunchRight(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	sm->m_sound->playSound(0, sm->m_player_rightpunch_sound);
}

void SoundMaker::nodeDug(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	NodeDugEvent *nde = static_cast<NodeDugEvent*>(e);
	sm->m_sound->playSound(0, sm->m_ndef->get(nde->n).sound_dug);
}

void SoundMaker::playerDamage(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	sm->m_sound->playSound(0, SoundSpec("player_damage", 0.5f));
}

void SoundMaker::playerFallingDamage(MtEvent *e, void *data)
{
	SoundMaker *sm = static_cast<SoundMaker*>(data);
	sm->m_sound->playSound(0, SoundSpec("player_falling_damage", 0.5f));
}

void SoundMaker::registerReceiver(MtEventManager *mgr)
{
	mgr->reg(MtEvent::VIEW_BOBBING_STEP, SoundMaker::viewBobbingStep, this);
	mgr->reg(MtEvent::PLAYER_REGAIN_GROUND, SoundMaker::playerRegainGround, this);
	mgr->reg(MtEvent::PLAYER_JUMP, SoundMaker::playerJump, this);
	mgr->reg(MtEvent::CAMERA_PUNCH_LEFT, SoundMaker::cameraPunchLeft, this);
	mgr->reg(MtEvent::CAMERA_PUNCH_RIGHT, SoundMaker::cameraPunchRight, this);
	mgr->reg(MtEvent::NODE_DUG, SoundMaker::nodeDug, this);
	mgr->reg(MtEvent::PLAYER_DAMAGE, SoundMaker::playerDamage, this);
	mgr->reg(MtEvent::PLAYER_FALLING_DAMAGE, SoundMaker::playerFallingDamage, this);
}

void SoundMaker::update(f32 dtime, bool makes_footstep_sound, const SoundSpec &sound_footstep)
{
	// Footstep sound and step
	this->makes_footstep_sound = makes_footstep_sound;
	if (makes_footstep_sound) {
		m_player_step_timer -= dtime;
		m_player_jump_timer -= dtime;
	}
	m_player_step_sound = sound_footstep;
}
