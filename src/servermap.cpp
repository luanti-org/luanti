// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2024 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <algorithm>

#include "map.h"
#include "mapsector.h"
#include "filesys.h"
#include "voxel.h"
#include "voxelalgorithms.h"
#include "porting.h"
#include "serialization.h"
#include "settings.h"
#include "log.h"
#include "profiler.h"
#include "gamedef.h"
#include "util/directiontables.h"
#include "rollback_interface.h"
#include "reflowscan.h"
#include "emerge.h"
#include "mapgen/mapgen_v6.h"
#include "mapgen/mg_biome.h"
#include "config.h"
#include "server.h"
#include "database/database.h"
#include "database/database-dummy.h"
#include "database/database-sqlite3.h"
#include "script/scripting_server.h"
#include "irrlicht_changes/printing.h"
#if USE_LEVELDB
#include "database/database-leveldb.h"
#endif
#if USE_REDIS
#include "database/database-redis.h"
#endif
#if USE_POSTGRESQL
#include "database/database-postgresql.h"
#endif

/*
	Helpers
*/

void MapDatabaseAccessor::loadBlock(v3s16 blockpos, std::string &ret)
{
	ret.clear();
	dbase->loadBlock(blockpos, &ret);
	if (ret.empty() && dbase_ro)
		dbase_ro->loadBlock(blockpos, &ret);
}

/*
	ServerMap
*/

ServerMap::ServerMap(const std::string &savedir, IGameDef *gamedef,
		EmergeManager *emerge, MetricsBackend *mb):
	Map(gamedef),
	settings_mgr(savedir + DIR_DELIM + "map_meta.txt"),
	m_emerge(emerge)
{
	// Tell the EmergeManager about our MapSettingsManager
	emerge->map_settings_mgr = &settings_mgr;

	/*
		Try to open map; if not found, create a new one.
	*/

	// Determine which database backend to use
	std::string conf_path = savedir + DIR_DELIM + "world.mt";
	Settings conf;
	bool succeeded = conf.readConfigFile(conf_path.c_str());
	if (!succeeded || !conf.exists("backend")) {
		// fall back to sqlite3
		conf.set("backend", "sqlite3");
	}
	std::string backend = conf.get("backend");
	m_db.dbase = createDatabase(backend, savedir, conf);
	if (conf.exists("readonly_backend")) {
		std::string readonly_dir = savedir + DIR_DELIM + "readonly";
		m_db.dbase_ro = createDatabase(conf.get("readonly_backend"), readonly_dir, conf);
	}
	if (!conf.updateConfigFile(conf_path.c_str()))
		errorstream << "ServerMap::ServerMap(): Failed to update world.mt!" << std::endl;

	m_savedir = savedir;
	m_map_saving_enabled = false;

	// Inform EmergeManager of db handles
	m_emerge->initMap(&m_db);

	m_save_time_counter = mb->addCounter(
		"minetest_map_save_time", "Time spent saving blocks (in microseconds)");
	m_save_count_counter = mb->addCounter(
		"minetest_map_saved_blocks", "Number of blocks saved");
	m_loaded_blocks_gauge = mb->addGauge(
		"minetest_map_loaded_blocks", "Number of loaded blocks");

	m_map_compression_level = rangelim(g_settings->getS16("map_compression_level_disk"), -1, 9);

	try {
		// If directory exists, check contents and load if possible
		if (fs::PathExists(m_savedir)) {
			// If directory is empty, it is safe to save into it.
			if (fs::GetDirListing(m_savedir).empty()) {
				infostream<<"ServerMap: Empty save directory is valid."
						<<std::endl;
				m_map_saving_enabled = true;
			}
			else
			{

				if (settings_mgr.loadMapMeta()) {
					infostream << "ServerMap: Metadata loaded from "
						<< savedir << std::endl;
				} else {
					infostream << "ServerMap: Metadata could not be loaded "
						"from " << savedir << ", assuming valid save "
						"directory." << std::endl;
				}

				m_map_saving_enabled = true;
				// Map loaded, not creating new one
				return;
			}
		}
		// If directory doesn't exist, it is safe to save to it
		else{
			m_map_saving_enabled = true;
		}
	}
	catch(std::exception &e)
	{
		warningstream<<"ServerMap: Failed to load map from "<<savedir
				<<", exception: "<<e.what()<<std::endl;
		infostream<<"Please remove the map or fix it."<<std::endl;
		warningstream<<"Map saving will be disabled."<<std::endl;
	}
}

ServerMap::~ServerMap()
{
	verbosestream<<FUNCTION_NAME<<std::endl;

	try
	{
		if (m_map_saving_enabled) {
			// Save only changed parts
			save(MOD_STATE_WRITE_AT_UNLOAD);
			infostream << "ServerMap: Saved map to " << m_savedir << std::endl;
		} else {
			infostream << "ServerMap: Map not saved" << std::endl;
		}
	}
	catch(std::exception &e)
	{
		errorstream << "ServerMap: Failed to save map to " << m_savedir
				 << ", exception: " << e.what() << std::endl;
	}

	m_emerge->resetMap();

	{
		MutexAutoLock dblock(m_db.mutex);
		delete m_db.dbase;
		m_db.dbase = nullptr;
		delete m_db.dbase_ro;
		m_db.dbase_ro = nullptr;
	}

	deleteDetachedBlocks();
}

