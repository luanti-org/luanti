// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <array>

#include "voxelalgorithms.h"
#include "nodedef.h"
#include "mapblock.h"
#include "map.h"

namespace voxalgo
{

/*!
 * A direction.
 * 0=X+
 * 1=Y+
 * 2=Z+
 * 3=Z-
 * 4=Y-
 * 5=X-
 * 6=no direction
 * Two directions are opposite only if their sum is 5.
 */
typedef u8 direction;
/*!
 * Relative node position.
 * This represents a node's position in its map block.
 * All coordinates must be between 0 and 15.
 */
typedef v3s16 relative_v3;
/*!
 * Position of a map block (block coordinates).
 * One block_pos unit is as long as 16 node position units.
 */
typedef v3s16 mapblock_v3;

//! Contains information about a node whose light is about to change.
struct ChangingLight {
	//! Pointer to the node's block.
	MapBlock *block = nullptr;
	//! Relative position of the node in its map block.
	relative_v3 rel_position;
	//! Position of the node's block.
	mapblock_v3 block_position;
	/*!
	 * Direction from the node that caused this node's changing
	 * to this node.
	 */
	direction source_direction = 6;

	ChangingLight() = default;

	ChangingLight(relative_v3 rel_pos, mapblock_v3 block_pos,
		MapBlock *b, direction source_dir) :
		block(b),
		rel_position(rel_pos),
		block_position(block_pos),
		source_direction(source_dir)
	{}
};

/*!
 * A fast, priority queue-like container to contain ChangingLights.
 * The ChangingLights are ordered by the given light levels.
 * The brightest ChangingLight is returned first.
 */
struct LightQueue {
	//! For each light level there is a vector.
	std::array<std::vector<ChangingLight>, LIGHT_SUN + 1> lights;
	//! Light of the brightest ChangingLight in the queue.
	u8 max_light;

	/*!
	 * Creates a LightQueue.
	 * \param reserve for each light level that many slots are reserved.
	 */
	LightQueue(size_t reserve)
	{
		max_light = LIGHT_SUN;
		for (auto &l : lights)
			l.reserve(reserve);
	}

	//! Clears a LightQueue.
	void clear() {
		max_light = LIGHT_SUN;
		for (auto &l : lights)
			l.clear();
	}

	/*!
	 * Returns the next brightest ChangingLight and
	 * removes it from the queue.
	 * If there were no elements in the queue, the given parameters
	 * remain unmodified.
	 * \param light light level of the popped ChangingLight
	 * \param data the ChangingLight that was popped
	 * \returns true if there was a ChangingLight in the queue.
	 */
	bool next(u8 &light, ChangingLight &data)
	{
		while (lights[max_light].empty()) {
			if (max_light == 0) {
				return false;
			}
			max_light--;
		}
		light = max_light;
		data = lights[max_light].back();
		lights[max_light].pop_back();
		return true;
	}

