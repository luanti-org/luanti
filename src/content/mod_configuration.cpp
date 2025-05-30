// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013-22 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "mod_configuration.h"
#include "log.h"
#include "settings.h"
#include "filesys.h"
#include "gettext.h"
#include "exceptions.h"
#include "util/numeric.h"
#include <optional>
#include <algorithm>

std::string ModConfiguration::getUnsatisfiedModsError() const
{
	std::ostringstream error;
	error << gettext("Some mods have unsatisfied dependencies:") << std::endl;

	for (const ModSpec &mod : m_unsatisfied_mods) {
		//~ Error when a mod is missing dependencies. Ex: "Mod Title is missing: mod1, mod2, mod3"
		error << " - " << fmtgettext("%s is missing:", mod.name.c_str());
		for (const std::string &unsatisfied_depend : mod.unsatisfied_depends)
			error << " " << unsatisfied_depend;
		error << "\n";
	}

	error << "\n"
		<< gettext("Install and enable the required mods, or disable the mods causing errors.") << "\n"
		<< gettext("Note: this may be caused by a dependency cycle, in which case try updating the mods.");

	return error.str();
}

void ModConfiguration::addModsInPath(const std::string &path, const std::string &virtual_path)
{
	verbosestream << "Adding mods from path " << path << " virtual=\""
		<< virtual_path << "\"" << std::endl;
	addMods(flattenMods(getModsInPath(path, virtual_path)));
}

void ModConfiguration::addMods(const std::vector<ModSpec> &new_mods)
{
	// Maintain a map of all existing m_unsatisfied_mods.
	// Keys are mod names and values are indices into m_unsatisfied_mods.
	std::map<std::string, u32> existing_mods;
	for (u32 i = 0; i < m_unsatisfied_mods.size(); ++i) {
		existing_mods[m_unsatisfied_mods[i].name] = i;
	}

	// Add new mods
	for (bool want_from_modpack : {true, false}) {
		// First iteration:
		// Add all the mods that come from modpacks
		// Second iteration:
		// Add all the mods that didn't come from modpacks

		std::set<std::string> seen_this_iteration;

		for (const ModSpec &mod : new_mods) {
			if (mod.part_of_modpack != want_from_modpack)
				continue;

			// unrelated to this code, but we want to assert it somewhere
			assert(fs::IsPathAbsolute(mod.path));

			if (existing_mods.count(mod.name) == 0) {
				// GOOD CASE: completely new mod.
				m_unsatisfied_mods.push_back(mod);
				existing_mods[mod.name] = m_unsatisfied_mods.size() - 1;
			} else if (seen_this_iteration.count(mod.name) == 0) {
				// BAD CASE: name conflict in different levels.
				u32 oldindex = existing_mods[mod.name];
				const ModSpec &oldmod = m_unsatisfied_mods[oldindex];
				warningstream << "Mod name conflict detected: \""
						<< mod.name << "\"" << std::endl
						<< "Will not load: " << oldmod.path
						<< std::endl
						<< "Overridden by: " << mod.path
						<< std::endl;
				m_unsatisfied_mods[oldindex] = mod;

				// If there was a "VERY BAD CASE" name conflict
				// in an earlier level, ignore it.
				m_name_conflicts.erase(mod.name);
			} else {
				// VERY BAD CASE: name conflict in the same level.
				u32 oldindex = existing_mods[mod.name];
				const ModSpec &oldmod = m_unsatisfied_mods[oldindex];
				warningstream << "Mod name conflict detected: \""
						<< mod.name << "\"" << std::endl
						<< "Will not load: " << oldmod.path
						<< std::endl
						<< "Will not load: " << mod.path
						<< std::endl;
				m_unsatisfied_mods[oldindex] = mod;
				m_name_conflicts.insert(mod.name);
			}

			seen_this_iteration.insert(mod.name);
		}
	}
}

void ModConfiguration::addGameMods(const SubgameSpec &gamespec)
{
	std::string game_virtual_path;
	game_virtual_path.append("games/").append(gamespec.id).append("/mods");
	addModsInPath(gamespec.gamemods_path, game_virtual_path);

	m_first_mod = gamespec.first_mod;
	m_last_mod = gamespec.last_mod;
}

void ModConfiguration::addModsFromConfig(
		const std::string &settings_path,
		const std::unordered_map<std::string, std::string> &modPaths)
{
	Settings conf;
	std::unordered_map<std::string, std::string> load_mod_names;

	conf.readConfigFile(settings_path.c_str());
	std::vector<std::string> names = conf.getNames();
	for (const std::string &name : names) {
		const auto &value = conf.get(name);
		if (name.compare(0, 9, "load_mod_") == 0 && value != "false" &&
				value != "nil")
			load_mod_names[name.substr(9)] = value;
	}

	// List of enabled non-game non-world mods
	std::vector<ModSpec> addon_mods;

	// Map of modname to a list candidate mod paths. Used to list
	// alternatives if a particular mod cannot be found.
	std::unordered_map<std::string, std::vector<std::string>> candidates;

	/*
	 * Iterate through all installed mods except game mods and world mods
	 *
	 * If the mod is enabled, add it to `addon_mods`. *
	 *
	 * Alternative candidates for a modname are stored in `candidates`,
	 * and used in an error message later.
	 */
	for (const auto &modPath : modPaths) {
		std::vector<ModSpec> addon_mods_in_path = flattenMods(getModsInPath(modPath.second, modPath.first));
		for (const auto &mod : addon_mods_in_path) {
			const auto &pair = load_mod_names.find(mod.name);
			if (pair != load_mod_names.end()) {
				if (is_yes(pair->second) || pair->second == mod.virtual_path) {
					addon_mods.push_back(mod);
				} else {
					candidates[pair->first].emplace_back(mod.virtual_path);
				}
			} else {
				conf.remove("load_mod_" + mod.name);
			}
		}
	}
	conf.updateConfigFile(settings_path.c_str());

	addMods(addon_mods);

	// Remove all loaded mods from `load_mod_names`
	// NB: as deps have not yet been resolved, `m_unsatisfied_mods` will contain all mods.
	for (const ModSpec &mod : m_unsatisfied_mods)
		load_mod_names.erase(mod.name);

	// Complain about mods declared to be loaded, but not found
	if (!load_mod_names.empty()) {
		errorstream << "The following mods could not be found:";
		for (const auto &pair : load_mod_names)
			errorstream << " \"" << pair.first << "\"";
		errorstream << std::endl;

		for (const auto &pair : load_mod_names) {
			const auto &candidate = candidates.find(pair.first);
			if (candidate != candidates.end()) {
				errorstream << "Unable to load " << pair.first << " as the specified path "
							<< pair.second << " could not be found. "
							<< "However, it is available in the following locations:"
							<< std::endl;
				for (const auto &path : candidate->second) {
					errorstream << " - " << path << std::endl;
				}
			}
		}
	}
}