MapgenParams *ServerMap::getMapgenParams()
{
	// getMapgenParams() should only ever be called after Server is initialized
	assert(settings_mgr.mapgen_params != NULL);
	return settings_mgr.mapgen_params;
}

u64 ServerMap::getSeed()
{
	return getMapgenParams()->seed;
}

bool ServerMap::blockpos_over_mapgen_limit(v3s16 p)
{
	const s16 mapgen_limit_bp = rangelim(
		getMapgenParams()->mapgen_limit, 0, MAX_MAP_GENERATION_LIMIT) /
		MAP_BLOCKSIZE;
	return p.X < -mapgen_limit_bp ||
		p.X >  mapgen_limit_bp ||
		p.Y < -mapgen_limit_bp ||
		p.Y >  mapgen_limit_bp ||
		p.Z < -mapgen_limit_bp ||
		p.Z >  mapgen_limit_bp;
}

bool ServerMap::initBlockMake(v3s16 blockpos, BlockMakeData *data)
{
	assert(data);
	s16 csize = getMapgenParams()->chunksize;
	const v3s16 bpmin = EmergeManager::getContainingChunk(blockpos, csize);
	const v3s16 bpmax = bpmin + v3s16(1, 1, 1) * (csize - 1);

	if (!m_chunks_in_progress.insert(bpmin).second)
		return false;

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("initBlockMake(): " << bpmin << " - " << bpmax);

	const v3s16 full_bpmin = bpmin - EMERGE_EXTRA_BORDER;
	const v3s16 full_bpmax = bpmax + EMERGE_EXTRA_BORDER;

	// Do nothing if not fully inside mapgen limits
	if (blockpos_over_mapgen_limit(full_bpmin) ||
			blockpos_over_mapgen_limit(full_bpmax))
		return false;

	data->seed = getSeed();
	data->blockpos_min = bpmin;
	data->blockpos_max = bpmax;
	data->nodedef = m_nodedef;

	/*
		Create the whole area of this and the neighboring blocks
	*/
	for (s16 x = full_bpmin.X; x <= full_bpmax.X; x++)
	for (s16 z = full_bpmin.Z; z <= full_bpmax.Z; z++) {
		v2s16 sectorpos(x, z);
		// Sector metadata is loaded from disk if not already loaded.
		MapSector *sector = createSector(sectorpos);
		FATAL_ERROR_IF(sector == NULL, "createSector() failed");

		for (s16 y = full_bpmin.Y; y <= full_bpmax.Y; y++) {
			v3s16 p(x, y, z);

			MapBlock *block = emergeBlock(p, false);
			if (block == NULL) {
				block = createBlock(p);

				// Block gets sunlight if this is true.
				// Refer to the map generator heuristics.
				bool ug = m_emerge->isBlockUnderground(p);
				block->setIsUnderground(ug);
			}
			block->refGrab();
		}
	}

	/*
		Now we have a big empty area.

		Make a ManualMapVoxelManipulator that contains this and the
		neighboring blocks
	*/

	data->vmanip = new MMVManip(this);
	data->vmanip->initialEmerge(full_bpmin, full_bpmax);

	// Data is ready now.
	return true;
}

void ServerMap::cancelBlockMake(BlockMakeData *data)
{
	assert(data->vmanip); // no vmanip = initBlockMake did not complete (caller mistake)

	const v3s16 full_bpmin = data->blockpos_min - EMERGE_EXTRA_BORDER;
	const v3s16 full_bpmax = data->blockpos_max + EMERGE_EXTRA_BORDER;
	for (s16 x = full_bpmin.X; x <= full_bpmax.X; x++)
	for (s16 z = full_bpmin.Z; z <= full_bpmax.Z; z++)
	for (s16 y = full_bpmin.Y; y <= full_bpmax.Y; y++) {
		MapBlock *block = getBlockNoCreateNoEx(v3s16(x, y, z));
		if (block)
			block->refDrop();
	}
}

void ServerMap::finishBlockMake(BlockMakeData *data,
	std::map<v3s16, MapBlock*> *changed_blocks, u32 now)
{
	assert(data);
	assert(changed_blocks);
	const v3s16 bpmin = data->blockpos_min;
	const v3s16 bpmax = data->blockpos_max;

	bool enable_mapgen_debug_info = m_emerge->enable_mapgen_debug_info;
	EMERGE_DBG_OUT("finishBlockMake(): " << bpmin << " - " << bpmax);

	/*
		Blit generated stuff to map
		NOTE: blitBackAll adds nearly everything to changed_blocks
	*/
	data->vmanip->blitBackAll(changed_blocks);

	EMERGE_DBG_OUT("finishBlockMake: changed_blocks.size()="
		<< changed_blocks->size());

	/*
		Copy transforming liquid information
	*/
	while (!data->transforming_liquid.empty()) {
		m_transforming_liquid.push_back(data->transforming_liquid.front());
		data->transforming_liquid.pop_front();
	}

	for (auto &changed_block : *changed_blocks) {
		MapBlock *block = changed_block.second;
		if (!block)
			continue;
		/*
			Update is air cache of the MapBlocks
		*/
		block->expireIsAirCache();
		/*
			Set block as modified (if it isn't already)
		*/
		block->raiseModified(MOD_STATE_WRITE_NEEDED,
			MOD_REASON_EXPIRE_IS_AIR);
	}

	const v3s16 full_bpmin = bpmin - EMERGE_EXTRA_BORDER;
	const v3s16 full_bpmax = bpmax + EMERGE_EXTRA_BORDER;

	v3s16 bp;
	for (bp.X = full_bpmin.X; bp.X <= full_bpmax.X; bp.X++)
	for (bp.Z = full_bpmin.Z; bp.Z <= full_bpmax.Z; bp.Z++)
	for (bp.Y = full_bpmin.Y; bp.Y <= full_bpmax.Y; bp.Y++) {
		MapBlock *block = getBlockNoCreateNoEx(bp);
		if (!block) {
			warningstream << "ServerMap::finishBlockMake: block " << bp
				<< " disappeared during generation" << std::endl;
			continue;
		}

		block->refDrop();

		/* Border blocks are grabbed during
		   generation but mustn't be marked generated. */
		if (bp.X >= bpmin.X && bp.X <= bpmax.X
				&& bp.Y >= bpmin.Y && bp.Y <= bpmax.Y
				&& bp.Z >= bpmin.Z && bp.Z <= bpmax.Z) {
			block->setGenerated(true);
			// Set timestamp to ensure correct application
			// of LBMs and other stuff.
			block->setTimestampNoChangedFlag(now);
		}
	}

	m_chunks_in_progress.erase(bpmin);
}

