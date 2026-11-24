// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#include "hotbar_source.h"

#include "serialize.h"
#include "numeric.h"

void HotbarSource::setHotbarItemcountLegacy(u16 count)
{
	sources.clear();
	sources.push_back({
		HOTBAR_INVENTORY_LIST_DEFAULT,
		rangelim(count, 0, HOTBAR_ITEMCOUNT_MAX),
		0
	});
}

u16 HotbarSource::getMaxLength() const
{
	u16 length = 0;
	for (const Source &source : sources)
		length += source.length;
	return length;
}

bool HotbarSource::getInventoryFromWieldIndex(u16 wield_index, std::pair<std::string, u16> &location) const
{
	for (auto &source : sources) {
		if (wield_index < source.length) {
			location.first = source.list;
			location.second = wield_index + source.offset;
			return true;
		}
		wield_index -= source.length;
	}
	return false;
}

void HotbarSource::serialize(std::ostream &os) const
{
	writeU16(os, sources.size());
	for (const auto &source : sources) {
		os << serializeString16(source.list);
		writeU16(os, source.length);
		writeU16(os, source.offset);
	}
}

void HotbarSource::deSerialize(std::istream &is)
{
	sources.clear();
	u16 size = readU16(is);
	for (u16 i = 0; i < size; i++) {
		Source source;
		source.list = deSerializeString16(is);
		source.length = readU16(is);
		source.offset = readU16(is);
		sources.push_back(std::move(source));
	}
}
