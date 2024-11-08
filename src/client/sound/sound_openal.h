// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "client/sound.h"
#include <memory>

namespace sound { class SoundManagerSingleton; }
using sound::SoundManagerSingleton;

extern std::shared_ptr<SoundManagerSingleton> g_sound_manager_singleton;

std::shared_ptr<SoundManagerSingleton> createSoundManagerSingleton();
std::unique_ptr<ISoundManager> createOpenALSoundManager(
		SoundManagerSingleton *smg,
		std::unique_ptr<SoundFallbackPathProvider> fallback_path_provider);