MapSector *ServerMap::createSector(v2s16 p2d)
{
	/*
		Check if it exists already in memory
	*/
	MapSector *sector = getSectorNoGenerate(p2d);
	if (sector)
		return sector;

	/*
		Do not create over max mapgen limit
	*/
	if (blockpos_over_max_limit(v3s16(p2d.X, 0, p2d.Y)))
		throw InvalidPositionException("createSector(): pos over max mapgen limit");

	/*
		Generate blank sector
	*/
	sector = new MapSector(this, p2d, m_gamedef);

	/*
		Insert to container
	*/
	m_sectors[p2d] = sector;

	return sector;
}

MapBlock * ServerMap::createBlock(v3s16 p)
{
	v2s16 p2d(p.X, p.Z);
	s16 block_y = p.Y;

	/*
		This will create or load a sector if not found in memory.
	*/
	MapSector *sector;
	try {
		sector = createSector(p2d);
	} catch (InvalidPositionException &e) {
		infostream<<"createBlock: createSector() failed"<<std::endl;
		throw e;
	}

	/*
		Try to get a block from the sector
	*/

	MapBlock *block = sector->getBlockNoCreateNoEx(block_y);
	if (block)
		return block;

	// Create blank
	try {
		block = sector->createBlankBlock(block_y);
	} catch (InvalidPositionException &e) {
		infostream << "createBlock: createBlankBlock() failed" << std::endl;
		throw e;
	}

	return block;
}

MapBlock * ServerMap::emergeBlock(v3s16 p, bool create_blank)
{
	{
		MapBlock *block = getBlockNoCreateNoEx(p);
		if (block)
			return block;
	}

	{
		MapBlock *block = loadBlock(p);
		if(block)
			return block;
	}

	if (create_blank) {
		try {
			MapSector *sector = createSector(v2s16(p.X, p.Z));
			return sector->createBlankBlock(p.Y);
		} catch (InvalidPositionException &e) {}
	}

	return NULL;
}

MapBlock *ServerMap::getBlockOrEmerge(v3s16 p3d, bool generate)
{
	MapBlock *block = getBlockNoCreateNoEx(p3d);
	if (block == NULL)
		m_emerge->enqueueBlockEmerge(PEER_ID_INEXISTENT, p3d, generate);

	return block;
}

bool ServerMap::isBlockInQueue(v3s16 pos)
{
	return m_emerge && m_emerge->isBlockInQueue(pos);
}

void ServerMap::addNodeAndUpdate(v3s16 p, MapNode n,
		std::map<v3s16, MapBlock*> &modified_blocks,
		bool remove_metadata)
{
	Map::addNodeAndUpdate(p, n, modified_blocks, remove_metadata);

	/*
		Add neighboring liquid nodes and this node to transform queue.
		(it's vital for the node itself to get updated last, if it was removed.)
	 */

	for (const v3s16 &dir : g_7dirs) {
		v3s16 p2 = p + dir;

		bool is_valid_position;
		MapNode n2 = getNode(p2, &is_valid_position);
		if(is_valid_position &&
				(m_nodedef->get(n2).isLiquid() ||
				n2.getContent() == CONTENT_AIR))
			m_transforming_liquid.push_back(p2);
	}
}

// N.B.  This requires no synchronization, since data will not be modified unless
// the VoxelManipulator being updated belongs to the same thread.
void ServerMap::updateVManip(v3s16 pos)
{
	Mapgen *mg = m_emerge->getCurrentMapgen();
	if (!mg)
		return;

	MMVManip *vm = mg->vm;
	if (!vm)
		return;

	if (!vm->m_area.contains(pos))
		return;

	s32 idx = vm->m_area.index(pos);
	vm->m_data[idx] = getNode(pos);
	vm->m_flags[idx] &= ~VOXELFLAG_NO_DATA;

	vm->m_is_dirty = true;
}