void ModConfiguration::checkConflictsAndDeps()
{
	// report on name conflicts
	if (!m_name_conflicts.empty()) {
		std::string s = "Unresolved name conflicts for mods ";

		bool add_comma = false;
		for (const auto& it : m_name_conflicts) {
			if (add_comma)
				s.append(", ");
			s.append("\"").append(it).append("\"");
			add_comma = true;
		}
		s.append(".");

		throw ModError(s);
	}

	// get the mods in order
	resolveDependencies();
}

void ModConfiguration::resolveDependencies()
{
	// Compile a list of the mod names we're working with
	std::set<std::string> modnames;
	std::optional<ModSpec> first_mod_spec, last_mod_spec;
	for (ModSpec &mod : m_unsatisfied_mods) {
		if (mod.name == m_first_mod) {
			first_mod_spec = mod;
		} else if (mod.name == m_last_mod) {
			last_mod_spec = mod;
		} else {
			modnames.insert(mod.name);
		}
	}

	// Optionally shuffle unsatisfied mods so non declared depends get found by their devs
	if (g_settings->getBool("random_mod_load_order")) {
		MyRandGenerator rg;
		std::shuffle(m_unsatisfied_mods.begin(),
			m_unsatisfied_mods.end(),
			rg
		);
	}

	// Check for presence of first and last mod
	if (!m_first_mod.empty() && !first_mod_spec.has_value())
		throw ModError("The mod specified as first by the game was not found.");
	if (!m_last_mod.empty() && !last_mod_spec.has_value())
		throw ModError("The mod specified as last by the game was not found.");

	// Check and add first mod
	if (first_mod_spec.has_value()) {
		// Dependencies are not allowed for first mod
		if (!first_mod_spec->depends.empty() || !first_mod_spec->optdepends.empty())
			throw ModError("Mod specified by first_mod cannot have dependencies");

		m_sorted_mods.push_back(*first_mod_spec);
	}

	// Get dependencies (including optional dependencies)
	// of each mod, split mods into satisfied and unsatisfied
	std::vector<ModSpec> satisfied;
	std::list<ModSpec> unsatisfied;
	for (ModSpec mod : m_unsatisfied_mods) {
		if (mod.name == m_first_mod || mod.name == m_last_mod)
			continue; // skip, handled separately

		mod.unsatisfied_depends = mod.depends;
		// check which optional dependencies actually exist
		for (const std::string &optdep : mod.optdepends) {
			if (modnames.count(optdep) != 0)
				mod.unsatisfied_depends.insert(optdep);
		}
		mod.unsatisfied_depends.erase(m_first_mod); // first is already satisfied

		if (last_mod_spec.has_value() && mod.unsatisfied_depends.count(last_mod_spec->name) != 0) {
			throw ModError("Impossible to depend on the mod specified by last_mod");
		}
		// if a mod has no depends it is initially satisfied
		if (mod.unsatisfied_depends.empty()) {
			satisfied.push_back(mod);
		} else {
			unsatisfied.push_back(mod);
		}
	}

	// All dependencies of the last mod are initially unsatisfied
	if (last_mod_spec.has_value()) {
		last_mod_spec->unsatisfied_depends = last_mod_spec->depends;
		last_mod_spec->unsatisfied_depends.erase(m_first_mod);
	}

	// Mods without unmet dependencies can be appended to
	// the sorted list.
	while (!satisfied.empty()) {
		ModSpec mod = satisfied.back();
		m_sorted_mods.push_back(mod);
		satisfied.pop_back();
		for (auto it = unsatisfied.begin(); it != unsatisfied.end();) {
			ModSpec &mod2 = *it;
			mod2.unsatisfied_depends.erase(mod.name);
			if (mod2.unsatisfied_depends.empty()) {
				satisfied.push_back(mod2);
				it = unsatisfied.erase(it);
			} else {
				++it;
			}
		}
		if (last_mod_spec.has_value())
			last_mod_spec->unsatisfied_depends.erase(mod.name);
	}

	// Write back list of unsatisfied mods
	m_unsatisfied_mods.assign(unsatisfied.begin(), unsatisfied.end());

	// Check and add last mod
	if (last_mod_spec.has_value()) {
		if (last_mod_spec->unsatisfied_depends.empty())
			m_sorted_mods.push_back(*last_mod_spec);
		else
			m_unsatisfied_mods.push_back(*last_mod_spec);
	}
}
