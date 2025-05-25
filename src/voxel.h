// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "irr_v3d.h"
#include <iostream>
#include <cassert>
#include "exceptions.h"
#include "mapnode.h"
#include <set>
#include <list>
#include "irrlicht_changes/printing.h"

class NodeDefManager;

// For VC++
#undef min
#undef max

/*
	A fast voxel manipulator class.

	In normal operation, it fetches more map when it is requested.
	It can also be used so that all allowed area is fetched at the
	start, using ManualMapVoxelManipulator.

	Not thread-safe.
*/

/*
	Debug stuff
*/
extern u64 emerge_time;

/*
	This class resembles aabbox3d<s16> a lot, but has inclusive
	edges for saner handling of integer sizes
*/
class VoxelArea
{
public:
	// Starts as zero sized
	constexpr VoxelArea() = default;

	VoxelArea(const v3s16 &min_edge, const v3s16 &max_edge):
		MinEdge(min_edge),
		MaxEdge(max_edge)
	{
		cacheExtent();
	}

	VoxelArea(const v3s16 &p):
		MinEdge(p),
		MaxEdge(p)
	{
		cacheExtent();
	}

	/*
		Modifying methods
	*/

	void addArea(const VoxelArea &a)
	{
		if (hasEmptyExtent())
		{
			*this = a;
			return;
		}
		if(a.MinEdge.X < MinEdge.X) MinEdge.X = a.MinEdge.X;
		if(a.MinEdge.Y < MinEdge.Y) MinEdge.Y = a.MinEdge.Y;
		if(a.MinEdge.Z < MinEdge.Z) MinEdge.Z = a.MinEdge.Z;
		if(a.MaxEdge.X > MaxEdge.X) MaxEdge.X = a.MaxEdge.X;
		if(a.MaxEdge.Y > MaxEdge.Y) MaxEdge.Y = a.MaxEdge.Y;
		if(a.MaxEdge.Z > MaxEdge.Z) MaxEdge.Z = a.MaxEdge.Z;
		cacheExtent();
	}

	void addPoint(const v3s16 &p)
	{
		if(hasEmptyExtent())
		{
			MinEdge = p;
			MaxEdge = p;
			cacheExtent();
			return;
		}
		if(p.X < MinEdge.X) MinEdge.X = p.X;
		if(p.Y < MinEdge.Y) MinEdge.Y = p.Y;
		if(p.Z < MinEdge.Z) MinEdge.Z = p.Z;
		if(p.X > MaxEdge.X) MaxEdge.X = p.X;
		if(p.Y > MaxEdge.Y) MaxEdge.Y = p.Y;
		if(p.Z > MaxEdge.Z) MaxEdge.Z = p.Z;
		cacheExtent();
	}

	// Pad with d nodes
	void pad(const v3s16 &d)
	{
		MinEdge -= d;
		MaxEdge += d;
		cacheExtent();
	}

	/*
		const methods
	*/

	const v3s32 &getExtent() const
	{
		return m_cache_extent;
	}

	bool hasEmptyExtent() const
	{
		return !m_cache_extent.X || !m_cache_extent.Y || !m_cache_extent.Z;
	}

	u32 getVolume() const
	{
		// FIXME: possible integer overflow here
		return (u32)m_cache_extent.X * (u32)m_cache_extent.Y * (u32)m_cache_extent.Z;
	}

	bool contains(const VoxelArea &a) const
	{
		// No area contains an empty area
		// NOTE: Algorithms depend on this, so do not change.
		if(a.hasEmptyExtent())
			return false;

		return(
			a.MinEdge.X >= MinEdge.X && a.MaxEdge.X <= MaxEdge.X &&
			a.MinEdge.Y >= MinEdge.Y && a.MaxEdge.Y <= MaxEdge.Y &&
			a.MinEdge.Z >= MinEdge.Z && a.MaxEdge.Z <= MaxEdge.Z
		);
	}
	bool contains(v3s16 p) const
	{
		return(
			p.X >= MinEdge.X && p.X <= MaxEdge.X &&
			p.Y >= MinEdge.Y && p.Y <= MaxEdge.Y &&
			p.Z >= MinEdge.Z && p.Z <= MaxEdge.Z
		);
	}
	bool contains(s32 i) const
	{
		return i >= 0 && static_cast<u32>(i) < getVolume();
	}

