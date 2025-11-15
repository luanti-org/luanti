// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

// Stores a list of player inventories to be used by a hotbar
// Independent of the HUD element

#pragma once

#include <vector>
#include <string>
#include <string_view>
#include "irrlichttypes.h"

#define HOTBAR_ITEMCOUNT_DEFAULT 8
#define HOTBAR_INVENTORY_LIST_DEFAULT "main"
#define HOTBAR_ITEMCOUNT_MAX 32

struct HotbarSource {
	struct Source {
		std::string list;
		u16 length;
		u16 offset;
	};

	// Old functionality which could only use the "main" list
	void setHotbarItemcountLegacy(u16 count);

	// Returns the total length of all sources
	u16 getMaxLength() const { return getLengthBefore(sources.size()); }

	// Returns list and index of the inventory if it exists
	bool getInventoryFromWieldIndex(u16 wield_index, std::string &list, u16 &index) const;

	// Returns number of inventory slots before the source at index
	u16 getLengthBefore(std::size_t index) const;

	const std::vector<Source> getSources() const { return sources; };

	void addSource(std::string_view list, u16 length, u16 offset) {
		sources.push_back(Source{std::string{list}, length, offset});
	};

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

private:
	std::vector<Source> sources;
};
