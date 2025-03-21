// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2020 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2015-2020 paramat
// Copyright (C) 2010-2016 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#include "util/numeric.h"
#include <cmath>
#include "map.h"
#include "mapgen.h"
#include "mapgen_v5.h"
#include "mapgen_v6.h"
#include "mapgen_v7.h"
#include "mg_biome.h"
#include "cavegen.h"

// TODO Remove this. Cave liquids are now defined and located using biome definitions
static NoiseParams nparams_caveliquids(0, 1, v3f(150.0, 150.0, 150.0), 776, 3, 0.6, 2.0);


////
//// CavesNoiseIntersection
////

CavesNoiseIntersection::CavesNoiseIntersection(
	const NodeDefManager *nodedef, BiomeManager *biomemgr, BiomeGen *biomegen, v3s16 chunksize,
	NoiseParams *np_cave1, NoiseParams *np_cave2, s32 seed, float cave_width)
{
	assert(nodedef);
	assert(biomemgr);
	assert(biomegen);

	m_ndef = nodedef;
	m_bmgr = biomemgr;
	m_bmgn = biomegen;

	m_csize = chunksize;
	m_cave_width = cave_width;

	m_ystride    = m_csize.X;
	m_zstride_1d = m_csize.X * (m_csize.Y + 1);

	// Noises are created using 1-down overgeneration
	// A Nx-by-1-by-Nz-sized plane is at the bottom of the desired for
	// re-carving the solid overtop placed for blocking sunlight
	noise_cave1 = new Noise(np_cave1, seed, m_csize.X, m_csize.Y + 1, m_csize.Z);
	noise_cave2 = new Noise(np_cave2, seed, m_csize.X, m_csize.Y + 1, m_csize.Z);
}


CavesNoiseIntersection::~CavesNoiseIntersection()
{
	delete noise_cave1;
	delete noise_cave2;
}


void CavesNoiseIntersection::generateCaves(MMVManip *vm,
	v3s16 nmin, v3s16 nmax, biome_t *biomemap)
{
	assert(vm);
	assert(biomemap);

	noise_cave1->noiseMap3D(nmin.X, nmin.Y - 1, nmin.Z);
	noise_cave2->noiseMap3D(nmin.X, nmin.Y - 1, nmin.Z);

	const v3s32 &em = vm->m_area.getExtent();
	u32 index2d = 0;  // Biomemap index

	for (s16 z = nmin.Z; z <= nmax.Z; z++)
	for (s16 x = nmin.X; x <= nmax.X; x++, index2d++) {
		bool column_is_open = false;  // Is column open to overground
		bool is_under_river = false;  // Is column under river water
		bool is_under_tunnel = false;  // Is tunnel or is under tunnel
		bool is_top_filler_above = false;  // Is top or filler above node
		// Indexes at column top
		u32 vi = vm->m_area.index(x, nmax.Y, z);
		u32 index3d = (z - nmin.Z) * m_zstride_1d + m_csize.Y * m_ystride +
			(x - nmin.X);  // 3D noise index
		// Biome of column
		Biome *biome = (Biome *)m_bmgr->getRaw(biomemap[index2d]);
		u16 depth_top = biome->depth_top;
		u16 base_filler = depth_top + biome->depth_filler;
		u16 depth_riverbed = biome->depth_riverbed;
		u16 nplaced = 0;

		s16 biome_y_next = m_bmgn->getNextTransitionY(nmax.Y);

		// Don't excavate the overgenerated stone at nmax.Y + 1,
		// this creates a 'roof' over the tunnel, preventing light in
		// tunnels at mapchunk borders when generating mapchunks upwards.
		// This 'roof' is removed when the mapchunk above is generated.
		for (s16 y = nmax.Y; y >= nmin.Y - 1; y--,
				index3d -= m_ystride,
				VoxelArea::add_y(em, vi, -1)) {
			// We need this check to make sure that biomes don't generate too far down
			if (y <= biome_y_next) {
				biome = m_bmgn->getBiomeAtIndex(index2d, v3s16(x, y, z));
				biome_y_next = m_bmgn->getNextTransitionY(y);

				if (x == nmin.X && z == nmin.Z && false) {
					dstream << "cavegen: biome at " << y << " is " << biome->name
						<< ", next at " << biome_y_next << std::endl;
				}
			}

			content_t c = vm->m_data[vi].getContent();

			if (c == CONTENT_AIR || c == biome->c_water_top ||
					c == biome->c_water) {
				column_is_open = true;
				is_top_filler_above = false;
				continue;
			}

			if (c == biome->c_river_water) {
				column_is_open = true;
				is_under_river = true;
				is_top_filler_above = false;
				continue;
			}

			// Ground
			float d1 = contour(noise_cave1->result[index3d]);
			float d2 = contour(noise_cave2->result[index3d]);

			if (d1 * d2 > m_cave_width && m_ndef->get(c).is_ground_content) {
				// In tunnel and ground content, excavate
				vm->m_data[vi] = MapNode(CONTENT_AIR);
				is_under_tunnel = true;
				// If tunnel roof is top or filler, replace with stone
				if (is_top_filler_above)
					vm->m_data[vi + em.X] = MapNode(biome->c_stone);
				is_top_filler_above = false;
			} else if (column_is_open && is_under_tunnel &&
					(c == biome->c_stone || c == biome->c_filler)) {
				// Tunnel entrance floor, place biome surface nodes
				if (is_under_river) {
					if (nplaced < depth_riverbed) {
						vm->m_data[vi] = MapNode(biome->c_riverbed);
						is_top_filler_above = true;
						nplaced++;
					} else {
						// Disable top/filler placement
						column_is_open = false;
						is_under_river = false;
						is_under_tunnel = false;
					}
				} else if (nplaced < depth_top) {
					vm->m_data[vi] = MapNode(biome->c_top);
					is_top_filler_above = true;
					nplaced++;
				} else if (nplaced < base_filler) {
					vm->m_data[vi] = MapNode(biome->c_filler);
					is_top_filler_above = true;
					nplaced++;
				} else {
					// Disable top/filler placement
					column_is_open = false;
					is_under_tunnel = false;
				}
			} else {
				// Not tunnel or tunnel entrance floor
				// Check node for possible replacing with stone for tunnel roof
				if (c == biome->c_top || c == biome->c_filler)
					is_top_filler_above = true;

				column_is_open = false;
			}
		}
	}
}


