// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "mapblock.h"

#include <memory>
#include <sstream>
#include "ididmapping.h"
#include "map.h"
#include "nodedef.h"
#include "nodemetadata.h"
#include "gamedef.h"
#include "log.h"
#include "content_mapnode.h"  // For legacy name-id mapping
#include "content_nodemeta.h" // For legacy deserialization
#include "serialization.h"
#if CHECK_CLIENT_BUILD()
#include "client/mapblock_mesh.h"
#endif
#include "porting.h"
#include "settings.h"
#include "util/string.h"
#include "util/serialize.h"
#include "util/basic_macros.h"

static const char *modified_reason_strings[] = {
	"reallocate or initial",
	"setIsUnderground",
	"setLightingComplete",
	"setGenerated",
	"setNode",
	"setTimestamp",
	"reportMetadataChange",
	"clearAllObjects",
	"Timestamp expired (step)",
	"addActiveObjectRaw",
	"removeRemovedObjects: remove",
	"removeRemovedObjects: deactivate",
	"objects cleared due to overflow",
	"deactivateFarObjects: static data moved in",
	"deactivateFarObjects: static data moved out",
	"deactivateFarObjects: static data changed",
	"finishBlockMake: expireIsAir",
	"MMVManip::blitBackAll",
	"unknown",
};

/*
	MapBlock
*/

MapBlock::MapBlock(v3s16 pos, IGameDef *gamedef):
		m_pos(pos),
		m_pos_relative(pos * MAP_BLOCKSIZE),
		m_gamedef(gamedef),
		m_is_mono_block(false)
{
	// We start with nodecount nodes, because in the vast
	// majority of the cases a block is created just before
	// it is de-serialized or generated.
	reallocate(nodecount, MapNode(CONTENT_IGNORE));
}

MapBlock::~MapBlock()
{
#if CHECK_CLIENT_BUILD()
	{
		delete mesh;
		mesh = nullptr;
	}
#endif

	delete[] data;
	if (!m_is_mono_block)
		porting::TrackFreedMemory(sizeof(MapNode) * nodecount);
}

static inline size_t get_max_objects_per_block()
{
	u16 ret = g_settings->getU16("max_objects_per_block");
	return MYMAX(256, ret);
}

bool MapBlock::onObjectsActivation()
{
	// Ignore if no stored objects (to not set changed flag)
	if (m_static_objects.getAllStored().empty())
		return false;

	const auto count = m_static_objects.getStoredSize();
	verbosestream << "MapBlock::onObjectsActivation(): "
			<< "activating " << count << " objects in block " << getPos()
			<< std::endl;

	if (count > get_max_objects_per_block()) {
		errorstream << "suspiciously large amount of objects detected: "
			<< count << " in " << getPos() << "; removing all of them."
			<< std::endl;
		// Clear stored list
		m_static_objects.clearStored();
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_TOO_MANY_OBJECTS);
		return false;
	}

	return true;
}

bool MapBlock::saveStaticObject(u16 id, const StaticObject &obj, u32 reason)
{
	if (m_static_objects.getStoredSize() >= get_max_objects_per_block()) {
		warningstream << "MapBlock::saveStaticObject(): Trying to store id = " << id
				<< " statically but block " << getPos() << " already contains "
				<< m_static_objects.getStoredSize() << " objects."
				<< std::endl;
		return false;
	}

	m_static_objects.insert(id, obj);
	if (reason != MOD_REASON_UNKNOWN) // Do not mark as modified if requested
		raiseModified(MOD_STATE_WRITE_NEEDED, reason);

	return true;
}

void MapBlock::step(float dtime, const std::function<bool(v3s16, MapNode, NodeTimer)> &on_timer_cb)
{
	// Run callbacks for elapsed node_timers
	std::vector<NodeTimer> elapsed_timers = m_node_timers.step(dtime);
	MapNode n;
	v3s16 p;
	for (const auto &it : elapsed_timers) {
		n = getNodeNoEx(it.position);
		p = it.position + getPosRelative();
		if (on_timer_cb(p, n, it)) {
			// restart
			setNodeTimer(NodeTimer(it.timeout, 0, it.position));
		}
		if (isOrphan())
			return;
	}
}

