// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2016 MillersMan <millersman@users.noreply.github.com>

#pragma once

#include "util/container.h"
#include "irrlichttypes.h"
#include "irr_v3d.h"

class NodeDefManager;
class Map;
class MapBlock;

class ReflowScan {
public:
	ReflowScan(Map *map, const NodeDefManager *ndef);
	void scan(MapBlock *block, UniqueQueue<v3s16> *liquid_queue);

private:
	MapBlock *lookupBlock(int x, int y, int z);
	bool isLiquidFlowableTo(int x, int y, int z);
	bool isLiquidHorizontallyFlowable(int x, int y, int z);
	void scanColumn(int x, int z);

private:
	Map *m_map = nullptr;
	const NodeDefManager *m_ndef = nullptr;
	v3s16 m_block_pos, m_rel_block_pos;
	UniqueQueue<v3s16> *m_liquid_queue = nullptr;
	MapBlock *m_lookup[3 * 3 * 3];
	u32 m_lookup_state_bitset;
};