////
//// CavernsNoise
////

CavernsNoise::CavernsNoise(
	const NodeDefManager *nodedef, v3s16 chunksize, NoiseParams *np_cavern,
	s32 seed, float cavern_limit, float cavern_taper, float cavern_threshold)
{
	assert(nodedef);

	m_ndef  = nodedef;

	m_csize            = chunksize;
	m_cavern_limit     = cavern_limit;
	m_cavern_taper     = cavern_taper;
	m_cavern_threshold = cavern_threshold;

	m_ystride = m_csize.X;
	m_zstride_1d = m_csize.X * (m_csize.Y + 1);

	// Noise is created using 1-down overgeneration
	// A Nx-by-1-by-Nz-sized plane is at the bottom of the desired for
	// re-carving the solid overtop placed for blocking sunlight
	noise_cavern = new Noise(np_cavern, seed, m_csize.X, m_csize.Y + 1, m_csize.Z);

	c_water_source = m_ndef->getId("mapgen_water_source");
	if (c_water_source == CONTENT_IGNORE)
		c_water_source = CONTENT_AIR;

	c_lava_source = m_ndef->getId("mapgen_lava_source");
	if (c_lava_source == CONTENT_IGNORE)
		c_lava_source = CONTENT_AIR;
}


CavernsNoise::~CavernsNoise()
{
	delete noise_cavern;
}