std::string MapBlock::getModifiedReasonString()
{
	std::string reason;

	const u32 ubound = MYMIN(sizeof(m_modified_reason) * CHAR_BIT,
		ARRLEN(modified_reason_strings));

	for (u32 i = 0; i != ubound; i++) {
		if ((m_modified_reason & (1 << i)) == 0)
			continue;

		reason += modified_reason_strings[i];
		reason += ", ";
	}

	if (reason.length() > 2)
		reason.resize(reason.length() - 2);

	return reason;
}


void MapBlock::copyTo(VoxelManipulator &dst)
{
	v3s16 data_size(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE);
	VoxelArea data_area(v3s16(0,0,0), data_size - v3s16(1,1,1));

	// Copy from data to VoxelManipulator
	dst.copyFrom(data, m_is_mono_block, data_area, v3s16(0,0,0),
			getPosRelative(), data_size);
}

void MapBlock::copyFrom(const VoxelManipulator &src)
{
	v3s16 data_size(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE);
	VoxelArea data_area(v3s16(0,0,0), data_size - v3s16(1,1,1));

	expandNodesIfNeeded();
	// Copy from VoxelManipulator to data
	src.copyTo(data, data_area, v3s16(0,0,0),
			getPosRelative(), data_size);
	tryShrinkNodes();
}

bool MapBlock::addNodeModifier(v3s16 pos, u16 id, bool prune_overridden)
{
	if (!isValidPosition(pos) || id == MODIFIER_IGNORE)
		return false;

	if (!m_node_modifiers.add(pos, id,
			prune_overridden ? m_gamedef->getNodeModifierManager() : nullptr))
		return false;

	raiseModified(MOD_STATE_WRITE_NEEDED);
	return true;
}

bool MapBlock::setNodeModifiers(v3s16 pos, const std::vector<u16> &ids)
{
	if (!isValidPosition(pos) || !m_node_modifiers.set(pos, ids))
		return false;

	raiseModified(MOD_STATE_WRITE_NEEDED);
	return true;
}

bool MapBlock::removeNodeModifier(v3s16 pos, u16 id)
{
	if (!isValidPosition(pos) || !m_node_modifiers.remove(pos, id))
		return false;

	raiseModified(MOD_STATE_WRITE_NEEDED);
	return true;
}

bool MapBlock::clearNodeModifiers(v3s16 pos)
{
	if (!isValidPosition(pos) || !m_node_modifiers.remove(pos))
		return false;

	raiseModified(MOD_STATE_WRITE_NEEDED);
	return true;
}

void MapBlock::copyNodeModifiersToSnapshot(AppliedNodeModifiersSnapshot &dst,
		v3s16 origin_block) const
{
	dst.append(m_node_modifiers, m_pos_relative - origin_block * MAP_BLOCKSIZE);
}

ContentLightingFlags MapBlock::getLightingFlags(v3s16 pos, const MapNode &node) const
{
	ContentLightingFlags base = m_gamedef->ndef()->getLightingFlags(node);
	if (m_node_modifiers.empty())
		return base;

	return m_node_modifiers.getViewAtPos(pos)
			.resolveLighting(*m_gamedef->getNodeModifierManager(), base);
}

void MapBlock::reallocate(u32 count, MapNode n)
{
	assert(count == 1 || count == nodecount);
	// For now monoblocks are disabled on the client.
	// The client has known data races on the block's data (FIXME).
	assert(!m_gamedef->isClient() || count == nodecount);

	delete[] data;
	if (data && !m_is_mono_block && count == 1)
		porting::TrackFreedMemory(sizeof(MapNode) * nodecount);

	data = new MapNode[count];
	std::fill_n(data, count, n);

	m_is_mono_block = (count == 1);
}

void MapBlock::tryShrinkNodes()
{
	// For now monoblocks are disabled on the client.
	// The client has known data races on the block's data (FIXME).
	if (m_gamedef->isClient())
		return;

	if (m_is_mono_block)
		return;

	MapNode n = data[0];
	bool is_mono_block = true;
	for (u32 i=1; i<nodecount; i++) {
		if (n != data[i]) {
			is_mono_block = false;
			break;
		}
	}

	if (is_mono_block) {
		reallocate(1, n);
		m_is_air = n.getContent() == CONTENT_AIR;
		m_is_air_expired = false;
	}
}

