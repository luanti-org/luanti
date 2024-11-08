// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "sound_openal.h"

#include "sound_singleton.h"
#include "proxy_sound_manager.h"

std::shared_ptr<SoundManagerSingleton> g_sound_manager_singleton;

std::shared_ptr<SoundManagerSingleton> createSoundManagerSingleton()
{
	auto smg = std::make_shared<SoundManagerSingleton>();
	if (!smg->init()) {
		smg.reset();
	}
	return smg;
}

std::unique_ptr<ISoundManager> createOpenALSoundManager(SoundManagerSingleton *smg,
		std::unique_ptr<SoundFallbackPathProvider> fallback_path_provider)
{
	return std::make_unique<sound::ProxySoundManager>(smg, std::move(fallback_path_provider));
};
