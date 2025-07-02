// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "mods.h"
#include "filesys.h"
#include "log.h"
#include "scripting_server.h"
#include "content/subgames.h"
#include "porting.h"

/**
 * Manage server mods
 *
 * All new calls to this class must be tested in test_servermodmanager.cpp
 */

ServerModManager::ServerModManager(const std::string &worldpath, SubgameSpec gamespec)
{
	// Add all game mods and all world mods
	configuration.addGameMods(gamespec);
	configuration.addModsInPath(worldpath + DIR_DELIM + "worldmods", "worldmods");

	// Load normal mods
	std::string worldmt = worldpath + DIR_DELIM + "world.mt";
	configuration.addModsFromConfig(worldmt, gamespec.addon_mods_paths);
	configuration.checkConflictsAndDeps();
}

// This function cannot be currenctly easily tested but it should be ASAP
void ServerModManager::loadMods(ServerScripting &script)
{
	// Print mods
	infostream << "Server: Loading mods: ";
	for (const ModSpec &mod : configuration.getMods()) {
		infostream << mod.name << " ";
	}
	infostream << std::endl;

	// Load and run "mod" scripts
	auto t0 = porting::getTimeMs();
	for (const ModSpec &mod : configuration.getMods()) {
		mod.checkAndLog();

		auto t1 = porting::getTimeMs();
		std::string script_path = mod.path + DIR_DELIM + "init.lua";
		script.loadMod(script_path, mod.name);
		infostream << "Mod \"" << mod.name << "\" loaded after "
			<< (porting::getTimeMs() - t1) << " ms" << std::endl;
	}

	// Run a callback when mods are loaded
	script.on_mods_loaded();

	infostream << "All mods loaded after " << (porting::getTimeMs() - t0)
		<< " ms" << std::endl;
}

const ModSpec *ServerModManager::getModSpec(const std::string &modname) const
{
	for (const auto &mod : configuration.getMods()) {
		if (mod.name == modname)
			return &mod;
	}

	return nullptr;
}

void ServerModManager::getModNames(std::vector<std::string> &modlist) const
{
	for (const ModSpec &spec : configuration.getMods())
		modlist.push_back(spec.name);
}

void ServerModManager::getModsMediaPaths(std::vector<std::string> &paths) const
{
	// Iterate mods in reverse load order: Media loading expects higher priority media files first
	// and mods loading later should be able to override media of already loaded mods
	const auto &mods = configuration.getMods();
	for (auto it = mods.crbegin(); it != mods.crend(); it++) {
		const ModSpec &spec = *it;
		fs::GetRecursiveDirs(paths, spec.path + DIR_DELIM + "textures");
		fs::GetRecursiveDirs(paths, spec.path + DIR_DELIM + "sounds");
		fs::GetRecursiveDirs(paths, spec.path + DIR_DELIM + "media");
		fs::GetRecursiveDirs(paths, spec.path + DIR_DELIM + "models");
		fs::GetRecursiveDirs(paths, spec.path + DIR_DELIM + "locale");
		fs::GetRecursiveDirs(paths, spec.path + DIR_DELIM + "fonts");
	}
}