	bool operator==(const VoxelArea &other) const
	{
		return (MinEdge == other.MinEdge
				&& MaxEdge == other.MaxEdge);
	}

	VoxelArea operator+(const v3s16 &off) const
	{
		return {MinEdge+off, MaxEdge+off};
	}

	VoxelArea operator-(const v3s16 &off) const
	{
		return {MinEdge-off, MaxEdge-off};
	}

	/*
		Returns the intersection of this area and `a`.
	*/
	VoxelArea intersect(const VoxelArea &a) const
	{
		// This is an example of an operation that would be simpler with
		// non-inclusive edges, but oh well.
		VoxelArea ret;

		if (a.MaxEdge.X < MinEdge.X || a.MinEdge.X > MaxEdge.X)
			return VoxelArea();
		if (a.MaxEdge.Y < MinEdge.Y || a.MinEdge.Y > MaxEdge.Y)
			return VoxelArea();
		if (a.MaxEdge.Z < MinEdge.Z || a.MinEdge.Z > MaxEdge.Z)
			return VoxelArea();
		ret.MinEdge.X = std::max(a.MinEdge.X, MinEdge.X);
		ret.MaxEdge.X = std::min(a.MaxEdge.X, MaxEdge.X);
		ret.MinEdge.Y = std::max(a.MinEdge.Y, MinEdge.Y);
		ret.MaxEdge.Y = std::min(a.MaxEdge.Y, MaxEdge.Y);
		ret.MinEdge.Z = std::max(a.MinEdge.Z, MinEdge.Z);
		ret.MaxEdge.Z = std::min(a.MaxEdge.Z, MaxEdge.Z);
		ret.cacheExtent();

		return ret;
	}

	/**
		Returns 0-6 non-overlapping areas that can be added to
		`a` to make up this area.

		@tparam C container that has push_back
		@param a area inside *this
	*/
	template <typename C>
	void diff(const VoxelArea &a, C &result) const
	{
		// If a is an empty area, return the current area as a whole
		if(a.hasEmptyExtent())
		{
			VoxelArea b = *this;
			if (!b.hasEmptyExtent())
				result.push_back(b);
			return;
		}

		assert(contains(a));	// pre-condition

		const auto &take = [&result] (v3s16 min, v3s16 max) {
			VoxelArea b(min, max);
			if (!b.hasEmptyExtent())
				result.push_back(b);
		};

		// Take back area, XY inclusive
		{
			v3s16 min(MinEdge.X, MinEdge.Y, a.MaxEdge.Z+1);
			v3s16 max(MaxEdge.X, MaxEdge.Y, MaxEdge.Z);
			take(min, max);
		}

		// Take front area, XY inclusive
		{
			v3s16 min(MinEdge.X, MinEdge.Y, MinEdge.Z);
			v3s16 max(MaxEdge.X, MaxEdge.Y, a.MinEdge.Z-1);
			take(min, max);
		}

		// Take top area, X inclusive
		{
			v3s16 min(MinEdge.X, a.MaxEdge.Y+1, a.MinEdge.Z);
			v3s16 max(MaxEdge.X, MaxEdge.Y, a.MaxEdge.Z);
			take(min, max);
		}

		// Take bottom area, X inclusive
		{
			v3s16 min(MinEdge.X, MinEdge.Y, a.MinEdge.Z);
			v3s16 max(MaxEdge.X, a.MinEdge.Y-1, a.MaxEdge.Z);
			take(min, max);
		}

		// Take left area, non-inclusive
		{
			v3s16 min(MinEdge.X, a.MinEdge.Y, a.MinEdge.Z);
			v3s16 max(a.MinEdge.X-1, a.MaxEdge.Y, a.MaxEdge.Z);
			take(min, max);
		}

		// Take right area, non-inclusive
		{
			v3s16 min(a.MaxEdge.X+1, a.MinEdge.Y, a.MinEdge.Z);
			v3s16 max(MaxEdge.X, a.MaxEdge.Y, a.MaxEdge.Z);
			take(min, max);
		}
	}

