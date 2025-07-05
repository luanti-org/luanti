// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "voxel.h"
#include "map.h"
#include "gettime.h"
#include "nodedef.h"
#include "util/directiontables.h"
#include "util/timetaker.h"
#include "porting.h"
#include <cstring>  // memcpy, memset

/*
	Debug stuff
*/
u64 emerge_time = 0;

VoxelManipulator::~VoxelManipulator()
{
	clear();
}

void VoxelManipulator::clear()
{
	// Reset area to empty volume
	VoxelArea old;
	std::swap(m_area, old);
	delete[] m_data;
	m_data = nullptr;
	delete[] m_flags;
	m_flags = nullptr;

	porting::TrackFreedMemory((sizeof(*m_data) + sizeof(*m_flags)) * old.getVolume());
}

void VoxelManipulator::print(std::ostream &o, const NodeDefManager *ndef,
	VoxelPrintMode mode) const
{
	auto &em = m_area.getExtent();
	v3s16 of = m_area.MinEdge;
	o<<"size: "<<em.X<<"x"<<em.Y<<"x"<<em.Z
	 <<" offset: ("<<of.X<<","<<of.Y<<","<<of.Z<<")"<<std::endl;

	for(s32 y=m_area.MaxEdge.Y; y>=m_area.MinEdge.Y; y--)
	{
		if(em.X >= 3 && em.Y >= 3)
		{
			if     (y==m_area.MinEdge.Y+2) o<<"^     ";
			else if(y==m_area.MinEdge.Y+1) o<<"|     ";
			else if(y==m_area.MinEdge.Y+0) o<<"y x-> ";
			else                           o<<"      ";
		}

		for(s32 z=m_area.MinEdge.Z; z<=m_area.MaxEdge.Z; z++)
		{
			for(s32 x=m_area.MinEdge.X; x<=m_area.MaxEdge.X; x++)
			{
				u8 f = m_flags[m_area.index(x,y,z)];
				char c;
				if(f & VOXELFLAG_NO_DATA)
					c = 'N';
				else
				{
					c = 'X';
					MapNode n = m_data[m_area.index(x,y,z)];
					content_t m = n.getContent();
					u8 pr = n.param2;
					if(mode == VOXELPRINT_MATERIAL)
					{
						if(m <= 9)
							c = m + '0';
					}
					else if(mode == VOXELPRINT_WATERPRESSURE)
					{
						if(ndef->get(m).isLiquid())
						{
							c = 'w';
							if(pr <= 9)
								c = pr + '0';
						}
						else if(m == CONTENT_AIR)
						{
							c = ' ';
						}
						else
						{
							c = '#';
						}
					}
					else if(mode == VOXELPRINT_LIGHT_DAY)
					{
						if(ndef->get(m).light_source != 0)
							c = 'S';
						else if(!ndef->get(m).light_propagates)
							c = 'X';
						else
						{
							u8 light = n.getLight(LIGHTBANK_DAY, ndef->getLightingFlags(n));
							if(light < 10)
								c = '0' + light;
							else
								c = 'a' + (light-10);
						}
					}
				}
				o<<c;
			}
			o<<' ';
		}
		o<<std::endl;
	}
}

static inline void checkArea(const VoxelArea &a)
{
	// won't overflow since cbrt(2^64) > 2^16
	u64 real_volume = static_cast<u64>(a.getExtent().X) * a.getExtent().Y * a.getExtent().Z;

	static_assert(MAX_WORKING_VOLUME < S32_MAX); // hard limit is somewhere here
	if (real_volume > MAX_WORKING_VOLUME) {
		throw BaseException("VoxelManipulator: "
			"Area volume exceeds allowed value of " + std::to_string(MAX_WORKING_VOLUME));
	}
}

void VoxelManipulator::addArea(const VoxelArea &area)
{
	// Cancel if requested area has zero volume
	if (area.hasEmptyExtent())
		return;

	// Cancel if m_area already contains the requested area
	if(m_area.contains(area))
		return;

	// Calculate new area
	VoxelArea new_area = m_area;
	new_area.addArea(area);

	checkArea(new_area);

	u32 new_size = new_area.getVolume();

	// Allocate new data and clear flags
	MapNode *new_data = new MapNode[new_size];
	assert(new_data);
	u8 *new_flags = new u8[new_size];
	assert(new_flags);
	memset(new_flags, VOXELFLAG_NO_DATA, new_size);

	// Copy old data
	u32 old_x_width = m_area.getExtent().X;
	for(s32 z=m_area.MinEdge.Z; z<=m_area.MaxEdge.Z; z++)
	for(s32 y=m_area.MinEdge.Y; y<=m_area.MaxEdge.Y; y++)
	{
		unsigned int old_index = m_area.index(m_area.MinEdge.X,y,z);
		unsigned int new_index = new_area.index(m_area.MinEdge.X,y,z);

		memcpy(&new_data[new_index], &m_data[old_index],
				old_x_width * sizeof(MapNode));
		memcpy(&new_flags[new_index], &m_flags[old_index],
				old_x_width * sizeof(u8));
	}

	// Replace area, data and flags

	m_area = new_area;

	MapNode *old_data = m_data;
	u8 *old_flags = m_flags;

	m_data = new_data;
	m_flags = new_flags;

	delete[] old_data;
	delete[] old_flags;
}