void ServerMap::reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks)
{
	m_loaded_blocks_gauge->set(all_blocks);
	m_save_time_counter->increment(save_time_us);
	m_save_count_counter->increment(saved_blocks);
}

void ServerMap::save(ModifiedState save_level)
{
	if (!m_map_saving_enabled) {
		warningstream<<"Not saving map, saving disabled."<<std::endl;
		return;
	}

	const auto start_time = porting::getTimeUs();

	if(save_level == MOD_STATE_CLEAN)
		infostream<<"ServerMap: Saving whole map, this can take time."
				<<std::endl;

	if (m_map_metadata_changed || save_level == MOD_STATE_CLEAN) {
		if (settings_mgr.saveMapMeta())
			m_map_metadata_changed = false;
	}

	// Profile modified reasons
	Profiler modprofiler;

	u32 block_count = 0;
	u32 block_count_all = 0; // Number of blocks in memory

	// Don't do anything with sqlite unless something is really saved
	bool save_started = false;

	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			block_count_all++;

			if(block->getModified() >= (u32)save_level) {
				// Lazy beginSave()
				if(!save_started) {
					beginSave();
					save_started = true;
				}

				modprofiler.add(block->getModifiedReasonString(), 1);

				saveBlock(block);
				block_count++;
			}
		}
	}

	if(save_started)
		endSave();

	/*
		Only print if something happened or saved whole map
	*/
	if(save_level == MOD_STATE_CLEAN
			|| block_count != 0) {
		infostream << "ServerMap: Written: "
				<< block_count << " blocks"
				<< ", " << block_count_all << " blocks in memory."
				<< std::endl;
		PrintInfo(infostream); // ServerMap/ClientMap:
		infostream<<"Blocks modified by: "<<std::endl;
		modprofiler.print(infostream);
	}

	const auto end_time = porting::getTimeUs();
	reportMetrics(end_time - start_time, block_count, block_count_all);
}

void ServerMap::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	MutexAutoLock dblock(m_db.mutex);
	m_db.dbase->listAllLoadableBlocks(dst);
	if (m_db.dbase_ro)
		m_db.dbase_ro->listAllLoadableBlocks(dst);
}

void ServerMap::listAllLoadedBlocks(std::vector<v3s16> &dst)
{
	for (auto &sector_it : m_sectors) {
		MapSector *sector = sector_it.second;

		MapBlockVect blocks;
		sector->getBlocks(blocks);

		for (MapBlock *block : blocks) {
			v3s16 p = block->getPos();
			dst.push_back(p);
		}
	}
}

std::vector<std::string> ServerMap::getDatabaseBackends()
{
	std::vector<std::string> ret;
	ret.emplace_back("sqlite3");
	ret.emplace_back("dummy");
#if USE_LEVELDB
	ret.emplace_back("leveldb");
#endif
#if USE_REDIS
	ret.emplace_back("redis");
#endif
#if USE_POSTGRESQL
	ret.emplace_back("postgresql");
#endif
	return ret;
}

MapDatabase *ServerMap::createDatabase(
	const std::string &name,
	const std::string &savedir,
	Settings &conf)
{
	// Hopefully this way we don't forget to keep them in sync.
	auto valid = getDatabaseBackends();
	if (!CONTAINS(valid, name)) {
		auto err = std::string("Database backend \"") + name + "\" unknown or not supported";
		errorstream << err << std::endl;
		throw BaseException(err);
	}

	MapDatabase *db = nullptr;
	infostream << "Creating map database with backend \"" << name << "\"" << std::endl;

	if (name == "sqlite3")
		db = new MapDatabaseSQLite3(savedir);
	else if (name == "dummy")
		db = new Database_Dummy();
#if USE_LEVELDB
	else if (name == "leveldb")
		db = new Database_LevelDB(savedir);
#endif
#if USE_REDIS
	else if (name == "redis")
		db = new Database_Redis(conf);
#endif
#if USE_POSTGRESQL
	else if (name == "postgresql") {
		std::string connect_string;
		conf.getNoEx("pgsql_connection", connect_string);
		db = new MapDatabasePostgreSQL(connect_string);
	}
#endif

	// Constructor can't return null, only throw
	sanity_check(db);

	// Do this to get feedback about errors asap
	db->verifyDatabase();
	assert(db->initialized());
	return db;
}

void ServerMap::beginSave()
{
	MutexAutoLock dblock(m_db.mutex);
	m_db.dbase->beginSave();
}

void ServerMap::endSave()
{
	MutexAutoLock dblock(m_db.mutex);
	m_db.dbase->endSave();
}

bool ServerMap::saveBlock(MapBlock *block)
{
	// FIXME: serialization happens under mutex
	MutexAutoLock dblock(m_db.mutex);
	return saveBlock(block, m_db.dbase, m_map_compression_level);
}

bool ServerMap::saveBlock(MapBlock *block, MapDatabase *db, int compression_level)
{
	v3s16 p3d = block->getPos();

	// Format used for writing
	u8 version = SER_FMT_VER_HIGHEST_WRITE;

	/*
		[0] u8 serialization version
		[1] data
	*/
	std::ostringstream o(std::ios_base::binary);
	o.write((char*) &version, 1);
	block->serialize(o, version, true, compression_level);

	// FIXME: zero copy possible in c++20 or with custom rdbuf
	bool ret = db->saveBlock(p3d, o.str());
	if (ret) {
		// We just wrote it to the disk so clear modified flag
		block->resetModified();
	}
	return ret;
}