	/*!
	 * Adds an element to the queue.
	 * The parameters are the same as in ChangingLight's constructor.
	 * \param light light level of the ChangingLight
	 */
	inline void push(u8 light, relative_v3 rel_pos,
		mapblock_v3 block_pos, MapBlock *block,
		direction source_dir)
	{
		assert(light <= LIGHT_SUN);
		lights[light].emplace_back(rel_pos, block_pos, block, source_dir);
	}
};

/*!
 * This type of light queue is for unlighting.
 * A node can be pushed in it only if its raw light is zero.
 * This prevents pushing nodes twice into this queue.
 * The light of the pushed ChangingLight must be the
 * light of the node before unlighting it.
 */
typedef LightQueue UnlightQueue;
/*!
 * This type of light queue is for spreading lights.
 * While spreading lights, all the nodes in it must
 * have the same light as the light level the ChangingLights
 * were pushed into this queue with. This prevents unnecessary
 * re-pushing of the nodes into the queue.
 * If a node doesn't let light trough but emits light, it can be added
 * too.
 */
typedef LightQueue ReLightQueue;

/*!
 * neighbor_dirs[i] points towards
 * the direction i.
 * See the definition of the type "direction"
 */
const static v3s16 neighbor_dirs[6] = {
	v3s16(1, 0, 0), // right
	v3s16(0, 1, 0), // top
	v3s16(0, 0, 1), // back
	v3s16(0, 0, -1), // front
	v3s16(0, -1, 0), // bottom
	v3s16(-1, 0, 0), // left
};

/*!
 * Transforms the given map block offset by one node towards
 * the specified direction.
 * \param dir the direction of the transformation
 * \param rel_pos the node's relative position in its map block
 * \param block_pos position of the node's block
 */
bool step_rel_block_pos(direction dir, relative_v3 &rel_pos,
	mapblock_v3 &block_pos)
{
	switch (dir) {
	case 0:
		if (rel_pos.X < MAP_BLOCKSIZE - 1) {
			rel_pos.X++;
		} else {
			rel_pos.X = 0;
			block_pos.X++;
			return true;
		}
		break;
	case 1:
		if (rel_pos.Y < MAP_BLOCKSIZE - 1) {
			rel_pos.Y++;
		} else {
			rel_pos.Y = 0;
			block_pos.Y++;
			return true;
		}
		break;
	case 2:
		if (rel_pos.Z < MAP_BLOCKSIZE - 1) {
			rel_pos.Z++;
		} else {
			rel_pos.Z = 0;
			block_pos.Z++;
			return true;
		}
		break;
	case 3:
		if (rel_pos.Z > 0) {
			rel_pos.Z--;
		} else {
			rel_pos.Z = MAP_BLOCKSIZE - 1;
			block_pos.Z--;
			return true;
		}
		break;
	case 4:
		if (rel_pos.Y > 0) {
			rel_pos.Y--;
		} else {
			rel_pos.Y = MAP_BLOCKSIZE - 1;
			block_pos.Y--;
			return true;
		}
		break;
	case 5:
		if (rel_pos.X > 0) {
			rel_pos.X--;
		} else {
			rel_pos.X = MAP_BLOCKSIZE - 1;
			block_pos.X--;
			return true;
		}
		break;
	}
	return false;
}

/*
 * Removes all light that is potentially emitted by the specified
 * light sources. These nodes will have zero light.
 * Returns all nodes whose light became zero but should be re-lighted.
 *
 * \param bank the light bank in which the procedure operates
 * \param from_nodes nodes whose light is removed
 * \param light_sources nodes that should be re-lighted
 * \param modified_blocks output, all modified map blocks are added to this
 */
void unspread_light(Map *map, const NodeDefManager *nodemgr, LightBank bank,
	UnlightQueue &from_nodes, ReLightQueue &light_sources,
	std::map<v3s16, MapBlock*> &modified_blocks)
{
	// Stores data popped from from_nodes
	u8 current_light;
	ChangingLight current;
	// Data of the current neighbor
	mapblock_v3 neighbor_block_pos;
	relative_v3 neighbor_rel_pos;
	// Direction of the brightest neighbor of the node
	direction source_dir;
	while (from_nodes.next(current_light, current)) {
		// For all nodes that need unlighting

		// There is no brightest neighbor
		source_dir = 6;
		// The current node
		const MapNode &node = current.block->getNodeNoCheck(current.rel_position);
		ContentLightingFlags f = nodemgr->getLightingFlags(node);
		// If the node emits light, it behaves like it had a
		// brighter neighbor.
		u8 brightest_neighbor_light = f.light_source + 1;
		for (direction i = 0; i < 6; i++) {
			//For each neighbor

			// The node that changed this node has already zero light
			// and it can't give light to this node
			if (current.source_direction + i == 5) {
				continue;
			}
			// Get the neighbor's position and block
			neighbor_rel_pos = current.rel_position;
			neighbor_block_pos = current.block_position;
			MapBlock *neighbor_block;
			if (step_rel_block_pos(i, neighbor_rel_pos, neighbor_block_pos)) {
				neighbor_block = map->getBlockNoCreateNoEx(neighbor_block_pos);
				if (neighbor_block == NULL) {
					current.block->setLightingComplete(bank, i, false);
					continue;
				}
			} else {
				neighbor_block = current.block;
			}
			// Get the neighbor itself
			MapNode neighbor = neighbor_block->getNodeNoCheck(neighbor_rel_pos);
			ContentLightingFlags neighbor_f = nodemgr->getLightingFlags(
				neighbor.getContent());
			u8 neighbor_light = neighbor.getLightRaw(bank, neighbor_f);
			// If the neighbor has at least as much light as this node, then
			// it won't lose its light, since it should have been added to
			// from_nodes earlier, so its light would be zero.
			if (neighbor_f.light_propagates && neighbor_light < current_light) {
				// Unlight, but only if the node has light.
				if (neighbor_light > 0) {
					neighbor.setLight(bank, 0, neighbor_f);
					neighbor_block->setNodeNoCheck(neighbor_rel_pos, neighbor);
					from_nodes.push(neighbor_light, neighbor_rel_pos,
						neighbor_block_pos, neighbor_block, i);
					// The current node was modified earlier, so its block
					// is in modified_blocks.
					if (current.block != neighbor_block) {
						modified_blocks[neighbor_block_pos] = neighbor_block;
					}
				}
			} else {
				// The neighbor can light up this node.
				if (neighbor_light < neighbor_f.light_source) {
					neighbor_light = neighbor_f.light_source;
				}
				if (brightest_neighbor_light < neighbor_light) {
					brightest_neighbor_light = neighbor_light;
					source_dir = i;
				}
			}
		}
		// If the brightest neighbor is able to light up this node,
		// then add this node to the output nodes.
		if (brightest_neighbor_light > 1 && f.light_propagates) {
			brightest_neighbor_light--;
			light_sources.push(brightest_neighbor_light, current.rel_position,
				current.block_position, current.block,
				(source_dir == 6) ? 6 : 5 - source_dir
				/* with opposite direction*/);
		}
	}
}

/*
 * Spreads light from the specified starting nodes.
 *
 * Before calling this procedure, make sure that all ChangingLights
 * in light_sources have as much light on the map as they have in
 * light_sources (if the queue contains a node multiple times, the brightest
 * occurrence counts).
 *
 * \param bank the light bank in which the procedure operates
 * \param light_sources starting nodes
 * \param modified_blocks output, all modified map blocks are added to this
 */
void spread_light(Map *map, const NodeDefManager *nodemgr, LightBank bank,
	LightQueue &light_sources,
	std::map<v3s16, MapBlock*> &modified_blocks)
{
	// The light the current node can provide to its neighbors.
	u8 spreading_light;
	// The ChangingLight for the current node.
	ChangingLight current;
	// Position of the current neighbor.
	mapblock_v3 neighbor_block_pos;
	relative_v3 neighbor_rel_pos;
	while (light_sources.next(spreading_light, current)) {
		spreading_light--;
		for (direction i = 0; i < 6; i++) {
			// This node can't light up its light source
			if (current.source_direction + i == 5) {
				continue;
			}
			// Get the neighbor's position and block
			neighbor_rel_pos = current.rel_position;
			neighbor_block_pos = current.block_position;
			MapBlock *neighbor_block;
			if (step_rel_block_pos(i, neighbor_rel_pos, neighbor_block_pos)) {
				neighbor_block = map->getBlockNoCreateNoEx(neighbor_block_pos);
				if (neighbor_block == NULL) {
					current.block->setLightingComplete(bank, i, false);
					continue;
				}
			} else {
				neighbor_block = current.block;
			}
			// Get the neighbor itself
			MapNode neighbor = neighbor_block->getNodeNoCheck(neighbor_rel_pos);
			ContentLightingFlags f = nodemgr->getLightingFlags(neighbor);
			if (f.light_propagates) {
				// Light up the neighbor, if it has less light than it should.
				u8 neighbor_light = neighbor.getLightRaw(bank, f);
				if (neighbor_light < spreading_light) {
					neighbor.setLight(bank, spreading_light, f);
					neighbor_block->setNodeNoCheck(neighbor_rel_pos, neighbor);
					light_sources.push(spreading_light, neighbor_rel_pos,
						neighbor_block_pos, neighbor_block, i);
					// The current node was modified earlier, so its block
					// is in modified_blocks.
					if (current.block != neighbor_block) {
						modified_blocks[neighbor_block_pos] = neighbor_block;
					}
				}
			}
		}
	}
}

struct SunlightPropagationUnit{
	v2s16 relative_pos;
	bool is_sunlit;