void VoxelManipulator::copyFrom(MapNode *src, bool is_mono_block, const VoxelArea& src_area,
		v3s16 from_pos, v3s16 to_pos, const v3s16 &size)
{
	/* The reason for this optimised code is that we're a member function
	 * and the data type/layout of m_data is know to us: it's stored as
	 * [z*h*w + y*h + x]. Therefore we can take the calls to m_area index
	 * (which performs the preceding mapping/indexing of m_data) out of the
	 * inner loop and calculate the next index as we're iterating to gain
	 * performance.
	 *
	 * src_step and dest_step is the amount required to be added to our index
	 * every time y increments. Because the destination area may be larger
	 * than the source area we need one additional variable (otherwise we could
	 * just continue adding dest_step as is done for the source data): dest_mod.
	 * dest_mod is the difference in size between a "row" in the source data
	 * and a "row" in the destination data (I am using the term row loosely
	 * and for illustrative purposes). E.g.
	 *
	 * src       <-------------------->|'''''' dest mod ''''''''
	 * dest      <--------------------------------------------->
	 *
	 * dest_mod (it's essentially a modulus) is added to the destination index
	 * after every full iteration of the y span.
	 *
	 * This method falls under the category "linear array and incrementing
	 * index".
	 */

	s32 src_step = src_area.getExtent().X;
	s32 dest_step = m_area.getExtent().X;
	s32 dest_mod = m_area.index(to_pos.X, to_pos.Y, to_pos.Z + 1)
			- m_area.index(to_pos.X, to_pos.Y, to_pos.Z)
			- dest_step * size.Y;

	s32 i_src = src_area.index(from_pos.X, from_pos.Y, from_pos.Z);
	s32 i_local = m_area.index(to_pos.X, to_pos.Y, to_pos.Z);

	for (s16 z = 0; z < size.Z; z++) {
		for (s16 y = 0; y < size.Y; y++) {
			if (is_mono_block) {
				std::fill_n(m_data + i_local, size.X, src[0]);
			} else {
				std::copy_n(src + i_src, size.X, m_data + i_local);
			}
			std::fill_n(m_flags + i_local, size.X, 0);
			i_src += src_step;
			i_local += dest_step;
		}
		i_local += dest_mod;
	}
}

void VoxelManipulator::copyTo(MapNode *dst, const VoxelArea& dst_area,
		v3s16 dst_pos, v3s16 from_pos, const v3s16 &size) const
{
	for(s16 z=0; z<size.Z; z++)
	for(s16 y=0; y<size.Y; y++)
	{
		s32 i_dst = dst_area.index(dst_pos.X, dst_pos.Y+y, dst_pos.Z+z);
		s32 i_local = m_area.index(from_pos.X, from_pos.Y+y, from_pos.Z+z);
		for (s16 x = 0; x < size.X; x++) {
			if (m_data[i_local].getContent() != CONTENT_IGNORE)
				dst[i_dst] = m_data[i_local];
			i_dst++;
			i_local++;
		}
	}
}

/*
	Algorithms
	-----------------------------------------------------
*/

void VoxelManipulator::setFlags(const VoxelArea &a, u8 flags)
{
	if (a.hasEmptyExtent())
		return;

	assert(m_area.contains(a));

	const s32 stride = a.getExtent().X;
	for (s32 z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++)
	for (s32 y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++)
	{
		const s32 start = m_area.index(a.MinEdge.X, y, z);
		for (s32 i = start; i < start + stride; i++)
			m_flags[i] |= flags;
	}
}

void VoxelManipulator::clearFlags(const VoxelArea &a, u8 flags)
{
	if (a.hasEmptyExtent())
		return;

	assert(m_area.contains(a));

	const s32 stride = a.getExtent().X;
	for (s32 z = a.MinEdge.Z; z <= a.MaxEdge.Z; z++)
	for (s32 y = a.MinEdge.Y; y <= a.MaxEdge.Y; y++)
	{
		const s32 start = m_area.index(a.MinEdge.X, y, z);
		for (s32 i = start; i < start + stride; i++)
			m_flags[i] &= ~flags;
	}
}

const MapNode VoxelManipulator::ContentIgnoreNode = MapNode(CONTENT_IGNORE);

//END
