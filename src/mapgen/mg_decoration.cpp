// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2015-2018 paramat

#include "mg_decoration.h"
#include "mg_schematic.h"
#include "mapgen.h"
#include "noise.h"
#include "map.h"
#include "log.h"
#include "util/numeric.h"
#include <algorithm>
#include <vector>
#include "mapgen/treegen.h"


const FlagDesc flagdesc_deco[] = {
	{"place_center_x",  DECO_PLACE_CENTER_X},
	{"place_center_y",  DECO_PLACE_CENTER_Y},
	{"place_center_z",  DECO_PLACE_CENTER_Z},
	{"force_placement", DECO_FORCE_PLACEMENT},
	{"liquid_surface",  DECO_LIQUID_SURFACE},
	{"all_floors",      DECO_ALL_FLOORS},
	{"all_ceilings",    DECO_ALL_CEILINGS},
	{NULL,              0}
};


///////////////////////////////////////////////////////////////////////////////


DecorationManager::DecorationManager(IGameDef *gamedef) :
	ObjDefManager(gamedef, OBJDEF_DECORATION)
{
}


void DecorationManager::placeAllDecos(Mapgen *mg, u32 blockseed,
	v3s16 nmin, v3s16 nmax)
{
	for (size_t i = 0; i != m_objects.size(); i++) {
		Decoration *deco = (Decoration *)m_objects[i];
		if (!deco)
			continue;

		deco->placeDeco(mg, blockseed, nmin, nmax);
		blockseed++;
	}
}

DecorationManager *DecorationManager::clone() const
{
	auto mgr = new DecorationManager();
	ObjDefManager::cloneTo(mgr);
	return mgr;
}


///////////////////////////////////////////////////////////////////////////////


void Decoration::resolveNodeNames()
{
	getIdsFromNrBacklog(&c_place_on);
	getIdsFromNrBacklog(&c_spawnby);
}


bool Decoration::canPlaceDecoration(MMVManip *vm, v3s16 p)
{
	// Note that `p` refers to the node the decoration will be placed ontop of,
	// not to the decoration itself.

	// Check if the decoration can be placed on this node
	u32 vi = vm->m_area.index(p);
	if (!CONTAINS(c_place_on, vm->m_data[vi].getContent()))
		return false;

	// Don't continue if there are no spawnby constraints
	if (nspawnby == -1)
		return true;

	int nneighs = 0;
	static const v3s16 dirs[8] = {
		v3s16( 0, 1,  1),
		v3s16( 0, 1, -1),
		v3s16( 1, 1,  0),
		v3s16(-1, 1,  0),
		v3s16( 1, 1,  1),
		v3s16(-1, 1,  1),
		v3s16(-1, 1, -1),
		v3s16( 1, 1, -1)
	};


	// Check these 16 neighboring nodes for enough spawnby nodes
	for (size_t i = 0; i != ARRLEN(dirs); i++) {
		u32 index = vm->m_area.index(p + dirs[i]);
		if (!vm->m_area.contains(index))
			continue;

		if (CONTAINS(c_spawnby, vm->m_data[index].getContent()))
			nneighs++;
	}

	if (check_offset != 0) {
		const v3s16 dir_offset(0, check_offset,  0);

		for (size_t i = 0; i != ARRLEN(dirs); i++) {
			u32 index = vm->m_area.index(p + dirs[i] + dir_offset);
			if (!vm->m_area.contains(index))
				continue;

			if (CONTAINS(c_spawnby, vm->m_data[index].getContent()))
				nneighs++;
		}

	}
	if (nneighs < nspawnby)
		return false;

	return true;
}


