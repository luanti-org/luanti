// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "settings.h"
#include "irrlichttypes_bloated.h"
#include "exceptions.h"
#include "threading/mutex_auto_lock.h"
#include "util/numeric.h" // rangelim
#include "util/strfnd.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "debug.h"
#include "log.h"
#include "util/serialize.h"
#include "filesys.h"
#include "noise.h"
#include <cctype>
#include <algorithm>

Settings *g_settings = nullptr;
static SettingsHierarchy g_hierarchy;
std::string g_settings_path;
bool g_first_run = false;

std::unordered_map<std::string, const FlagDesc *> Settings::s_flags;

/* Settings hierarchy implementation */

SettingsHierarchy::SettingsHierarchy(Settings *fallback)
{
	layers.push_back(fallback);
}


Settings *SettingsHierarchy::getLayer(int layer) const
{
	if (layer < 0 || layer >= (int)layers.size())
		throw BaseException("Invalid settings layer");
	return layers[layer];
}


Settings *SettingsHierarchy::getParent(int layer) const
{
	assert(layer >= 0 && layer < (int)layers.size());
	// iterate towards the origin (0) to find the next fallback layer
	for (int i = layer - 1; i >= 0; --i) {
		if (layers[i])
			return layers[i];
	}

	return nullptr;
}


void SettingsHierarchy::onLayerCreated(int layer, Settings *obj)
{
	if (layer < 0)
		throw BaseException("Invalid settings layer");
	if ((int)layers.size() < layer + 1)
		layers.resize(layer + 1);

	Settings *&pos = layers[layer];
	if (pos)
		throw BaseException("Setting layer " + itos(layer) + " already exists");

	pos = obj;
	// This feels bad
	if (this == &g_hierarchy && layer == (int)SL_GLOBAL)
		g_settings = obj;
}


void SettingsHierarchy::onLayerRemoved(int layer)
{
	assert(layer >= 0 && layer < (int)layers.size());
	layers[layer] = nullptr;
	if (this == &g_hierarchy && layer == (int)SL_GLOBAL)
		g_settings = nullptr;
}

/* Settings implementation */

Settings *Settings::createLayer(SettingsLayer sl, std::string_view end_tag)
{
	return new Settings(end_tag, &g_hierarchy, (int)sl);
}


Settings *Settings::getLayer(SettingsLayer sl)
{
	return g_hierarchy.getLayer(sl);
}


Settings::Settings(std::string_view end_tag, SettingsHierarchy *h,
		int settings_layer) :
	m_end_tag(end_tag),
	m_hierarchy(h),
	m_settingslayer(settings_layer)
{
	if (m_hierarchy)
		m_hierarchy->onLayerCreated(m_settingslayer, this);
}


Settings::~Settings()
{
	MutexAutoLock lock(m_mutex);

	if (m_hierarchy)
		m_hierarchy->onLayerRemoved(m_settingslayer);

	clearNoLock();
}


Settings & Settings::operator=(const Settings &other)
{
	if (&other == this)
		return *this;

	// TODO: Avoid copying Settings objects. Make this private.
	FATAL_ERROR_IF(m_hierarchy || other.m_hierarchy,
		"Cannot copy or overwrite Settings object that belongs to a hierarchy");

	MutexAutoLock lock(m_mutex);
	MutexAutoLock lock2(other.m_mutex);

	clearNoLock();
	m_settings = other.m_settings;
	m_callbacks = other.m_callbacks;

	return *this;
}


bool Settings::checkNameValid(std::string_view name)
{
	bool valid = name.find_first_of("=\"{}#") == std::string::npos;
	if (valid)
		valid = std::find_if(name.begin(), name.end(), ::isspace) == name.end();

	if (!valid) {
		errorstream << "Invalid setting name \"" << name << "\""
			<< std::endl;
		return false;
	}
	return true;
}


bool Settings::checkValueValid(std::string_view value)
{
	if (value.substr(0, 3) == "\"\"\"" ||
		value.find("\n\"\"\"") != std::string::npos) {
		errorstream << "Invalid character sequence '\"\"\"' found in"
			" setting value!" << std::endl;
		return false;
	}
	return true;
}

