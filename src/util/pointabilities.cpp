// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 cx384

#include "pointabilities.h"

#include "serialize.h"
#include "exceptions.h"
#include <sstream>

PointabilityType Pointabilities::deSerializePointabilityType(std::istream &is)
{
	PointabilityType pointable_type = static_cast<PointabilityType>(readU8(is));
	switch(pointable_type) {
		case PointabilityType::POINTABLE:
		case PointabilityType::POINTABLE_NOT:
		case PointabilityType::POINTABLE_BLOCKING:
			break;
		default:
			// Default to POINTABLE in case of unknown PointabilityType type.
			pointable_type = PointabilityType::POINTABLE;
			break;
	}
	return pointable_type;
}

void Pointabilities::serializePointabilityType(std::ostream &os, PointabilityType pointable_type)
{
	writeU8(os, static_cast<u8>(pointable_type));
}

std::string Pointabilities::toStringPointabilityType(PointabilityType pointable_type)
{
	switch(pointable_type) {
		case PointabilityType::POINTABLE:
			return "true";
		case PointabilityType::POINTABLE_NOT:
			return "false";
		case PointabilityType::POINTABLE_BLOCKING:
			return "\"blocking\"";
	}
	return "unknown";
}

std::optional<PointabilityType> Pointabilities::matchNode(const std::string &name,
	const ItemGroupList &groups) const
{
	auto i = nodes.find(name);
	return i == nodes.end() ? matchGroups(groups, node_groups) : i->second;
}

std::optional<PointabilityType> Pointabilities::matchObject(const std::string &name,
	const ItemGroupList &groups) const
{
	auto i = objects.find(name);
	return i == objects.end() ? matchGroups(groups, object_groups) : i->second;
}

std::optional<PointabilityType> Pointabilities::matchPlayer(const ItemGroupList &groups) const
{
	return matchGroups(groups, object_groups);
}

std::optional<PointabilityType> Pointabilities::matchGroups(const ItemGroupList &groups,
	const std::unordered_map<std::string, PointabilityType> &pointable_groups)
{
	// prefers POINTABLE over POINTABLE_NOT over POINTABLE_BLOCKING
	bool blocking = false;
	bool not_pointable = false;
	for (auto const &ability : pointable_groups) {
		if (itemgroup_get(groups, ability.first) > 0) {
			switch(ability.second) {
				case PointabilityType::POINTABLE:
					return PointabilityType::POINTABLE;
				case PointabilityType::POINTABLE_NOT:
					not_pointable = true;
					break;
				default:
					blocking = true;
					break;
			}
		}
	}
	if (not_pointable)
		return PointabilityType::POINTABLE_NOT;
	if (blocking)
		return PointabilityType::POINTABLE_BLOCKING;
	return std::nullopt;
}

void Pointabilities::serializeTypeMap(std::ostream &os,
	const std::unordered_map<std::string, PointabilityType> &map)
{
	writeU32(os, map.size());
	for (const auto &entry : map) {
		os << serializeString16(entry.first);
		writeU8(os, (u8)entry.second);
	}
}

void Pointabilities::deSerializeTypeMap(std::istream &is,
	std::unordered_map<std::string, PointabilityType> &map)
{
	map.clear();
	u32 size = readU32(is);
	for (u32 i = 0; i < size; i++) {
		std::string name = deSerializeString16(is);
		PointabilityType type = Pointabilities::deSerializePointabilityType(is);
		map[name] = type;
	}
}

void Pointabilities::serialize(std::ostream &os) const
{
	writeU8(os, 0); // version
	serializeTypeMap(os, nodes);
	serializeTypeMap(os, node_groups);
	serializeTypeMap(os, objects);
	serializeTypeMap(os, object_groups);
}

void Pointabilities::deSerialize(std::istream &is)
{
	int version = readU8(is);
	if (version != 0)
		throw SerializationError("unsupported Pointabilities version");

	deSerializeTypeMap(is, nodes);
	deSerializeTypeMap(is, node_groups);
	deSerializeTypeMap(is, objects);
	deSerializeTypeMap(is, object_groups);
}