void MapBlock::expandNodesIfNeeded()
{
	if (m_is_mono_block) {
		reallocate(nodecount, data[0]);
	}
}

void MapBlock::actuallyUpdateIsAir()
{
	// Running this function un-expires m_is_air
	m_is_air_expired = false;

	if (m_is_mono_block) {
		m_is_air = data[0].getContent() == CONTENT_AIR;
		return;
	}
	bool only_air = true;
	for (u32 i = 0; i < nodecount; i++) {
		MapNode &n = data[i];
		if (n.getContent() != CONTENT_AIR) {
			only_air = false;
			break;
		}
	}

	// Set member variable
	m_is_air = only_air;
}

void MapBlock::expireIsAirCache()
{
	m_is_air_expired = true;
}

/*
	Serialization
*/

// List relevant id-name pairs for ids in the block using nodedef
// Renumbers the content IDs (starting at 0 and incrementing)
// Note that there's no technical reason why we *have to* renumber the IDs,
// but we do it anyway as it also helps compressability.
void MapBlock::getBlockNodeIdMapping(NameIdMapping *nimap, MapNode *nodes,
	u32 count, const NodeDefManager *nodedef)
{
	IdIdMapping &mapping = IdIdMapping::giveClearedThreadLocalInstance();

	content_t id_counter = 0;
	for (u32 i = 0; i < count; i++) {
		content_t global_id = nodes[i].getContent();
		content_t id = CONTENT_IGNORE;

		// Try to find an existing mapping
		if (auto found = mapping.get(global_id); found != 0xFFFF) {
			id = found;
		} else {
			// We have to assign a new mapping
			id = id_counter++;
			mapping.set(global_id, id);

			const auto &name = nodedef->get(global_id).name;
			nimap->set(id, name);
		}

		// Update the MapNode
		nodes[i].setContent(id);
	}
}

// Correct ids in the block to match nodedef based on names.
// Unknown ones are added to nodedef.
// Will not update itself to match id-name pairs in nodedef.
void MapBlock::correctBlockNodeIds(const NameIdMapping *nimap, MapNode *nodes,
		IGameDef *gamedef)
{
	const NodeDefManager *nodedef = gamedef->ndef();

	// Used to cache local to global id lookup.
	IdIdMapping &mapping_cache = IdIdMapping::giveClearedThreadLocalInstance();

	for (u32 i = 0; i < MapBlock::nodecount; i++) {
		content_t local_id = nodes[i].getContent();

		if (auto found = mapping_cache.get(local_id); found != 0xFFFF) {
			nodes[i].setContent(found);
			continue;
		}

		std::string name;
		if (!nimap->getName(local_id, name)) {
			throw SerializationError("MapBlock::correctBlockNodeIds(): "
				"Block contains id " + itos(local_id) + " with no name mapping");
		}

		content_t global_id;
		if (!nodedef->getId(name, global_id)) {
			global_id = gamedef->allocateUnknownNodeId(name);
			if (global_id == CONTENT_IGNORE) {
				throw SerializationError("MapBlock::correctBlockNodeIds(): "
					"Could not allocate global id for node name \"" + name + "\"");
			}
		}
		nodes[i].setContent(global_id);

		// Save previous node local_id & global_id result
		mapping_cache.set(local_id, global_id);
	}
}

void MapBlock::getNodeModifierIdMapping(NameIdMapping &nimap, IdIdMapping &mapping) const
{
	const auto *nmod = m_gamedef->getNodeModifierManager();
	u16 next_id = 0;

	m_node_modifiers.forEach([&](const AppliedNodeModifier &entry) {
		const u16 global_id = entry.id;

		if (global_id >= nmod->size())
			throw SerializationError("MapBlock::getNodeModifierIdMapping(): "
					"Unknown global ID " + std::to_string(global_id));

		if (mapping.get(global_id) != MODIFIER_IGNORE)
			return;

		u16 local_id = next_id++;
		nimap.set(local_id, nmod->get(global_id).m_name);
		mapping.set(global_id, local_id);
	});
}

