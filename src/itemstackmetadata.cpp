// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>


#include "itemstackmetadata.h"
#include "node_modifier.h"
#include "constants.h"
#include "util/serialize.h"
#include "util/string.h"

#include <algorithm>
#include <optional>
#include <unordered_set>

#define DESERIALIZE_START '\x01'
#define DESERIALIZE_KV_DELIM '\x02'
#define DESERIALIZE_PAIR_DELIM '\x03'

#define TOOLCAP_KEY "tool_capabilities"
#define WEAR_BAR_KEY "wear_color"

bool ItemStackMetadata::empty() const
{
	return SimpleMetadata::empty() && m_modifiers.empty();
}

bool ItemStackMetadata::operator==(const ItemStackMetadata &other) const
{
	return IMetadata::operator==(other) && m_modifiers == other.m_modifiers;
}

bool ItemStackMetadata::addModifier(const std::string &name,
		const NodeModifierManager *prune_manager)
{
	if (!isValidModScopedName(name))
		return false;

	if (prune_manager) {
		std::vector<std::string> survivors;
		survivors.reserve(m_modifiers.size() + 1);
		survivors.push_back(name);

		const NodeModifier &added = prune_manager->get(prune_manager->getId(name));
		u64 covered = added.m_is_dummy ? 0 : added.m_modified_props_mask;
		for (auto it = m_modifiers.rbegin(); it != m_modifiers.rend(); ++it) {
			if (*it == name)
				continue;

			const NodeModifier &modifier = prune_manager->get(prune_manager->getId(*it));
			const u64 mask = modifier.m_is_dummy ? 0 : modifier.m_modified_props_mask;
			if (mask != 0 && (covered | mask) == covered)
				continue;

			covered |= mask;
			survivors.push_back(*it);
		}

		if (survivors.size() > MAX_NODE_MODIFIERS)
			return false;

		std::reverse(survivors.begin(), survivors.end());
		if (survivors != m_modifiers) {
			m_modifiers = std::move(survivors);
			setModified(true);
		}

		return true;
	}

	auto it = std::find(m_modifiers.begin(), m_modifiers.end(), name);
	if (it != m_modifiers.end()) {
		if (it + 1 != m_modifiers.end()) {
			std::rotate(it, it + 1, m_modifiers.end());
			setModified(true);
		}

		return true;
	}

	if (m_modifiers.size() >= MAX_NODE_MODIFIERS)
		return false;

	m_modifiers.push_back(name);

	setModified(true);
	return true;
}

void ItemStackMetadata::removeModifier(const std::string &name)
{
	auto it = std::find(m_modifiers.begin(), m_modifiers.end(), name);
	if (it == m_modifiers.end())
		return;

	m_modifiers.erase(it);
	setModified(true);
}

bool ItemStackMetadata::setModifiers(std::vector<std::string> &&modifiers) {
	for (auto &it : modifiers) {
		if (!isValidModScopedName(it))
			return false;
	}

	// Scan backwards to retain the last occurrence of each name.
	std::unordered_set<std::string> seen;
	auto unique_end = std::remove_if(modifiers.rbegin(), modifiers.rend(),
			[&seen](const std::string &name) {
				return !seen.insert(name).second;
			});
	modifiers.erase(modifiers.begin(), unique_end.base());
	if (modifiers.size() > MAX_NODE_MODIFIERS)
		return false;

	m_modifiers = std::move(modifiers);

	setModified(true);
	return true;
}

void ItemStackMetadata::clear()
{
	SimpleMetadata::clear();
	m_modifiers.clear();
	updateToolCapabilities();
	updateWearBarParams();
}

static void sanitize_string(std::string &str)
{
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_START), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_KV_DELIM), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_PAIR_DELIM), str.end());
}

bool ItemStackMetadata::setString(const std::string &name, std::string_view var)
{
	std::string clean_name = name;
	std::string clean_var(var);
	sanitize_string(clean_name);
	sanitize_string(clean_var);

	bool result = SimpleMetadata::setString(clean_name, clean_var);
	if (clean_name == TOOLCAP_KEY)
		updateToolCapabilities();
	else if (clean_name == WEAR_BAR_KEY)
		updateWearBarParams();
	return result;
}