void ServerMap::deSerializeBlock(MapBlock *block, std::istream &is)
{
	ScopeProfiler sp(g_profiler, "ServerMap: deSer block", SPT_AVG, PRECISION_MICRO);

	u8 version = readU8(is);
	if (is.fail())
		throw SerializationError("Failed to read MapBlock version");

	block->deSerialize(is, version, true);
}

MapBlock *ServerMap::loadBlock(const std::string &blob, v3s16 p3d, bool save_after_load)
{
	ScopeProfiler sp(g_profiler, "ServerMap: load block", SPT_AVG, PRECISION_MICRO);
	MapBlock *block = nullptr;
	bool created_new = false;

	try {
		v2s16 p2d(p3d.X, p3d.Z);
		MapSector *sector = createSector(p2d);

		std::unique_ptr<MapBlock> block_created_new;
		block = sector->getBlockNoCreateNoEx(p3d.Y);
		if (!block) {
			block_created_new = sector->createBlankBlockNoInsert(p3d.Y);
			block = block_created_new.get();
		}

		{
			std::istringstream iss(blob, std::ios_base::binary);
			deSerializeBlock(block, iss);
		}

		// If it's a new block, insert it to the map
		if (block_created_new) {
			sector->insertBlock(std::move(block_created_new));
			created_new = true;
		}
	} catch (SerializationError &e) {
		errorstream<<"Invalid block data in database"
				<<" ("<<p3d.X<<","<<p3d.Y<<","<<p3d.Z<<")"
				<<" (SerializationError): "<<e.what()<<std::endl;

		// TODO: Block should be marked as invalid in memory so that it is
		// not touched but the game can run

		if(g_settings->getBool("ignore_world_load_errors")){
			errorstream<<"Ignoring block load error. Duck and cover! "
					<<"(ignore_world_load_errors)"<<std::endl;
		} else {
			throw SerializationError("Invalid block data in database");
		}
	}

	assert(block);

	if (created_new) {
		ReflowScan scanner(this, m_emerge->ndef);
		scanner.scan(block, &m_transforming_liquid);

		std::map<v3s16, MapBlock*> modified_blocks;
		// Fix lighting if necessary
		voxalgo::update_block_border_lighting(this, block, modified_blocks);
		if (!modified_blocks.empty()) {
			MapEditEvent event;
			event.type = MEET_OTHER;
			event.low_priority = true;
			event.setModifiedBlocks(modified_blocks);
			dispatchEvent(event);
		}
	}

	if (save_after_load)
		saveBlock(block);

	// We just loaded it, so it's up-to-date.
	block->resetModified();

	return block;
}

MapBlock* ServerMap::loadBlock(v3s16 blockpos)
{
	std::string data;
	{
		ScopeProfiler sp(g_profiler, "ServerMap: load block - sync (sum)");
		MutexAutoLock dblock(m_db.mutex);
		m_db.loadBlock(blockpos, data);
	}

	if (!data.empty())
		return loadBlock(data, blockpos);
	return getBlockNoCreateNoEx(blockpos);
}

bool ServerMap::deleteBlock(v3s16 blockpos)
{
	MutexAutoLock dblock(m_db.mutex);
	if (!m_db.dbase->deleteBlock(blockpos))
		return false;

	MapBlock *block = getBlockNoCreateNoEx(blockpos);
	if (block) {
		v2s16 p2d(blockpos.X, blockpos.Z);
		MapSector *sector = getSectorNoGenerate(p2d);
		if (!sector)
			return false;
		// It may not be safe to delete the block from memory at the moment
		// (pointers to it could still be in use)
		m_detached_blocks.push_back(sector->detachBlock(block));
	}

	return true;
}

void ServerMap::deleteDetachedBlocks()
{
	for (const auto &block : m_detached_blocks) {
		assert(block->isOrphan());
		(void)block; // silence unused-variable warning in release builds
	}

	m_detached_blocks.clear();
}

void ServerMap::step()
{
	// Delete from memory blocks removed by deleteBlocks() only when pointers
	// to them are (probably) no longer in use
	deleteDetachedBlocks();
}

void ServerMap::PrintInfo(std::ostream &out)
{
	out<<"ServerMap: ";
}

bool ServerMap::repairBlockLight(v3s16 blockpos,
	std::map<v3s16, MapBlock *> *modified_blocks)
{
	MapBlock *block = emergeBlock(blockpos, false);
	if (!block || !block->isGenerated())
		return false;
	voxalgo::repair_block_light(this, block, modified_blocks);
	return true;
}

/*
	Liquids
*/

#define WATER_DROP_BOOST 4

const static v3s16 liquid_6dirs[6] = {
	// order: upper before same level before lower
	v3s16( 0, 1, 0),
	v3s16( 0, 0, 1),
	v3s16( 1, 0, 0),
	v3s16( 0, 0,-1),
	v3s16(-1, 0, 0),
	v3s16( 0,-1, 0)
};

enum NeighborType : u8 {
	NEIGHBOR_UPPER,
	NEIGHBOR_SAME_LEVEL,
	NEIGHBOR_LOWER
};

struct NodeNeighbor {
	MapNode n;
	NeighborType t;
	v3s16 p;

	NodeNeighbor()
		: n(CONTENT_AIR), t(NEIGHBOR_SAME_LEVEL)
	{ }

