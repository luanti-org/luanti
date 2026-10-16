// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "metadata.h"
#include "tool.h"

#include <optional>

inline bool nodeModifiersSupportForProtocol(u16 protocol)
{
	return protocol >= 54;
}

class ItemStackMetadata : public SimpleMetadata
{
public:
	ItemStackMetadata()
	{}

	// Overrides
	void clear() override;
	bool empty() const override;
	bool setString(const std::string &name, std::string_view var) override;

	void serialize(std::ostream &os, bool with_modifiers) const;
	void deSerialize(std::istream &is);

	// Persist names rather than runtime IDs, in application order.
	// Re-adding a name moves it to the end; unknown names are preserved.
	bool addModifier(const std::string &name);
	void removeModifier(const std::string &name);
	bool setModifiers(std::vector<std::string> &&modifiers);
	const std::vector<std::string> &getModifiersList() const { return m_modifiers; }

	const std::optional<ToolCapabilities> &getToolCapabilitiesOverride() const
	{
		return toolcaps_override;
	}

	void setToolCapabilities(const ToolCapabilities &caps);
	void clearToolCapabilities();

	const std::optional<WearBarParams> &getWearBarParamOverride() const
	{
		return wear_bar_override;
	}

	void setWearBarParams(const WearBarParams &params);
	void clearWearBarParams();

private:
	bool equalsExtra(const IMetadata &other) const override;
	void updateToolCapabilities();
	void updateWearBarParams();

	std::vector<std::string> m_modifiers;
	std::optional<ToolCapabilities> toolcaps_override;
	std::optional<WearBarParams> wear_bar_override;
};