std::string Settings::getMultiline(std::istream &is, size_t *num_lines)
{
	size_t lines = 1;
	std::string value;
	std::string line;

	while (is.good()) {
		lines++;
		std::getline(is, line);
		if (line == "\"\"\"")
			break;
		value += line;
		value.push_back('\n');
	}

	size_t len = value.size();
	if (len)
		value.erase(len - 1);

	if (num_lines)
		*num_lines = lines;

	return value;
}


bool Settings::readConfigFile(const char *filename)
{
	std::ifstream is(filename);
	if (!is.good())
		return false;

	return parseConfigLines(is);
}


bool Settings::parseConfigLines(std::istream &is)
{
	MutexAutoLock lock(m_mutex);

	std::string line, name, value;

	while (is.good()) {
		std::getline(is, line);
		SettingsParseEvent event = parseConfigObject(line, name, value);

		switch (event) {
		case SPE_NONE:
		case SPE_INVALID:
		case SPE_COMMENT:
			break;
		case SPE_KVPAIR:
			m_settings[name] = SettingsEntry(value);
			break;
		case SPE_END:
			return true;
		case SPE_GROUP: {
			Settings *group = new Settings("}");
			if (!group->parseConfigLines(is)) {
				delete group;
				return false;
			}
			m_settings[name] = SettingsEntry(group);
			break;
		}
		case SPE_MULTILINE:
			m_settings[name] = SettingsEntry(getMultiline(is));
			break;
		}
	}

	// false (failure) if end tag not found
	return m_end_tag.empty();
}


void Settings::writeLines(std::ostream &os, u32 tab_depth) const
{
	MutexAutoLock lock(m_mutex);

	for (const auto &setting_it : m_settings)
		printEntry(os, setting_it.first, setting_it.second, tab_depth);

	// For groups this must be "}" !
	if (!m_end_tag.empty()) {
		for (u32 i = 0; i < tab_depth; i++)
			os << "\t";

		os << m_end_tag << "\n";
	}
}


void Settings::printEntry(std::ostream &os, const std::string &name,
	const SettingsEntry &entry, u32 tab_depth)
{
	for (u32 i = 0; i != tab_depth; i++)
		os << "\t";

	if (entry.is_group) {
		os << name << " = {\n";

		entry.group->writeLines(os, tab_depth + 1);

		// Closing bracket handled by writeLines
	} else {
		os << name << " = ";

		if (entry.value.find('\n') != std::string::npos)
			os << "\"\"\"\n" << entry.value << "\n\"\"\"\n";
		else
			os << entry.value << "\n";
	}
}


bool Settings::updateConfigObject(std::istream &is, std::ostream &os, u32 tab_depth)
{
	SettingEntries::const_iterator it;
	std::set<std::string> present_entries;
	std::string line, name, value;
	bool was_modified = false;
	bool end_found = false;

	// Add any settings that exist in the config file with the current value
	// in the object if existing
	while (is.good() && !end_found) {
		std::getline(is, line);
		SettingsParseEvent event = parseConfigObject(line, name, value);

		switch (event) {
		case SPE_END:
			// Skip end tag. Append later.
			end_found = true;
			break;
		case SPE_MULTILINE:
			value = getMultiline(is);
			/* FALLTHROUGH */
		case SPE_KVPAIR:
			it = m_settings.find(name);
			if (it != m_settings.end() &&
					(it->second.is_group || it->second.value != value)) {
				printEntry(os, name, it->second, tab_depth);
				was_modified = true;
			} else if (it == m_settings.end()) {
				// Remove by skipping
				was_modified = true;
				break;
			} else {
				os << line << "\n";
				if (event == SPE_MULTILINE)
					os << value << "\n\"\"\"\n";
			}
			present_entries.insert(name);
			break;
		case SPE_GROUP:
			it = m_settings.find(name);
			if (it != m_settings.end() && it->second.is_group) {
				os << line << "\n";
				sanity_check(it->second.group != NULL);
				was_modified |= it->second.group->updateConfigObject(is, os, tab_depth + 1);
			} else if (it == m_settings.end()) {
				// Remove by skipping
				was_modified = true;
				Settings removed_group("}"); // Move 'is' to group end
				std::stringstream ss;
				removed_group.updateConfigObject(is, ss, tab_depth + 1);
				break;
			} else {
				printEntry(os, name, it->second, tab_depth);
				was_modified = true;
			}
			present_entries.insert(name);
			break;
		default:
			os << line << (is.eof() ? "" : "\n");
			break;
		}
	}

	if (!line.empty() && is.eof())
		os << "\n";

	// Add any settings in the object that don't exist in the config file yet
	for (it = m_settings.begin(); it != m_settings.end(); ++it) {
		if (present_entries.find(it->first) != present_entries.end())
			continue;

		printEntry(os, it->first, it->second, tab_depth);
		was_modified = true;
	}

	// Append ending tag
	if (!m_end_tag.empty()) {
		os << m_end_tag << "\n";
		was_modified |= !end_found;
	}

	return was_modified;
}