bool CavernsNoise::generateCaverns(MMVManip *vm, v3s16 nmin, v3s16 nmax)
{
	assert(vm);

	// Calculate noise
	noise_cavern->noiseMap3D(nmin.X, nmin.Y - 1, nmin.Z);

	// Cache cavern_amp values
	float *cavern_amp = new float[m_csize.Y + 1];
	u8 cavern_amp_index = 0;  // Index zero at column top
	for (s16 y = nmax.Y; y >= nmin.Y - 1; y--, cavern_amp_index++) {
		cavern_amp[cavern_amp_index] =
			MYMIN((m_cavern_limit - y) / (float)m_cavern_taper, 1.0f);
	}

	//// Place nodes
	bool near_cavern = false;
	const v3s32 &em = vm->m_area.getExtent();
	u32 index2d = 0;

	for (s16 z = nmin.Z; z <= nmax.Z; z++)
	for (s16 x = nmin.X; x <= nmax.X; x++, index2d++) {
		// Reset cave_amp index to column top
		cavern_amp_index = 0;
		// Initial voxelmanip index at column top
		u32 vi = vm->m_area.index(x, nmax.Y, z);
		// Initial 3D noise index at column top
		u32 index3d = (z - nmin.Z) * m_zstride_1d + m_csize.Y * m_ystride +
			(x - nmin.X);
		// Don't excavate the overgenerated stone at node_max.Y + 1,
		// this creates a 'roof' over the cavern, preventing light in
		// caverns at mapchunk borders when generating mapchunks upwards.
		// This 'roof' is excavated when the mapchunk above is generated.
		for (s16 y = nmax.Y; y >= nmin.Y - 1; y--,
				index3d -= m_ystride,
				VoxelArea::add_y(em, vi, -1),
				cavern_amp_index++) {
			content_t c = vm->m_data[vi].getContent();
			float n_absamp_cavern = std::fabs(noise_cavern->result[index3d]) *
				cavern_amp[cavern_amp_index];
			// Disable CavesRandomWalk at a safe distance from caverns
			// to avoid excessively spreading liquids in caverns.
			if (n_absamp_cavern > m_cavern_threshold - 0.1f) {
				near_cavern = true;
				if (n_absamp_cavern > m_cavern_threshold &&
						m_ndef->get(c).is_ground_content)
					vm->m_data[vi] = MapNode(CONTENT_AIR);
			}
		}
	}

	delete[] cavern_amp;

	return near_cavern;
}


////
//// CavesRandomWalk
////

CavesRandomWalk::CavesRandomWalk(
	const NodeDefManager *ndef,
	GenerateNotifier *gennotify,
	s32 seed,
	int water_level,
	content_t water_source,
	content_t lava_source,
	float large_cave_flooded,
	BiomeGen *biomegen)
{
	assert(ndef);

	this->ndef               = ndef;
	this->gennotify          = gennotify;
	this->seed               = seed;
	this->water_level        = water_level;
	this->np_caveliquids     = &nparams_caveliquids;
	this->large_cave_flooded = large_cave_flooded;
	this->bmgn               = biomegen;

	c_water_source = water_source;
	if (c_water_source == CONTENT_IGNORE)
		c_water_source = ndef->getId("mapgen_water_source");
	if (c_water_source == CONTENT_IGNORE)
		c_water_source = CONTENT_AIR;

	c_lava_source = lava_source;
	if (c_lava_source == CONTENT_IGNORE)
		c_lava_source = ndef->getId("mapgen_lava_source");
	if (c_lava_source == CONTENT_IGNORE)
		c_lava_source = CONTENT_AIR;
}


