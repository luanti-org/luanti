// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013-22 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "mods.h"


/**
 * ModConfiguration is a subset of installed mods. This class
 * is used to resolve dependencies and return a sorted list of mods.
 *
 * This class should not be extended from, but instead used as a
 * component in other classes.
 */
class ModConfiguration
{
public:
	/**
	 * @returns true if all dependencies are fulfilled.
	 */
	inline bool isConsistent() const { return m_unsatisfied_mods.empty(); }

	inline const std::vector<ModSpec> &getUnsatisfiedMods() const
	{
		return m_unsatisfied_mods;
	}

	/**
	 * List of mods sorted such that they can be loaded in the
	 * given order with all dependencies being fulfilled.
	 *
	 * I.e: every mod in this list has only dependencies on mods which
	 * appear earlier in the vector.
	 */
	const std::vector<ModSpec> &getMods() const { return m_sorted_mods; }

	std::string getUnsatisfiedModsError() const;

	/**
	 * Adds all mods in the given path. used for games, modpacks
	 * and world-specific mods (worldmods-folders)
	 *
	 * @param path To search, should be absolute
	 * @param virtual_path Virtual path for this directory, see comment in ModSpec
	 */
	void addModsInPath(const std::string &path, const std::string &virtual_path);

	/**
	 * Adds all mods in `new_mods`
	 */
	void addMods(const std::vector<ModSpec> &new_mods);

	/**
	 * Adds game mods
	 */
	void addGameMods(const SubgameSpec &gamespec);

	/**
	 * Adds mods specified by a world.mt config
	 *
	 * @param settings_path Path to world.mt
	 * @param modPaths Map from virtual name to mod path
	 */
	void addModsFromConfig(const std::string &settings_path,
			const std::unordered_map<std::string, std::string> &modPaths);

	/**
	 * Call this function once all mods have been added
	 */
	void checkConflictsAndDeps();

private:
	std::string m_first_mod; // "" <=> no mod
	std::string m_last_mod; // "" <=> no mod

	std::vector<ModSpec> m_sorted_mods;

	/**
	 * move mods from m_unsatisfied_mods to m_sorted_mods
	 * in an order that satisfies dependencies
	 */
	void resolveDependencies();

	// mods with unmet dependencies. Before dependencies are resolved,
	// this is where all mods are stored. Afterwards this contains
	// only the ones with really unsatisfied dependencies.
	std::vector<ModSpec> m_unsatisfied_mods;

	// set of mod names for which an unresolved name conflict
	// exists. A name conflict happens when two or more mods
	// at the same level have the same name but different paths.
	// Levels (mods in higher levels override mods in lower levels):
	// 1. game mod in modpack; 2. game mod;
	// 3. world mod in modpack; 4. world mod;
	// 5. addon mod in modpack; 6. addon mod.
	std::unordered_set<std::string> m_name_conflicts;
};
