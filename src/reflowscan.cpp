// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 MillersMan <millersman@users.noreply.github.com>

#include "reflowscan.h"
#include "map.h"
#include "mapblock.h"
#include "nodedef.h"
#include "util/timetaker.h"


ReflowScan::ReflowScan(Map *map, const NodeDefManager *ndef) :
	m_map(map),
	m_ndef(ndef)
{
}

void ReflowScan::scan(MapBlock *block, UniqueQueue<v3s16> *liquid_queue)
{
	m_block_pos = block->getPos();
	m_rel_block_pos = block->getPosRelative();
	m_liquid_queue = liquid_queue;

	// Prepare the lookup which is a 3x3x3 array of the blocks surrounding the
	// scanned block. Blocks are only added to the lookup if they are really
	// needed. The lookup is indexed manually to use the same index in a
	// bit-array (of uint32 type) which stores for unloaded blocks that they
	// were already fetched from Map but were actually nullptr.
	memset(m_lookup, 0, sizeof(m_lookup));
	int block_idx = 1 + (1 * 9) + (1 * 3);
	m_lookup[block_idx] = block;
	m_lookup_state_bitset = 1 << block_idx;

	// Scan the columns in the block
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
		scanColumn(x, z);
	}

	// Scan neighboring columns from the nearby blocks as they might contain
	// liquid nodes that weren't allowed to flow to prevent gaps.
	for (s16 i = 0; i < MAP_BLOCKSIZE; i++) {
		scanColumn(i, -1);
		scanColumn(i, MAP_BLOCKSIZE);
		scanColumn(-1, i);
		scanColumn(MAP_BLOCKSIZE, i);
	}
}

inline MapBlock *ReflowScan::lookupBlock(int x, int y, int z)
{
	// Gets the block that contains (x,y,z) relativ to the scanned block.
	// This uses a lookup as there might be many lookups into the same
	// neighboring block which makes fetches from Map costly.
	int bx = (MAP_BLOCKSIZE + x) / MAP_BLOCKSIZE;
	int by = (MAP_BLOCKSIZE + y) / MAP_BLOCKSIZE;
	int bz = (MAP_BLOCKSIZE + z) / MAP_BLOCKSIZE;
	int idx = (bx + (by * 9) + (bz * 3));
	MapBlock *result = m_lookup[idx];
	if (!result && (m_lookup_state_bitset & (1 << idx)) == 0) {
		// The block wasn't requested yet so fetch it from Map and store it
		// in the lookup
		v3s16 pos = m_block_pos + v3s16(bx - 1, by - 1, bz - 1);
		m_lookup[idx] = result = m_map->getBlockNoCreateNoEx(pos);
		m_lookup_state_bitset |= (1 << idx);
	}
	return result;
}

inline bool ReflowScan::isLiquidFlowableTo(int x, int y, int z)
{
	// Tests whether (x,y,z) is a node to which liquid might flow.
	MapBlock *block = lookupBlock(x, y, z);
	if (block) {
		int dx = (MAP_BLOCKSIZE + x) % MAP_BLOCKSIZE;
		int dy = (MAP_BLOCKSIZE + y) % MAP_BLOCKSIZE;
		int dz = (MAP_BLOCKSIZE + z) % MAP_BLOCKSIZE;
		MapNode node = block->getNodeNoCheck(dx, dy, dz);
		if (node.getContent() != CONTENT_IGNORE) {
			const ContentFeatures &f = m_ndef->get(node);
			// NOTE: No need to check for flowing nodes with lower liquid level
			// as they should only occur on top of other columns where they
			// will be added to the queue themselves.
			return f.floodable;
		}
	}
	return false;
}

inline bool ReflowScan::isLiquidHorizontallyFlowable(int x, int y, int z)
{
	// Check if the (x,y,z) might spread to one of the horizontally
	// neighboring nodes
	return isLiquidFlowableTo(x - 1, y, z) ||
		isLiquidFlowableTo(x + 1, y, z) ||
		isLiquidFlowableTo(x, y, z - 1) ||
		isLiquidFlowableTo(x, y, z + 1);
}

