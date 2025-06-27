// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <set>
#include <unordered_map>
#include <vector>

class Settings;

struct SubgameSpec
{
	std::string id;
	std::string title;
	std::string author;
	int release;
	std::string first_mod; // "" <=> no mod
	std::string last_mod; // "" <=> no mod
	std::string lua_mapgen; // "" <=> use internal
	std::string path;
	std::string gamemods_path;

	/**
	 * Map from virtual path to mods path
	 */
	std::unordered_map<std::string, std::string> addon_mods_paths;
	std::unordered_map<std::string, std::string> mapgen_paths;

	// For logging purposes
	std::vector<const char *> deprecation_msgs;

	SubgameSpec(const std::string &id = "", const std::string &path = "",
			const std::string &gamemods_path = "",
			const std::unordered_map<std::string, std::string> &addon_mods_paths = {},
			const std::string &title = "",
			const std::string &author = "", int release = 0,
			const std::string &first_mod = "",
			const std::string &last_mod = "",
			const std::string &lua_mapgen = "",
			const std::unordered_map<std::string, std::string> &mapgen_paths = {}) :
			id(id),
			title(title), author(author), release(release),
			first_mod(first_mod),
			last_mod(last_mod),
			lua_mapgen(lua_mapgen),
			path(path),
			gamemods_path(gamemods_path),
			addon_mods_paths(addon_mods_paths),
			mapgen_paths(mapgen_paths)
	{
	}

	bool isValid() const { return (!id.empty() && !path.empty()); }
	void checkAndLog() const;
};

SubgameSpec findSubgame(const std::string &id);
SubgameSpec findWorldSubgame(const std::string &world_path);

std::set<std::string> getAvailableGameIds();
std::vector<SubgameSpec> getAvailableGames();
// Get the list of paths to mods in the environment variable $MINETEST_MOD_PATH
std::vector<std::string> getEnvModPaths();

bool getWorldExists(const std::string &world_path);
//! Try to get the displayed name of a world
std::string getWorldName(const std::string &world_path, const std::string &default_name);
std::string getWorldGameId(const std::string &world_path, bool can_be_legacy = false);

struct WorldSpec
{
	std::string path;
	std::string name;
	std::string gameid;

	WorldSpec(const std::string &path = "", const std::string &name = "",
			const std::string &gameid = "") :
			path(path),
			name(name), gameid(gameid)
	{
	}

	bool isValid() const
	{
		return (!name.empty() && !path.empty() && !gameid.empty());
	}
};

std::vector<WorldSpec> getAvailableWorlds();

// loads the subgame's config and creates world directory
// and world.mt if they don't exist
void loadGameConfAndInitWorld(const std::string &path, const std::string &name,
		const SubgameSpec &gamespec, bool create_world, const std::string &lua_mapgen);