bool Settings::updateConfigFile(const char *filename)
{
	MutexAutoLock lock(m_mutex);

	std::ifstream is(filename);
	std::ostringstream os(std::ios_base::binary);

	bool was_modified = updateConfigObject(is, os);
	is.close();

	if (!was_modified)
		return true;

	if (!fs::safeWriteToFile(filename, os.str()))
		return false;

	return true;
}


bool Settings::parseCommandLine(int argc, char *argv[],
		const std::map<std::string, ValueSpec> &allowed_options)
{
	int nonopt_index = 0;
	for (int i = 1; i < argc; i++) {
		std::string_view arg_name(argv[i]);
		if (arg_name.substr(0, 2) != "--") {
			// If option doesn't start with -, read it in as nonoptX
			if (arg_name[0] != '-') {
				std::string name = "nonopt";
				name += itos(nonopt_index);
				set(name, std::string(arg_name));
				nonopt_index++;
				continue;
			}
			errorstream << "Invalid command-line parameter \""
					<< arg_name << "\": --<option> expected." << std::endl;
			return false;
		}

		std::string name(arg_name.substr(2));

		auto n = allowed_options.find(name);
		if (n == allowed_options.end()) {
			errorstream << "Unknown command-line parameter \""
					<< arg_name << "\"" << std::endl;
			return false;
		}

		ValueType type = n->second.type;

		std::string value;

		if (type == VALUETYPE_FLAG) {
			value = "true";
		} else {
			if ((i + 1) >= argc) {
				errorstream << "Invalid command-line parameter \""
						<< name << "\": missing value" << std::endl;
				return false;
			}
			value = argv[++i];
		}

		set(name, value);
	}

	return true;
}



/***********
 * Getters *
 ***********/

Settings *Settings::getParent() const
{
	return m_hierarchy ? m_hierarchy->getParent(m_settingslayer) : nullptr;
}


const SettingsEntry &Settings::getEntry(const std::string &name) const
{
	{
		MutexAutoLock lock(m_mutex);

		SettingEntries::const_iterator n;
		if ((n = m_settings.find(name)) != m_settings.end())
			return n->second;
	}

	if (auto parent = getParent())
		return parent->getEntry(name);

	throw SettingNotFoundException("Setting [" + name + "] not found.");
}


Settings *Settings::getGroup(const std::string &name) const
{
	const SettingsEntry &entry = getEntry(name);
	if (!entry.is_group)
		throw SettingNotFoundException("Setting [" + name + "] is not a group.");
	return entry.group;
}


const std::string &Settings::get(const std::string &name) const
{
	const SettingsEntry &entry = getEntry(name);
	if (entry.is_group)
		throw SettingNotFoundException("Setting [" + name + "] is a group.");
	return entry.value;
}


bool Settings::getBool(const std::string &name) const
{
	return is_yes(get(name));
}


u16 Settings::getU16(const std::string &name) const
{
	return stoi(get(name), 0, 65535);
}


s16 Settings::getS16(const std::string &name) const
{
	return stoi(get(name), -32768, 32767);
}


u32 Settings::getU32(const std::string &name) const
{
	return (u32) stoi(get(name));
}