void CavesRandomWalk::makeCave(MMVManip *vm, v3s16 nmin, v3s16 nmax,
	PseudoRandom *ps, bool is_large_cave, int max_stone_height, s16 *heightmap)
{
	assert(vm);
	assert(ps);

	this->vm         = vm;
	this->ps         = ps;
	this->node_min   = nmin;
	this->node_max   = nmax;
	this->heightmap  = heightmap;
	this->large_cave = is_large_cave;

	this->ystride = nmax.X - nmin.X + 1;

	flooded = ps->range(1, 1000) <= large_cave_flooded * 1000.0f;

	// If flooded:
	// Get biome at mapchunk midpoint. If cave liquid defined for biome, use it.
	// If defined liquid is "air", disable 'flooded' to avoid placing "air".
	use_biome_liquid = false;
	if (flooded && bmgn) {
		v3s16 midp = node_min + (node_max - node_min) / v3s16(2, 2, 2);
		Biome *biome = (Biome *)bmgn->getBiomeAtPoint(midp);
		if (biome->c_cave_liquid[0] != CONTENT_IGNORE) {
			use_biome_liquid = true;
			c_biome_liquid =
				biome->c_cave_liquid[ps->range(0, biome->c_cave_liquid.size() - 1)];
			if (c_biome_liquid == CONTENT_AIR)
				flooded = false;
		}
	}

	// Set initial parameters from randomness
	int dswitchint = ps->range(1, 14);

	if (large_cave) {
		part_max_length_rs = ps->range(2, 4);
		tunnel_routepoints = ps->range(5, ps->range(15, 30));
		min_tunnel_diameter = 5;
		max_tunnel_diameter = ps->range(7, ps->range(8, 24));
	} else {
		part_max_length_rs = ps->range(2, 9);
		tunnel_routepoints = ps->range(10, ps->range(15, 30));
		min_tunnel_diameter = 2;
		max_tunnel_diameter = ps->range(2, 6);
	}

	large_cave_is_flat = (ps->range(0, 1) == 0);

	main_direction = v3f(0, 0, 0);

	// Allowed route area size in nodes
	ar = node_max - node_min + v3s16(1, 1, 1);
	// Area starting point in nodes
	of = node_min;

	// Allow caves to extend up to 16 nodes beyond the mapchunk edge, to allow
	// connecting with caves of neighbor mapchunks.
	// 'insure' is needed to avoid many 'out of voxelmanip' cave nodes.
	const s16 insure = 2;
	s16 more = MYMAX(MAP_BLOCKSIZE - max_tunnel_diameter / 2 - insure, 1);
	ar += v3s16(1, 1, 1) * more * 2;
	of -= v3s16(1, 1, 1) * more;

	route_y_min = 0;
	// Allow half a diameter + 7 over stone surface
	route_y_max = -of.Y + max_stone_height + max_tunnel_diameter / 2 + 7;

	// Limit maximum to area
	route_y_max = rangelim(route_y_max, 0, ar.Y - 1);

	if (large_cave) {
		s16 minpos = 0;
		if (node_min.Y < water_level && node_max.Y > water_level) {
			minpos = water_level - max_tunnel_diameter / 3 - of.Y;
			route_y_max = water_level + max_tunnel_diameter / 3 - of.Y;
		}
		route_y_min = ps->range(minpos, minpos + max_tunnel_diameter);
		route_y_min = rangelim(route_y_min, 0, route_y_max);
	}

	s16 route_start_y_min = route_y_min;
	s16 route_start_y_max = route_y_max;

	route_start_y_min = rangelim(route_start_y_min, 0, ar.Y - 1);
	route_start_y_max = rangelim(route_start_y_max, route_start_y_min, ar.Y - 1);

	// Randomize starting position
	orp.Z = (float)(ps->next() % ar.Z) + 0.5f;
	orp.Y = (float)(ps->range(route_start_y_min, route_start_y_max)) + 0.5f;
	orp.X = (float)(ps->next() % ar.X) + 0.5f;

	// Add generation notify begin event
	if (gennotify) {
		v3s16 abs_pos(of.X + orp.X, of.Y + orp.Y, of.Z + orp.Z);
		GenNotifyType notifytype = large_cave ?
			GENNOTIFY_LARGECAVE_BEGIN : GENNOTIFY_CAVE_BEGIN;
		gennotify->addEvent(notifytype, abs_pos);
	}

	// Generate some tunnel starting from orp
	for (u16 j = 0; j < tunnel_routepoints; j++)
		makeTunnel(j % dswitchint == 0);

	// Add generation notify end event
	if (gennotify) {
		v3s16 abs_pos(of.X + orp.X, of.Y + orp.Y, of.Z + orp.Z);
		GenNotifyType notifytype = large_cave ?
			GENNOTIFY_LARGECAVE_END : GENNOTIFY_CAVE_END;
		gennotify->addEvent(notifytype, abs_pos);
	}
}


