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

bool HotbarSource::getInventoryFromWieldIndex(u16 wield_index, std::string &list, u16 &index) const
{
	for (auto &source : sources) {
		if (wield_index < source.length) {
			list = source.list;
			index = wield_index + source.offset;
			return true;
		}
		wield_index -= source.length;
	}
	return false;
}

u16 HotbarSource::getLengthBefore(std::size_t index) const
{
	index = std::min(index, sources.size());
	u16 length = 0;
	while (index-- > 0)
		length += sources[index].length;
	return length;
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