void MapBlock::correctNodeModifierIds(const NameIdMapping &nimap)
{
	const auto *nmod = m_gamedef->getNodeModifierManager();
	auto &mapping = IdIdMapping::giveClearedThreadLocalInstance();

	m_node_modifiers.remapIds([&](u16 local_id) -> u16 {
		if (auto cached = mapping.get(local_id); cached != MODIFIER_IGNORE)
			return cached;

		std::string name;
		if (!nimap.getName(local_id, name))
			throw SerializationError("MapBlock::correctNodeModifierIds(): "
					"No name mapping for ID " + std::to_string(local_id));

		u16 global_id = nmod->getId(name);
		if (global_id == MODIFIER_IGNORE) {
			global_id = m_gamedef->allocateUnknownNodeModifierId(name);
			if (global_id == MODIFIER_IGNORE)
				throw SerializationError("MapBlock::correctNodeModifierIds(): "
						"Cannot allocate ID for modifier " + name);
		}

		mapping.set(local_id, global_id);

		return global_id;
	});
}

void MapBlock::serialize(std::ostream &os_compressed, u8 version, bool disk, int compression_level)
{
	if (!ser_ver_supported_write(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");

	std::ostringstream os_raw(std::ios_base::binary);
	std::ostream &os = version >= 29 ? os_raw : os_compressed;

	// First byte
	u8 flags = 0;
	if(is_underground)
		flags |= 0x01;
	// This flag used to be day-night-differs, and it is no longer used.
	// We write it anyway so that old servers can still use this.
	// Above ground isAir implies !day-night-differs, !isAir is good enough for old servers
	// to check whether above ground blocks should be sent.
	// See RemoteClient::getNextBlocks(...)
	if(!isAir())
		flags |= 0x02;
	if (!m_generated)
		flags |= 0x08;
	writeU8(os, flags);
	if (version >= 27) {
		writeU16(os, m_lighting_complete);
	}

	/*
		Bulk node data
	*/
	NameIdMapping nimap;
	Buffer<u8> buf;
	const u8 content_width = 2;
	const u8 params_width = 2;
	if(disk)
	{
		const size_t size = m_is_mono_block ? 1 : nodecount;
		std::unique_ptr<MapNode[]> tmp_nodes(new MapNode[size]);
		std::copy_n(data, size, tmp_nodes.get());
		getBlockNodeIdMapping(&nimap, tmp_nodes.get(), size, m_gamedef->ndef());

		buf = MapNode::serializeBulk(version, tmp_nodes.get(), nodecount,
				content_width, params_width, m_is_mono_block);

		// write timestamp and node/id mapping first
		if (version >= 29) {
			writeU32(os, getTimestamp());

			nimap.serialize(os);
		}
	}
	else
	{
		buf = MapNode::serializeBulk(version, data, nodecount,
				content_width, params_width, m_is_mono_block);
	}

	writeU8(os, content_width);
	writeU8(os, params_width);
	if (version >= 29) {
		os.write(reinterpret_cast<char*>(*buf), buf.getSize());
	} else {
		// prior to 29 node data was compressed individually
		compress(buf, os, version, compression_level);
	}

	/*
		Node metadata
	*/
	if (version >= 29) {
		m_node_metadata.serialize(os, version, disk);
	} else {
		// use os_raw from above to avoid allocating another stream object
		m_node_metadata.serialize(os_raw, version, disk);
		// prior to 29 node data was compressed individually
		compress(os_raw.str(), os, version, compression_level);
	}

	/*
		Data that goes to disk, but not the network
	*/
	if (disk) {
		if (version <= 24) {
			// Node timers
			m_node_timers.serialize(os, version);
		}

		// Static objects
		m_static_objects.serialize(os);

		if (version < 29) {
			// Timestamp
			writeU32(os, getTimestamp());

			// Write block-specific node definition id mapping
			nimap.serialize(os);
		}

		if (version >= 25) {
			// Node timers
			m_node_timers.serialize(os, version);
		}
	}

	if (version >= 30) {
		NameIdMapping modifier_names;
		if (disk) {
			auto &mapping = IdIdMapping::giveClearedThreadLocalInstance();
			getNodeModifierIdMapping(modifier_names, mapping);
			m_node_modifiers.serialize(os, version, true, modifier_names, &mapping);
		} else {
			m_node_modifiers.serialize(os, version, false, modifier_names);
		}
	}

	if (version >= 29) {
		// now compress the whole thing
		compress(os_raw.str(), os_compressed, version, compression_level);
	}
}

void MapBlock::serializeNetworkSpecific(std::ostream &os)
{
	writeU8(os, 2); // version
}

void MapBlock::deSerialize(std::istream &in_compressed, u8 version, bool disk)
{
	if (!ser_ver_supported_read(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");

	TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()<<std::endl);

	if(version < 29)
	{
		deSerialize_pre29(in_compressed, version, disk);
		return;
	}

	// Decompress the whole block (version >= 29)
	std::stringstream is(std::ios_base::binary | std::ios_base::in | std::ios_base::out);
	decompress(in_compressed, is, version);
	deSerializeUncompressed(is, version, disk);
}

void MapBlock::deSerializeUncompressed(std::istream &is, u8 version, bool disk)
{
	m_is_air_expired = true;
	expandNodesIfNeeded();
	assert(version >= 29);

	u8 flags = readU8(is);
	is_underground = (flags & 0x01) != 0;
	// IMPORTANT: when the version is bumped to 30 we can read m_is_air from here
	// m_is_air = (flags & 0x02) == 0;

	m_lighting_complete = readU16(is);
	m_generated = (flags & 0x08) == 0;

	NameIdMapping nimap;
	if (disk) {
		// Timestamp
		TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
				<<": Timestamp"<<std::endl);
		setTimestampNoChangedFlag(readU32(is));
		m_disk_timestamp = m_timestamp;

		// Node/id mapping
		TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
				<<": NameIdMapping"<<std::endl);
		nimap.deSerialize(is);
	}

	TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
			<<": Bulk node data"<<std::endl);
	u8 content_width = readU8(is);
	u8 params_width = readU8(is);
	if(content_width != 1 && content_width != 2)
		throw SerializationError("MapBlock::deSerialize(): invalid content_width");
	if(params_width != 2)
		throw SerializationError("MapBlock::deSerialize(): invalid params_width");

	/*
		Bulk node data
	*/
	MapNode::deSerializeBulk(is, version, data, nodecount,
		content_width, params_width);

	/*
		NodeMetadata
	*/
	TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
			<<": Node metadata"<<std::endl);
	m_node_metadata.deSerialize(is, m_gamedef->idef());

	/*
		Data that is only on disk
	*/
	if (disk) {
		// Static objects
		TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
				<<": Static objects"<<std::endl);
		m_static_objects.deSerialize(is);

		// Dynamically re-set ids based on node names
		correctBlockNodeIds(&nimap, data, m_gamedef);

		TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
				<<": Node timers (ver>=25)"<<std::endl);
		m_node_timers.deSerialize(is, version);

		if (nimap.size() == 1) {
			tryShrinkNodes();

			u16 dummy;
			m_is_air = nimap.getId("air", dummy);
			m_is_air_expired = false;
		}
	}

	if (version >= 30) {
		NameIdMapping modifier_names;
		m_node_modifiers.deSerialize(is, version, disk, modifier_names);
		if (disk)
			correctNodeModifierIds(modifier_names);
	}

	TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
			<<": Done."<<std::endl);
}

