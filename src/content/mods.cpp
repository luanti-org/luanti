// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <cctype>
#include <fstream>
#include <json/json.h>
#include <algorithm>
#include "content/mods.h"
#include "database/database.h"
#include "filesys.h"
#include "log.h"
#include "content/subgames.h"
#include "settings.h"
#include "porting.h"
#include "convert_json.h"
#include "script/common/c_internal.h"
#include "exceptions.h"

void ModSpec::checkAndLog() const
{
	if (!string_allowed(name, MODNAME_ALLOWED_CHARS)) {
		throw ModError("Error loading mod \"" + name +
			"\": Mod name does not follow naming conventions: "
				"Only characters [a-z0-9_] are allowed.");
	}

	// Log deprecation messages
	auto handling_mode = get_deprecated_handling_mode();
	if (!deprecation_msgs.empty() && handling_mode != DeprecatedHandlingMode::Ignore) {
		std::ostringstream os;
		os << "Mod " << name << " at " << path << ":" << std::endl;
		for (auto msg : deprecation_msgs)
			os << "\t" << msg << std::endl;

		if (handling_mode == DeprecatedHandlingMode::Error)
			throw ModError(os.str());
		else
			warningstream << os.str();
	}
}

bool parseDependsString(std::string &dep, std::unordered_set<char> &symbols)
{
	dep = trim(dep);
	symbols.clear();
	size_t pos = dep.size();
	while (pos > 0 &&
			!string_allowed(dep.substr(pos - 1, 1), MODNAME_ALLOWED_CHARS)) {
		// last character is a symbol, not part of the modname
		symbols.insert(dep[pos - 1]);
		--pos;
	}
	dep = trim(dep.substr(0, pos));
	return !dep.empty();
}

bool parseModContents(ModSpec &spec)
{
	// NOTE: this function works in mutual recursion with getModsInPath

	spec.depends.clear();
	spec.optdepends.clear();
	spec.is_modpack = false;
	spec.modpack_content.clear();

	// Handle modpacks (defined by containing modpack.txt)
	if (fs::IsFile(spec.path + DIR_DELIM + "modpack.txt") ||
			fs::IsFile(spec.path + DIR_DELIM + "modpack.conf")) {
		spec.is_modpack = true;
		spec.modpack_content = getModsInPath(spec.path, spec.virtual_path, true);
		return true;
	} else if (!fs::IsFile(spec.path + DIR_DELIM + "init.lua")) {
		return false;
	}


	Settings info;
	info.readConfigFile((spec.path + DIR_DELIM + "mod.conf").c_str());

	if (info.exists("name"))
		spec.name = info.get("name");
	else
		spec.deprecation_msgs.push_back("Mods not having a mod.conf file with the name is deprecated.");

	if (info.exists("author"))
		spec.author = info.get("author");

	if (info.exists("release"))
		spec.release = info.getS32("release");

	// Attempt to load dependencies from mod.conf
	bool mod_conf_has_depends = false;
	if (info.exists("depends")) {
		mod_conf_has_depends = true;
		std::string dep = info.get("depends");
		dep.erase(std::remove_if(dep.begin(), dep.end(),
				static_cast<int (*)(int)>(&std::isspace)), dep.end());
		for (const auto &dependency : str_split(dep, ',')) {
			spec.depends.insert(dependency);
		}
	}

	if (info.exists("optional_depends")) {
		mod_conf_has_depends = true;
		std::string dep = info.get("optional_depends");
		dep.erase(std::remove_if(dep.begin(), dep.end(),
				static_cast<int (*)(int)>(&std::isspace)), dep.end());
		for (const auto &dependency : str_split(dep, ',')) {
			spec.optdepends.insert(dependency);
		}
	}

	// Fallback to depends.txt
	if (!mod_conf_has_depends) {
		std::vector<std::string> dependencies;

		std::ifstream is((spec.path + DIR_DELIM + "depends.txt").c_str());

		if (is.good())
			spec.deprecation_msgs.push_back("depends.txt is deprecated, please use mod.conf instead.");

		while (is.good()) {
			std::string dep;
			std::getline(is, dep);
			dependencies.push_back(dep);
		}

		for (auto &dependency : dependencies) {
			std::unordered_set<char> symbols;
			if (parseDependsString(dependency, symbols)) {
				if (symbols.count('?') != 0) {
					spec.optdepends.insert(dependency);
				} else {
					spec.depends.insert(dependency);
				}
			}
		}
	}

	if (info.exists("description"))
		spec.desc = info.get("description");
	else if (fs::ReadFile(spec.path + DIR_DELIM + "description.txt", spec.desc))
		spec.deprecation_msgs.push_back("description.txt is deprecated, please use mod.conf instead.");

	return true;
}

std::map<std::string, ModSpec> getModsInPath(
		const std::string &path, const std::string &virtual_path, bool part_of_modpack)
{
	// NOTE: this function works in mutual recursion with parseModContents

	std::map<std::string, ModSpec> result;
	std::vector<fs::DirListNode> dirlist = fs::GetDirListing(path);
	std::string mod_path;
	std::string mod_virtual_path;

	for (const fs::DirListNode &dln : dirlist) {
		if (!dln.dir)
			continue;

		const std::string &modname = dln.name;
		// Ignore all directories beginning with a ".", especially
		// VCS directories like ".git" or ".svn"
		if (modname[0] == '.')
			continue;

		mod_path.clear();
		mod_path.append(path).append(DIR_DELIM).append(modname);
		mod_path = fs::AbsolutePath(mod_path);

		mod_virtual_path.clear();
		// Intentionally uses / to keep paths same on different platforms
		mod_virtual_path.append(virtual_path).append("/").append(modname);

		ModSpec spec(modname, mod_path, part_of_modpack, mod_virtual_path);
		if (parseModContents(spec)) {
			result[modname] = std::move(spec);
		}
	}
	return result;
}

std::vector<ModSpec> flattenMods(const std::map<std::string, ModSpec> &mods)
{
	std::vector<ModSpec> result;
	for (const auto &it : mods) {
		const ModSpec &mod = it.second;
		if (mod.is_modpack) {
			std::vector<ModSpec> content = flattenMods(mod.modpack_content);
			result.reserve(result.size() + content.size());
			result.insert(result.end(), content.begin(), content.end());

		} else // not a modpack
		{
			result.push_back(mod);
		}
	}
	return result;
}


ModStorage::ModStorage(const std::string &mod_name, ModStorageDatabase *database):
	m_mod_name(mod_name), m_database(database)
{
}

void ModStorage::clear()
{
	m_database->removeModEntries(m_mod_name);
}

bool ModStorage::contains(const std::string &name) const
{
	return m_database->hasModEntry(m_mod_name, name);
}

bool ModStorage::setString(const std::string &name, std::string_view var)
{
	if (var.empty())
		return m_database->removeModEntry(m_mod_name, name);
	else
		return m_database->setModEntry(m_mod_name, name, var);
}

const StringMap &ModStorage::getStrings(StringMap *place) const
{
	place->clear();
	m_database->getModEntries(m_mod_name, place);
	return *place;
}

const std::vector<std::string> &ModStorage::getKeys(std::vector<std::string> *place) const
{
	place->clear();
	m_database->getModKeys(m_mod_name, place);
	return *place;
}

const std::string *ModStorage::getStringRaw(const std::string &name, std::string *place) const
{
	return m_database->getModEntry(m_mod_name, name, place) ? place : nullptr;
}