void CavesRandomWalk::makeTunnel(bool dirswitch)
{
	if (dirswitch && !large_cave) {
		main_direction.Z = ((float)(ps->next() % 20) - (float)10) / 10;
		main_direction.Y = ((float)(ps->next() % 20) - (float)10) / 30;
		main_direction.X = ((float)(ps->next() % 20) - (float)10) / 10;

		main_direction *= (float)ps->range(0, 10) / 10;
	}

	// Randomize size
	s16 min_d = min_tunnel_diameter;
	s16 max_d = max_tunnel_diameter;
	rs = ps->range(min_d, max_d);
	s16 rs_part_max_length_rs = rs * part_max_length_rs;

	v3s16 maxlen;
	if (large_cave) {
		maxlen = v3s16(
			rs_part_max_length_rs,
			rs_part_max_length_rs / 2,
			rs_part_max_length_rs
		);
	} else {
		maxlen = v3s16(
			rs_part_max_length_rs,
			ps->range(1, rs_part_max_length_rs),
			rs_part_max_length_rs
		);
	}

	v3f vec;
	// Jump downward sometimes
	if (!large_cave && ps->range(0, 12) == 0) {
		vec.Z = (float)(ps->next() % (maxlen.Z * 1)) - (float)maxlen.Z / 2;
		vec.Y = (float)(ps->next() % (maxlen.Y * 2)) - (float)maxlen.Y;
		vec.X = (float)(ps->next() % (maxlen.X * 1)) - (float)maxlen.X / 2;
	} else {
		vec.Z = (float)(ps->next() % (maxlen.Z * 1)) - (float)maxlen.Z / 2;
		vec.Y = (float)(ps->next() % (maxlen.Y * 1)) - (float)maxlen.Y / 2;
		vec.X = (float)(ps->next() % (maxlen.X * 1)) - (float)maxlen.X / 2;
	}

	// Do not make caves that are above ground.
	// It is only necessary to check the startpoint and endpoint.
	v3s16 p1 = v3s16(orp.X, orp.Y, orp.Z) + of + rs / 2;
	v3s16 p2 = v3s16(vec.X, vec.Y, vec.Z) + p1;
	if (isPosAboveSurface(p1) || isPosAboveSurface(p2))
		return;

	vec += main_direction;

	v3f rp = orp + vec;
	if (rp.X < 0)
		rp.X = 0;
	else if (rp.X >= ar.X)
		rp.X = ar.X - 1;

	if (rp.Y < route_y_min)
		rp.Y = route_y_min;
	else if (rp.Y >= route_y_max)
		rp.Y = route_y_max - 1;

	if (rp.Z < 0)
		rp.Z = 0;
	else if (rp.Z >= ar.Z)
		rp.Z = ar.Z - 1;

	vec = rp - orp;

	float veclen = vec.getLength();
	if (veclen < 0.05f)
		veclen = 1.0f;

	// Every second section is rough
	bool randomize_xz = (ps->range(1, 2) == 1);

	// Carve routes
	for (float f = 0.f; f < 1.0f; f += 1.0f / veclen)
		carveRoute(vec, f, randomize_xz);

	orp = rp;
}


void CavesRandomWalk::carveRoute(v3f vec, float f, bool randomize_xz)
{
	MapNode airnode(CONTENT_AIR);
	MapNode waternode(c_water_source);
	MapNode lavanode(c_lava_source);

	v3s16 startp(orp.X, orp.Y, orp.Z);
	startp += of;

	v3f fp = orp + vec * f;
	fp.X += 0.1f * ps->range(-10, 10);
	fp.Z += 0.1f * ps->range(-10, 10);
	v3s16 cp(fp.X, fp.Y, fp.Z);

	// Choose cave liquid
	MapNode liquidnode = CONTENT_IGNORE;

	if (flooded) {
		if (use_biome_liquid) {
			liquidnode = c_biome_liquid;
		} else {
			// If cave liquid not defined by biome, fallback to old hardcoded behavior.
			// TODO 'np_caveliquids' is deprecated and should eventually be removed.
			// Cave liquids are now defined and located using biome definitions.
			float nval = NoiseFractal3D(np_caveliquids, startp.X,
				startp.Y, startp.Z, seed);
			liquidnode = (nval < 0.40f && node_max.Y < water_level - 256) ?
				lavanode : waternode;
		}
	}

	s16 d0 = -rs / 2;
	s16 d1 = d0 + rs;
	if (randomize_xz) {
		d0 += ps->range(-1, 1);
		d1 += ps->range(-1, 1);
	}

	bool flat_cave_floor = !large_cave && ps->range(0, 2) == 2;

	for (s16 z0 = d0; z0 <= d1; z0++) {
		s16 si = rs / 2 - MYMAX(0, abs(z0) - rs / 7 - 1);
		for (s16 x0 = -si - ps->range(0,1); x0 <= si - 1 + ps->range(0,1); x0++) {
			s16 maxabsxz = MYMAX(abs(x0), abs(z0));

			s16 si2 = rs / 2 - MYMAX(0, maxabsxz - rs / 7 - 1);

			for (s16 y0 = -si2; y0 <= si2; y0++) {
				// Make better floors in small caves
				if (flat_cave_floor && y0 <= -rs / 2 && rs <= 7)
					continue;

				if (large_cave_is_flat) {
					// Make large caves not so tall
					if (rs > 7 && abs(y0) >= rs / 3)
						continue;
				}

				v3s16 p(cp.X + x0, cp.Y + y0, cp.Z + z0);
				p += of;

				if (!vm->m_area.contains(p))
					continue;

				u32 i = vm->m_area.index(p);
				content_t c = vm->m_data[i].getContent();
				if (!ndef->get(c).is_ground_content)
					continue;

				if (large_cave) {
					int full_ymin = node_min.Y - MAP_BLOCKSIZE;
					int full_ymax = node_max.Y + MAP_BLOCKSIZE;

					if (flooded && full_ymin < water_level && full_ymax > water_level)
						vm->m_data[i] = (p.Y <= water_level) ? waternode : airnode;
					else if (flooded && full_ymax < water_level)
						vm->m_data[i] = (p.Y < startp.Y - 4) ? liquidnode : airnode;
					else
						vm->m_data[i] = airnode;
				} else {
					vm->m_data[i] = airnode;
					vm->m_flags[i] |= VMANIP_FLAG_CAVE;
				}
			}
		}
	}
}


