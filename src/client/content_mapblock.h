// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "nodedef.h"
#include "tile.h"
#include <array>

struct MeshMakeData;
struct MeshCollector;

enum class QuadDiagonal {
	Diag02,
	Diag13,
};

struct LightPair {
	u8 lightDay;
	u8 lightNight;

	LightPair() = default;
	explicit LightPair(u16 value) : lightDay(value & 0xff), lightNight(value >> 8) {}
	LightPair(u8 valueA, u8 valueB) : lightDay(valueA), lightNight(valueB) {}
	LightPair(float valueA, float valueB) :
		lightDay(core::clamp(core::round32(valueA), 0, 255)),
		lightNight(core::clamp(core::round32(valueB), 0, 255)) {}
	operator u16() const { return lightDay | lightNight << 8; }

	static int lightDiff(LightPair a, LightPair b);
	static QuadDiagonal quadDiagonalForFace(LightPair final_lights[4]);
};

struct LightInfo {
	float light_day;
	float light_night;
	float light_boosted;

	LightPair getPair(float sunlight_boost = 0.0f) const
	{
		return LightPair(
			(1 - sunlight_boost) * light_day
			+ sunlight_boost * light_boosted,
			light_night);
	}
};

struct LightFrame {
	f32 lightsDay[8];
	f32 lightsNight[8];
	bool sunlight[8];
};

class MapblockMeshGenerator
{
public:
	MapblockMeshGenerator(MeshMakeData *input, MeshCollector *output);
	void generate();

private:
	MeshMakeData *const data;
	MeshCollector *const collector;

	const NodeDefManager *const nodedef;

	const v3s16 blockpos_nodes;

// current node
	struct {
		v3s16 p; // relative to blockpos_nodes
		v3f origin; // p in BS space
		MapNode n;
		const ContentFeatures *f;
		LightFrame lframe; // smooth lighting
		video::SColor lcolor; // unsmooth lighting
	} cur_node;

// lighting
	void getSmoothLightFrame();
	LightInfo blendLight(const v3f &vertex_pos);
	video::SColor blendLightColor(const v3f &vertex_pos);
	video::SColor blendLightColor(const v3f &vertex_pos, const v3f &vertex_normal);

	void useTile(TileSpec *tile_ret, int index = 0, u8 set_flags = 0,
		u8 reset_flags = 0, bool special = false);
	void getTile(int index, TileSpec *tile_ret);
	void getTile(v3s16 direction, TileSpec *tile_ret);
	void getSpecialTile(int index, TileSpec *tile_ret, bool apply_crack = false);

// face drawing
	void drawQuad(const TileSpec &tile, v3f *vertices, const v3s16 &normal = v3s16(0, 0, 0),
		float vertical_tiling = 1.0);

// cuboid drawing!
	template <typename Fn>
	void drawCuboid(const aabb3f &box, const TileSpec *tiles, int tilecount,
			const f32 *txc, u8 mask, Fn &&face_lighter);
	static void generateCuboidTextureCoords(aabb3f const &box, f32 *coords);
	void drawAutoLightedCuboid(aabb3f box, const TileSpec &tile, f32 const *txc	= nullptr, u8 mask = 0);
	void drawAutoLightedCuboid(aabb3f box, const TileSpec *tiles, int tile_count, f32 const *txc = nullptr, u8 mask = 0);
	u8 getNodeBoxMask(aabb3f box, u8 solid_neighbors, u8 sametype_neighbors) const;

// solid-specific
public:
	static bool isSolidFaceDrawn(content_t n1, const ContentFeatures &f1,
		content_t n2, const NodeDefManager *nodedef,
		bool *backface_culling_out = nullptr);

// liquid-specific
	struct LiquidData {
		struct NeighborData {
			f32 level; // in range [-0.5, 0.5]
			content_t content;
			bool is_same_liquid;
			bool top_is_same_liquid;
		};

		bool top_is_same_liquid;
		bool draw_bottom;
		TileSpec tile;
		TileSpec tile_top;
		content_t c_flowing;
		content_t c_source;
		video::SColor color_top;
		std::array<std::array<NeighborData, 3>, 3> neighbors;
		std::array<std::array<f32, 2>, 2> corner_levels;
	};
	LiquidData cur_liquid;

	static std::array<std::array<LiquidData::NeighborData, 3>, 3>
	getLiquidNeighborsData(content_t c_source, content_t c_flowing, u8 liquid_range,
			const std::function<MapNode(v3s16)> &get_node_rel);

	// the arrays are indexed Z-first, i.e. neighbors[z][x], ret[z][x]
	static std::array<std::array<f32, 2>, 2>
	calculateLiquidCornerLevels(content_t c_source, content_t c_flowing,
			const std::array<std::array<LiquidData::NeighborData, 3>, 3> &neighbors);

private:
	template <typename F>
	static std::array<std::array<LiquidData::NeighborData, 3>, 3>
	getLiquidNeighborsDataRaw(content_t c_source, content_t c_flowing,
			u8 liquid_range, F &&get_node_rel);

	static f32 calculateLiquidCornerLevel(content_t c_source, content_t c_flowing,
			const std::array<std::reference_wrapper<const LiquidData::NeighborData>, 4> &neighbors);

	void prepareLiquidNodeDrawing();
	void getLiquidNeighborhood();
	void calculateCornerLevels();
	void drawLiquidSides();
	void drawLiquidTop();
	void drawLiquidBottom();

// raillike-specific
	// name of the group that enables connecting to raillike nodes of different kind
	static const std::string raillike_groupname;
	struct RaillikeData {
		int raillike_group;
	};
	RaillikeData cur_rail;
	bool isSameRail(v3s16 dir);

// plantlike-specific
	struct PlantlikeData {
		PlantlikeStyle draw_style;
		v3f offset;
		float scale;
		float rotate_degree;
		bool random_offset_Y;
		int face_num;
		float plant_height;
	};
	PlantlikeData cur_plant;

	void drawPlantlikeQuad(const TileSpec &tile, float rotation, float quad_offset = 0,
		bool offset_top_only = false);
	void drawPlantlike(const TileSpec &tile, bool is_rooted = false);

// firelike-specific
	void drawFirelikeQuad(const TileSpec &tile, float rotation, float opening_angle,
		float offset_h, float offset_v = 0.0);

// drawtypes
	void drawSolidNode();
	void drawLiquidNode();
	void drawGlasslikeNode();
	void drawGlasslikeFramedNode();
	void drawAllfacesNode();
	void drawTorchlikeNode();
	void drawSignlikeNode();
	void drawPlantlikeNode();
	void drawPlantlikeRootedNode();
	void drawFirelikeNode();
	void drawFencelikeNode();
	void drawRaillikeNode();
	void drawNodeboxNode();
	void drawMeshNode();

// common
	void errorUnknownDrawtype();
	void drawNode();
};
