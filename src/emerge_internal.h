// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

/******************************************************************/
/* may only be included by emerge.cpp or emerge scripting related */
/******************************************************************/

#include "emerge.h"

#include <queue>

#include "util/thread.h"
#include "threading/event.h"

class Server;
class ServerMap;
class Mapgen;

class EmergeManager;
class EmergeScripting;

class EmergeThread : public Thread {
public:
	bool enable_mapgen_debug_info;
	const int id; // Index of this thread

	EmergeThread(Server *server, int ethreadid);
	~EmergeThread() = default;

	void *run();
	void signal();

	// Requires queue mutex held
	bool pushBlock(v3s16 pos);

	void cancelPendingItems();

	EmergeManager *getEmergeManager() { return m_emerge; }
	Mapgen *getMapgen() { return m_mapgen; }

protected:

	void runCompletionCallbacks(
		v3s16 pos, EmergeAction action,
		const EmergeCallbackList &callbacks);

private:
	Server *m_server;
	ServerMap *m_map;
	EmergeManager *m_emerge;
	Mapgen *m_mapgen;

	std::unique_ptr<EmergeScripting> m_script;
	// read from scripting:
	UniqueQueue<v3s16> *m_trans_liquid; //< non-null only when generating a mapblock

	Event m_queue_event;
	std::queue<v3s16> m_block_queue;

	bool initScripting();

	bool popBlockEmerge(v3s16 *pos, BlockEmergeData *bedata);

	/**
	 * Try to get a block from memory and decide what to do.
	 *
	 * @param pos block position
	 * @param from_db serialized block data, optional
	 *                (for second call after EMERGE_FROM_DISK was returned)
	 * @param allow_gen allow invoking mapgen?
	 * @param block output pointer for block
	 * @param data info for mapgen
	 * @return what to do for this block
	 */
	EmergeAction getBlockOrStartGen(v3s16 pos, bool allow_gen,
		const std::string *from_db,  MapBlock **block, BlockMakeData *data);

	MapBlock *finishGen(v3s16 pos, BlockMakeData *bmdata,
		std::map<v3s16, MapBlock *> *modified_blocks);

	friend class EmergeManager;
	friend class EmergeScripting;
	friend class ModApiMapgen;
};

// Scoped helper to set Server::m_ignore_map_edit_events_area
class MapEditEventAreaIgnorer
{
public:
	MapEditEventAreaIgnorer(VoxelArea *ignorevariable, const VoxelArea &a):
		m_ignorevariable(ignorevariable)
	{
		if (m_ignorevariable->hasEmptyExtent())
			*m_ignorevariable = a;
		else
			m_ignorevariable = nullptr;
	}

	~MapEditEventAreaIgnorer()
	{
		if (m_ignorevariable) {
			assert(!m_ignorevariable->hasEmptyExtent());
			*m_ignorevariable = VoxelArea();
		}
	}

private:
	VoxelArea *m_ignorevariable;
};