	/*
		Translates position from virtual coordinates to array index
	*/
	s32 index(s16 x, s16 y, s16 z) const
	{
		s32 i = (s32)(z - MinEdge.Z) * m_cache_extent.Y * m_cache_extent.X
			+ (y - MinEdge.Y) * m_cache_extent.X
			+ (x - MinEdge.X);
		return i;
	}
	s32 index(v3s16 p) const
	{
		return index(p.X, p.Y, p.Z);
	}

	/**
	 * Translate index in the X coordinate
	 */
	static void add_x(const v3s32 &extent, u32 &i, s16 a)
	{
		(void)extent;
		i += a;
	}

	/**
	 * Translate index in the Y coordinate
	 */
	static void add_y(const v3s32 &extent, u32 &i, s16 a)
	{
		i += a * extent.X;
	}

	/**
	 * Translate index in the Z coordinate
	 */
	static void add_z(const v3s32 &extent, u32 &i, s16 a)
	{
		i += a * extent.X * extent.Y;
	}

	/**
	 * Translate index in space
	 */
	static void add_p(const v3s32 &extent, u32 &i, v3s16 a)
	{
		i += a.Z * extent.X * extent.Y + a.Y * extent.X + a.X;
	}

	/*
		Print method for debugging
	*/
	void print(std::ostream &o) const
	{
		o << MinEdge << MaxEdge << "="
			<< m_cache_extent.X << "x" << m_cache_extent.Y << "x" << m_cache_extent.Z
			<< "=" << getVolume();
	}

	/// Minimum edge of the area (inclusive)
	/// @warning read-only!
	v3s16 MinEdge = v3s16(1,1,1);
	/// Maximum edge of the area (inclusive)
	/// @warning read-only!
	v3s16 MaxEdge;

private:
	void cacheExtent()
	{
		m_cache_extent = {
			MaxEdge.X - MinEdge.X + 1,
			MaxEdge.Y - MinEdge.Y + 1,
			MaxEdge.Z - MinEdge.Z + 1
		};
		// If positions were sorted correctly this must always hold.
		// Note that this still permits empty areas (where MinEdge = MaxEdge + 1).
		assert(m_cache_extent.X >= 0 && m_cache_extent.X <= MAX_EXTENT);
		assert(m_cache_extent.Y >= 0 && m_cache_extent.Y <= MAX_EXTENT);
		assert(m_cache_extent.Z >= 0 && m_cache_extent.Z <= MAX_EXTENT);
	}

	static constexpr s32 MAX_EXTENT = S16_MAX - S16_MIN + 1;
	v3s32 m_cache_extent;
};

enum : u8 {
	VOXELFLAG_NO_DATA  = 1 << 0, // no data about that node
	VOXELFLAG_CHECKED1 = 1 << 1, // Algorithm-dependent
	VOXELFLAG_CHECKED2 = 1 << 2, // Algorithm-dependent
	VOXELFLAG_CHECKED3 = 1 << 3, // Algorithm-dependent
	VOXELFLAG_CHECKED4 = 1 << 4, // Algorithm-dependent
};

enum VoxelPrintMode
{
	VOXELPRINT_NOTHING,
	VOXELPRINT_MATERIAL,
	VOXELPRINT_WATERPRESSURE,
	VOXELPRINT_LIGHT_DAY,
};

class VoxelManipulator
{
public:
	VoxelManipulator() = default;
	virtual ~VoxelManipulator();