inline bool CavesRandomWalk::isPosAboveSurface(v3s16 p)
{
	if (heightmap != NULL &&
			p.Z >= node_min.Z && p.Z <= node_max.Z &&
			p.X >= node_min.X && p.X <= node_max.X) {
		u32 index = (p.Z - node_min.Z) * ystride + (p.X - node_min.X);
		if (heightmap[index] < p.Y)
			return true;
	} else if (p.Y > water_level) {
		return true;
	}

	return false;
}


////
//// CavesV6
////

CavesV6::CavesV6(const NodeDefManager *ndef, GenerateNotifier *gennotify,
	int water_level, content_t water_source, content_t lava_source)
{
	assert(ndef);

	this->ndef        = ndef;
	this->gennotify   = gennotify;
	this->water_level = water_level;

	c_water_source = water_source;
	if (c_water_source == CONTENT_IGNORE)
		c_water_source = ndef->getId("mapgen_water_source");
	if (c_water_source == CONTENT_IGNORE)
		c_water_source = CONTENT_AIR;

	c_lava_source = lava_source;
	if (c_lava_source == CONTENT_IGNORE)
		c_lava_source = ndef->getId("mapgen_lava_source");
	if (c_lava_source == CONTENT_IGNORE)
		c_lava_source = CONTENT_AIR;
}


