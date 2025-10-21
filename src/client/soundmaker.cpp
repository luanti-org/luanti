// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Luanti contributors

#include "soundmaker.h"
#include "client/event_manager.h"

void SoundMaker::nodeDug(MtEvent *e, void *data)
{
	SoundMaker *sm = (SoundMaker *)data;
	NodeDugEvent *nde = (NodeDugEvent *)e;
	sm->m_sound->playSound(0, sm->m_ndef->get(nde->n).sound_dug);
}
