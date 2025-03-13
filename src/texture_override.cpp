// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2020 Hugues Ross <hugues.ross@gmail.com>

#include "texture_override.h"

#include "log.h"
#include "filesys.h"
#include "util/string.h"
#include <algorithm>
#include <fstream>
#include <map>

#define override_cast static_cast<override_t>

static const std::map<std::string, OverrideTarget> override_LUT = {
	{ "top", OverrideTarget::TOP },
	{ "bottom", OverrideTarget::BOTTOM },
	{ "left", OverrideTarget::LEFT },
	{ "right", OverrideTarget::RIGHT },
	{ "front", OverrideTarget::FRONT },
	{ "back", OverrideTarget::BACK },
	{ "inventory", OverrideTarget::INVENTORY },
	{ "wield", OverrideTarget::WIELD },
	{ "special1", OverrideTarget::SPECIAL_1 },
	{ "special2", OverrideTarget::SPECIAL_2 },
	{ "special3", OverrideTarget::SPECIAL_3 },
	{ "special4", OverrideTarget::SPECIAL_4 },
	{ "special5", OverrideTarget::SPECIAL_5 },
	{ "special6", OverrideTarget::SPECIAL_6 },
	{ "sides", OverrideTarget::SIDES },
	{ "all", OverrideTarget::ALL_FACES },
	{ "*", OverrideTarget::ALL_FACES }
};

TextureOverrideSource::TextureOverrideSource(const std::string &filepath)
{
	auto infile = open_ifstream(filepath.c_str(), false);
	std::string line;
	int line_index = 0;
	while (std::getline(infile, line)) {
		line_index++;

		// Also trim '\r' on DOS-style files
		line = trim(line);

		// Ignore empty lines and comments
		if (line.empty() || line[0] == '#')
			continue;

		// Format: mod_name:item_name target1[,...] texture_name.png
		std::vector<std::string> splitted = str_split(line, ' ');
		if (splitted.size() < 3) {
			warningstream << filepath << ":" << line_index
					<< " Syntax error in texture override \"" << line
					<< "\": Expected 3 arguments, got " << splitted.size()
					<< std::endl;
			continue;
		}

		TextureOverride texture_override = {};
		texture_override.id = splitted[0];
		texture_override.texture = splitted[2];

		// Parse the target mask
		std::vector<std::string> targets = str_split(splitted[1], ',');
		for (const std::string &target : targets) {
			std::vector<std::string> kvpair = str_split(target, '=');
			if (kvpair.size() == 2) {
				// Key-value pairs
				if (kvpair[0] == "align_world") {
					// Global textures
					texture_override.world_scale = stoi(kvpair[1], 0, U8_MAX);
					continue;
				}
			}
			if (kvpair.size() == 1) {
				// Regular override flags
				auto pair = override_LUT.find(target);

				if (pair != override_LUT.end()) {
					texture_override.target |= override_cast(pair->second);
					continue;
				}
			}

			// Report invalid target
			warningstream << filepath << ":" << line_index
					<< " Syntax error in texture override \"" << line
					<< "\": Unknown target \"" << target << "\""
					<< std::endl;

		}

		// If there are no valid targets, skip adding this override
		if (texture_override.target == override_cast(OverrideTarget::INVALID)) {
			continue;
		}

		m_overrides.push_back(texture_override);
	}
}

//! Get all overrides that apply to item definitions
std::vector<TextureOverride> TextureOverrideSource::getItemTextureOverrides() const
{
	std::vector<TextureOverride> found_overrides;

	for (const TextureOverride &texture_override : m_overrides) {
		if (texture_override.hasTarget(OverrideTarget::ITEM_TARGETS))
			found_overrides.push_back(texture_override);
	}

	return found_overrides;
}

//! Get all overrides that apply to node definitions
std::vector<TextureOverride> TextureOverrideSource::getNodeTileOverrides() const
{
	std::vector<TextureOverride> found_overrides;

	for (const TextureOverride &texture_override : m_overrides) {
		if (texture_override.hasTarget(OverrideTarget::NODE_TARGETS))
			found_overrides.push_back(texture_override);
	}

	return found_overrides;
}
