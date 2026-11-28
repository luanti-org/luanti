// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 DS

#include "al_extensions.h"

#include "settings.h"
#include "util/string.h"
#include <unordered_set>

namespace sound {

ALExtensions::ALExtensions(ALCdevice *deviceHandle [[maybe_unused]])
{
	auto blacklist_vec = str_split(g_settings->get("sound_extensions_blacklist"), ',');
	for (auto &s : blacklist_vec) {
		s = trim(s);
	}
	std::unordered_set<std::string> blacklist;
	blacklist.insert(blacklist_vec.begin(), blacklist_vec.end());

	const auto check_and_log_extension = [&blacklist](const char *ext_name,
			bool compiled, bool found) {
		bool blacklisted = blacklist.find(ext_name) != blacklist.end();
		if (blacklisted)
			infostream << "ALExtensions: Blacklisted: " << ext_name << std::endl;
		if (!compiled) {
			infostream << "ALExtensions: Not compiled with: " << ext_name << std::endl;
			return false;
		}
		infostream << "ALExtensions: " << (found ? "Found: " : "Not found: ")
				<< ext_name << std::endl;
		return found && !blacklisted;
	};

	{
		constexpr const char *ext_name = "AL_SOFT_direct_channels_remix";
#ifndef AL_SOFT_direct_channels_remix
		check_and_log_extension(ext_name, false, false);
#else
		have_ext_AL_SOFT_direct_channels_remix = check_and_log_extension(
				ext_name, true, alIsExtensionPresent(ext_name));
#endif
	}

	{
		constexpr const char *ext_name = "ALC_SOFT_reopen_device";
#ifndef ALC_SOFT_reopen_device
		check_and_log_extension(ext_name, false, false);
#else
		if (check_and_log_extension(ext_name, true,
				alcIsExtensionPresent(deviceHandle, ext_name))) {
			alcReopenDeviceSOFT = reinterpret_cast<LPALCREOPENDEVICESOFT>(
					alcGetProcAddress(deviceHandle, "alcReopenDeviceSOFT"));
			if (!alcReopenDeviceSOFT)
				warningstream << "ALExtensions: Function not found: "
						"alcReopenDeviceSOFT" << std::endl;
		}
#endif
	}

	{
		constexpr const char *ext_name = "ALC_SOFT_system_events";
#ifndef ALC_SOFT_system_events
		check_and_log_extension(ext_name, false, false);
#else
		if (check_and_log_extension(ext_name, true,
				alcIsExtensionPresent(nullptr, ext_name))) {
			alcEventIsSupportedSOFT = reinterpret_cast<LPALCEVENTISSUPPORTEDSOFT>(
					alcGetProcAddress(nullptr, "alcEventIsSupportedSOFT"));
			alcEventControlSOFT = reinterpret_cast<LPALCEVENTCONTROLSOFT>(
					alcGetProcAddress(nullptr, "alcEventControlSOFT"));
			alcEventCallbackSOFT = reinterpret_cast<LPALCEVENTCALLBACKSOFT>(
					alcGetProcAddress(nullptr, "alcEventCallbackSOFT"));
			if (!alcEventIsSupportedSOFT || !alcEventControlSOFT
					|| !alcEventCallbackSOFT) {
				warningstream << "ALExtensions: Functions not found for: "
						<< ext_name << std::endl;
				alcEventIsSupportedSOFT = nullptr;
				alcEventControlSOFT = nullptr;
				alcEventCallbackSOFT = nullptr;
			}
		}
#endif
	}
}

}