void CavesV6::makeCave(MMVManip *vm, v3s16 nmin, v3s16 nmax,
	PseudoRandom *ps, PseudoRandom *ps2,
	bool is_large_cave, int max_stone_height, s16 *heightmap)
{
	assert(vm);
	assert(ps);
	assert(ps2);

	this->vm         = vm;
	this->ps         = ps;
	this->ps2        = ps2;
	this->node_min   = nmin;
	this->node_max   = nmax;
	this->heightmap  = heightmap;
	this->large_cave = is_large_cave;

	this->ystride = nmax.X - nmin.X + 1;

	// Set initial parameters from randomness
	min_tunnel_diameter = 2;
	max_tunnel_diameter = ps->range(2, 6);
	int dswitchint      = ps->range(1, 14);
	if (large_cave) {
		part_max_length_rs  = ps->range(2, 4);
		tunnel_routepoints  = ps->range(5, ps->range(15, 30));
		min_tunnel_diameter = 5;
		max_tunnel_diameter = ps->range(7, ps->range(8, 24));
	} else {
		part_max_length_rs = ps->range(2, 9);
		tunnel_routepoints = ps->range(10, ps->range(15, 30));
	}
	large_cave_is_flat = (ps->range(0, 1) == 0);

	main_direction = v3f(0, 0, 0);

	// Allowed route area size in nodes
	ar = node_max - node_min + v3s16(1, 1, 1);
	// Area starting point in nodes
	of = node_min;

	// Allow a bit more
	//(this should be more than the maximum radius of the tunnel)
	const s16 max_spread_amount = MAP_BLOCKSIZE;
	const s16 insure = 10;
	s16 more = MYMAX(max_spread_amount - max_tunnel_diameter / 2 - insure, 1);
	ar += v3s16(1, 0, 1) * more * 2;
	of -= v3s16(1, 0, 1) * more;

	route_y_min = 0;
	// Allow half a diameter + 7 over stone surface
	route_y_max = -of.Y + max_stone_height + max_tunnel_diameter / 2 + 7;

	// Limit maximum to area
	route_y_max = rangelim(route_y_max, 0, ar.Y - 1);

	if (large_cave) {
		s16 minpos = 0;
		if (node_min.Y < water_level && node_max.Y > water_level) {
			minpos = water_level - max_tunnel_diameter / 3 - of.Y;
			route_y_max = water_level + max_tunnel_diameter / 3 - of.Y;
		}
		route_y_min = ps->range(minpos, minpos + max_tunnel_diameter);
		route_y_min = rangelim(route_y_min, 0, route_y_max);
	}

	s16 route_start_y_min = route_y_min;
	s16 route_start_y_max = route_y_max;

	route_start_y_min = rangelim(route_start_y_min, 0, ar.Y - 1);
	route_start_y_max = rangelim(route_start_y_max, route_start_y_min, ar.Y - 1);

	// Randomize starting position
	orp.Z = (float)(ps->next() % ar.Z) + 0.5f;
	orp.Y = (float)(ps->range(route_start_y_min, route_start_y_max)) + 0.5f;
	orp.X = (float)(ps->next() % ar.X) + 0.5f;

	// Add generation notify begin event
	if (gennotify != NULL) {
		v3s16 abs_pos(of.X + orp.X, of.Y + orp.Y, of.Z + orp.Z);
		GenNotifyType notifytype = large_cave ?
			GENNOTIFY_LARGECAVE_BEGIN : GENNOTIFY_CAVE_BEGIN;
		gennotify->addEvent(notifytype, abs_pos);
	}

	// Generate some tunnel starting from orp
	for (u16 j = 0; j < tunnel_routepoints; j++)
		makeTunnel(j % dswitchint == 0);

	// Add generation notify end event
	if (gennotify != NULL) {
		v3s16 abs_pos(of.X + orp.X, of.Y + orp.Y, of.Z + orp.Z);
		GenNotifyType notifytype = large_cave ?
			GENNOTIFY_LARGECAVE_END : GENNOTIFY_CAVE_END;
		gennotify->addEvent(notifytype, abs_pos);
	}
}


void CavesV6::makeTunnel(bool dirswitch)
{
	if (dirswitch && !large_cave) {
		main_direction.Z = ((float)(ps->next() % 20) - (float)10) / 10;
		main_direction.Y = ((float)(ps->next() % 20) - (float)10) / 30;
		main_direction.X = ((float)(ps->next() % 20) - (float)10) / 10;

		main_direction *= (float)ps->range(0, 10) / 10;
	}

	// Randomize size
	s16 min_d = min_tunnel_diameter;
	s16 max_d = max_tunnel_diameter;
	rs = ps->range(min_d, max_d);
	s16 rs_part_max_length_rs = rs * part_max_length_rs;

	v3s16 maxlen;
	if (large_cave) {
		maxlen = v3s16(
			rs_part_max_length_rs,
			rs_part_max_length_rs / 2,
			rs_part_max_length_rs
		);
	} else {
		maxlen = v3s16(
			rs_part_max_length_rs,
			ps->range(1, rs_part_max_length_rs),
			rs_part_max_length_rs
		);
	}

	v3f vec;
	vec.Z = (float)(ps->next() % maxlen.Z) - (float)maxlen.Z / 2;
	vec.Y = (float)(ps->next() % maxlen.Y) - (float)maxlen.Y / 2;
	vec.X = (float)(ps->next() % maxlen.X) - (float)maxlen.X / 2;

	// Jump downward sometimes
	if (!large_cave && ps->range(0, 12) == 0) {
		vec.Z = (float)(ps->next() % maxlen.Z) - (float)maxlen.Z / 2;
		vec.Y = (float)(ps->next() % (maxlen.Y * 2)) - (float)maxlen.Y;
		vec.X = (float)(ps->next() % maxlen.X) - (float)maxlen.X / 2;
	}

	// Do not make caves that are entirely above ground, to fix shadow bugs
	// caused by overgenerated large caves.
	// It is only necessary to check the startpoint and endpoint.
	v3s16 p1 = v3s16(orp.X, orp.Y, orp.Z) + of + rs / 2;
	v3s16 p2 = v3s16(vec.X, vec.Y, vec.Z) + p1;

	// If startpoint and endpoint are above ground, disable placement of nodes
	// in carveRoute while still running all PseudoRandom calls to ensure caves
	// are consistent with existing worlds.
	bool tunnel_above_ground =
		p1.Y > getSurfaceFromHeightmap(p1) &&
		p2.Y > getSurfaceFromHeightmap(p2);

	vec += main_direction;

	v3f rp = orp + vec;
	if (rp.X < 0)
		rp.X = 0;
	else if (rp.X >= ar.X)
		rp.X = ar.X - 1;

	if (rp.Y < route_y_min)
		rp.Y = route_y_min;
	else if (rp.Y >= route_y_max)
		rp.Y = route_y_max - 1;

	if (rp.Z < 0)
		rp.Z = 0;
	else if (rp.Z >= ar.Z)
		rp.Z = ar.Z - 1;

	vec = rp - orp;

	float veclen = vec.getLength();
	// As odd as it sounds, veclen is *exactly* 0.0 sometimes, causing a FPE
	if (veclen < 0.05f)
		veclen = 1.0f;

	// Every second section is rough
	bool randomize_xz = (ps2->range(1, 2) == 1);

	// Carve routes
	for (float f = 0.f; f < 1.0f; f += 1.0f / veclen)
		carveRoute(vec, f, randomize_xz, tunnel_above_ground);

	orp = rp;
}


