// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#pragma once

#include <memory>
#include "al_helpers.h"

namespace sound {

/**
 * Class for the openal device and context
 */
class SoundManagerSingleton
{
public:
	struct AlcDeviceDeleter {
		void operator()(ALCdevice *p)
		{
			alcCloseDevice(p);
		}
	};

	struct AlcContextDeleter {
		void operator()(ALCcontext *p)
		{
			alcMakeContextCurrent(nullptr);
			alcDestroyContext(p);
		}
	};

	using unique_ptr_alcdevice = std::unique_ptr<ALCdevice, AlcDeviceDeleter>;
	using unique_ptr_alccontext = std::unique_ptr<ALCcontext, AlcContextDeleter>;

	unique_ptr_alcdevice  m_device;
	unique_ptr_alccontext m_context;

public:
	bool init();

	~SoundManagerSingleton();
};

} // namespace sound