	SunlightPropagationUnit(v2s16 relpos, bool sunlit):
		relative_pos(relpos),
		is_sunlit(sunlit)
	{}
};

struct SunlightPropagationData{
	std::vector<SunlightPropagationUnit> data;
	v3s16 target_block;
};

/*!
 * Returns true if the node gets sunlight from the
 * node above it.
 *
 * \param pos position of the node.
 */
bool is_sunlight_above(Map *map, v3s16 pos, const NodeDefManager *ndef)
{
	bool sunlight = true;
	mapblock_v3 source_block_pos;
	relative_v3 source_rel_pos;
	getNodeBlockPosWithOffset(pos + v3s16(0, 1, 0), source_block_pos,
		source_rel_pos);
	// If the node above has sunlight, this node also can get it.
	MapBlock *source_block = map->getBlockNoCreateNoEx(source_block_pos);
	if (source_block == NULL) {
		// But if there is no node above, then use heuristics
		MapBlock *node_block = map->getBlockNoCreateNoEx(getNodeBlockPos(pos));
		if (node_block == NULL) {
			sunlight = false;
		} else {
			sunlight = !node_block->getIsUnderground();
		}
	} else {
		MapNode above = source_block->getNodeNoCheck(source_rel_pos);
		if (above.getContent() == CONTENT_IGNORE) {
			// Trust heuristics
			if (source_block->getIsUnderground()) {
				sunlight = false;
			}
		} else {
			ContentLightingFlags above_f = ndef->getLightingFlags(above);
			if (above.getLight(LIGHTBANK_DAY, above_f) != LIGHT_SUN) {
				// If the node above doesn't have sunlight, this
				// node is in shadow.
				sunlight = false;
			}
		}
	}
	return sunlight;
}

static constexpr LightBank banks[] = { LIGHTBANK_DAY, LIGHTBANK_NIGHT };

void update_lighting_nodes(Map *map,
	const std::vector<std::pair<v3s16, MapNode>> &oldnodes,
	std::map<v3s16, MapBlock*> &modified_blocks)
{
	const NodeDefManager *ndef = map->getNodeDefManager();
	// For node getter functions
	bool is_valid_position;

	// cached allocations
	thread_local UnlightQueue disappearing_lights(1);
	thread_local ReLightQueue light_sources(4);

	// Process each light bank separately
	for (LightBank bank : banks) {
		disappearing_lights.clear();
		light_sources.clear();
		// Nodes that are brighter than the brightest modified node was
		// won't change, since they didn't get their light from a
		// modified node.
		u8 min_safe_light = 0;
		for (auto it = oldnodes.cbegin(); it < oldnodes.cend(); ++it) {
			u8 old_light = it->second.getLight(bank, ndef->getLightingFlags(it->second));
			if (old_light > min_safe_light) {
				min_safe_light = old_light;
			}
		}
		// If only one node changed, even nodes with the same brightness
		// didn't get their light from the changed node.
		if (oldnodes.size() > 1) {
			min_safe_light++;
		}
		// For each changed node process sunlight and initialize
		for (auto it = oldnodes.cbegin(); it < oldnodes.cend(); ++it) {
			// Get position and block of the changed node
			v3s16 p = it->first;
			relative_v3 rel_pos;
			mapblock_v3 block_pos;
			getNodeBlockPosWithOffset(p, block_pos, rel_pos);
			MapBlock *block = map->getBlockNoCreateNoEx(block_pos);
			if (block == NULL) {
				continue;
			}
			// Get the new node
			MapNode n = block->getNodeNoCheck(rel_pos);

			// Light of the old node
			u8 old_light = it->second.getLight(bank, ndef->getLightingFlags(it->second));

			// Add the block of the added node to modified_blocks
			modified_blocks[block_pos] = block;

			// Get new light level of the node
			u8 new_light = 0;
			ContentLightingFlags f = ndef->getLightingFlags(n);
			if (f.light_propagates) {
				if (bank == LIGHTBANK_DAY && f.sunlight_propagates
					&& is_sunlight_above(map, p, ndef)) {
					new_light = LIGHT_SUN;
				} else {
					new_light = f.light_source;
					for (const v3s16 &neighbor_dir : neighbor_dirs) {
						v3s16 p2 = p + neighbor_dir;
						MapNode n2 = map->getNode(p2, &is_valid_position);
						if (is_valid_position) {
							u8 spread = n2.getLight(bank, ndef->getLightingFlags(n2));
							// If it is sure that the neighbor won't be
							// unlighted, its light can spread to this node.
							if (spread > new_light && spread >= min_safe_light) {
								new_light = spread - 1;
							}
						}
					}
				}
			} else {
				// If this is an opaque node, it still can emit light.
				new_light = f.light_source;
			}

			if (new_light > 0) {
				light_sources.push(new_light, rel_pos, block_pos, block, 6);
			}

			if (new_light < old_light) {
				// The node became opaque or doesn't provide as much
				// light as the previous one, so it must be unlighted.

				// Add to unlight queue
				n.setLight(bank, 0, f);
				block->setNodeNoCheck(rel_pos, n);
				disappearing_lights.push(old_light, rel_pos, block_pos, block,
					6);

				// Remove sunlight, if there was any
				if (bank == LIGHTBANK_DAY && old_light == LIGHT_SUN) {
					for (s16 y = p.Y - 1;; y--) {
						v3s16 n2pos(p.X, y, p.Z);

						MapNode n2;

						n2 = map->getNode(n2pos, &is_valid_position);
						if (!is_valid_position)
							break;

						// If this node doesn't have sunlight, the nodes below
						// it don't have too.
						ContentLightingFlags f2 = ndef->getLightingFlags(n2);
						if (n2.getLight(LIGHTBANK_DAY, f2) != LIGHT_SUN) {
							break;
						}
						// Remove sunlight and add to unlight queue.
						n2.setLight(LIGHTBANK_DAY, 0, f2);
						map->setNode(n2pos, n2);
						relative_v3 rel_pos2;
						mapblock_v3 block_pos2;
						getNodeBlockPosWithOffset(n2pos, block_pos2, rel_pos2);
						MapBlock *block2 = map->getBlockNoCreateNoEx(
							block_pos2);
						disappearing_lights.push(LIGHT_SUN, rel_pos2,
							block_pos2, block2,
							4 /* The node above caused the change */);
					}
				}
			} else if (new_light > old_light) {
				// It is sure that the node provides more light than the previous
				// one, unlighting is not necessary.
				// Propagate sunlight
				if (bank == LIGHTBANK_DAY && new_light == LIGHT_SUN) {
					for (s16 y = p.Y - 1;; y--) {
						v3s16 n2pos(p.X, y, p.Z);

						MapNode n2;

						n2 = map->getNode(n2pos, &is_valid_position);
						if (!is_valid_position)
							break;

						// This should not happen, but if the node has sunlight
						// then the iteration should stop.
						ContentLightingFlags f2 = ndef->getLightingFlags(n2);
						if (n2.getLight(LIGHTBANK_DAY, f2) == LIGHT_SUN) {
							break;
						}
						// If the node terminates sunlight, stop.
						if (!f2.sunlight_propagates) {
							break;
						}
						relative_v3 rel_pos2;
						mapblock_v3 block_pos2;
						getNodeBlockPosWithOffset(n2pos, block_pos2, rel_pos2);
						MapBlock *block2 = map->getBlockNoCreateNoEx(
							block_pos2);
						// Mark node for lighting.
						light_sources.push(LIGHT_SUN, rel_pos2, block_pos2,
							block2, 4);
					}
				}
			}

		}
		// Remove lights
		unspread_light(map, ndef, bank, disappearing_lights, light_sources,
			modified_blocks);
		// Initialize light values for light spreading.
		for (u8 i = 0; i <= LIGHT_SUN; i++) {
			const auto &lights = light_sources.lights[i];
			for (auto it = lights.begin(); it < lights.end(); ++it) {
				MapNode n = it->block->getNodeNoCheck(it->rel_position);
				n.setLight(bank, i, ndef->getLightingFlags(n));
				it->block->setNodeNoCheck(it->rel_position, n);
			}
		}
		// Spread lights.
		spread_light(map, ndef, bank, light_sources, modified_blocks);
	}
}

/*!
 * Borders of a map block in relative node coordinates.
 * Compatible with type 'direction'.
 */
#define B_1 (MAP_BLOCKSIZE - 1)
const static VoxelArea block_borders[] = {
	VoxelArea(v3s16(B_1, 0, 0), v3s16(B_1, B_1, B_1)), //X+
	VoxelArea(v3s16(0, B_1, 0), v3s16(B_1, B_1, B_1)), //Y+
	VoxelArea(v3s16(0, 0, B_1), v3s16(B_1, B_1, B_1)), //Z+
	VoxelArea(v3s16(0, 0, 0), v3s16(B_1, B_1, 0)),   //Z-
	VoxelArea(v3s16(0, 0, 0), v3s16(B_1, 0, B_1)),   //Y-
	VoxelArea(v3s16(0, 0, 0), v3s16(0, B_1, B_1))    //X-
};
#undef B_1

/*!
 * Returns true if:
 * -the node has unloaded neighbors
 * -the node doesn't have light
 * -the node's light is the same as the maximum of
 * its light source and its brightest neighbor minus one.
 * .
 */
bool is_light_locally_correct(Map *map, const NodeDefManager *ndef,
	LightBank bank, v3s16 pos)
{
	bool is_valid_position;
	MapNode n = map->getNode(pos, &is_valid_position);
	ContentLightingFlags f = ndef->getLightingFlags(n);
	if (!f.has_light) {
		return true;
	}
	u8 light = n.getLight(bank, f);
	assert(f.light_source <= LIGHT_MAX);
	u8 brightest_neighbor = f.light_source + 1;
	for (const v3s16 &neighbor_dir : neighbor_dirs) {
		MapNode n2 = map->getNode(pos + neighbor_dir,
			&is_valid_position);
		u8 light2 = n2.getLight(bank, ndef->getLightingFlags(n2));
		if (brightest_neighbor < light2) {
			brightest_neighbor = light2;
		}
	}
	assert(light <= LIGHT_SUN);
	return brightest_neighbor == light + 1;
}

void update_block_border_lighting(Map *map, MapBlock *block,
	std::map<v3s16, MapBlock*> &modified_blocks)
{
	const NodeDefManager *ndef = map->getNodeDefManager();
	// Since invalid light is not common, do not allocate
	// memory if not needed.
	UnlightQueue disappearing_lights(0);
	ReLightQueue light_sources(0);

	for (LightBank bank : banks) {
		disappearing_lights.clear();
		light_sources.clear();
		// Get incorrect lights
		for (direction d = 0; d < 6; d++) {
			// For each direction
			// Get neighbor block
			v3s16 otherpos = block->getPos() + neighbor_dirs[d];
			MapBlock *other = map->getBlockNoCreateNoEx(otherpos);
			if (other == NULL) {
				continue;
			}
			// Only update if lighting was not completed.
			if (block->isLightingComplete(bank, d) &&
					other->isLightingComplete(bank, 5 - d))
				continue;
			// Reset flags
			block->setLightingComplete(bank, d, true);
			other->setLightingComplete(bank, 5 - d, true);
			// The two blocks and their connecting surfaces
			MapBlock *blocks[] = {block, other};
			VoxelArea areas[] = {block_borders[d], block_borders[5 - d]};
			// For both blocks
			for (u8 blocknum = 0; blocknum < 2; blocknum++) {
				MapBlock *b = blocks[blocknum];
				VoxelArea a = areas[blocknum];
				// For all nodes
				for (s32 x = a.MinEdge.X; x <= a.MaxEdge.X; x++)
				for (s32 z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++)
				for (s32 y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++) {
					MapNode n = b->getNodeNoCheck(x, y, z);
					ContentLightingFlags f = ndef->getLightingFlags(n);
					u8 light = n.getLight(bank, f);
					// Sunlight is fixed
					if (light < LIGHT_SUN) {
						// Unlight if not correct
						if (!is_light_locally_correct(map, ndef, bank,
								v3s16(x, y, z) + b->getPosRelative())) {
							// Initialize for unlighting
							n.setLight(bank, 0, ndef->getLightingFlags(n));
							b->setNodeNoCheck(x, y, z, n);
							modified_blocks[b->getPos()]=b;
							disappearing_lights.push(light,
								relative_v3(x, y, z), b->getPos(), b,
								6);
						}
					}
				}
			}
		}
		// Remove lights
		unspread_light(map, ndef, bank, disappearing_lights, light_sources,
			modified_blocks);
		// Initialize light values for light spreading.
		for (u8 i = 0; i <= LIGHT_SUN; i++) {
			const auto &lights = light_sources.lights[i];
			for (auto it = lights.begin(); it < lights.end(); ++it) {
				MapNode n = it->block->getNodeNoCheck(it->rel_position);
				n.setLight(bank, i, ndef->getLightingFlags(n));
				it->block->setNodeNoCheck(it->rel_position, n);
			}
		}
		// Spread lights.
		spread_light(map, ndef, bank, light_sources, modified_blocks);
	}
}

/*!
 * Resets the lighting of the given VoxelManipulator to
 * complete darkness and full sunlight.
 * Operates in one map sector.
 *
 * \param offset contains the least x and z node coordinates
 * of the map sector.
 * \param light incoming sunlight, light[x][z] is true if there
 * is sunlight above the voxel manipulator at the given x-z coordinates.
 * The array's indices are relative node coordinates in the sector.
 * After the procedure returns, this contains outgoing light at
 * the bottom of the voxel manipulator.
 */
void fill_with_sunlight(MMVManip *vm, const NodeDefManager *ndef, v2s16 offset,
	bool light[MAP_BLOCKSIZE][MAP_BLOCKSIZE])
{
	// Distance in array between two nodes on top of each other.
	s32 ystride = vm->m_area.getExtent().X;
	// Cache the ignore node.
	MapNode ignore = MapNode(CONTENT_IGNORE);
	// For each column of nodes:
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
		// Position of the column on the map.
		v2s16 realpos = offset + v2s16(x, z);
		// Array indices in the voxel manipulator
		s32 maxindex = vm->m_area.index(realpos.X, vm->m_area.MaxEdge.Y,
			realpos.Y);
		s32 minindex = vm->m_area.index(realpos.X, vm->m_area.MinEdge.Y,
			realpos.Y);
		// True if the current node has sunlight.
		bool lig = light[z][x];
		// For each node, downwards:
		for (s32 i = maxindex; i >= minindex; i -= ystride) {
			MapNode *n;
			if (vm->m_flags[i] & VOXELFLAG_NO_DATA)
				n = &ignore;
			else
				n = &vm->m_data[i];
			// Ignore IGNORE nodes, these are not generated yet.
			if(n->getContent() == CONTENT_IGNORE)
				continue;
			ContentLightingFlags f = ndef->getLightingFlags(*n);
			if (lig && !f.sunlight_propagates)
				// Sunlight is stopped.
				lig = false;
			// Reset light
			n->setLight(LIGHTBANK_DAY, lig ? 15 : 0, f);
			n->setLight(LIGHTBANK_NIGHT, 0, f);
		}
		// Output outgoing light.
		light[z][x] = lig;
	}
}