	/*
		These are a bit slow and shouldn't be used internally.
		Use m_data[m_area.index(p)] instead.
	*/
	MapNode getNode(const v3s16 &p)
	{
		VoxelArea voxel_area(p);
		addArea(voxel_area);

		const s32 index = m_area.index(p);

		if (m_flags[index] & VOXELFLAG_NO_DATA) {
			throw InvalidPositionException
			("VoxelManipulator: getNode: inexistent");
		}

		return m_data[index];
	}
	MapNode getNodeNoEx(const v3s16 &p)
	{
		VoxelArea voxel_area(p);
		addArea(voxel_area);

		const s32 index = m_area.index(p);

		if (m_flags[index] & VOXELFLAG_NO_DATA) {
			return {CONTENT_IGNORE};
		}

		return m_data[index];
	}
	MapNode getNodeNoExNoEmerge(const v3s16 &p) const
	{
		if (!m_area.contains(p))
			return {CONTENT_IGNORE};
		const s32 index = m_area.index(p);
		if (m_flags[index] & VOXELFLAG_NO_DATA)
			return {CONTENT_IGNORE};
		return m_data[index];
	}
	// Stuff explodes if non-emerged area is touched with this.
	// Emerge first, and check VOXELFLAG_NO_DATA if appropriate.
	MapNode & getNodeRefUnsafe(const v3s16 &p)
	{
		return m_data[m_area.index(p)];
	}

	const MapNode & getNodeRefUnsafeCheckFlags(const v3s16 &p) const
	{
		s32 index = m_area.index(p);

		if (m_flags[index] & VOXELFLAG_NO_DATA)
			return ContentIgnoreNode;

		return m_data[index];
	}

	u8 & getFlagsRefUnsafe(const v3s16 &p)
	{
		return m_flags[m_area.index(p)];
	}

	bool exists(const v3s16 &p)
	{
		return m_area.contains(p) &&
			!(getFlagsRefUnsafe(p) & VOXELFLAG_NO_DATA);
	}

	void setNode(const v3s16 &p, const MapNode &n)
	{
		VoxelArea voxel_area(p);
		addArea(voxel_area);

		const s32 index = m_area.index(p);

		m_data[index] = n;
		m_flags[index] &= ~VOXELFLAG_NO_DATA;
	}

	/*
		Set stuff if available without an emerge.
		Return false if failed.
		This is convenient but slower than playing around directly
		with the m_data table with indices.
	*/
	bool setNodeNoEmerge(const v3s16 &p, MapNode n)
	{
		if(!m_area.contains(p))
			return false;
		const s32 index = m_area.index(p);
		m_data[index] = n;
		m_flags[index] &= ~VOXELFLAG_NO_DATA;
		return true;
	}

	/*
		Control
	*/

	void clear();

	void print(std::ostream &o, const NodeDefManager *nodemgr,
			VoxelPrintMode mode=VOXELPRINT_MATERIAL) const;

	void addArea(const VoxelArea &area);

	void setFlags(const VoxelArea &area, u8 flag);
	void clearFlags(const VoxelArea &area, u8 flag);

	/*
		Copy data and set flags to 0
		dst_area.getExtent() <= src_area.getExtent()
	*/
	void copyFrom(MapNode *src, const VoxelArea& src_area,
			v3s16 from_pos, v3s16 to_pos, const v3s16 &size);

	// Copy data
	void copyTo(MapNode *dst, const VoxelArea& dst_area,
			v3s16 dst_pos, v3s16 from_pos, const v3s16 &size) const;

	/*
		Member variables
	*/

	/*
		The area that is stored in m_data.
		MaxEdge is 1 higher than maximum allowed position.
	*/
	VoxelArea m_area;

	/*
		nullptr if data size is 0 (empty extent)
		Data is stored as [z*h*w + y*h + x]
	*/
	MapNode *m_data = nullptr;

	/*
		Flags of all nodes
	*/
	u8 *m_flags = nullptr;

	static const MapNode ContentIgnoreNode;
};