void Decoration::placeDeco(Mapgen *mg, u32 blockseed, v3s16 nmin, v3s16 nmax)
{
	// Skip if y ranges do not overlap
	if (nmax.Y < y_min || y_max < nmin.Y)
		return;

	PcgRandom ps(blockseed + 53);
	int carea_size = nmax.X - nmin.X + 1;
	if (nmax.Z - nmin.Z + 1 != carea_size) {
		// TODO: this is a stupid restriction, which we should lift
		throw BaseException("Decoration::placeDeco requires a square area (XZ)");
	}

	// Divide area into parts
	// If chunksize is changed it may no longer be divisable by sidelen
	if (carea_size % sidelen != 0)
		sidelen = carea_size;

	int area = sidelen * sidelen;

	for (s16 z0 = 0; z0 < carea_size; z0 += sidelen)
	for (s16 x0 = 0; x0 < carea_size; x0 += sidelen) {
		v2s16 p2d_min(nmin.X + x0, nmin.Z + z0);
		v2s16 p2d_max(nmin.X + x0 + sidelen - 1, nmin.Z + z0 + sidelen - 1);

		bool cover = false;
		// Amount of decorations
		float nval = (flags & DECO_USE_NOISE) ?
			NoiseFractal2D(&np, p2d_min.X + sidelen / 2, p2d_min.Y + sidelen / 2, mapseed) :
			fill_ratio;
		u32 deco_count = 0;

		if (nval >= 10.0f) {
			// Complete coverage. Disable random placement to avoid
			// redundant multiple placements at one position.
			cover = true;
			deco_count = area;
		} else {
			float deco_count_f = (float)area * nval;
			if (deco_count_f >= 1.0f) {
				deco_count = deco_count_f;
			} else if (deco_count_f > 0.0f) {
				// For very low density calculate a chance for 1 decoration
				if (ps.next() <= deco_count_f * static_cast<float>(PcgRandom::RANDOM_RANGE))
					deco_count = 1;
			}
		}

		s16 x = p2d_min.X - 1;
		s16 z = p2d_min.Y;

		for (u32 i = 0; i < deco_count; i++) {
			if (!cover) {
				x = ps.range(p2d_min.X, p2d_max.X);
				z = ps.range(p2d_min.Y, p2d_max.Y);
			} else {
				x++;
				if (x == p2d_max.X + 1) {
					z++;
					x = p2d_min.X;
				}
			}
			int mapindex = carea_size * (z - nmin.Z) + (x - nmin.X);

			if ((flags & DECO_ALL_FLOORS) ||
					(flags & DECO_ALL_CEILINGS)) {
				// All-surfaces decorations
				// Check biome of column
				if (mg->biomemap && !biomes.empty()) {
					auto iter = biomes.find(mg->biomemap[mapindex]);
					if (iter == biomes.end())
						continue;
				}

				// Get all floors and ceilings in node column
				u16 size = (nmax.Y - nmin.Y + 1) / 2;
				std::vector<s16> floors;
				std::vector<s16> ceilings;
				floors.reserve(size);
				ceilings.reserve(size);

				mg->getSurfaces(v2s16(x, z), nmin.Y, nmax.Y, floors, ceilings);

				if (flags & DECO_ALL_FLOORS) {
					// Floor decorations
					for (const s16 y : floors) {
						if (y < y_min || y > y_max)
							continue;

						v3s16 pos(x, y, z);
						if (generate(mg->vm, &ps, pos, false))
							mg->gennotify.addDecorationEvent(pos, index);
					}
				}

				if (flags & DECO_ALL_CEILINGS) {
					// Ceiling decorations
					for (const s16 y : ceilings) {
						if (y < y_min || y > y_max)
							continue;

						v3s16 pos(x, y, z);
						if (generate(mg->vm, &ps, pos, true))
							mg->gennotify.addDecorationEvent(pos, index);
					}
				}
			} else { // Heightmap decorations
				s16 y = -MAX_MAP_GENERATION_LIMIT;
				if (flags & DECO_LIQUID_SURFACE)
					y = mg->findLiquidSurface(v2s16(x, z), nmin.Y, nmax.Y);
				else if (mg->heightmap)
					y = mg->heightmap[mapindex];
				else
					y = mg->findGroundLevel(v2s16(x, z), nmin.Y, nmax.Y);

				if (y < y_min || y > y_max || y < nmin.Y || y > nmax.Y)
					continue;

				if (mg->biomemap && !biomes.empty()) {
					auto iter = biomes.find(mg->biomemap[mapindex]);
					if (iter == biomes.end())
						continue;
				}

				v3s16 pos(x, y, z);
				if (generate(mg->vm, &ps, pos, false))
					mg->gennotify.addDecorationEvent(pos, index);
			}
		}
	}

	return;
}


void Decoration::cloneTo(Decoration *def) const
{
	ObjDef::cloneTo(def);
	def->flags = flags;
	def->mapseed = mapseed;
	def->c_place_on = c_place_on;
	def->check_offset = check_offset;
	def->sidelen = sidelen;
	def->y_min = y_min;
	def->y_max = y_max;
	def->fill_ratio = fill_ratio;
	def->np = np;
	def->c_spawnby = c_spawnby;
	def->nspawnby = nspawnby;
	def->place_offset_y = place_offset_y;
	def->biomes = biomes;
}


///////////////////////////////////////////////////////////////////////////////


ObjDef *DecoSimple::clone() const
{
	auto def = new DecoSimple();
	Decoration::cloneTo(def);

	def->c_decos = c_decos;
	def->deco_height = deco_height;
	def->deco_height_max = deco_height_max;
	def->deco_param2 = deco_param2;
	def->deco_param2_max = deco_param2_max;

	return def;
}


void DecoSimple::resolveNodeNames()
{
	Decoration::resolveNodeNames();
	getIdsFromNrBacklog(&c_decos);
}


