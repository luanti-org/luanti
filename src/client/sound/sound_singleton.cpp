// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "sound_singleton.h"
#include "al_extensions.h"
#include "porting.h"

#include <cassert>

namespace sound {

bool SoundManagerSingleton::init()
{
	if (!(m_device = unique_ptr_alcdevice(alcOpenDevice(nullptr)))) {
		errorstream << "Audio: Global Initialization: Failed to open device" << std::endl;
		return false;
	}

	if (!(m_context = unique_ptr_alccontext(alcCreateContext(m_device.get(), nullptr)))) {
		errorstream << "Audio: Global Initialization: Failed to create context" << std::endl;
		return false;
	}

	if (!alcMakeContextCurrent(m_context.get())) {
		errorstream << "Audio: Global Initialization: Failed to make current context" << std::endl;
		return false;
	}

	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

	// Speed of sound in nodes per second
	// FIXME: This value assumes 1 node sidelength = 1 meter, and "normal" air.
	//        Ideally this should be mod-controlled.
	alSpeedOfSound(343.3f);

	// doppler effect turned off for now, for best backwards compatibility
	alDopplerFactor(0.0f);

	if (alGetError() != AL_NO_ERROR) {
		errorstream << "Audio: Global Initialization: OpenAL Error " << alGetError() << std::endl;
		return false;
	}

#if defined(ALC_SOFT_system_events) && defined(ALC_SOFT_reopen_device)
	const ALExtensions ext(m_device.get());
	constexpr ALCenum event_type = ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT;
	if (ext.alcReopenDeviceSOFT && ext.alcEventCallbackSOFT &&
			ext.alcEventIsSupportedSOFT(event_type, ALC_PLAYBACK_DEVICE_SOFT) ==
					ALC_EVENT_SUPPORTED_SOFT) {
		ext.alcEventCallbackSOFT(&SoundManagerSingleton::onSystemEvent, this);
		m_system_events_registered = ext.alcEventControlSOFT(1, &event_type, ALC_TRUE);
		if (m_system_events_registered) {
			infostream << "Audio: Default output device notifications enabled"
					<< std::endl;
		} else {
			ext.alcEventCallbackSOFT(nullptr, nullptr);
			m_default_device_change_pending.store(false);
			warningstream << "Audio: Failed to enable default output device notifications: "
					<< alcGetError(nullptr) << std::endl;
		}
	}
#endif

	infostream << "Audio: Global Initialized: OpenAL " << alGetString(AL_VERSION)
		<< ", using " << alcGetString(m_device.get(), ALC_DEVICE_SPECIFIER)
		<< std::endl;

	return true;
}

#ifdef ALC_SOFT_reopen_device
void SoundManagerSingleton::updateDefaultOutput(LPALCREOPENDEVICESOFT reopen)
{
	if (!reopen)
		return;

	constexpr unsigned MAX_ATTEMPTS = 3;
	constexpr u64 RETRY_DELAY_MS = 1000;
	if (m_default_device_change_pending.exchange(false)) {
		m_default_output_attempts_left = MAX_ATTEMPTS;
		m_next_default_output_attempt_ms = 0;
	}

	if (m_default_output_attempts_left == 0 ||
			porting::getTimeMs() < m_next_default_output_attempt_ms)
		return;

	--m_default_output_attempts_left;
	if (reopen(m_device.get(), nullptr, nullptr)) {
		m_default_output_attempts_left = 0;
		infostream << "Audio: Switched to default output device" << std::endl;
	} else {
		warningstream << "Audio: Failed to switch to default output device: "
				<< alcGetError(m_device.get()) << std::endl;
		if (m_default_output_attempts_left > 0)
			m_next_default_output_attempt_ms = porting::getTimeMs() + RETRY_DELAY_MS;
	}
}
#endif

#ifdef ALC_SOFT_system_events
void ALC_APIENTRY SoundManagerSingleton::onSystemEvent(ALCenum eventType,
		ALCenum deviceType, ALCdevice *, ALCsizei, const ALCchar *,
		void *userParam) noexcept
{
	if (eventType != ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT
			|| deviceType != ALC_PLAYBACK_DEVICE_SOFT)
		return;

	auto *singleton = static_cast<SoundManagerSingleton *>(userParam);
	singleton->m_default_device_change_pending.store(true);
}
#endif

SoundManagerSingleton::~SoundManagerSingleton()
{
#ifdef ALC_SOFT_system_events
	if (m_system_events_registered) {
		auto set_callback = reinterpret_cast<LPALCEVENTCALLBACKSOFT>(
				alcGetProcAddress(nullptr, "alcEventCallbackSOFT"));
		assert(set_callback);
		set_callback(nullptr, nullptr);
	}
#endif

	infostream << "Audio: Global Deinitialized." << std::endl;
}

} // namespace sound
