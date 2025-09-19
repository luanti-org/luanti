// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include "light.h"
#include "util/pointer.h"
#include <string>
#include <vector>

class NodeDefManager;
class Map;

/*
	Naming scheme:
	- Material = irrlicht's Material class
	- Content = (content_t) content of a node
	- Tile = TileSpec at some side of a node of some content type
*/
typedef u16 content_t;
#define CONTENT_MAX UINT16_MAX

/*
	The maximum node ID that can be registered by mods. This must
	be significantly lower than the maximum content_t value, so that
	there is enough room for dummy node IDs, which are created when
	a MapBlock containing unknown node names is loaded from disk.
*/
#define MAX_REGISTERED_CONTENT 0x7fffU

/*
	A solid walkable node with the texture unknown_node.png.

	For example, used on the client to display unregistered node IDs
	(instead of expanding the vector of node definitions each time
	such a node is received).
*/
#define CONTENT_UNKNOWN 125

/*
	The common material through which the player can walk and which
	is transparent to light
*/
#define CONTENT_AIR 126

/*
	Ignored node.

	Unloaded chunks are considered to consist of this. Several other
	methods return this when an error occurs. Also, during
	map generation this means the node has not been set yet.

	Doesn't create faces with anything and is considered being
	out-of-map in the game map.
*/
#define CONTENT_IGNORE 127

/*
	Content lighting information that fits into a single byte.
*/
struct ContentLightingFlags {
	u8 light_source : 4;
	bool has_light : 1;
	bool light_propagates : 1;
	bool sunlight_propagates : 1;

	bool operator==(const ContentLightingFlags &other) const
	{
		return has_light == other.has_light && light_propagates == other.light_propagates &&
				sunlight_propagates == other.sunlight_propagates &&
				light_source == other.light_source;
	}
	bool operator!=(const ContentLightingFlags &other) const { return !(*this == other); }
};
static_assert(sizeof(ContentLightingFlags) == 1, "Unexpected ContentLightingFlags size");

enum LightBank
{
	LIGHTBANK_DAY,
	LIGHTBANK_NIGHT
};

/*
	Simple rotation enum.
*/
enum Rotation {
	ROTATE_0,
	ROTATE_90,
	ROTATE_180,
	ROTATE_270,
	ROTATE_RAND,
};

/*
	Masks for MapNode.param2 of flowing liquids
 */
#define LIQUID_LEVEL_MASK 0x07
#define LIQUID_FLOW_DOWN_MASK 0x08

//#define LIQUID_LEVEL_MASK 0x3f // better finite water
//#define LIQUID_FLOW_DOWN_MASK 0x40 // not used when finite water

/* maximum amount of liquid in a block */
#define LIQUID_LEVEL_MAX LIQUID_LEVEL_MASK
#define LIQUID_LEVEL_SOURCE (LIQUID_LEVEL_MAX+1)

#define LIQUID_INFINITY_MASK 0x80 //0b10000000

// mask for leveled nodebox param2
#define LEVELED_MASK 0x7F
#define LEVELED_MAX LEVELED_MASK


struct ContentFeatures;

/*
	This is the stuff what the whole world consists of.
*/


struct alignas(u32) MapNode
{
	/*
		Main content
	*/
	u16 param0;

	/*
		Misc parameter. Initialized to 0.
		- For light_propagates() blocks, this is light intensity,
		  stored logarithmically from 0 to LIGHT_MAX.
		  Sunlight is LIGHT_SUN, which is LIGHT_MAX+1.
		  - Contains 2 values, day- and night lighting. Each takes 4 bits.
		- Uhh... well, most blocks have light or nothing in here.
	*/
	u8 param1;

	/*
		The second parameter. Initialized to 0.
		E.g. direction for torches and flowing water.
	*/
	u8 param2;

	MapNode() = default;

	constexpr MapNode(content_t content, u8 a_param1=0, u8 a_param2=0) noexcept
		: param0(content),
		  param1(a_param1),
		  param2(a_param2)
	{ }

	bool operator==(const MapNode &other) const noexcept
	{
		return (param0 == other.param0
				&& param1 == other.param1
				&& param2 == other.param2);
	}
	bool operator!=(const MapNode &other) const noexcept
	{
		return !(*this == other);
	}

	// To be used everywhere
	content_t getContent() const noexcept
	{
		return param0;
	}
	void setContent(content_t c) noexcept
	{
		param0 = c;
	}
	u8 getParam1() const noexcept
	{
		return param1;
	}
	void setParam1(u8 p) noexcept
	{
		param1 = p;
	}
	u8 getParam2() const noexcept
	{
		return param2;
	}
	void setParam2(u8 p) noexcept
	{
		param2 = p;
	}