s32 Settings::getS32(const std::string &name) const
{
	return stoi(get(name));
}


float Settings::getFloat(const std::string &name) const
{
	return stof(get(name));
}


float Settings::getFloat(const std::string &name, float min, float max) const
{
	float val = stof(get(name));
	return rangelim(val, min, max);
}


u64 Settings::getU64(const std::string &name) const
{
	std::string s = get(name);
	return from_string<u64>(s);
}


v2f Settings::getV2F(const std::string &name) const
{
	v2f value;
	Strfnd f(get(name));
	f.next("(");
	value.X = stof(f.next(","));
	value.Y = stof(f.next(")"));
	return value;
}


std::optional<v3f> Settings::getV3F(const std::string &name) const
{
	return str_to_v3f(get(name));
}


u32 Settings::getFlagStr(const std::string &name, const FlagDesc *flagdesc,
	u32 *flagmask) const
{
	u32 flags = 0;

	// Read default value (if there is any)
	if (auto parent = getParent())
		flags = parent->getFlagStr(name, flagdesc, flagmask);

	// Apply custom flags "on top"
	if (m_settings.find(name) != m_settings.end()) {
		std::string value = get(name);
		u32 flags_user;
		u32 mask_user = U32_MAX;
		flags_user = std::isdigit(value[0])
			? stoi(value) // Override default
			: readFlagString(value, flagdesc, &mask_user);

		flags &= ~mask_user;
		flags |=  flags_user;
		if (flagmask)
			*flagmask |= mask_user;
	}

	return flags;
}


bool Settings::getNoiseParams(const std::string &name, NoiseParams &np) const
{
	if (getNoiseParamsFromGroup(name, np) || getNoiseParamsFromValue(name, np))
		return true;
	if (auto parent = getParent())
		return parent->getNoiseParams(name, np);

	return false;
}


bool Settings::getNoiseParamsFromValue(const std::string &name,
	NoiseParams &np) const
{
	std::string value;

	if (!getNoEx(name, value))
		return false;

	// Format: f32,f32,(f32,f32,f32),s32,s32,f32[,f32]
	Strfnd f(value);

	np.offset   = stof(f.next(","));
	np.scale    = stof(f.next(","));
	f.next("(");
	np.spread.X = stof(f.next(","));
	np.spread.Y = stof(f.next(","));
	np.spread.Z = stof(f.next(")"));
	f.next(",");
	np.seed     = stoi(f.next(","));
	np.octaves  = stoi(f.next(","));
	np.persist  = stof(f.next(","));

	std::string optional_params = f.next("");
	if (!optional_params.empty())
		np.lacunarity = stof(optional_params);

	return true;
}


bool Settings::getNoiseParamsFromGroup(const std::string &name,
	NoiseParams &np) const
{
	Settings *group = NULL;

	if (!getGroupNoEx(name, group))
		return false;

	group->getFloatNoEx("offset",      np.offset);
	group->getFloatNoEx("scale",       np.scale);

	std::optional<v3f> spread;
	if (group->getV3FNoEx("spread", spread) && spread.has_value())
		np.spread = *spread;

	group->getS32NoEx("seed",          np.seed);
	group->getU16NoEx("octaves",       np.octaves);
	group->getFloatNoEx("persistence", np.persist);
	group->getFloatNoEx("lacunarity",  np.lacunarity);

	np.flags = 0;
	if (!group->getFlagStrNoEx("flags", np.flags, flagdesc_noiseparams))
		np.flags = NOISE_FLAG_DEFAULTS;

	return true;
}


bool Settings::exists(const std::string &name) const
{
	if (existsLocal(name))
		return true;
	if (auto parent = getParent())
		return parent->exists(name);
	return false;
}


bool Settings::existsLocal(const std::string &name) const
{
	MutexAutoLock lock(m_mutex);

	return m_settings.find(name) != m_settings.end();
}


std::vector<std::string> Settings::getNames() const
{
	MutexAutoLock lock(m_mutex);

	std::vector<std::string> names;
	names.reserve(m_settings.size());
	for (const auto &settings_it : m_settings) {
		names.push_back(settings_it.first);
	}
	return names;
}



