// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "content/mod_configuration.h"
#include <memory>

class ServerScripting;

/**
 * Manages server mods
 *
 * All new calls to this class must be tested in test_servermodmanager.cpp
 */
class ServerModManager
{
	ModConfiguration configuration;

public:

	/**
	 * Creates a ServerModManager
	 * @param worldpath path to world
	 * @param gamespec game used by the world
	 */
	ServerModManager(const std::string &worldpath, SubgameSpec gamespec);

	/**
	 * Creates an empty ServerModManager. For testing purposes.
	 * Note: definition looks like this so it isn't accidentally used.
	*/
	explicit ServerModManager(std::nullptr_t) {};

	void loadMods(ServerScripting &script);
	const ModSpec *getModSpec(const std::string &modname) const;
	void getModNames(std::vector<std::string> &modlist) const;

	inline const std::vector<ModSpec> &getMods() const {
		return configuration.getMods();
	}

	inline const std::vector<ModSpec> &getUnsatisfiedMods() const {
		return configuration.getUnsatisfiedMods();
	}

	inline bool isConsistent() const {
		return configuration.isConsistent();
	}

	inline std::string getUnsatisfiedModsError() const {
		return configuration.getUnsatisfiedModsError();
	}

	/**
	 * Recursively gets all paths of mod folders that can contain media files.
	 *
	 * Result is ordered in descending priority, ie. files from an earlier path
	 * should not be replaced by files from a latter one.
	 *
	 * @param paths result vector
	 */
	void getModsMediaPaths(std::vector<std::string> &paths) const;
};