/*!
 * Returns incoming sunlight for one map block.
 * If block above is not found, it is loaded.
 *
 * \param pos position of the map block that gets the sunlight.
 * \param light incoming sunlight, light[z][x] is true if there
 * is sunlight above the block at the given z-x relative
 * node coordinates.
 */
void is_sunlight_above_block(Map *map, mapblock_v3 pos,
	const NodeDefManager *ndef, bool light[MAP_BLOCKSIZE][MAP_BLOCKSIZE])
{
	mapblock_v3 source_block_pos = pos + v3s16(0, 1, 0);
	// Get or load source block.
	// It might take a while to load, but correcting incorrect
	// sunlight may be even slower.
	MapBlock *source_block = map->emergeBlock(source_block_pos, false);
	// Trust only generated blocks.
	if (source_block == NULL || !source_block->isGenerated()) {
		// But if there is no block above, then use heuristics
		bool sunlight = true;
		MapBlock *node_block = map->getBlockNoCreateNoEx(pos);
		if (node_block == NULL)
			// This should not happen.
			sunlight = false;
		else
			sunlight = !node_block->getIsUnderground();
		for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
		for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
			light[z][x] = sunlight;
	} else {
		// For each column:
		for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
		for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
			// Get the bottom block.
			MapNode above = source_block->getNodeNoCheck(x, 0, z);
			ContentLightingFlags above_f = ndef->getLightingFlags(above);
			light[z][x] = above.getLight(LIGHTBANK_DAY, above_f) == LIGHT_SUN;
		}
	}
}