void ReflowScan::scanColumn(int x, int z)
{
	// Is the column inside a loaded block?
	MapBlock *block = lookupBlock(x, 0, z);
	if (!block)
		return;

	MapBlock *above = lookupBlock(x, MAP_BLOCKSIZE, z);
	int dx = (MAP_BLOCKSIZE + x) % MAP_BLOCKSIZE;
	int dz = (MAP_BLOCKSIZE + z) % MAP_BLOCKSIZE;

	// Get the state from the node above the scanned block
	bool was_ignore, was_liquid;
	if (above) {
		MapNode node = above->getNodeNoCheck(dx, 0, dz);
		was_ignore = node.getContent() == CONTENT_IGNORE;
		was_liquid = m_ndef->get(node).isLiquid();
	} else {
		was_ignore = true;
		was_liquid = false;
	}
	bool was_checked = false;
	bool was_pushed = false;

	// if there is no liquid above and the current block is air
	// we can skip scanning the block
	if (!was_liquid && block->isAir()) {
		// continue after the block with air
		was_ignore = false;
	} else {
		// Scan through the whole block
		for (s16 y = MAP_BLOCKSIZE - 1; y >= 0; y--) {
			MapNode node = block->getNodeNoCheck(dx, y, dz);
			const ContentFeatures &f = m_ndef->get(node);
			bool is_ignore = node.getContent() == CONTENT_IGNORE;
			bool is_liquid = f.isLiquid();

			if (is_ignore || was_ignore || is_liquid == was_liquid) {
				// Neither topmost node of liquid column nor topmost node below column
				was_checked = false;
				was_pushed = false;
			} else if (is_liquid) {
				// This is the topmost node in the column
				bool is_pushed = false;
				if (f.liquid_type == LIQUID_FLOWING ||
						isLiquidHorizontallyFlowable(x, y, z)) {
					m_liquid_queue->push_back(m_rel_block_pos + v3s16(x, y, z));
					is_pushed = true;
				}
				// Remember waschecked and waspushed to avoid repeated
				// checks/pushes in case the column consists of only this node
				was_checked = true;
				was_pushed = is_pushed;
			} else {
				// This is the topmost node below a liquid column
				if (!was_pushed && (f.floodable ||
						(!was_checked && isLiquidHorizontallyFlowable(x, y + 1, z)))) {
					// Activate the lowest node in the column which is one
					// node above this one
					m_liquid_queue->push_back(m_rel_block_pos + v3s16(x, y + 1, z));
				}
			}

			was_liquid = is_liquid;
			was_ignore = is_ignore;
		}
	}

	// Check the node below the current block
	MapBlock *below = lookupBlock(x, -1, z);
	if (below) {
		MapNode node = below->getNodeNoCheck(dx, MAP_BLOCKSIZE - 1, dz);
		const ContentFeatures &f = m_ndef->get(node);
		bool is_ignore = node.getContent() == CONTENT_IGNORE;
		bool is_liquid = f.isLiquid();

		if (is_ignore || was_ignore || is_liquid == was_liquid) {
			// Neither topmost node of liquid column nor topmost node below column
		} else if (is_liquid) {
			// This is the topmost node in the column and might want to flow away
			if (f.liquid_type == LIQUID_FLOWING ||
					isLiquidHorizontallyFlowable(x, -1, z)) {
				m_liquid_queue->push_back(m_rel_block_pos + v3s16(x, -1, z));
			}
		} else {
			// This is the topmost node below a liquid column
			if (!was_pushed && (f.floodable ||
					(!was_checked && isLiquidHorizontallyFlowable(x, 0, z)))) {
				// Activate the lowest node in the column which is one
				// node above this one
				m_liquid_queue->push_back(m_rel_block_pos + v3s16(x, 0, z));
			}
		}
	}
}
