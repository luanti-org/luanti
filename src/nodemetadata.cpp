// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "nodemetadata.h"
#include "exceptions.h"
#include "gamedef.h"
#include "inventory.h"
#include "irrlicht_changes/printing.h"
#include "log.h"
#include "debug.h"
#include "util/serialize.h"
#include "constants.h" // MAP_BLOCKSIZE
#include <sstream>

/*
	NodeMetadata
*/

NodeMetadata::NodeMetadata(IItemDefManager *item_def_mgr):
	m_inventory(std::make_unique<Inventory>(item_def_mgr))
{}

NodeMetadata::~NodeMetadata() = default;

void NodeMetadata::serialize(std::ostream &os, u8 version, bool disk) const
{
	int num_vars = disk ? m_stringvars.size() : countNonPrivate();
	writeU32(os, num_vars);
	for (const auto &sv : m_stringvars) {
		bool priv = isPrivate(sv.first);
		if (!disk && priv)
			continue;

		os << serializeString16(sv.first);
		os << serializeString32(sv.second);
		if (version >= 2)
			writeU8(os, (priv) ? 1 : 0);
	}

	m_inventory->serialize(os);
}

void NodeMetadata::deSerialize(std::istream &is, u8 version)
{
	clear();
	u32 num_vars = readU32(is);
	for (u32 i = 0; i < num_vars; i++){
		std::string name = deSerializeString16(is);
		std::string var = deSerializeString32(is);
		m_stringvars[name] = std::move(var);
		if (version >= 2) {
			if (readU8(is) == 1)
				m_privatevars.insert(name);
		}
	}

	m_inventory->deSerialize(is);
}

void NodeMetadata::clear()
{
	SimpleMetadata::clear();
	m_privatevars.clear();
	m_inventory->clear();
}

bool NodeMetadata::empty() const
{
	return SimpleMetadata::empty() && m_inventory->getLists().empty();
}


bool NodeMetadata::markPrivate(const std::string &name, bool set)
{
	if (set)
		return m_privatevars.insert(name).second;
	else
		return m_privatevars.erase(name) > 0;
}

int NodeMetadata::countNonPrivate() const
{
	// m_privatevars can contain names not actually present
	// DON'T: return m_stringvars.size() - m_privatevars.size();
	int n = 0;
	for (const auto &sv : m_stringvars) {
		if (!isPrivate(sv.first))
			n++;
	}
	return n;
}

/*
	NodeMetadataList
*/

void NodeMetadataList::serialize(std::ostream &os, u8 blockver, bool disk,
	bool absolute_pos, bool include_empty) const
{
	/*
		Version 0 is a placeholder for "nothing to see here; go away."
	*/

	u16 count = include_empty ? m_data.size() : countNonEmpty();
	if (count == 0) {
		writeU8(os, 0); // version
		return;
	}

	u8 version = (blockver > 27) ? 2 : 1;
	writeU8(os, version);
	writeU16(os, count);

	for (NodeMetadataMap::const_iterator
			i = m_data.begin();
			i != m_data.end(); ++i) {
		v3s16 p = i->first;
		NodeMetadata *data = i->second;
		if (!include_empty && data->empty())
			continue;

		if (absolute_pos) {
			writeS16(os, p.X);
			writeS16(os, p.Y);
			writeS16(os, p.Z);
		} else {
			// Serialize positions within a mapblock
			static_assert(MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE <= U16_MAX,
				"position too big to serialize");
			u16 p16 = (p.Z * MAP_BLOCKSIZE + p.Y) * MAP_BLOCKSIZE + p.X;
			writeU16(os, p16);
		}
		data->serialize(os, version, disk);
	}
}

void NodeMetadataList::deSerialize(std::istream &is,
	IItemDefManager *item_def_mgr, bool absolute_pos)
{
	clear();

	u8 version = readU8(is);

	if (version == 0) {
		// Nothing
		return;
	}

	if (version > 2) {
		std::string err_str = std::string(FUNCTION_NAME)
			+ ": version " + itos(version) + " not supported";
		infostream << err_str << std::endl;
		throw SerializationError(err_str);
	}

	u16 count = readU16(is);

	for (u16 i = 0; i < count; i++) {
		v3s16 p;
		if (absolute_pos) {
			p.X = readS16(is);
			p.Y = readS16(is);
			p.Z = readS16(is);
		} else {
			u16 p16 = readU16(is);
			p.X = p16 & (MAP_BLOCKSIZE - 1);
			p16 /= MAP_BLOCKSIZE;
			p.Y = p16 & (MAP_BLOCKSIZE - 1);
			p16 /= MAP_BLOCKSIZE;
			p.Z = p16;
		}
		if (m_data.find(p) != m_data.end()) {
			warningstream << "NodeMetadataList::deSerialize(): "
					<< "already set data at position " << p
					<< ": Ignoring." << std::endl;
			continue;
		}

		NodeMetadata *data = new NodeMetadata(item_def_mgr);
		data->deSerialize(is, version);
		m_data[p] = data;
	}
}

NodeMetadataList::~NodeMetadataList()
{
	clear();
}

std::vector<v3s16> NodeMetadataList::getAllKeys()
{
	std::vector<v3s16> keys;
	keys.reserve(m_data.size());
	for (const auto &it : m_data)
		keys.push_back(it.first);

	return keys;
}

NodeMetadata *NodeMetadataList::get(v3s16 p)
{
	NodeMetadataMap::const_iterator n = m_data.find(p);
	if (n == m_data.end())
		return nullptr;
	return n->second;
}

void NodeMetadataList::remove(v3s16 p)
{
	NodeMetadata *olddata = get(p);
	if (olddata) {
		if (m_is_metadata_owner) {
			// clearing can throw an exception due to the invlist resize lock,
			// which we don't want to happen in the noexcept destructor
			// => call clear before
			olddata->clear();
			delete olddata;
		}
		m_data.erase(p);
	}
}

void NodeMetadataList::set(v3s16 p, NodeMetadata *d)
{
	remove(p);
	m_data.emplace(p, d);
}

void NodeMetadataList::clear()
{
	if (m_is_metadata_owner) {
		for (auto it = m_data.begin(); it != m_data.end(); ++it)
			delete it->second;
	}
	m_data.clear();
}

int NodeMetadataList::countNonEmpty() const
{
	int n = 0;
	for (const auto &it : m_data) {
		if (!it.second->empty())
			n++;
	}
	return n;
}
