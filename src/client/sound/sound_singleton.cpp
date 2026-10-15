// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "sound_singleton.h"

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

	if (ALenum err = alGetError(); err != AL_NO_ERROR) {
		errorstream << "Audio: Global Initialization: OpenAL Error " << err << std::endl;
		return false;
	}

	// Resolve extensions at runtime so older OpenAL headers still work.
	// Holding sources prevents a transient disconnect from making live sounds
	// look finished, and preserves stream queues and playback offsets on reopen.
	if (alcIsExtensionPresent(m_device.get(), "ALC_EXT_disconnect") &&
			alcIsExtensionPresent(m_device.get(), "ALC_SOFT_reopen_device") &&
			(alIsExtensionPresent("AL_SOFT_hold_on_disconnect") ||
			 alIsExtensionPresent("AL_SOFTX_hold_on_disconnect"))) {
		auto reopen = reinterpret_cast<ReopenDevice>(
				alcGetProcAddress(m_device.get(), "alcReopenDeviceSOFT"));
		ALCenum connected = alcGetEnumValue(m_device.get(), "ALC_CONNECTED");
		ALenum stop_sources = alGetEnumValue("AL_STOP_SOURCES_ON_DISCONNECT_SOFT");
		if (reopen && connected && stop_sources) {
			alDisable(stop_sources);
			if (alGetError() == AL_NO_ERROR) {
				m_reopen_device = reopen;
				m_connected_enum = connected;
				infostream << "Audio: Automatic device recovery enabled" << std::endl;
			}
		}
	}

	infostream << "Audio: Global Initialized: OpenAL " << alGetString(AL_VERSION)
		<< ", using " << alcGetString(m_device.get(), ALC_DEVICE_SPECIFIER)
		<< std::endl;

	return true;
}

bool SoundManagerSingleton::recoverDevice()
{
	if (!m_reopen_device)
		return true; // Keep the existing behavior on other OpenAL implementations.

	// The menu and game sound managers share this device and context.
	std::lock_guard<std::mutex> lock(m_reconnect_mutex);
	ALCint connected = ALC_TRUE;
	alcGetIntegerv(m_device.get(), m_connected_enum, 1, &connected);
	auto result = m_reconnect.poll(DeviceReconnect::Clock::now(),
			connected == ALC_TRUE, [&] {
				if (m_reopen_device(m_device.get(), nullptr, nullptr))
					return true;
				// A missing endpoint is expected during a display reconnect.
				alcGetError(m_device.get());
				return false;
			});
	using Result = DeviceReconnect::Result;
	if (result == Result::Lost)
		warningstream << "Audio: Output device disconnected; waiting to reconnect"
				<< std::endl;
	else if (result == Result::Recovered)
		actionstream << "Audio: Output device reconnected" << std::endl;

	return result == Result::Connected || result == Result::Recovered;
}

SoundManagerSingleton::~SoundManagerSingleton()
{
	infostream << "Audio: Global Deinitialized." << std::endl;
}

} // namespace sound