size_t DecoSimple::generate(MMVManip *vm, PcgRandom *pr, v3s16 p, bool ceiling)
{
	// Don't bother if there aren't any decorations to place
	if (c_decos.empty())
		return 0;

	if (!canPlaceDecoration(vm, p))
		return 0;

	// Check for placement outside the voxelmanip volume
	if (ceiling) {
		// Ceiling decorations
		// 'place offset y' is inverted
		if (p.Y - place_offset_y - std::max(deco_height, deco_height_max) <
				vm->m_area.MinEdge.Y)
			return 0;

		if (p.Y - 1 - place_offset_y > vm->m_area.MaxEdge.Y)
			return 0;

	} else { // Heightmap and floor decorations
		if (p.Y + place_offset_y + std::max(deco_height, deco_height_max) >
				vm->m_area.MaxEdge.Y)
			return 0;

		if (p.Y + 1 + place_offset_y < vm->m_area.MinEdge.Y)
			return 0;
	}

	content_t c_place = c_decos[pr->range(0, c_decos.size() - 1)];
	s16 height = (deco_height_max > 0) ?
		pr->range(deco_height, deco_height_max) : deco_height;
	u8 param2 = (deco_param2_max > 0) ?
		pr->range(deco_param2, deco_param2_max) : deco_param2;
	bool force_placement = (flags & DECO_FORCE_PLACEMENT);

	const v3s32 &em = vm->m_area.getExtent();
	u32 vi = vm->m_area.index(p);

	if (ceiling) {
		// Ceiling decorations
		// 'place offset y' is inverted
		VoxelArea::add_y(em, vi, -place_offset_y);

		for (int i = 0; i < height; i++) {
			VoxelArea::add_y(em, vi, -1);
			content_t c = vm->m_data[vi].getContent();
			if (c != CONTENT_AIR && c != CONTENT_IGNORE && !force_placement)
				break;

			vm->m_data[vi] = MapNode(c_place, 0, param2);
		}
	} else { // Heightmap and floor decorations
		VoxelArea::add_y(em, vi, place_offset_y);

		for (int i = 0; i < height; i++) {
			VoxelArea::add_y(em, vi, 1);
			content_t c = vm->m_data[vi].getContent();
			if (c != CONTENT_AIR && c != CONTENT_IGNORE && !force_placement)
				break;

			vm->m_data[vi] = MapNode(c_place, 0, param2);
		}
	}

	return 1;
}


///////////////////////////////////////////////////////////////////////////////


DecoSchematic::~DecoSchematic()
{
	if (was_cloned)
		delete schematic;
}


ObjDef *DecoSchematic::clone() const
{
	auto def = new DecoSchematic();
	Decoration::cloneTo(def);
	NodeResolver::cloneTo(def);

	def->rotation = rotation;
	/* FIXME: We do not own this schematic, yet we only have a pointer to it
	 * and not a handle. We are left with no option but to clone it ourselves.
	 * This is a waste of memory and should be replaced with an alternative
	 * approach sometime. */
	def->schematic = dynamic_cast<Schematic*>(schematic->clone());
	def->was_cloned = true;

	return def;
}


size_t DecoSchematic::generate(MMVManip *vm, PcgRandom *pr, v3s16 p, bool ceiling)
{
	// Schematic could have been unloaded but not the decoration
	// In this case generate() does nothing (but doesn't *fail*)
	if (schematic == NULL)
		return 0;

	if (!canPlaceDecoration(vm, p))
		return 0;

	if (flags & DECO_PLACE_CENTER_Y) {
		p.Y -= (schematic->size.Y - 1) / 2;
	} else {
		// Only apply 'place offset y' if not 'deco place center y'
		if (ceiling)
			// Shift down so that schematic top layer is level with ceiling
			// 'place offset y' is inverted
			p.Y -= (place_offset_y + schematic->size.Y - 1);
		else
			p.Y += place_offset_y;
	}

	// Check schematic top and base are in voxelmanip
	if (p.Y + schematic->size.Y - 1 > vm->m_area.MaxEdge.Y)
		return 0;

	if (p.Y < vm->m_area.MinEdge.Y)
		return 0;

	Rotation rot = (rotation == ROTATE_RAND) ?
		(Rotation)pr->range(ROTATE_0, ROTATE_270) : rotation;

	if (flags & DECO_PLACE_CENTER_X) {
		if (rot == ROTATE_0 || rot == ROTATE_180)
			p.X -= (schematic->size.X - 1) / 2;
		else
			p.Z -= (schematic->size.X - 1) / 2;
	}
	if (flags & DECO_PLACE_CENTER_Z) {
		if (rot == ROTATE_0 || rot == ROTATE_180)
			p.Z -= (schematic->size.Z - 1) / 2;
		else
			p.X -= (schematic->size.Z - 1) / 2;
	}

	bool force_placement = (flags & DECO_FORCE_PLACEMENT);

	schematic->blitToVManip(vm, p, rot, force_placement);

	return 1;
}

///////////////////////////////////////////////////////////////////////////////
ObjDef *DecoLSystem::clone() const
{
	auto def = new DecoLSystem();
	Decoration::cloneTo(def);

	def->tree_def = tree_def;
	return def;
}


size_t DecoLSystem::generate(MMVManip *vm, PcgRandom *pr, v3s16 p, bool ceiling)
{
	if (!canPlaceDecoration(vm, p))
		return 0;

	// Make sure that tree_def can't be modified, since it is shared.
	const auto &ref = *tree_def;
	return treegen::make_ltree(*vm, p, ref);
}