/*!
 * Propagates sunlight down in a given map block.
 *
 * \param data contains incoming sunlight and shadow and
 * the coordinates of the target block.
 * \param unlight propagated shadow is inserted here
 * \param relight propagated sunlight is inserted here
 *
 * \returns true if the block was modified, false otherwise.
 */
bool propagate_block_sunlight(Map *map, const NodeDefManager *ndef,
	SunlightPropagationData *data, UnlightQueue *unlight, ReLightQueue *relight)
{
	bool modified = false;
	// Get the block.
	MapBlock *block = map->getBlockNoCreateNoEx(data->target_block);
	if (block == NULL) {
		// The work is done if the block does not contain data.
		data->data.clear();
		return false;
	}
	// For each changing column of nodes:
	size_t index;
	for (index = 0; index < data->data.size(); index++) {
		SunlightPropagationUnit it = data->data[index];
		// Relative position of the currently inspected node.
		relative_v3 current_pos(it.relative_pos.X, MAP_BLOCKSIZE - 1,
			it.relative_pos.Y);
		if (it.is_sunlit) {
			// Propagate sunlight.
			// For each node downwards:
			for (; current_pos.Y >= 0; current_pos.Y--) {
				MapNode n = block->getNodeNoCheck(current_pos);
				ContentLightingFlags f = ndef->getLightingFlags(n);
				if (n.getLightRaw(LIGHTBANK_DAY, f) < LIGHT_SUN
						&& f.sunlight_propagates) {
					// This node gets sunlight.
					n.setLight(LIGHTBANK_DAY, LIGHT_SUN, f);
					block->setNodeNoCheck(current_pos, n);
					modified = true;
					relight->push(LIGHT_SUN, current_pos, data->target_block,
						block, 4);
				} else {
					// Light already valid, propagation stopped.
					break;
				}
			}
		} else {
			// Propagate shadow.
			// For each node downwards:
			for (; current_pos.Y >= 0; current_pos.Y--) {
				MapNode n = block->getNodeNoCheck(current_pos);
				ContentLightingFlags f = ndef->getLightingFlags(n);
				if (n.getLightRaw(LIGHTBANK_DAY, f) == LIGHT_SUN) {
					// The sunlight is no longer valid.
					n.setLight(LIGHTBANK_DAY, 0, f);
					block->setNodeNoCheck(current_pos, n);
					modified = true;
					unlight->push(LIGHT_SUN, current_pos, data->target_block,
						block, 4);
				} else {
					// Reached shadow, propagation stopped.
					break;
				}
			}
		}
		if (current_pos.Y >= 0) {
			// Propagation stopped, remove from data.
			data->data[index] = data->data.back();
			data->data.pop_back();
			index--;
		}
	}
	return modified;
}