void MapBlock::deSerializeNetworkSpecific(std::istream &is)
{
	(void)readU8(is); // version

	//if (version < 3)
	//	return;
	// Add new code here
}

bool MapBlock::storeActiveObject(u16 id)
{
	if (m_static_objects.storeActiveObject(id)) {
		raiseModified(MOD_STATE_WRITE_NEEDED,
			MOD_REASON_REMOVE_OBJECTS_DEACTIVATE);
		return true;
	}

	return false;
}

u32 MapBlock::clearObjects()
{
	u32 size = m_static_objects.size();
	if (size > 0) {
		m_static_objects.clear();
		raiseModified(MOD_STATE_WRITE_NEEDED, MOD_REASON_CLEAR_ALL_OBJECTS);
	}
	return size;
}
/*
	Legacy serialization
*/

void MapBlock::deSerialize_pre29(std::istream &is, u8 version, bool disk)
{
	m_is_air_expired = true;
	expandNodesIfNeeded();
	assert(version < 29);

	if(version <= 21)
	{
		deSerialize_pre22(is, version, disk);
		return;
	}

	u8 flags = readU8(is);
	is_underground = (flags & 0x01) != 0;

	if (version < 27)
		m_lighting_complete = 0xFFFF;
	else
		m_lighting_complete = readU16(is);
	m_generated = (flags & 0x08) == 0;

	TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
			<<": Bulk node data"<<std::endl);
	u8 content_width = readU8(is);
	u8 params_width = readU8(is);
	if(content_width != 1 && content_width != 2)
		throw SerializationError("MapBlock::deSerialize(): invalid content_width");
	if(params_width != 2)
		throw SerializationError("MapBlock::deSerialize(): invalid params_width");

	/*
		Bulk node data
	*/
	std::stringstream in_raw(std::ios_base::binary | std::ios_base::in | std::ios_base::out);
	decompress(is, in_raw, version);
	MapNode::deSerializeBulk(in_raw, version, data, nodecount,
		content_width, params_width);

	/*
		NodeMetadata
	*/
	TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
			<<": Node metadata"<<std::endl);
	try {
		// reuse in_raw
		in_raw.str("");
		in_raw.clear();
		decompress(is, in_raw, version);
		if (version >= 23)
			m_node_metadata.deSerialize(in_raw, m_gamedef->idef());
		else
			content_nodemeta_deserialize_legacy(in_raw,
				&m_node_metadata, &m_node_timers,
				m_gamedef->idef());
	} catch(SerializationError &e) {
		warningstream<<"MapBlock::deSerialize(): Ignoring an error"
				<<" while deserializing node metadata at ("
				<<getPos()<<": "<<e.what()<<std::endl;
	}

	/*
		Data that is only on disk
	*/
	if (disk) {
		// Node timers
		if (version == 23) {
			// Read unused zero
			readU8(is);
		}
		if (version == 24) {
			TRACESTREAM(<< "MapBlock::deSerialize " << getPos()
						<< ": Node timers (ver==24)" << std::endl);
			m_node_timers.deSerialize(is, version);
		}

		// Static objects
		TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
				<<": Static objects"<<std::endl);
		m_static_objects.deSerialize(is);

		// Timestamp
		TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
				<<": Timestamp"<<std::endl);
		setTimestampNoChangedFlag(readU32(is));
		m_disk_timestamp = m_timestamp;

		NameIdMapping nimap;

		// Node/id mapping
		TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
				<<": NameIdMapping"<<std::endl);
		nimap.deSerialize(is);

		// Dynamically re-set ids based on node names
		correctBlockNodeIds(&nimap, data, m_gamedef);

		if(version >= 25){
			TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
					<<": Node timers (ver>=25)"<<std::endl);
			m_node_timers.deSerialize(is, version);
		}

		if (nimap.size() == 1) {
			tryShrinkNodes();

			u16 dummy;
			m_is_air = nimap.getId("air", dummy);
			m_is_air_expired = false;
		}
	}

	TRACESTREAM(<<"MapBlock::deSerialize "<<getPos()
			<<": Done."<<std::endl);
}

