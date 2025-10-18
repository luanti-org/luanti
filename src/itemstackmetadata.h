// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "metadata.h"
#include "tool.h"

#include <optional>


class ItemStackMetadata : public SimpleMetadata
{
public:
	ItemStackMetadata()
	{}

	// Overrides
	void clear() override;
	bool setString(const std::string &name, std::string_view var) override;

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

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
	void updateToolCapabilities();
	void updateWearBarParams();

	std::optional<ToolCapabilities> toolcaps_override;
	std::optional<WearBarParams> wear_bar_override;
};