/***************************************
 * Getters that don't throw exceptions *
 ***************************************/

bool Settings::getGroupNoEx(const std::string &name, Settings *&val) const
{
	try {
		val = getGroup(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getNoEx(const std::string &name, std::string &val) const
{
	try {
		val = get(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getFlag(const std::string &name) const
{
	try {
		return getBool(name);
	} catch(SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getFloatNoEx(const std::string &name, float &val) const
{
	try {
		val = getFloat(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getU16NoEx(const std::string &name, u16 &val) const
{
	try {
		val = getU16(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getS16NoEx(const std::string &name, s16 &val) const
{
	try {
		val = getS16(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}

bool Settings::getU32NoEx(const std::string &name, u32 &val) const
{
	try {
		val = getU32(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}

bool Settings::getS32NoEx(const std::string &name, s32 &val) const
{
	try {
		val = getS32(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getU64NoEx(const std::string &name, u64 &val) const
{
	try {
		val = getU64(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getV2FNoEx(const std::string &name, v2f &val) const
{
	try {
		val = getV2F(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getV3FNoEx(const std::string &name, std::optional<v3f> &val) const
{
	try {
		val = getV3F(name);
		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


bool Settings::getFlagStrNoEx(const std::string &name, u32 &val,
	const FlagDesc *flagdesc) const
{
	if (!flagdesc) {
		if (!(flagdesc = getFlagDescFallback(name)))
			return false; // Not found
	}

	try {
		val = getFlagStr(name, flagdesc, nullptr);

		return true;
	} catch (SettingNotFoundException &e) {
		return false;
	}
}


/***********
 * Setters *
 ***********/

bool Settings::setEntry(const std::string &name, const void *data,
	bool set_group)
{
	if (!checkNameValid(name))
		return false;
	if (!set_group && !checkValueValid(*(const std::string *)data))
		return false;

	Settings *old_group = NULL;
	{
		MutexAutoLock lock(m_mutex);

		SettingsEntry &entry = m_settings[name];
		old_group = entry.group;

		entry.value    = set_group ? "" : *(const std::string *)data;
		entry.group    = set_group ? *(Settings **)data : NULL;
		entry.is_group = set_group;
		if (set_group)
			entry.group->m_end_tag = "}";
	}

	delete old_group;

	return true;
}


bool Settings::set(const std::string &name, const std::string &value)
{
	if (!setEntry(name, &value, false))
		return false;

	doCallbacks(name);
	return true;
}


// TODO: Remove this function
bool Settings::setDefault(const std::string &name, const std::string &value)
{
	FATAL_ERROR_IF(m_hierarchy != &g_hierarchy, "setDefault is only valid on "
		"global settings");
	return getLayer(SL_DEFAULTS)->set(name, value);
}


bool Settings::setGroup(const std::string &name, const Settings &group)
{
	// Settings must own the group pointer
	// avoid double-free by copying the source
	Settings *copy = new Settings();
	*copy = group;
	return setEntry(name, &copy, true);
}


bool Settings::setBool(const std::string &name, bool value)
{
	return set(name, value ? "true" : "false");
}


bool Settings::setS16(const std::string &name, s16 value)
{
	return set(name, itos(value));
}


bool Settings::setU16(const std::string &name, u16 value)
{
	return set(name, itos(value));
}


bool Settings::setS32(const std::string &name, s32 value)
{
	return set(name, itos(value));
}


bool Settings::setU64(const std::string &name, u64 value)
{
	std::ostringstream os;
	os << value;
	return set(name, os.str());
}


bool Settings::setFloat(const std::string &name, float value)
{
	return set(name, ftos(value));
}


bool Settings::setV2F(const std::string &name, v2f value)
{
	std::ostringstream os;
	os << "(" << value.X << "," << value.Y << ")";
	return set(name, os.str());
}


bool Settings::setV3F(const std::string &name, v3f value)
{
	std::ostringstream os;
	os << "(" << value.X << "," << value.Y << "," << value.Z << ")";
	return set(name, os.str());
}


bool Settings::setFlagStr(const std::string &name, u32 flags,
	const FlagDesc *flagdesc, u32 flagmask)
{
	if (!flagdesc) {
		if (!(flagdesc = getFlagDescFallback(name)))
			return false; // Not found
	}

	return set(name, writeFlagString(flags, flagdesc, flagmask));
}


bool Settings::setNoiseParams(const std::string &name, const NoiseParams &np)
{
	Settings *group = new Settings;

	group->setFloat("offset",      np.offset);
	group->setFloat("scale",       np.scale);
	group->setV3F("spread",        np.spread);
	group->setS32("seed",          np.seed);
	group->setU16("octaves",       np.octaves);
	group->setFloat("persistence", np.persist);
	group->setFloat("lacunarity",  np.lacunarity);
	group->setFlagStr("flags",     np.flags, flagdesc_noiseparams, np.flags);

	return setEntry(name, &group, true);
}


bool Settings::remove(const std::string &name)
{
	// Lock as short as possible, unlock before doCallbacks()
	m_mutex.lock();

	SettingEntries::iterator it = m_settings.find(name);
	if (it != m_settings.end()) {
		delete it->second.group;
		m_settings.erase(it);
		m_mutex.unlock();

		doCallbacks(name);
		return true;
	}

	m_mutex.unlock();
	return false;
}


SettingsParseEvent Settings::parseConfigObject(const std::string &line,
	std::string &name, std::string &value)
{
	auto trimmed_line = trim(line);

	if (trimmed_line.empty())
		return SPE_NONE;
	if (trimmed_line[0] == '#')
		return SPE_COMMENT;
	if (trimmed_line == m_end_tag)
		return SPE_END;

	size_t pos = trimmed_line.find('=');
	if (pos == std::string::npos)
		return SPE_INVALID;

	name  = trim(trimmed_line.substr(0, pos));
	value = trim(trimmed_line.substr(pos + 1));

	if (value == "{")
		return SPE_GROUP;
	if (value == "\"\"\"")
		return SPE_MULTILINE;

	return SPE_KVPAIR;
}


void Settings::clearNoLock()
{
	for (SettingEntries::const_iterator it = m_settings.begin();
			it != m_settings.end(); ++it)
		delete it->second.group;
	m_settings.clear();
}


void Settings::setDefault(const std::string &name, const FlagDesc *flagdesc,
	u32 flags)
{
	s_flags[name] = flagdesc;
	setDefault(name, writeFlagString(flags, flagdesc, U32_MAX));
}

const FlagDesc *Settings::getFlagDescFallback(const std::string &name) const
{
	auto it = s_flags.find(name);
	return it == s_flags.end() ? nullptr : it->second;
}

void Settings::registerChangedCallback(const std::string &name,
	SettingsChangedCallback cbf, void *userdata)
{
	MutexAutoLock lock(m_callback_mutex);
	m_callbacks[name].emplace_back(cbf, userdata);
}

size_t Settings::deregisterAllChangedCallbacks(const void *userdata)
{
	MutexAutoLock lock(m_callback_mutex);

	size_t n_removed = 0;
	for (auto &settings : m_callbacks) {
		for (auto cb = settings.second.begin(); cb != settings.second.end();) {
			if (cb->second == userdata) {
				cb = settings.second.erase(cb);
				n_removed++;
			} else {
				++cb;
			}
		}
	}
	return n_removed;
}

void Settings::removeSecureSettings()
{
	for (const auto &name : getNames()) {
		if (name.compare(0, 7, "secure.") != 0)
			continue;

		errorstream << "Secure setting " << name
				<< " isn't allowed, so was ignored."
				<< std::endl;
		remove(name);
	}
}

void Settings::doCallbacks(const std::string &name) const
{
	MutexAutoLock lock(m_callback_mutex);

	SettingsCallbackMap::const_iterator it_cbks = m_callbacks.find(name);
	if (it_cbks != m_callbacks.end()) {
		SettingsCallbackList::const_iterator it;
		for (it = it_cbks->second.begin(); it != it_cbks->second.end(); ++it)
			(it->first)(name, it->second);
	}
}