	NodeNeighbor(const MapNode &node, NeighborType n_type, const v3s16 &pos)
		: n(node),
		  t(n_type),
		  p(pos)
	{ }
};

static s8 get_max_liquid_level(NodeNeighbor nb, s8 current_max_node_level)
{
	s8 max_node_level = current_max_node_level;
	u8 nb_liquid_level = (nb.n.param2 & LIQUID_LEVEL_MASK);
	switch (nb.t) {
		case NEIGHBOR_UPPER:
			if (nb_liquid_level + WATER_DROP_BOOST > current_max_node_level) {
				max_node_level = LIQUID_LEVEL_MAX;
				if (nb_liquid_level + WATER_DROP_BOOST < LIQUID_LEVEL_MAX)
					max_node_level = nb_liquid_level + WATER_DROP_BOOST;
			} else if (nb_liquid_level > current_max_node_level) {
				max_node_level = nb_liquid_level;
			}
			break;
		case NEIGHBOR_LOWER:
			break;
		case NEIGHBOR_SAME_LEVEL:
			if ((nb.n.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK &&
					nb_liquid_level > 0 && nb_liquid_level - 1 > max_node_level)
				max_node_level = nb_liquid_level - 1;
			break;
	}
	return max_node_level;
}

void ServerMap::transforming_liquid_add(v3s16 p)
{
	m_transforming_liquid.push_back(p);
}

void ServerMap::transformLiquids(std::map<v3s16, MapBlock*> &modified_blocks,
		ServerEnvironment *env)
{
	u32 loopcount = 0;
	u32 initial_size = m_transforming_liquid.size();

	/*if(initial_size != 0)
		infostream<<"transformLiquids(): initial_size="<<initial_size<<std::endl;*/

	// list of nodes that due to viscosity have not reached their max level height
	std::vector<v3s16> must_reflow;

	std::vector<std::pair<v3s16, MapNode> > changed_nodes;

	std::vector<v3s16> check_for_falling;

	u32 liquid_loop_max = g_settings->getS32("liquid_loop_max");
	u32 loop_max = liquid_loop_max;

	while (m_transforming_liquid.size() != 0)
	{
		// This should be done here so that it is done when continue is used
		if (loopcount >= initial_size || loopcount >= loop_max)
			break;
		loopcount++;

		/*
			Get a queued transforming liquid node
		*/
		v3s16 p0 = m_transforming_liquid.front();
		m_transforming_liquid.pop_front();

		MapNode n0 = getNode(p0);

		/*
			Collect information about current node
		 */
		s8 liquid_level = -1;
		// The liquid node which will be placed there if
		// the liquid flows into this node.
		content_t liquid_kind = CONTENT_IGNORE;
		// The node which will be placed there if liquid
		// can't flow into this node.
		content_t floodable_node = CONTENT_AIR;
		const ContentFeatures &cf = m_nodedef->get(n0);
		LiquidType liquid_type = cf.liquid_type;
		switch (liquid_type) {
			case LIQUID_SOURCE:
				liquid_level = LIQUID_LEVEL_SOURCE;
				liquid_kind = cf.liquid_alternative_flowing_id;
				break;
			case LIQUID_FLOWING:
				liquid_level = (n0.param2 & LIQUID_LEVEL_MASK);
				liquid_kind = n0.getContent();
				break;
			case LIQUID_NONE:
				// if this node is 'floodable', it *could* be transformed
				// into a liquid, otherwise, continue with the next node.
				if (!cf.floodable)
					continue;
				floodable_node = n0.getContent();
				liquid_kind = CONTENT_AIR;
				break;
			case LiquidType_END:
				break;
		}

		/*
			Collect information about the environment
		 */
		NodeNeighbor sources[6]; // surrounding sources
		int num_sources = 0;
		NodeNeighbor flows[6]; // surrounding flowing liquid nodes
		int num_flows = 0;
		NodeNeighbor airs[6]; // surrounding air
		int num_airs = 0;
		NodeNeighbor neutrals[6]; // nodes that are solid or another kind of liquid
		int num_neutrals = 0;
		bool flowing_down = false;
		bool ignored_sources = false;
		bool floating_node_above = false;
		for (u16 i = 0; i < 6; i++) {
			NeighborType nt = NEIGHBOR_SAME_LEVEL;
			switch (i) {
				case 0:
					nt = NEIGHBOR_UPPER;
					break;
				case 5:
					nt = NEIGHBOR_LOWER;
					break;
				default:
					break;
			}
			v3s16 npos = p0 + liquid_6dirs[i];
			NodeNeighbor nb(getNode(npos), nt, npos);
			const ContentFeatures &cfnb = m_nodedef->get(nb.n);
			if (nt == NEIGHBOR_UPPER && cfnb.floats)
				floating_node_above = true;
			switch (cfnb.liquid_type) {
				case LIQUID_NONE:
					if (cfnb.floodable) {
						airs[num_airs++] = nb;
						// if the current node is a water source the neighbor
						// should be enqueded for transformation regardless of whether the
						// current node changes or not.
						if (nb.t != NEIGHBOR_UPPER && liquid_type != LIQUID_NONE)
							m_transforming_liquid.push_back(npos);
						// if the current node happens to be a flowing node, it will start to flow down here.
						if (nb.t == NEIGHBOR_LOWER)
							flowing_down = true;
					} else {
						neutrals[num_neutrals++] = nb;
						if (nb.n.getContent() == CONTENT_IGNORE) {
							// If node below is ignore prevent water from
							// spreading outwards and otherwise prevent from
							// flowing away as ignore node might be the source
							if (nb.t == NEIGHBOR_LOWER)
								flowing_down = true;
							else
								ignored_sources = true;
						}
					}
					break;
				case LIQUID_SOURCE:
					// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
					if (liquid_kind == CONTENT_AIR)
						liquid_kind = cfnb.liquid_alternative_flowing_id;
					if (cfnb.liquid_alternative_flowing_id != liquid_kind) {
						neutrals[num_neutrals++] = nb;
					} else {
						// Do not count bottom source, it will screw things up
						if(nt != NEIGHBOR_LOWER)
							sources[num_sources++] = nb;
					}
					break;
				case LIQUID_FLOWING:
					if (nb.t != NEIGHBOR_SAME_LEVEL ||
						(nb.n.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK) {
						// if this node is not (yet) of a liquid type, choose the first liquid type we encounter
						// but exclude falling liquids on the same level, they cannot flow here anyway

						// used to determine if the neighbor can even flow into this node
						s8 max_level_from_neighbor = get_max_liquid_level(nb, -1);
						u8 range = m_nodedef->get(cfnb.liquid_alternative_flowing_id).liquid_range;

						if (liquid_kind == CONTENT_AIR &&
								max_level_from_neighbor >= (LIQUID_LEVEL_MAX + 1 - range))
							liquid_kind = cfnb.liquid_alternative_flowing_id;
					}
					if (cfnb.liquid_alternative_flowing_id != liquid_kind) {
						neutrals[num_neutrals++] = nb;
					} else {
						flows[num_flows++] = nb;
						if (nb.t == NEIGHBOR_LOWER)
							flowing_down = true;
					}
					break;
				case LiquidType_END:
					break;
			}
		}

		/*
			decide on the type (and possibly level) of the current node
		 */
		content_t new_node_content;
		s8 new_node_level = -1;
		s8 max_node_level = -1;

		u8 range = m_nodedef->get(liquid_kind).liquid_range;
		if (range > LIQUID_LEVEL_MAX + 1)
			range = LIQUID_LEVEL_MAX + 1;

		if ((num_sources >= 2 && m_nodedef->get(liquid_kind).liquid_renewable) || liquid_type == LIQUID_SOURCE) {
			// liquid_kind will be set to either the flowing alternative of the node (if it's a liquid)
			// or the flowing alternative of the first of the surrounding sources (if it's air), so
			// it's perfectly safe to use liquid_kind here to determine the new node content.
			new_node_content = m_nodedef->get(liquid_kind).liquid_alternative_source_id;
		} else if (num_sources >= 1 && sources[0].t != NEIGHBOR_LOWER) {
			// liquid_kind is set properly, see above
			max_node_level = new_node_level = LIQUID_LEVEL_MAX;
			if (new_node_level >= (LIQUID_LEVEL_MAX + 1 - range))
				new_node_content = liquid_kind;
			else
				new_node_content = floodable_node;
		} else if (ignored_sources && liquid_level >= 0) {
			// Maybe there are neighboring sources that aren't loaded yet
			// so prevent flowing away.
			new_node_level = liquid_level;
			new_node_content = liquid_kind;
		} else {
			// no surrounding sources, so get the maximum level that can flow into this node
			for (u16 i = 0; i < num_flows; i++) {
				max_node_level = get_max_liquid_level(flows[i], max_node_level);
			}

			u8 viscosity = m_nodedef->get(liquid_kind).liquid_viscosity;
			if (viscosity > 1 && max_node_level != liquid_level) {
				// amount to gain, limited by viscosity
				// must be at least 1 in absolute value
				s8 level_inc = max_node_level - liquid_level;
				if (level_inc < -viscosity || level_inc > viscosity)
					new_node_level = liquid_level + level_inc/viscosity;
				else if (level_inc < 0)
					new_node_level = liquid_level - 1;
				else if (level_inc > 0)
					new_node_level = liquid_level + 1;
				if (new_node_level != max_node_level)
					must_reflow.push_back(p0);
			} else {
				new_node_level = max_node_level;
			}

			if (max_node_level >= (LIQUID_LEVEL_MAX + 1 - range))
				new_node_content = liquid_kind;
			else
				new_node_content = floodable_node;

		}

		/*
			check if anything has changed. if not, just continue with the next node.
		 */
		if (new_node_content == n0.getContent() &&
				(m_nodedef->get(n0.getContent()).liquid_type != LIQUID_FLOWING ||
				((n0.param2 & LIQUID_LEVEL_MASK) == (u8)new_node_level &&
				((n0.param2 & LIQUID_FLOW_DOWN_MASK) == LIQUID_FLOW_DOWN_MASK)
				== flowing_down)))
			continue;

		/*
			check if there is a floating node above that needs to be updated.
		 */
		if (floating_node_above && new_node_content == CONTENT_AIR)
			check_for_falling.push_back(p0);

		/*
			update the current node
		 */
		MapNode n00 = n0;
		//bool flow_down_enabled = (flowing_down && ((n0.param2 & LIQUID_FLOW_DOWN_MASK) != LIQUID_FLOW_DOWN_MASK));
		if (m_nodedef->get(new_node_content).liquid_type == LIQUID_FLOWING) {
			// set level to last 3 bits, flowing down bit to 4th bit
			n0.param2 = (flowing_down ? LIQUID_FLOW_DOWN_MASK : 0x00) | (new_node_level & LIQUID_LEVEL_MASK);
		} else {
			// set the liquid level and flow bits to 0
			n0.param2 &= ~(LIQUID_LEVEL_MASK | LIQUID_FLOW_DOWN_MASK);
		}

		// change the node.
		n0.setContent(new_node_content);

		// on_flood() the node
		if (floodable_node != CONTENT_AIR) {
			if (env->getScriptIface()->node_on_flood(p0, n00, n0))
				continue;
		}

		// Ignore light (because calling voxalgo::update_lighting_nodes)
		ContentLightingFlags f0 = m_nodedef->getLightingFlags(n0);
		n0.setLight(LIGHTBANK_DAY, 0, f0);
		n0.setLight(LIGHTBANK_NIGHT, 0, f0);

		// Find out whether there is a suspect for this action
		std::string suspect;
		if (m_gamedef->rollback())
			suspect = m_gamedef->rollback()->getSuspect(p0, 83, 1);

		if (m_gamedef->rollback() && !suspect.empty()) {
			// Blame suspect
			RollbackScopeActor rollback_scope(m_gamedef->rollback(), suspect, true);
			// Get old node for rollback
			RollbackNode rollback_oldnode(this, p0, m_gamedef);
			// Set node
			setNode(p0, n0);
			// Report
			RollbackNode rollback_newnode(this, p0, m_gamedef);
			RollbackAction action;
			action.setSetNode(p0, rollback_oldnode, rollback_newnode);
			m_gamedef->rollback()->reportAction(action);
		} else {
			// Set node
			setNode(p0, n0);
		}

		v3s16 blockpos = getNodeBlockPos(p0);
		MapBlock *block = getBlockNoCreateNoEx(blockpos);
		if (block != NULL) {
			modified_blocks[blockpos] =  block;
			changed_nodes.emplace_back(p0, n00);
		}

		/*
			enqueue neighbors for update if necessary
		 */
		switch (m_nodedef->get(n0.getContent()).liquid_type) {
			case LIQUID_SOURCE:
			case LIQUID_FLOWING:
				// make sure source flows into all neighboring nodes
				for (u16 i = 0; i < num_flows; i++)
					if (flows[i].t != NEIGHBOR_UPPER)
						m_transforming_liquid.push_back(flows[i].p);
				for (u16 i = 0; i < num_airs; i++)
					if (airs[i].t != NEIGHBOR_UPPER)
						m_transforming_liquid.push_back(airs[i].p);
				break;
			case LIQUID_NONE:
				// this flow has turned to air; neighboring flows might need to do the same
				for (u16 i = 0; i < num_flows; i++)
					m_transforming_liquid.push_back(flows[i].p);
				break;
			case LiquidType_END:
				break;
		}
	}
	//infostream<<"Map::transformLiquids(): loopcount="<<loopcount<<std::endl;

	for (const auto &iter : must_reflow)
		m_transforming_liquid.push_back(iter);

	voxalgo::update_lighting_nodes(this, changed_nodes, modified_blocks);

	for (const v3s16 &p : check_for_falling) {
		env->getScriptIface()->check_for_falling(p);
	}

	env->getScriptIface()->on_liquid_transformed(changed_nodes);

	/* ----------------------------------------------------------------------
	 * Manage the queue so that it does not grow indefinitely
	 */
	u16 time_until_purge = g_settings->getU16("liquid_queue_purge_time");

	if (time_until_purge == 0)
		return; // Feature disabled

	time_until_purge *= 1000;	// seconds -> milliseconds

	u64 curr_time = porting::getTimeMs();
	u32 prev_unprocessed = m_unprocessed_count;
	m_unprocessed_count = m_transforming_liquid.size();

	// if unprocessed block count is decreasing or stable
	if (m_unprocessed_count <= prev_unprocessed) {
		m_queue_size_timer_started = false;
	} else {
		if (!m_queue_size_timer_started)
			m_inc_trending_up_start_time = curr_time;
		m_queue_size_timer_started = true;
	}

	// Account for curr_time overflowing
	if (m_queue_size_timer_started && m_inc_trending_up_start_time > curr_time)
		m_queue_size_timer_started = false;

	/* If the queue has been growing for more than liquid_queue_purge_time seconds
	 * and the number of unprocessed blocks is still > liquid_loop_max then we
	 * cannot keep up; dump the oldest blocks from the queue so that the queue
	 * has liquid_loop_max items in it
	 */
	if (m_queue_size_timer_started
			&& curr_time - m_inc_trending_up_start_time > time_until_purge
			&& m_unprocessed_count > liquid_loop_max) {

		size_t dump_qty = m_unprocessed_count - liquid_loop_max;

		infostream << "transformLiquids(): DUMPING " << dump_qty
		           << " blocks from the queue" << std::endl;

		while (dump_qty--)
			m_transforming_liquid.pop_front();

		m_queue_size_timer_started = false; // optimistically assume we can keep up now
		m_unprocessed_count = m_transforming_liquid.size();
	}
}
