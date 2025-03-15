// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#include "hotbar_source.h"
#include "serialize.h"

void HotbarSource::setHotbarItemcountLegacy(s32 count)
{
	sources.clear();
	sources.push_back({HOTBAR_INVENTORY_LIST_DEFAULT, (u16) count, 0});
}

u16 HotbarSource::getMaxLength() const
{
	u16 length = 0;
	for (auto& source : sources) {
		length += source.length;
	}
	return length ;
}

bool HotbarSource::getInventoryFromWieldIndex(u16 wield_index, std::string &list, u16 &index) const
{
	for (auto& source : sources) {
		if (wield_index < source.length) {
			list = source.list;
			index = wield_index + source.offset;
			return true;
		}
		wield_index -= source.length;
	}
	return false;
}

u16 HotbarSource::getLengthBefore(std::size_t index) const {
	u16 length_before = 0;
	for (std::size_t i = 0; i < index && i < sources.size(); i++) {
		length_before += sources[i].length;
	}
	return length_before;
}

void HotbarSource::serialize(std::ostream &os) const
{
	writeU8(os, 0); // version

	writeU16(os, sources.size());
	for (const auto &source : sources) {
		os << serializeString16(source.list);
		writeU16(os, source.length);
		writeU16(os, source.offset);
	}
}

void HotbarSource::deSerialize(std::istream &is)
{
	int version = readU8(is);
	if (version != 0)
		throw SerializationError("unsupported HotbarSource version");

	sources.clear();
	u16 size = readU16(is);
	for (u16 i = 0; i < size; i++) {
		Source source;
		source.list = deSerializeString16(is);
		source.length = readU16(is);
		source.offset = readU16(is);
		sources.push_back(source);
	}
}
