// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#pragma once

#include <atomic>
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

#ifdef ALC_SOFT_reopen_device
	void updateDefaultOutput(LPALCREOPENDEVICESOFT reopen);
#endif

private:
	std::atomic<bool> m_default_device_change_pending{false};
#ifdef ALC_SOFT_reopen_device
	// Kept with the shared device across the menu and game audio threads.
	unsigned m_default_output_attempts_left = 0;
	u64 m_next_default_output_attempt_ms = 0;
#endif

#ifdef ALC_SOFT_system_events
	bool m_system_events_registered = false;

	static void ALC_APIENTRY onSystemEvent(ALCenum eventType, ALCenum deviceType,
			ALCdevice *device, ALCsizei length, const ALCchar *message,
			void *userParam) noexcept;
#endif
};

} // namespace sound
