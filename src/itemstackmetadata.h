// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "metadata.h"
#include "tool.h"

#include <optional>

class Inventory;
class IItemDefManager;

class ItemStackMetadata : public SimpleMetadata
{
public:
	ItemStackMetadata():
			toolcaps_overridden(false)
	{}

	// Overrides
	void clear() override;
	bool setString(const std::string &name, std::string_view var) override;

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

	const ToolCapabilities &getToolCapabilities(
			const ToolCapabilities &default_caps) const
	{
		return toolcaps_overridden ? toolcaps_override : default_caps;
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
	void updateToolCapabilities();
	void updateWearBarParams();

	bool toolcaps_overridden;
	ToolCapabilities toolcaps_override;
	std::optional<WearBarParams> wear_bar_override;
};