void CavesV6::carveRoute(v3f vec, float f, bool randomize_xz,
	bool tunnel_above_ground)
{
	MapNode airnode(CONTENT_AIR);
	MapNode waternode(c_water_source);
	MapNode lavanode(c_lava_source);

	v3s16 startp(orp.X, orp.Y, orp.Z);
	startp += of;

	v3f fp = orp + vec * f;
	fp.X += 0.1f * ps->range(-10, 10);
	fp.Z += 0.1f * ps->range(-10, 10);
	v3s16 cp(fp.X, fp.Y, fp.Z);

	s16 d0 = -rs / 2;
	s16 d1 = d0 + rs;
	if (randomize_xz) {
		d0 += ps->range(-1, 1);
		d1 += ps->range(-1, 1);
	}

	for (s16 z0 = d0; z0 <= d1; z0++) {
		s16 si = rs / 2 - MYMAX(0, abs(z0) - rs / 7 - 1);
		for (s16 x0 = -si - ps->range(0,1); x0 <= si - 1 + ps->range(0,1); x0++) {
			if (tunnel_above_ground)
				continue;

			s16 maxabsxz = MYMAX(abs(x0), abs(z0));
			s16 si2 = rs / 2 - MYMAX(0, maxabsxz - rs / 7 - 1);
			for (s16 y0 = -si2; y0 <= si2; y0++) {
				if (large_cave_is_flat) {
					// Make large caves not so tall
					if (rs > 7 && abs(y0) >= rs / 3)
						continue;
				}

				v3s16 p(cp.X + x0, cp.Y + y0, cp.Z + z0);
				p += of;

				if (!vm->m_area.contains(p))
					continue;

				u32 i = vm->m_area.index(p);
				content_t c = vm->m_data[i].getContent();
				if (!ndef->get(c).is_ground_content)
					continue;

				if (large_cave) {
					int full_ymin = node_min.Y - MAP_BLOCKSIZE;
					int full_ymax = node_max.Y + MAP_BLOCKSIZE;

					if (full_ymin < water_level && full_ymax > water_level) {
						vm->m_data[i] = (p.Y <= water_level) ? waternode : airnode;
					} else if (full_ymax < water_level) {
						vm->m_data[i] = (p.Y < startp.Y - 2) ? lavanode : airnode;
					} else {
						vm->m_data[i] = airnode;
					}
				} else {
					if (c == CONTENT_AIR)
						continue;

					vm->m_data[i] = airnode;
					vm->m_flags[i] |= VMANIP_FLAG_CAVE;
				}
			}
		}
	}
}


inline s16 CavesV6::getSurfaceFromHeightmap(v3s16 p)
{
	if (heightmap != NULL &&
			p.Z >= node_min.Z && p.Z <= node_max.Z &&
			p.X >= node_min.X && p.X <= node_max.X) {
		u32 index = (p.Z - node_min.Z) * ystride + (p.X - node_min.X);
		return heightmap[index];
	}

	return water_level;

}