void MapBlock::deSerialize_pre22(std::istream &is, u8 version, bool disk)
{
	// Initialize default flags
	is_underground = false;
	m_is_air = false;
	m_lighting_complete = 0xFFFF;
	m_generated = true;

	// Make a temporary buffer
	u32 ser_length = MapNode::serializedLength(version);
	Buffer<u8> databuf_nodelist(nodecount * ser_length);

	// These have no compression
	if (version <= 3 || version == 5 || version == 6) {
		char tmp;
		is.read(&tmp, 1);
		if (is.gcount() != 1)
			throw SerializationError(std::string(FUNCTION_NAME)
				+ ": not enough input data");
		is_underground = tmp;
		is.read((char *)*databuf_nodelist, nodecount * ser_length);
		if ((u32)is.gcount() != nodecount * ser_length)
			throw SerializationError(std::string(FUNCTION_NAME)
				+ ": not enough input data");
	} else if (version <= 10) {
		u8 t8;
		is.read((char *)&t8, 1);
		is_underground = t8;

		{
			// Uncompress and set material data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if (s.size() != nodecount)
				throw SerializationError(std::string(FUNCTION_NAME)
					+ ": not enough input data");
			for (u32 i = 0; i < s.size(); i++) {
				databuf_nodelist[i*ser_length] = s[i];
			}
		}
		{
			// Uncompress and set param data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if (s.size() != nodecount)
				throw SerializationError(std::string(FUNCTION_NAME)
					+ ": not enough input data");
			for (u32 i = 0; i < s.size(); i++) {
				databuf_nodelist[i*ser_length + 1] = s[i];
			}
		}

		if (version >= 10) {
			// Uncompress and set param2 data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if (s.size() != nodecount)
				throw SerializationError(std::string(FUNCTION_NAME)
					+ ": not enough input data");
			for (u32 i = 0; i < s.size(); i++) {
				databuf_nodelist[i*ser_length + 2] = s[i];
			}
		}
	} else { // All other versions (10 to 21)
		u8 flags;
		is.read((char*)&flags, 1);
		is_underground = (flags & 0x01) != 0;
		if (version >= 18)
			m_generated = (flags & 0x08) == 0;

		// Uncompress data
		std::ostringstream os(std::ios_base::binary);
		decompress(is, os, version);
		std::string s = os.str();
		if (s.size() != nodecount * 3)
			throw SerializationError(std::string(FUNCTION_NAME)
				+ ": decompress resulted in size other than nodecount*3");

		// deserialize nodes from buffer
		for (u32 i = 0; i < nodecount; i++) {
			databuf_nodelist[i*ser_length] = s[i];
			databuf_nodelist[i*ser_length + 1] = s[i+nodecount];
			databuf_nodelist[i*ser_length + 2] = s[i+nodecount*2];
		}

		/*
			NodeMetadata
		*/
		if (version >= 14) {
			// Ignore errors
			try {
				if (version <= 15) {
					std::string data = deSerializeString16(is);
					std::istringstream iss(data, std::ios_base::binary);
					content_nodemeta_deserialize_legacy(iss,
						&m_node_metadata, &m_node_timers,
						m_gamedef->idef());
				} else {
					//std::string data = deSerializeString32(is);
					std::ostringstream oss(std::ios_base::binary);
					decompressZlib(is, oss);
					std::istringstream iss(oss.str(), std::ios_base::binary);
					content_nodemeta_deserialize_legacy(iss,
						&m_node_metadata, &m_node_timers,
						m_gamedef->idef());
				}
			} catch(SerializationError &e) {
				warningstream<<"MapBlock::deSerialize(): Ignoring an error"
						<<" while deserializing node metadata"<<std::endl;
			}
		}
	}

	// Deserialize node data
	for (u32 i = 0; i < nodecount; i++) {
		data[i].deSerialize(&databuf_nodelist[i * ser_length], version);
	}

	if (disk) {
		/*
			Versions up from 9 have block objects. (DEPRECATED)
		*/
		if (version >= 9) {
			u16 count = readU16(is);
			// Not supported and length not known if count is not 0
			if(count != 0){
				warningstream<<"MapBlock::deSerialize_pre22(): "
						<<"Ignoring stuff coming at and after MBOs"<<std::endl;
				return;
			}
		}

		/*
			Versions up from 15 have static objects.
		*/
		if (version >= 15)
			m_static_objects.deSerialize(is);

		// Timestamp
		if (version >= 17) {
			setTimestampNoChangedFlag(readU32(is));
			m_disk_timestamp = m_timestamp;
		} else {
			setTimestampNoChangedFlag(BLOCK_TIMESTAMP_UNDEFINED);
		}

		// Dynamically re-set ids based on node names
		NameIdMapping nimap;
		// If supported, read node definition id mapping
		if (version >= 21) {
			nimap.deSerialize(is);
			u16 dummy;
			m_is_air = nimap.size() == 1 && nimap.getId("air", dummy);
		// Else set the legacy mapping
		} else {
			content_mapnode_get_name_id_mapping(&nimap);
			m_is_air = false;
			m_is_air_expired = true;
		}
		correctBlockNodeIds(&nimap, data, m_gamedef);
	}

	// Legacy data changes
	// This code has to convert from pre-22 to post-22 format.
	const NodeDefManager *nodedef = m_gamedef->ndef();
	for (u32 i = 0; i < nodecount; i++) {
		const ContentFeatures &f = nodedef->get(data[i].getContent());
		// Mineral
		if(nodedef->getId("default:stone") == data[i].getContent()
				&& data[i].getParam1() == 1)
		{
			data[i].setContent(nodedef->getId("default:stone_with_coal"));
			data[i].setParam1(0);
		}
		else if(nodedef->getId("default:stone") == data[i].getContent()
				&& data[i].getParam1() == 2)
		{
			data[i].setContent(nodedef->getId("default:stone_with_iron"));
			data[i].setParam1(0);
		}
		// facedir_simple
		if (f.legacy_facedir_simple) {
			data[i].setParam2(data[i].getParam1());
			data[i].setParam1(0);
		}
		// wall_mounted
		if (f.legacy_wallmounted) {
			u8 wallmounted_new_to_old[8] = {0x04, 0x08, 0x01, 0x02, 0x10, 0x20, 0, 0};
			u8 dir_old_format = data[i].getParam2();
			u8 dir_new_format = 0;
			for (u8 j = 0; j < 8; j++) {
				if ((dir_old_format & wallmounted_new_to_old[j]) != 0) {
					dir_new_format = j;
					break;
				}
			}
			data[i].setParam2(dir_new_format);
		}
	}
}