	/*!
	 * Returns the color of the node.
	 *
	 * \param f content features of this node
	 * \param color output, contains the node's color.
	 */
	void getColor(const ContentFeatures &f, video::SColor *color) const;

	inline void setLight(LightBank bank, u8 a_light, ContentLightingFlags f) noexcept
	{
		// If node doesn't contain light data, ignore this
		if (!f.has_light)
			return;
		if (bank == LIGHTBANK_DAY) {
			param1 &= 0xf0;
			param1 |= a_light & 0x0f;
		} else {
			assert(bank == LIGHTBANK_NIGHT);
			param1 &= 0x0f;
			param1 |= (a_light & 0x0f)<<4;
		}
	}

	/**
	 * Check if the light value for night differs from the light value for day.
	 *
	 * @return If the light values are equal, returns true; otherwise false
	 */
	inline bool isLightDayNightEq(ContentLightingFlags f) const noexcept
	{
		return !f.has_light || getLight(LIGHTBANK_DAY, f) == getLight(LIGHTBANK_NIGHT, f);
	}

	inline u8 getLight(LightBank bank, ContentLightingFlags f) const noexcept
	{
		u8 raw_light = getLightRaw(bank, f);
		return MYMAX(f.light_source, raw_light);
	}

	/*!
	 * Returns the node's light level from param1.
	 * If the node emits light, it is ignored.
	 * \param f the ContentLightingFlags of this node.
	 */
	inline u8 getLightRaw(LightBank bank, ContentLightingFlags f) const noexcept
	{
		if(f.has_light)
			return bank == LIGHTBANK_DAY ? param1 & 0x0f : (param1 >> 4) & 0x0f;
		return 0;
	}

	// 0 <= daylight_factor <= 1000
	// 0 <= return value <= LIGHT_SUN
	u8 getLightBlend(u32 daylight_factor, ContentLightingFlags f) const
	{
		u8 lightday = getLight(LIGHTBANK_DAY, f);
		u8 lightnight = getLight(LIGHTBANK_NIGHT, f);
		return blend_light(daylight_factor, lightday, lightnight);
	}

	u8 getFaceDir(const NodeDefManager *nodemgr, bool allow_wallmounted = false) const;
	u8 getWallMounted(const NodeDefManager *nodemgr) const;
	v3s16 getWallMountedDir(const NodeDefManager *nodemgr) const;

	/// @returns Rotation in range 0–239 (in 1.5° steps)
	u8 getDegRotate(const NodeDefManager *nodemgr) const;

	void rotateAlongYAxis(const NodeDefManager *nodemgr, Rotation rot);

	/*!
	 * Checks which neighbors does this node connect to.
	 *
	 * \param p coordinates of the node
	 */
	u8 getNeighbors(v3s16 p, Map *map) const;

	/*
		Gets list of node boxes (used for rendering (NDT_NODEBOX))
	*/
	void getNodeBoxes(const NodeDefManager *nodemgr, std::vector<aabb3f> *boxes,
		u8 neighbors = 0) const;

	/*
		Gets list of selection boxes
	*/
	void getSelectionBoxes(const NodeDefManager *nodemg,
		std::vector<aabb3f> *boxes, u8 neighbors = 0) const;

	/*
		Gets list of collision boxes
	*/
	void getCollisionBoxes(const NodeDefManager *nodemgr,
		std::vector<aabb3f> *boxes, u8 neighbors = 0) const;

	/*
		Liquid/leveled helpers
	*/
	u8 getMaxLevel(const NodeDefManager *nodemgr) const;
	u8 getLevel(const NodeDefManager *nodemgr) const;
	s8 setLevel(const NodeDefManager *nodemgr, s16 level = 1);
	s8 addLevel(const NodeDefManager *nodemgr, s16 add = 1);

	/*
		Serialization functions
	*/

	static u32 serializedLength(u8 version);
	void serialize(u8 *dest, u8 version) const;
	void deSerialize(const u8 *source, u8 version);

	// Serializes or deserializes a list of nodes in bulk format (first the
	// content of all nodes, then the param1 of all nodes, then the param2
	// of all nodes).
	//   version = serialization version. Must be >= 22
	//   content_width = the number of bytes of content per node
	//   params_width = the number of bytes of params per node
	//   compressed = true to zlib-compress output
	//   is_mono_block = if true, nodes is array of size 1
	static Buffer<u8> serializeBulk(int version,
			const MapNode *nodes, u32 nodecount,
			u8 content_width, u8 params_width, bool is_mono_block = false);
	static void deSerializeBulk(std::istream &is, int version,
			MapNode *nodes, u32 nodecount,
			u8 content_width, u8 params_width);

private:
	// Deprecated serialization methods
	void deSerialize_pre22(const u8 *source, u8 version);
};
