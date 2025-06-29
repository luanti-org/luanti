// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#include <memory>
#include <string>
#include "settings.h"

struct NoiseParams;
struct MapgenParams;

/*
	MapSettingsManager is a centralized object for management (creating,
	loading, storing, saving, etc.) of config settings related to the Map.

	It has two phases: the initial r/w "gather and modify settings" state, and
	the final r/o "read and save settings" state.

	The typical use case is, in order, as follows:
	- Create a MapSettingsManager object
	- Try to load map metadata into it from the metadata file
	- Manually view and modify the current configuration as desired through a
	  Settings-like interface
	- When all modifications are finished, create a 'Parameters' object
	  containing the finalized, active parameters.  This could be passed along
	  to whichever Map-related objects that may require it.
	- Save these active settings to the metadata file when requested
*/
class MapSettingsManager {
public:
	MapSettingsManager(const std::string &map_meta_path);
	~MapSettingsManager();

	// Finalized map generation parameters
	MapgenParams *mapgen_params = nullptr;

	bool getMapSetting(const std::string &name, std::string *value_out) const;

	bool getNoiseParams(const std::string &name,
		NoiseParams *value_out) const;

	// Note: Map config becomes read-only after makeMapgenParams() gets called
	// (i.e. mapgen_params is non-NULL).  Attempts to set map config after
	// params have been finalized will result in failure.
	bool setMapSetting(const std::string &name,
		const std::string &value, bool override_meta = false);

	bool setMapSettingNoiseParams(const std::string &name,
		const NoiseParams *value, bool override_meta = false);

	bool loadMapMeta();
	bool saveMapMeta();
	MapgenParams *makeMapgenParams();

private:
	std::string m_map_meta_path;

	SettingsHierarchy m_hierarchy;
	std::unique_ptr<Settings> m_defaults;
	std::unique_ptr<Settings> m_map_settings;
};