/*
	Get a quick string to describe what a block actually contains
*/
std::string analyze_block(MapBlock *block)
{
	if (block == NULL)
		return "NULL";

	std::ostringstream desc;

	v3s16 p = block->getPos();
	char spos[25];
	porting::mt_snprintf(spos, sizeof(spos), "(%2d,%2d,%2d), ", p.X, p.Y, p.Z);
	desc<<spos;

	switch(block->getModified())
	{
	case MOD_STATE_CLEAN:
		desc<<"CLEAN,           ";
		break;
	case MOD_STATE_WRITE_AT_UNLOAD:
		desc<<"WRITE_AT_UNLOAD, ";
		break;
	case MOD_STATE_WRITE_NEEDED:
		desc<<"WRITE_NEEDED,    ";
		break;
	default:
		desc<<"unknown getModified()="+itos(block->getModified())+", ";
	}

	if(block->isGenerated())
		desc<<"is_gen [X], ";
	else
		desc<<"is_gen [ ], ";

	if(block->getIsUnderground())
		desc<<"is_ug [X], ";
	else
		desc<<"is_ug [ ], ";

	desc<<"lighting_complete: "<<block->getLightingComplete()<<", ";

	bool full_ignore = true;
	bool some_ignore = false;
	bool full_air = true;
	bool some_air = false;
	for(s16 z0=0; z0<MAP_BLOCKSIZE; z0++)
	for(s16 y0=0; y0<MAP_BLOCKSIZE; y0++)
	for(s16 x0=0; x0<MAP_BLOCKSIZE; x0++)
	{
		v3s16 p(x0,y0,z0);
		MapNode n = block->getNodeNoEx(p);
		content_t c = n.getContent();
		if(c == CONTENT_IGNORE)
			some_ignore = true;
		else
			full_ignore = false;
		if(c == CONTENT_AIR)
			some_air = true;
		else
			full_air = false;
	}

	desc<<"content {";

	std::ostringstream ss;

	if(full_ignore)
		ss<<"IGNORE (full), ";
	else if(some_ignore)
		ss<<"IGNORE, ";

	if(full_air)
		ss<<"AIR (full), ";
	else if(some_air)
		ss<<"AIR, ";

	if(ss.str().size()>=2)
		desc<<ss.str().substr(0, ss.str().size()-2);

	desc<<"}, ";

	return desc.str().substr(0, desc.str().size()-2);
}


//END