/*!
 * Borders of a map block in relative node coordinates.
 * The areas do not overlap.
 * Compatible with type 'direction'.
 */
#define B_1 (MAP_BLOCKSIZE - 1)
#define B_2 (MAP_BLOCKSIZE - 2)
const static VoxelArea block_pad[] = {
	VoxelArea({B_1, 0, 0}, {B_1, B_1, B_1}), //X+
	VoxelArea({1, B_1, 0}, {B_2, B_1, B_1}), //Y+
	VoxelArea({1, 1, B_1}, {B_2, B_2, B_1}), //Z+
	VoxelArea({1, 1, 0},   {B_2, B_2, 0}),   //Z-
	VoxelArea({1, 0, 0},   {B_2, 0, B_1}),   //Y-
	VoxelArea({0, 0, 0},   {0, B_1, B_1})    //X-
};
#undef B_1
#undef B_2

/*!
 * The common part of bulk light updates - it is always executed.
 * The procedure takes the nodes that should be unlit, and the
 * full modified area.
 *
 * The procedure handles the correction of all lighting except
 * direct sunlight spreading.
 *
 * \param minblock least coordinates of the changed area in block
 * coordinates
 * \param maxblock greatest coordinates of the changed area in block
 * coordinates
 * \param unlight the first queue is for day light, the second is for
 * night light. Contains all nodes on the borders that need to be unlit.
 * \param relight the first queue is for day light, the second is for
 * night light. Contains nodes that were not modified, but got sunlight
 * because the changes.
 * \param modified_blocks the procedure adds all modified blocks to
 * this map
 */
void finish_bulk_light_update(Map *map, mapblock_v3 minblock,
	mapblock_v3 maxblock, UnlightQueue unlight[2], ReLightQueue relight[2],
	std::map<v3s16, MapBlock*> *modified_blocks)
{
	const NodeDefManager *ndef = map->getNodeDefManager();

	// --- STEP 1: Do unlighting

	for (size_t bank = 0; bank < 2; bank++) {
		LightBank b = banks[bank];
		unspread_light(map, ndef, b, unlight[bank], relight[bank],
			*modified_blocks);
	}

	// --- STEP 2: Get all newly inserted light sources

	// For each block:
	v3s16 blockpos;
	v3s16 relpos;
	for (blockpos.X = minblock.X; blockpos.X <= maxblock.X; blockpos.X++)
	for (blockpos.Y = minblock.Y; blockpos.Y <= maxblock.Y; blockpos.Y++)
	for (blockpos.Z = minblock.Z; blockpos.Z <= maxblock.Z; blockpos.Z++) {
		MapBlock *block = map->getBlockNoCreateNoEx(blockpos);
		if (!block)
			// Skip not existing blocks
			continue;
		// For each node in the block:
		for (relpos.Z = 0; relpos.Z < MAP_BLOCKSIZE; relpos.Z++)
		for (relpos.X = 0; relpos.X < MAP_BLOCKSIZE; relpos.X++)
		for (relpos.Y = 0; relpos.Y < MAP_BLOCKSIZE; relpos.Y++) {
			MapNode node = block->getNodeNoCheck(relpos.X, relpos.Y, relpos.Z);
			ContentLightingFlags f = ndef->getLightingFlags(node);

			// For each light bank
			for (size_t b = 0; b < 2; b++) {
				LightBank bank = banks[b];
				u8 light = f.has_light ?
					node.getLight(bank, f):
					f.light_source;
				if (light > 1)
					relight[b].push(light, relpos, blockpos, block, 6);
			} // end of banks
		} // end of nodes
	} // end of blocks

	// --- STEP 3: do light spreading

	// For each light bank:
	for (size_t b = 0; b < 2; b++) {
		LightBank bank = banks[b];
		// Sunlight is already initialized.
		u8 maxlight = (b == 0) ? LIGHT_MAX : LIGHT_SUN;
		// Initialize light values for light spreading.
		for (u8 i = 0; i <= maxlight; i++) {
			const auto &lights = relight[b].lights[i];
			for (auto it = lights.begin(); it < lights.end(); ++it) {
				MapNode n = it->block->getNodeNoCheck(it->rel_position);
				n.setLight(bank, i, ndef->getLightingFlags(n));
				it->block->setNodeNoCheck(it->rel_position, n);
			}
		}
		// Spread lights.
		spread_light(map, ndef, bank, relight[b], *modified_blocks);
	}
}