void ItemStackMetadata::serialize(std::ostream &os, bool with_modifiers) const
{
	std::ostringstream os2(std::ios_base::binary);
	os2 << DESERIALIZE_START;

	for (const auto &stringvar : m_stringvars) {
		if (!stringvar.first.empty() || !stringvar.second.empty())
			os2 << stringvar.first << DESERIALIZE_KV_DELIM
				<< stringvar.second << DESERIALIZE_PAIR_DELIM;
	}

	if (with_modifiers && !m_modifiers.empty()) {
		// A second start byte separates modifiers from ordinary fields.
		// Field setters strip this byte, so legacy fields cannot collide.
		os2 << DESERIALIZE_START;
		for (const auto &name : m_modifiers)
			os2 << name << DESERIALIZE_PAIR_DELIM;
	}

	os << serializeJsonStringIfNeeded(os2.str());
}

void ItemStackMetadata::deSerialize(std::istream &is)
{
	std::string in = deSerializeJsonStringIfNeeded(is);

	m_stringvars.clear();
	m_modifiers.clear();

	if (!in.empty()) {
		if (in[0] == DESERIALIZE_START) {
			std::string_view data(in.data() + 1, in.size() - 1);

			const size_t modifiers_start = data.find(DESERIALIZE_START);
			std::string_view fields = data.substr(0, modifiers_start);

			// Preserve the legacy reader's handling of missing field delimiters.
			auto next = [](std::string_view &remaining, char delimiter) {
				const auto end = remaining.find(delimiter);
				auto value = remaining.substr(0, end);
				remaining.remove_prefix(end == std::string_view::npos ?
						remaining.size() : end + 1);
				return value;
			};

			while (!fields.empty()) {
				auto name = next(fields, DESERIALIZE_KV_DELIM);
				auto value = next(fields, DESERIALIZE_PAIR_DELIM);
				m_stringvars[std::string(name)] = std::string(value);
			}

			if (modifiers_start != std::string_view::npos) {
				auto modifiers = data.substr(modifiers_start + 1);
				std::unordered_set<std::string_view> seen;
				while (!modifiers.empty()) {
					if (modifiers.find(DESERIALIZE_PAIR_DELIM) == std::string_view::npos)
						throw SerializationError("Unterminated ItemStack modifier name");

					auto name = next(modifiers, DESERIALIZE_PAIR_DELIM);
					if (!isValidModScopedName(name) || !seen.insert(name).second ||
							m_modifiers.size() >= MAX_NODE_MODIFIERS)
						throw SerializationError("Invalid ItemStack modifier list");

					m_modifiers.emplace_back(name);
				}
			}
		} else {
			// BACKWARDS COMPATIBILITY
			m_stringvars[""] = std::move(in);
		}
	}

	updateToolCapabilities();
	updateWearBarParams();
}

void ItemStackMetadata::updateToolCapabilities()
{
	if (contains(TOOLCAP_KEY)) {
		toolcaps_override = ToolCapabilities();
		std::istringstream is(getString(TOOLCAP_KEY));
		toolcaps_override->deserializeJson(is);
	} else {
		toolcaps_override = std::nullopt;
	}
}

void ItemStackMetadata::setToolCapabilities(const ToolCapabilities &caps)
{
	std::ostringstream os;
	caps.serializeJson(os);
	setString(TOOLCAP_KEY, os.str());
}

void ItemStackMetadata::clearToolCapabilities()
{
	setString(TOOLCAP_KEY, "");
}

void ItemStackMetadata::updateWearBarParams()
{
	if (contains(WEAR_BAR_KEY)) {
		std::istringstream is(getString(WEAR_BAR_KEY));
		wear_bar_override = WearBarParams::deserializeJson(is);
	} else {
		wear_bar_override.reset();
	}
}

void ItemStackMetadata::setWearBarParams(const WearBarParams &params)
{
	std::ostringstream os;
	params.serializeJson(os);
	setString(WEAR_BAR_KEY, os.str());
}

void ItemStackMetadata::clearWearBarParams()
{
	setString(WEAR_BAR_KEY, "");
}
