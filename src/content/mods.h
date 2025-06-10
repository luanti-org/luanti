// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include <list>
#include <set>
#include <vector>
#include <string>
#include <map>
#include "json-forwards.h"
#include <unordered_set>
#include "util/basic_macros.h"
#include "config.h"
#include "metadata.h"
#include "subgames.h"

class ModStorageDatabase;

#define MODNAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyz0123456789_"

struct ModSpec
{
	std::string name;
	std::string author;
	std::string path; // absolute path on disk
	std::string desc;
	int release = 0;

	// if normal mod:
	std::unordered_set<std::string> depends;
	std::unordered_set<std::string> optdepends;
	std::unordered_set<std::string> unsatisfied_depends;

	bool part_of_modpack = false;
	bool is_modpack = false;

	// lua-defined mapgen
	bool is_mapgen = false;

	/**
	 * A constructed canonical path to represent this mod's location.
	 * This intended to be used as an identifier for a modpath that tolerates file movement,
	 * and cannot be used to read the mod files.
	 *
	 * Note that `mymod` is the directory name, not the mod name specified in mod.conf.
	 *
	 * Ex:
	 *
	 * - mods/mymod
	 * - mods/mymod (1)
	 *     (^ this would have name=mymod in mod.conf)
	 * - mods/modpack1/mymod
	 * - games/mygame/mods/mymod
	 * - worldmods/mymod
	 */
	std::string virtual_path;

	// For logging purposes
	std::vector<const char *> deprecation_msgs;

	// if modpack:
	std::map<std::string, ModSpec> modpack_content;

	ModSpec()
	{
	}

	ModSpec(const std::string &name, const std::string &path, bool part_of_modpack, const std::string &virtual_path) :
			name(name), path(path), part_of_modpack(part_of_modpack), virtual_path(virtual_path)
	{
	}

	void checkAndLog() const;
};

/**
 * Retrieves depends, optdepends, is_modpack and modpack_content
 *
 * @returns false if not a mod
 */
[[nodiscard]] bool parseModContents(ModSpec &mod);

/**
 * Gets a list of all mods and modpacks in path
 *
 * @param Path to search, should be absolute
 * @param part_of_modpack Is this searching within a modpack?
 * @param virtual_path Virtual path for this directory, see comment in ModSpec
 * @returns map of mods
 */
std::map<std::string, ModSpec> getModsInPath(const std::string &path,
		const std::string &virtual_path, bool part_of_modpack = false);

// replaces modpack Modspecs with their content
std::vector<ModSpec> flattenMods(const std::map<std::string, ModSpec> &mods);


class ModStorage : public IMetadata
{
public:
	ModStorage() = delete;
	ModStorage(const std::string &mod_name, ModStorageDatabase *database);
	~ModStorage() = default;

	const std::string &getModName() const { return m_mod_name; }

	void clear() override;

	bool contains(const std::string &name) const override;

	bool setString(const std::string &name, std::string_view var) override;

	const StringMap &getStrings(StringMap *place) const override;

	const std::vector<std::string> &getKeys(std::vector<std::string> *place) const override;

protected:
	const std::string *getStringRaw(const std::string &name,
			std::string *place) const override;

private:
	std::string m_mod_name;
	ModStorageDatabase *m_database;
};