void blit_back_with_light(Map *map, MMVManip *vm,
	std::map<v3s16, MapBlock*> *modified_blocks)
{
	const NodeDefManager *ndef = map->getNodeDefManager();

	if (vm->m_area.hasEmptyExtent())
		return;
	mapblock_v3 minblock = getNodeBlockPos(vm->m_area.MinEdge);
	mapblock_v3 maxblock = getNodeBlockPos(vm->m_area.MaxEdge);
	// First queue is for day light, second is for night light.
	UnlightQueue unlight[] = { UnlightQueue(256), UnlightQueue(256) };
	ReLightQueue relight[] = { ReLightQueue(256), ReLightQueue(256) };
	// Will hold sunlight data.
	bool lights[MAP_BLOCKSIZE][MAP_BLOCKSIZE];
	SunlightPropagationData data;

	// --- STEP 1: reset everything to sunlight

	// For each map block:
	for (s16 x = minblock.X; x <= maxblock.X; x++)
	for (s16 z = minblock.Z; z <= maxblock.Z; z++) {
		// Extract sunlight above.
		is_sunlight_above_block(map, v3s16(x, maxblock.Y, z), ndef, lights);
		v2s16 offset(x, z);
		offset *= MAP_BLOCKSIZE;
		// Reset the voxel manipulator.
		fill_with_sunlight(vm, ndef, offset, lights);
		// Copy sunlight data
		data.target_block = v3s16(x, minblock.Y - 1, z);
		for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
		for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
			data.data.emplace_back(v2s16(x, z), lights[z][x]);
		// Propagate sunlight and shadow below the voxel manipulator.
		while (!data.data.empty()) {
			if (propagate_block_sunlight(map, ndef, &data, &unlight[0],
					&relight[0]))
				(*modified_blocks)[data.target_block] =
					map->getBlockNoCreateNoEx(data.target_block);
			// Step downwards.
			data.target_block.Y--;
		}
	}

	// --- STEP 2: Get nodes from borders to unlight
	v3s16 blockpos;
	v3s16 relpos;

	// In case there are unloaded holes in the voxel manipulator
	// unlight each block.
	// For each block:
	for (blockpos.X = minblock.X; blockpos.X <= maxblock.X; blockpos.X++)
	for (blockpos.Y = minblock.Y; blockpos.Y <= maxblock.Y; blockpos.Y++)
	for (blockpos.Z = minblock.Z; blockpos.Z <= maxblock.Z; blockpos.Z++) {
		MapBlock *block = map->getBlockNoCreateNoEx(blockpos);
		if (!block)
			// Skip not existing blocks.
			continue;
		v3s16 offset = block->getPosRelative();
		// For each border of the block:
		for (const VoxelArea &a : block_pad) {
			// For each node of the border:
			for (relpos.Z = a.MinEdge.Z; relpos.Z <= a.MaxEdge.Z; relpos.Z++)
			for (relpos.X = a.MinEdge.X; relpos.X <= a.MaxEdge.X; relpos.X++)
			for (relpos.Y = a.MinEdge.Y; relpos.Y <= a.MaxEdge.Y; relpos.Y++) {

				// Get old and new node
				MapNode oldnode = block->getNodeNoCheck(relpos);
				ContentLightingFlags oldf = ndef->getLightingFlags(oldnode);
				MapNode newnode = vm->getNodeNoExNoEmerge(relpos + offset);
				ContentLightingFlags newf = ndef->getLightingFlags(newnode);

				// For each light bank
				for (size_t b = 0; b < 2; b++) {
					LightBank bank = banks[b];
					u8 oldlight = oldf.has_light ?
						oldnode.getLight(bank, oldf):
						LIGHT_SUN; // no light information, force unlighting
					u8 newlight = newf.has_light ?
						newnode.getLight(bank, newf):
						newf.light_source;
					// If the new node is dimmer, unlight.
					if (oldlight > newlight) {
						unlight[b].push(
							oldlight, relpos, blockpos, block, 6);
					}
				} // end of banks
			} // end of nodes
		} // end of borders
	} // end of blocks

	// --- STEP 3: All information extracted, overwrite

	vm->blitBackAll(modified_blocks, true);

	// --- STEP 4: Finish light update

	finish_bulk_light_update(map, minblock, maxblock, unlight, relight,
		modified_blocks);
}

/*!
 * Resets the lighting of the given map block to
 * complete darkness and full sunlight.
 *
 * \param light incoming sunlight, light[x][z] is true if there
 * is sunlight above the map block at the given x-z coordinates.
 * The array's indices are relative node coordinates in the block.
 * After the procedure returns, this contains outgoing light at
 * the bottom of the map block.
 */
void fill_with_sunlight(MapBlock *block, const NodeDefManager *ndef,
	bool light[MAP_BLOCKSIZE][MAP_BLOCKSIZE])
{
	// For each column of nodes:
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
		// True if the current node has sunlight.
		bool lig = light[z][x];
		// For each node, downwards:
		for (s16 y = MAP_BLOCKSIZE - 1; y >= 0; y--) {
			MapNode n = block->getNodeNoCheck(x, y, z);
			// Ignore IGNORE nodes, these are not generated yet.
			if (n.getContent() == CONTENT_IGNORE)
				continue;
			ContentLightingFlags f = ndef->getLightingFlags(n);
			if (lig && !f.sunlight_propagates) {
				// Sunlight is stopped.
				lig = false;
			}
			// Reset light
			n.setLight(LIGHTBANK_DAY, lig ? 15 : 0, f);
			n.setLight(LIGHTBANK_NIGHT, 0, f);
			block->setNodeNoCheck(x, y, z, n);
		}
		// Output outgoing light.
		light[z][x] = lig;
	}
}

void repair_block_light(Map *map, MapBlock *block,
	std::map<v3s16, MapBlock*> *modified_blocks)
{
	if (!block)
		return;
	const NodeDefManager *ndef = map->getNodeDefManager();
	// First queue is for day light, second is for night light.
	UnlightQueue unlight[] = { UnlightQueue(256), UnlightQueue(256) };
	ReLightQueue relight[] = { ReLightQueue(256), ReLightQueue(256) };
	// Will hold sunlight data.
	bool lights[MAP_BLOCKSIZE][MAP_BLOCKSIZE];
	SunlightPropagationData data;

	// --- STEP 1: reset everything to sunlight

	mapblock_v3 blockpos = block->getPos();
	(*modified_blocks)[blockpos] = block;
	// For each map block:
	// Extract sunlight above.
	is_sunlight_above_block(map, blockpos, ndef, lights);
	// Reset the voxel manipulator.
	fill_with_sunlight(block, ndef, lights);
	// Copy sunlight data
	data.target_block = v3s16(blockpos.X, blockpos.Y - 1, blockpos.Z);
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++)
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++) {
		data.data.emplace_back(v2s16(x, z), lights[z][x]);
	}
	// Propagate sunlight and shadow below the voxel manipulator.
	while (!data.data.empty()) {
		if (propagate_block_sunlight(map, ndef, &data, &unlight[0],
				&relight[0]))
			(*modified_blocks)[data.target_block] =
				map->getBlockNoCreateNoEx(data.target_block);
		// Step downwards.
		data.target_block.Y--;
	}

	// --- STEP 2: Get nodes from borders to unlight

	// For each border of the block:
	for (const VoxelArea &a : block_pad) {
		v3s16 relpos;
		// For each node of the border:
		for (relpos.X = a.MinEdge.X; relpos.X <= a.MaxEdge.X; relpos.X++)
		for (relpos.Z = a.MinEdge.Z; relpos.Z <= a.MaxEdge.Z; relpos.Z++)
		for (relpos.Y = a.MinEdge.Y; relpos.Y <= a.MaxEdge.Y; relpos.Y++) {

			// Get node
			MapNode node = block->getNodeNoCheck(relpos);
			ContentLightingFlags f = ndef->getLightingFlags(node);
			// For each light bank
			for (size_t b = 0; b < 2; b++) {
				LightBank bank = banks[b];
				u8 light = f.has_light ?
					node.getLight(bank, f):
					f.light_source;
				// If the new node is dimmer than sunlight, unlight.
				// (if it has maximal light, it is pointless to remove
				// surrounding light, as it can only become brighter)
				if (LIGHT_SUN > light) {
					unlight[b].push(
						LIGHT_SUN, relpos, blockpos, block, 6);
				}
			} // end of banks
		} // end of nodes
	} // end of borders

	// STEP 3: Remove and spread light

	finish_bulk_light_update(map, blockpos, blockpos, unlight, relight,
		modified_blocks);
}

VoxelLineIterator::VoxelLineIterator(const v3f &start_position, const v3f &line_vector) :
	m_start_position(start_position),
	m_line_vector(line_vector)
{
	m_current_node_pos = floatToInt(m_start_position, 1);
	m_start_node_pos = m_current_node_pos;
	m_last_index = getIndex(floatToInt(start_position + line_vector, 1));

	if (m_line_vector.X > 0) {
		m_next_intersection_multi.X = (m_current_node_pos.X + 0.5f
			- m_start_position.X) / m_line_vector.X;
		m_intersection_multi_inc.X = 1 / m_line_vector.X;
	} else if (m_line_vector.X < 0) {
		m_next_intersection_multi.X = (m_current_node_pos.X - 0.5f
			- m_start_position.X) / m_line_vector.X;
		m_intersection_multi_inc.X = -1 / m_line_vector.X;
		m_step_directions.X = -1;
	}

	if (m_line_vector.Y > 0) {
		m_next_intersection_multi.Y = (m_current_node_pos.Y + 0.5f
			- m_start_position.Y) / m_line_vector.Y;
		m_intersection_multi_inc.Y = 1 / m_line_vector.Y;
	} else if (m_line_vector.Y < 0) {
		m_next_intersection_multi.Y = (m_current_node_pos.Y - 0.5f
			- m_start_position.Y) / m_line_vector.Y;
		m_intersection_multi_inc.Y = -1 / m_line_vector.Y;
		m_step_directions.Y = -1;
	}

	if (m_line_vector.Z > 0) {
		m_next_intersection_multi.Z = (m_current_node_pos.Z + 0.5f
			- m_start_position.Z) / m_line_vector.Z;
		m_intersection_multi_inc.Z = 1 / m_line_vector.Z;
	} else if (m_line_vector.Z < 0) {
		m_next_intersection_multi.Z = (m_current_node_pos.Z - 0.5f
			- m_start_position.Z) / m_line_vector.Z;
		m_intersection_multi_inc.Z = -1 / m_line_vector.Z;
		m_step_directions.Z = -1;
	}
}

void VoxelLineIterator::next()
{
	m_current_index++;
	if ((m_next_intersection_multi.X < m_next_intersection_multi.Y)
			&& (m_next_intersection_multi.X < m_next_intersection_multi.Z)) {
		m_next_intersection_multi.X += m_intersection_multi_inc.X;
		m_current_node_pos.X += m_step_directions.X;
	} else if ((m_next_intersection_multi.Y < m_next_intersection_multi.Z)) {
		m_next_intersection_multi.Y += m_intersection_multi_inc.Y;
		m_current_node_pos.Y += m_step_directions.Y;
	} else {
		m_next_intersection_multi.Z += m_intersection_multi_inc.Z;
		m_current_node_pos.Z += m_step_directions.Z;
	}
}

s16 VoxelLineIterator::getIndex(v3s16 voxel){
	return
		abs(voxel.X - m_start_node_pos.X) +
		abs(voxel.Y - m_start_node_pos.Y) +
		abs(voxel.Z - m_start_node_pos.Z);
}

} // namespace voxalgo

