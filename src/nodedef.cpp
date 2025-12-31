// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "nodedef.h"

#include "itemdef.h"
#include "log.h"
#include "settings.h"
#include "nameidmapping.h"
#include "util/numeric.h"
#include "util/serialize.h"
#include "util/string.h"
#include "exceptions.h"
#include "debug.h"
#include "gamedef.h"
#include "mapnode.h"
#include <algorithm>
#include <cmath>
#if CHECK_CLIENT_BUILD()
#include "client/node_visuals.h" // ~NodeVisuals
#endif

/*
	NodeBox
*/

void NodeBox::reset()
{
	type = NODEBOX_REGULAR;
	// default is empty
	fixed.clear();
	// default is sign/ladder-like
	wall_top = aabb3f(-BS/2, BS/2-BS/16., -BS/2, BS/2, BS/2, BS/2);
	wall_bottom = aabb3f(-BS/2, -BS/2, -BS/2, BS/2, -BS/2+BS/16., BS/2);
	wall_side = aabb3f(-BS/2, -BS/2, -BS/2, -BS/2+BS/16., BS/2, BS/2);
	// no default for other parts
	connected.reset();
}

void NodeBox::serialize(std::ostream &os, u16 protocol_version) const
{
	writeU8(os, 6); // version. Protocol >= 36

	switch (type) {
	case NODEBOX_LEVELED:
	case NODEBOX_FIXED:
		writeU8(os, type);

		writeU16(os, fixed.size());
		for (const aabb3f &nodebox : fixed) {
			writeV3F32(os, nodebox.MinEdge);
			writeV3F32(os, nodebox.MaxEdge);
		}
		break;
	case NODEBOX_WALLMOUNTED:
		writeU8(os, type);

		writeV3F32(os, wall_top.MinEdge);
		writeV3F32(os, wall_top.MaxEdge);
		writeV3F32(os, wall_bottom.MinEdge);
		writeV3F32(os, wall_bottom.MaxEdge);
		writeV3F32(os, wall_side.MinEdge);
		writeV3F32(os, wall_side.MaxEdge);
		break;
	case NODEBOX_CONNECTED: {
		writeU8(os, type);

#define WRITEBOX(box) \
		writeU16(os, (box).size()); \
		for (const aabb3f &i: (box)) { \
			writeV3F32(os, i.MinEdge); \
			writeV3F32(os, i.MaxEdge); \
		};

		const auto &c = getConnected();

		WRITEBOX(fixed);
		WRITEBOX(c.connect_top);
		WRITEBOX(c.connect_bottom);
		WRITEBOX(c.connect_front);
		WRITEBOX(c.connect_left);
		WRITEBOX(c.connect_back);
		WRITEBOX(c.connect_right);
		WRITEBOX(c.disconnected_top);
		WRITEBOX(c.disconnected_bottom);
		WRITEBOX(c.disconnected_front);
		WRITEBOX(c.disconnected_left);
		WRITEBOX(c.disconnected_back);
		WRITEBOX(c.disconnected_right);
		WRITEBOX(c.disconnected);
		WRITEBOX(c.disconnected_sides);
		break;
	}
	default:
		writeU8(os, type);
		break;
	}
}

void NodeBox::deSerialize(std::istream &is)
{
	if (readU8(is) < 6)
		throw SerializationError("unsupported NodeBox version");

	reset();

	type = static_cast<NodeBoxType>(readU8(is));
	switch (type) {
		case NODEBOX_REGULAR:
			break;
		case NODEBOX_FIXED:
		case NODEBOX_LEVELED: {
			u16 fixed_count = readU16(is);
			while(fixed_count--) {
				aabb3f box{{0.0f, 0.0f, 0.0f}};
				box.MinEdge = readV3F32(is);
				box.MaxEdge = readV3F32(is);
				fixed.push_back(box);
			}
			break;
		}
		case NODEBOX_WALLMOUNTED:
			wall_top.MinEdge = readV3F32(is);
			wall_top.MaxEdge = readV3F32(is);
			wall_bottom.MinEdge = readV3F32(is);
			wall_bottom.MaxEdge = readV3F32(is);
			wall_side.MinEdge = readV3F32(is);
			wall_side.MaxEdge = readV3F32(is);
			break;
		case NODEBOX_CONNECTED: {
#define READBOXES(box) { \
			count = readU16(is); \
			(box).reserve(count); \
			while (count--) { \
				v3f min = readV3F32(is); \
				v3f max = readV3F32(is); \
				(box).emplace_back(min, max); }; }

			u16 count;

			auto &c = getConnected();

			READBOXES(fixed);
			READBOXES(c.connect_top);
			READBOXES(c.connect_bottom);
			READBOXES(c.connect_front);
			READBOXES(c.connect_left);
			READBOXES(c.connect_back);
			READBOXES(c.connect_right);
			READBOXES(c.disconnected_top);
			READBOXES(c.disconnected_bottom);
			READBOXES(c.disconnected_front);
			READBOXES(c.disconnected_left);
			READBOXES(c.disconnected_back);
			READBOXES(c.disconnected_right);
			READBOXES(c.disconnected);
			READBOXES(c.disconnected_sides);
			break;
		}
		default:
			type = NODEBOX_REGULAR;
			break;
	}
}

/*
	TileDef
*/

#define TILE_FLAG_BACKFACE_CULLING	(1 << 0)
#define TILE_FLAG_TILEABLE_HORIZONTAL	(1 << 1)
#define TILE_FLAG_TILEABLE_VERTICAL	(1 << 2)
#define TILE_FLAG_HAS_COLOR	(1 << 3)
#define TILE_FLAG_HAS_SCALE	(1 << 4)
#define TILE_FLAG_HAS_ALIGN_STYLE	(1 << 5)

void TileDef::serialize(std::ostream &os, u16 protocol_version) const
{
	// protocol_version >= 36
	u8 version = 6;
	writeU8(os, version);

	if (protocol_version > 39) {
		os << serializeString16(name);
	} else {
		// Before f018737, TextureSource::getTextureAverageColor did not handle
		// missing textures. "[png" can be used as base texture, but is not known
		// on older clients. Hence use "blank.png" to avoid this problem.
		// To be forward-compatible with future base textures/modifiers,
		// we apply the same prefix to any texture beginning with [,
		// except for the ones that are supported on older clients.
		bool pass_through = true;

		if (!name.empty() && name[0] == '[') {
			pass_through = str_starts_with(name, "[combine:") ||
				str_starts_with(name, "[inventorycube{") ||
				str_starts_with(name, "[lowpart:");
		}

		if (pass_through)
			os << serializeString16(name);
		else
			os << serializeString16("blank.png^" + name);
	}
	animation.serialize(os, version);
	bool has_scale = scale > 0;
	u16 flags = 0;
	if (backface_culling)
		flags |= TILE_FLAG_BACKFACE_CULLING;
	if (tileable_horizontal)
		flags |= TILE_FLAG_TILEABLE_HORIZONTAL;
	if (tileable_vertical)
		flags |= TILE_FLAG_TILEABLE_VERTICAL;
	if (has_color)
		flags |= TILE_FLAG_HAS_COLOR;
	if (has_scale)
		flags |= TILE_FLAG_HAS_SCALE;
	if (align_style != ALIGN_STYLE_NODE)
		flags |= TILE_FLAG_HAS_ALIGN_STYLE;
	writeU16(os, flags);
	if (has_color) {
		writeU8(os, color.getRed());
		writeU8(os, color.getGreen());
		writeU8(os, color.getBlue());
	}
	if (has_scale)
		writeU8(os, scale);
	if (align_style != ALIGN_STYLE_NODE)
		writeU8(os, align_style);
}

void TileDef::deSerialize(std::istream &is, NodeDrawType drawtype, u16 protocol_version)
{
	if (readU8(is) < 6)
		throw SerializationError("unsupported TileDef version");

	name = deSerializeString16(is);
	animation.deSerialize(is, protocol_version);
	u16 flags = readU16(is);
	backface_culling = flags & TILE_FLAG_BACKFACE_CULLING;
	tileable_horizontal = flags & TILE_FLAG_TILEABLE_HORIZONTAL;
	tileable_vertical = flags & TILE_FLAG_TILEABLE_VERTICAL;
	has_color = flags & TILE_FLAG_HAS_COLOR;
	bool has_scale = flags & TILE_FLAG_HAS_SCALE;
	bool has_align_style = flags & TILE_FLAG_HAS_ALIGN_STYLE;
	if (has_color) {
		color.setRed(readU8(is));
		color.setGreen(readU8(is));
		color.setBlue(readU8(is));
	}
	scale = has_scale ? readU8(is) : 0;
	if (has_align_style) {
		align_style = static_cast<AlignStyle>(readU8(is));
		if (align_style >= AlignStyle_END)
			align_style = ALIGN_STYLE_NODE;
	} else {
		align_style = ALIGN_STYLE_NODE;
	}
}

/*
	TextureSettings
*/

void TextureSettings::readSettings()
{
	connected_glass                = g_settings->getBool("connected_glass");
	translucent_liquids            = g_settings->getBool("translucent_liquids");
	enable_minimap                 = g_settings->getBool("enable_minimap");
	node_texture_size              = rangelim(g_settings->getU16("texture_min_size"),
		TEXTURE_FILTER_MIN_SIZE, 16384);
	std::string leaves_style_str   = g_settings->get("leaves_style");
	std::string world_aligned_mode_str = g_settings->get("world_aligned_mode");
	std::string autoscale_mode_str = g_settings->get("autoscale_mode");

	if (leaves_style_str == "fancy") {
		leaves_style = LEAVES_FANCY;
	} else if (leaves_style_str == "simple") {
		leaves_style = LEAVES_SIMPLE;
	} else {
		leaves_style = LEAVES_OPAQUE;
	}

	if (world_aligned_mode_str == "enable")
		world_aligned_mode = WORLDALIGN_ENABLE;
	else if (world_aligned_mode_str == "force_solid")
		world_aligned_mode = WORLDALIGN_FORCE;
	else if (world_aligned_mode_str == "force_nodebox")
		world_aligned_mode = WORLDALIGN_FORCE_NODEBOX;
	else
		world_aligned_mode = WORLDALIGN_DISABLE;

	if (autoscale_mode_str == "enable")
		autoscale_mode = AUTOSCALE_ENABLE;
	else if (autoscale_mode_str == "force")
		autoscale_mode = AUTOSCALE_FORCE;
	else
		autoscale_mode = AUTOSCALE_DISABLE;
}


/*
	ContentFeatures
*/

ContentFeatures::ContentFeatures()
{
	reset();
}

ContentFeatures::~ContentFeatures()
{
#if CHECK_CLIENT_BUILD()
	delete visuals;
#endif
}

void ContentFeatures::reset()
{
	/*
		Cached stuff
	*/
	has_on_construct = false;
	has_on_destruct = false;
	has_after_destruct = false;
	floats = false;

	/*
		Actual data

		NOTE: Most of this is always overridden by the default values given
		      in builtin.lua
	*/
	name.clear();
	groups.clear();
	// Unknown nodes can be dug
	groups["dig_immediate"] = 2;
	drawtype = NDT_NORMAL;
	mesh.clear();
	visual_scale = 1.0;
	for (auto &i : tiledef)
		i = TileDef();
	for (auto &j : tiledef_special)
		j = TileDef();
	alpha = ALPHAMODE_OPAQUE;
	post_effect_color = video::SColor(0, 0, 0, 0);
	param_type = CPT_NONE;
	param_type_2 = CPT2_NONE;
	is_ground_content = false;
	light_propagates = false;
	sunlight_propagates = false;
	walkable = true;
	pointable = PointabilityType::POINTABLE;
	diggable = true;
	climbable = false;
	buildable_to = false;
	floodable = false;
	rightclickable = true;
	leveled = 0;
	leveled_max = LEVELED_MAX;
	liquid_type = LIQUID_NONE;
	liquid_alternative_flowing.clear();
	liquid_alternative_flowing_id = CONTENT_IGNORE;
	liquid_alternative_source.clear();
	liquid_alternative_source_id = CONTENT_IGNORE;
	liquid_viscosity = 0;
	liquid_renewable = true;
	liquid_range = LIQUID_LEVEL_MAX+1;
	drowning = 0;
	light_source = 0;
	damage_per_second = 0;
	node_box.reset();
	selection_box.reset();
	collision_box.reset();
	waving = 0;
	legacy_facedir_simple = false;
	legacy_wallmounted = false;
	sound_footstep = SoundSpec();
	sound_dig = SoundSpec("__group");
	sound_dug = SoundSpec();
	connects_to.clear();
	connects_to_ids.clear();
	connect_sides = 0;
	color = video::SColor(0xFFFFFFFF);
	palette_name.clear();
	node_dig_prediction = "air";
	move_resistance = 0;
	liquid_move_physics = false;
	post_effect_color_shaded = false;
}

void ContentFeatures::setAlphaFromLegacy(u8 legacy_alpha)
{
	switch (drawtype) {
	case NDT_NORMAL:
		alpha = legacy_alpha == 255 ? ALPHAMODE_OPAQUE : ALPHAMODE_CLIP;
		break;
	case NDT_LIQUID:
	case NDT_FLOWINGLIQUID:
		alpha = legacy_alpha == 255 ? ALPHAMODE_OPAQUE : ALPHAMODE_BLEND;
		break;
	default:
		alpha = legacy_alpha == 255 ? ALPHAMODE_CLIP : ALPHAMODE_BLEND;
		break;
	}
}

u8 ContentFeatures::getAlphaForLegacy() const
{
	// This is so simple only because 255 and 0 mean wildly different things
	// depending on drawtype...
	return alpha == ALPHAMODE_OPAQUE ? 255 : 0;
}

void ContentFeatures::serialize(std::ostream &os, u16 protocol_version) const
{
	writeU8(os, CONTENTFEATURES_VERSION);

	// general
	os << serializeString16(name);
	writeU16(os, groups.size());
	for (const auto &group : groups) {
		os << serializeString16(group.first);
		if (protocol_version < 41 && group.first.compare("bouncy") == 0) {
			// Old clients may choke on negative bouncy value
			writeS16(os, abs(group.second));
		} else {
			writeS16(os, group.second);
		}
	}
	writeU8(os, param_type);
	writeU8(os, param_type_2);

	// visual
	writeU8(os, drawtype);
	os << serializeString16(mesh);
	writeF32(os, visual_scale);
	writeU8(os, 6);
	for (const TileDef &td : tiledef)
		td.serialize(os, protocol_version);
	for (const TileDef &td : tiledef_overlay)
		td.serialize(os, protocol_version);
	writeU8(os, CF_SPECIAL_COUNT);
	for (const TileDef &td : tiledef_special) {
		td.serialize(os, protocol_version);
	}
	writeU8(os, getAlphaForLegacy());
	writeU8(os, color.getRed());
	writeU8(os, color.getGreen());
	writeU8(os, color.getBlue());
	os << serializeString16(palette_name);
	writeU8(os, waving);
	writeU8(os, connect_sides);
	writeU16(os, connects_to_ids.size());
	for (u16 connects_to_id : connects_to_ids)
		writeU16(os, connects_to_id);
	writeARGB8(os, post_effect_color);
	writeU8(os, leveled);

	// lighting
	writeU8(os, light_propagates);
	writeU8(os, sunlight_propagates);
	writeU8(os, light_source);

	// map generation
	writeU8(os, is_ground_content);

	// interaction
	writeU8(os, walkable);
	Pointabilities::serializePointabilityType(os, pointable);
	writeU8(os, diggable);
	writeU8(os, climbable);
	writeU8(os, buildable_to);
	writeU8(os, rightclickable);
	writeU32(os, damage_per_second);

	// liquid
	LiquidType liquid_type_bc = liquid_type;
	if (protocol_version <= 39) {
		// Since commit 7f25823, liquid drawtypes can be used even with LIQUID_NONE
		// solution: force liquid type accordingly to accepted values
		if (drawtype == NDT_LIQUID)
			liquid_type_bc = LIQUID_SOURCE;
		else if (drawtype == NDT_FLOWINGLIQUID)
			liquid_type_bc = LIQUID_FLOWING;
	}
	writeU8(os, liquid_type_bc);
	os << serializeString16(liquid_alternative_flowing);
	os << serializeString16(liquid_alternative_source);
	writeU8(os, liquid_viscosity);
	writeU8(os, liquid_renewable);
	writeU8(os, liquid_range);
	writeU8(os, drowning);
	writeU8(os, floodable);

	// node boxes
	node_box.serialize(os, protocol_version);
	selection_box.serialize(os, protocol_version);
	collision_box.serialize(os, protocol_version);

	// sound
	sound_footstep.serializeSimple(os, protocol_version);
	sound_dig.serializeSimple(os, protocol_version);
	sound_dug.serializeSimple(os, protocol_version);

	// legacy
	writeU8(os, legacy_facedir_simple);
	writeU8(os, legacy_wallmounted);

	// new attributes
	os << serializeString16(node_dig_prediction);
	writeU8(os, leveled_max);
	writeU8(os, alpha);
	writeU8(os, move_resistance);
	writeU8(os, liquid_move_physics);
	writeU8(os, post_effect_color_shaded);
}

void ContentFeatures::deSerialize(std::istream &is, u16 protocol_version)
{
	if (readU8(is) < CONTENTFEATURES_VERSION)
		throw SerializationError("unsupported ContentFeatures version");

	// general
	name = deSerializeString16(is);
	groups.clear();
	u32 groups_size = readU16(is);
	for (u32 i = 0; i < groups_size; i++) {
		std::string name = deSerializeString16(is);
		int value = readS16(is);
		groups[name] = value;
	}

	param_type = static_cast<ContentParamType>(readU8(is));
	if (param_type >= ContentParamType_END)
		param_type = CPT_NONE;

	param_type_2 = static_cast<ContentParamType2>(readU8(is));
	if (param_type_2 >= ContentParamType2_END)
		param_type_2 = CPT2_NONE;

	// visual
	drawtype = static_cast<NodeDrawType>(readU8(is));
	if (drawtype >= NodeDrawType_END)
		drawtype = NDT_NORMAL;
	mesh = deSerializeString16(is);
	visual_scale = readF32(is);
	if (readU8(is) != 6)
		throw SerializationError("unsupported tile count");
	for (TileDef &td : tiledef)
		td.deSerialize(is, drawtype, protocol_version);
	for (TileDef &td : tiledef_overlay)
		td.deSerialize(is, drawtype, protocol_version);
	if (readU8(is) != CF_SPECIAL_COUNT)
		throw SerializationError("unsupported CF_SPECIAL_COUNT");
	for (TileDef &td : tiledef_special)
		td.deSerialize(is, drawtype, protocol_version);
	setAlphaFromLegacy(readU8(is));
	color.setRed(readU8(is));
	color.setGreen(readU8(is));
	color.setBlue(readU8(is));
	palette_name = deSerializeString16(is);
	waving = readU8(is);
	connect_sides = readU8(is);
	u16 connects_to_size = readU16(is);
	connects_to_ids.clear();
	for (u16 i = 0; i < connects_to_size; i++)
		connects_to_ids.push_back(readU16(is));
	post_effect_color = readARGB8(is);
	leveled = readU8(is);

	// lighting-related
	light_propagates = readU8(is);
	sunlight_propagates = readU8(is);
	light_source = readU8(is);
	light_source = MYMIN(light_source, LIGHT_MAX);

	// map generation
	is_ground_content = readU8(is);

	// interaction
	walkable = readU8(is);
	pointable = Pointabilities::deSerializePointabilityType(is);
	diggable = readU8(is);
	climbable = readU8(is);
	buildable_to = readU8(is);
	rightclickable = readU8(is);
	damage_per_second = readU32(is);

	// liquid
	liquid_type = static_cast<LiquidType>(readU8(is));
	if (liquid_type >= LiquidType_END)
		liquid_type = LIQUID_NONE;
	liquid_move_physics = liquid_type != LIQUID_NONE;
	liquid_alternative_flowing = deSerializeString16(is);
	liquid_alternative_source = deSerializeString16(is);
	liquid_viscosity = readU8(is);
	move_resistance = liquid_viscosity; // set default move_resistance
	liquid_renewable = readU8(is);
	liquid_range = readU8(is);
	drowning = readU8(is);
	floodable = readU8(is);

	// node boxes
	node_box.deSerialize(is);
	selection_box.deSerialize(is);
	collision_box.deSerialize(is);

	// sounds
	sound_footstep.deSerializeSimple(is, protocol_version);
	sound_dig.deSerializeSimple(is, protocol_version);
	sound_dug.deSerializeSimple(is, protocol_version);

	// read legacy properties
	legacy_facedir_simple = readU8(is);
	legacy_wallmounted = readU8(is);

	try {
		node_dig_prediction = deSerializeString16(is);

		u8 tmp = readU8(is);
		if (is.eof()) /* readU8 doesn't throw exceptions so we have to do this */
			throw SerializationError("");
		leveled_max = tmp;

		tmp = readU8(is);
		if (is.eof())
			throw SerializationError("");
		alpha = static_cast<enum AlphaMode>(tmp);
		if (alpha >= AlphaMode_END || alpha == ALPHAMODE_LEGACY_COMPAT)
			alpha = ALPHAMODE_OPAQUE;

		tmp = readU8(is);
		if (is.eof())
			throw SerializationError("");
		move_resistance = tmp;

		tmp = readU8(is);
		if (is.eof())
			throw SerializationError("");
		liquid_move_physics = tmp;

		tmp = readU8(is);
		if (is.eof())
			throw SerializationError("");
		post_effect_color_shaded = tmp;
	} catch (SerializationError &e) {};
}

/*
	NodeDefManager
*/


NodeDefManager::NodeDefManager()
{
	clear();
}

void NodeDefManager::clear()
{
	m_content_features.clear();
	m_name_id_mapping.clear();
	m_name_id_mapping_with_aliases.clear();
	m_group_to_items.clear();
	m_next_id = 0;
	m_selection_box_union.reset(0,0,0);
	m_selection_box_int_union.reset(0,0,0);
#if CHECK_CLIENT_BUILD()
	m_leaves_materials.clear();
#endif

	resetNodeResolveState();

	constexpr u32 initial_length = std::max({CONTENT_UNKNOWN, CONTENT_AIR, CONTENT_IGNORE}) + 1;
	m_content_features.resize(initial_length);

	// Set CONTENT_UNKNOWN
	{
		ContentFeatures f;
		f.name = "unknown";
		for (int t = 0; t < 6; t++)
			f.tiledef[t].name = "unknown_node.png";
		// Insert directly into containers
		content_t c = CONTENT_UNKNOWN;
		m_content_features[c] = f;
		for (u32 ci = 0; ci <= CONTENT_MAX; ci++)
			m_content_lighting_flag_cache[ci] = f.getLightingFlags();
		addNameIdMapping(c, f.name);
	}

	// Set CONTENT_AIR
	{
		ContentFeatures f;
		f.name                = "air";
		f.drawtype            = NDT_AIRLIKE;
		f.param_type          = CPT_LIGHT;
		f.light_propagates    = true;
		f.sunlight_propagates = true;
		f.walkable            = false;
		f.pointable           = PointabilityType::POINTABLE_NOT;
		f.diggable            = false;
		f.buildable_to        = true;
		f.floodable           = true;
		f.is_ground_content   = true;
		// Insert directly into containers
		content_t c = CONTENT_AIR;
		m_content_features[c] = f;
		m_content_lighting_flag_cache[c] = f.getLightingFlags();
		addNameIdMapping(c, f.name);
	}

	// Set CONTENT_IGNORE
	{
		ContentFeatures f;
		f.name                = "ignore";
		f.drawtype            = NDT_AIRLIKE;
		f.param_type          = CPT_NONE;
		f.light_propagates    = false;
		f.sunlight_propagates = false;
		f.walkable            = false;
		f.pointable           = PointabilityType::POINTABLE_NOT;
		f.diggable            = false;
		f.buildable_to        = true; // A way to remove accidental CONTENT_IGNOREs
		f.is_ground_content   = true;
		// Insert directly into containers
		content_t c = CONTENT_IGNORE;
		m_content_features[c] = f;
		m_content_lighting_flag_cache[c] = f.getLightingFlags();
		addNameIdMapping(c, f.name);
	}
}


bool NodeDefManager::getId(const std::string &name, content_t &result) const
{
	auto i = m_name_id_mapping_with_aliases.find(name);
	if(i == m_name_id_mapping_with_aliases.end())
		return false;
	result = i->second;
	return true;
}


content_t NodeDefManager::getId(const std::string &name) const
{
	content_t id = CONTENT_IGNORE;
	getId(name, id);
	return id;
}


bool NodeDefManager::getIds(const std::string &name,
		std::vector<content_t> &result) const
{
	//TimeTaker t("getIds", NULL, PRECISION_MICRO);
	if (!str_starts_with(name, "group:")) {
		content_t id = CONTENT_IGNORE;
		bool exists = getId(name, id);
		if (exists)
			result.push_back(id);
		return exists;
	}

	std::string group = name.substr(6);
	auto i = m_group_to_items.find(group);
	if (i == m_group_to_items.end())
		return true;

	const std::vector<content_t> &items = i->second;
	result.insert(result.end(), items.begin(), items.end());
	//printf("getIds: %dus\n", t.stop());
	return true;
}


const ContentFeatures& NodeDefManager::get(const std::string &name) const
{
	content_t id = CONTENT_UNKNOWN;
	getId(name, id);
	return get(id);
}


// returns CONTENT_IGNORE if no free ID found
content_t NodeDefManager::allocateId()
{
	for (content_t id = m_next_id;
			id >= m_next_id; // overflow?
			++id) {
		while (id >= m_content_features.size()) {
			m_content_features.emplace_back();
		}
		const ContentFeatures &f = m_content_features[id];
		if (f.name.empty()) {
			// use saturating add, to avoid overflow of m_next_id, which would
			// make the loop not terminate (loop cond id >= 0 is always true)
			if (id < CONTENT_MAX)
				m_next_id = id + 1;
			else
				m_next_id = CONTENT_MAX;
			return id;
		}
	}
	// If we arrive here, an overflow occurred in id.
	// That means no ID was found
	return CONTENT_IGNORE;
}


/*!
 * Returns the smallest box that contains all boxes
 * in the vector. Box_union is expanded.
 * @param[in]      boxes     the vector containing the boxes
 * @param[in, out] box_union the union of the arguments
 */
static void boxVectorUnion(const std::vector<aabb3f> &boxes, aabb3f *box_union)
{
	for (const aabb3f &box : boxes) {
		box_union->addInternalBox(box);
	}
}


/*!
 * Returns a box that contains the nodebox in every case.
 * The argument node_union is expanded.
 * @param[in]      nodebox  the nodebox to be measured
 * @param[in]      features  used to decide whether the nodebox
 * can be rotated
 * @param[in, out] box_union the union of the arguments
 */
static void getNodeBoxUnion(const NodeBox &nodebox, const ContentFeatures &features,
	aabb3f *box_union)
{
	switch(nodebox.type) {
		case NODEBOX_FIXED:
		case NODEBOX_LEVELED: {
			// Raw union
			aabb3f half_processed(0, 0, 0, 0, 0, 0);
			boxVectorUnion(nodebox.fixed, &half_processed);
			// Set leveled boxes to maximal
			if (nodebox.type == NODEBOX_LEVELED) {
				half_processed.MaxEdge.Y = +BS / 2;
			}
			if (features.param_type_2 == CPT2_FACEDIR ||
					features.param_type_2 == CPT2_COLORED_FACEDIR ||
					features.param_type_2 == CPT2_4DIR ||
					features.param_type_2 == CPT2_COLORED_4DIR) {
				// Get maximal coordinate
				f32 coords[] = {
					fabsf(half_processed.MinEdge.X),
					fabsf(half_processed.MinEdge.Y),
					fabsf(half_processed.MinEdge.Z),
					fabsf(half_processed.MaxEdge.X),
					fabsf(half_processed.MaxEdge.Y),
					fabsf(half_processed.MaxEdge.Z) };
				f32 max = 0;
				for (float coord : coords) {
					if (max < coord) {
						max = coord;
					}
				}
				// Add the union of all possible rotated boxes
				box_union->addInternalPoint(-max, -max, -max);
				box_union->addInternalPoint(+max, +max, +max);
			} else {
				box_union->addInternalBox(half_processed);
			}
			break;
		}
		case NODEBOX_WALLMOUNTED: {
			// Add fix boxes
			box_union->addInternalBox(nodebox.wall_top);
			box_union->addInternalBox(nodebox.wall_bottom);
			// Find maximal coordinate in the X-Z plane
			f32 coords[] = {
				fabsf(nodebox.wall_side.MinEdge.X),
				fabsf(nodebox.wall_side.MinEdge.Z),
				fabsf(nodebox.wall_side.MaxEdge.X),
				fabsf(nodebox.wall_side.MaxEdge.Z) };
			f32 max = 0;
			for (float coord : coords) {
				if (max < coord) {
					max = coord;
				}
			}
			// Add the union of all possible rotated boxes
			box_union->addInternalPoint(-max, nodebox.wall_side.MinEdge.Y, -max);
			box_union->addInternalPoint(max, nodebox.wall_side.MaxEdge.Y, max);
			break;
		}
		case NODEBOX_CONNECTED: {
			const auto &c = nodebox.getConnected();
			// Add all possible connected boxes
			boxVectorUnion(nodebox.fixed,         box_union);
			boxVectorUnion(c.connect_top,         box_union);
			boxVectorUnion(c.connect_bottom,      box_union);
			boxVectorUnion(c.connect_front,       box_union);
			boxVectorUnion(c.connect_left,        box_union);
			boxVectorUnion(c.connect_back,        box_union);
			boxVectorUnion(c.connect_right,       box_union);
			boxVectorUnion(c.disconnected_top,    box_union);
			boxVectorUnion(c.disconnected_bottom, box_union);
			boxVectorUnion(c.disconnected_front,  box_union);
			boxVectorUnion(c.disconnected_left,   box_union);
			boxVectorUnion(c.disconnected_back,   box_union);
			boxVectorUnion(c.disconnected_right,  box_union);
			boxVectorUnion(c.disconnected,        box_union);
			boxVectorUnion(c.disconnected_sides,  box_union);
			break;
		}
		default: {
			// NODEBOX_REGULAR
			box_union->addInternalPoint(-BS / 2, -BS / 2, -BS / 2);
			box_union->addInternalPoint(+BS / 2, +BS / 2, +BS / 2);
		}
	}
}


inline void NodeDefManager::fixSelectionBoxIntUnion()
{
	m_selection_box_int_union.MinEdge.X = floorf(
		m_selection_box_union.MinEdge.X / BS + 0.5f);
	m_selection_box_int_union.MinEdge.Y = floorf(
		m_selection_box_union.MinEdge.Y / BS + 0.5f);
	m_selection_box_int_union.MinEdge.Z = floorf(
		m_selection_box_union.MinEdge.Z / BS + 0.5f);
	m_selection_box_int_union.MaxEdge.X = ceilf(
		m_selection_box_union.MaxEdge.X / BS - 0.5f);
	m_selection_box_int_union.MaxEdge.Y = ceilf(
		m_selection_box_union.MaxEdge.Y / BS - 0.5f);
	m_selection_box_int_union.MaxEdge.Z = ceilf(
		m_selection_box_union.MaxEdge.Z / BS - 0.5f);
}


void NodeDefManager::eraseIdFromGroups(content_t id)
{
	// For all groups in m_group_to_items...
	for (auto iter_groups = m_group_to_items.begin();
			iter_groups != m_group_to_items.end();) {
		// Get the group items vector.
		std::vector<content_t> &items = iter_groups->second;

		// Remove any occurrence of the id in the group items vector.
		items.erase(std::remove(items.begin(), items.end(), id), items.end());

		// If group is empty, erase its vector from the map.
		if (items.empty())
			iter_groups = m_group_to_items.erase(iter_groups);
		else
			++iter_groups;
	}
}


// IWritableNodeDefManager
content_t NodeDefManager::set(const std::string &name, const ContentFeatures &def)
{
	// Pre-conditions
	assert(!name.empty());
	assert(name != "ignore");
	assert(name == def.name);

	content_t id = CONTENT_IGNORE;
	if (!m_name_id_mapping.getId(name, id)) { // ignore aliases
		// Get new id
		id = allocateId();
		if (id == CONTENT_IGNORE) {
			errorstream << "NodeDefManager: Absolute "
				"limit reached" << std::endl;
			return CONTENT_IGNORE;
		}
		assert(id != CONTENT_IGNORE);
		addNameIdMapping(id, name);
	}

	// Clear old groups in case of re-registration
	eraseIdFromGroups(id);

	m_content_features[id] = def;
	m_content_features[id].floats = itemgroup_get(def.groups, "float") != 0;
	m_content_lighting_flag_cache[id] = def.getLightingFlags();
	verbosestream << "NodeDefManager: registering content id " << id
		<< ": name=\"" << def.name << "\"" << std::endl;

	getNodeBoxUnion(def.selection_box, def, &m_selection_box_union);
	fixSelectionBoxIntUnion();

	// Add this content to the list of all groups it belongs to
	for (const auto &group : def.groups) {
		const std::string &group_name = group.first;
		m_group_to_items[group_name].push_back(id);
	}

	return id;
}


content_t NodeDefManager::allocateDummy(const std::string &name)
{
	assert(!name.empty());	// Pre-condition
	ContentFeatures f;
	f.name = name;
	return set(name, f);
}


void NodeDefManager::removeNode(const std::string &name)
{
	// Pre-condition
	assert(!name.empty());

	// Erase name from name ID mapping
	content_t id = CONTENT_IGNORE;
	if (m_name_id_mapping.getId(name, id)) {
		m_name_id_mapping.eraseName(name);
		m_name_id_mapping_with_aliases.erase(name);

		eraseIdFromGroups(id);
	}
}


void NodeDefManager::updateAliases(IItemDefManager *idef)
{
	std::set<std::string> all;
	idef->getAll(all);
	m_name_id_mapping_with_aliases.clear();
	for (const std::string &name : all) {
		const std::string &convert_to = idef->getAlias(name);
		content_t id;
		if (m_name_id_mapping.getId(convert_to, id)) {
			m_name_id_mapping_with_aliases.insert(
				std::make_pair(name, id));
		}
	}
}

void NodeDefManager::applyTextureOverrides(const std::vector<TextureOverride> &overrides)
{
	if (overrides.empty())
		return;

	infostream << "NodeDefManager::applyTextureOverrides(): Applying "
		"overrides to textures" << std::endl;

	for (const TextureOverride& texture_override : overrides) {
		content_t id;
		if (!getId(texture_override.id, id))
			continue; // Ignore unknown node

		ContentFeatures &nodedef = m_content_features[id];

		auto apply = [&] (TileDef &tile) {
			tile.name = texture_override.texture;
			if (texture_override.world_scale > 0) {
				tile.align_style = ALIGN_STYLE_WORLD;
				tile.scale = texture_override.world_scale;
			}
		};

		// Override tiles
		if (texture_override.hasTarget(OverrideTarget::TOP))
			apply(nodedef.tiledef[0]);

		if (texture_override.hasTarget(OverrideTarget::BOTTOM))
			apply(nodedef.tiledef[1]);

		if (texture_override.hasTarget(OverrideTarget::RIGHT))
			apply(nodedef.tiledef[2]);

		if (texture_override.hasTarget(OverrideTarget::LEFT))
			apply(nodedef.tiledef[3]);

		if (texture_override.hasTarget(OverrideTarget::BACK))
			apply(nodedef.tiledef[4]);

		if (texture_override.hasTarget(OverrideTarget::FRONT))
			apply(nodedef.tiledef[5]);


		// Override special tiles, if applicable
		if (texture_override.hasTarget(OverrideTarget::SPECIAL_1))
			apply(nodedef.tiledef_special[0]);

		if (texture_override.hasTarget(OverrideTarget::SPECIAL_2))
			apply(nodedef.tiledef_special[1]);

		if (texture_override.hasTarget(OverrideTarget::SPECIAL_3))
			apply(nodedef.tiledef_special[2]);

		if (texture_override.hasTarget(OverrideTarget::SPECIAL_4))
			apply(nodedef.tiledef_special[3]);

		if (texture_override.hasTarget(OverrideTarget::SPECIAL_5))
			apply(nodedef.tiledef_special[4]);

		if (texture_override.hasTarget(OverrideTarget::SPECIAL_6))
			apply(nodedef.tiledef_special[5]);
	}
}

void NodeDefManager::applyFunction(const std::function<void(ContentFeatures&)> &function)
{
	for (ContentFeatures &f : m_content_features)
		function(f);
}

void NodeDefManager::serialize(std::ostream &os, u16 protocol_version) const
{
	writeU8(os, 1); // version
	u16 count = 0;
	std::ostringstream os2(std::ios::binary);
	for (u32 i = 0; i < m_content_features.size(); i++) {
		if (i == CONTENT_IGNORE || i == CONTENT_AIR
				|| i == CONTENT_UNKNOWN)
			continue;
		const ContentFeatures *f = &m_content_features[i];
		if (f->name.empty())
			continue;
		writeU16(os2, i);
		// Wrap it in a string to allow different lengths without
		// strict version incompatibilities
		std::ostringstream wrapper_os(std::ios::binary);
		f->serialize(wrapper_os, protocol_version);
		os2<<serializeString16(wrapper_os.str());

		// must not overflow
		FATAL_ERROR_IF(count == U16_MAX, "overflow");
		count++;
	}
	writeU16(os, count);
	os << serializeString32(os2.str());
}


void NodeDefManager::deSerialize(std::istream &is, u16 protocol_version)
{
	clear();

	if (readU8(is) < 1)
		throw SerializationError("unsupported NodeDefinitionManager version");

	u16 count = readU16(is);
	std::istringstream is2(deSerializeString32(is), std::ios::binary);
	ContentFeatures f;
	for (u16 n = 0; n < count; n++) {
		u16 i = readU16(is2);

		// Read it from the string wrapper
		std::string wrapper = deSerializeString16(is2);
		std::istringstream wrapper_is(wrapper, std::ios::binary);
		f.deSerialize(wrapper_is, protocol_version);

		// Check error conditions
		if (i == CONTENT_IGNORE || i == CONTENT_AIR || i == CONTENT_UNKNOWN) {
			warningstream << "NodeDefManager::deSerialize(): "
				"not changing builtin node " << i << std::endl;
			continue;
		}
		if (f.name.empty()) {
			warningstream << "NodeDefManager::deSerialize(): "
				"received empty name" << std::endl;
			continue;
		}

		// Ignore aliases
		u16 existing_id;
		if (m_name_id_mapping.getId(f.name, existing_id) && i != existing_id) {
			warningstream << "NodeDefManager::deSerialize(): "
				"already defined with different ID: " << f.name << std::endl;
			continue;
		}

		// All is ok, add node definition with the requested ID
		if (i >= m_content_features.size())
			m_content_features.resize((size_t)(i) + 1);
		m_content_features[i] = f;
		m_content_features[i].floats = itemgroup_get(f.groups, "float") != 0;
		m_content_lighting_flag_cache[i] = f.getLightingFlags();
		addNameIdMapping(i, f.name);
		TRACESTREAM(<< "NodeDef: deserialized " << f.name << std::endl);

		getNodeBoxUnion(f.selection_box, f, &m_selection_box_union);
		fixSelectionBoxIntUnion();
	}

	// Since liquid_alternative_flowing_id and liquid_alternative_source_id
	// are not sent, resolve them client-side too.
	resolveCrossrefs();
}


void NodeDefManager::addNameIdMapping(content_t i, const std::string &name)
{
	m_name_id_mapping.set(i, name);
	m_name_id_mapping_with_aliases.emplace(name, i);
}


NodeDefManager *createNodeDefManager()
{
	return new NodeDefManager();
}


void NodeDefManager::pendNodeResolve(NodeResolver *nr) const
{
	nr->m_ndef = this;
	if (m_node_registration_complete)
		nr->nodeResolveInternal();
	else
		m_pending_resolve_callbacks.push_back(nr);
}


bool NodeDefManager::cancelNodeResolveCallback(NodeResolver *nr) const
{
	size_t len = m_pending_resolve_callbacks.size();
	for (size_t i = 0; i != len; i++) {
		if (nr != m_pending_resolve_callbacks[i])
			continue;

		len--;
		m_pending_resolve_callbacks[i] = m_pending_resolve_callbacks[len];
		m_pending_resolve_callbacks.resize(len);
		return true;
	}

	return false;
}


void NodeDefManager::runNodeResolveCallbacks()
{
	for (size_t i = 0; i != m_pending_resolve_callbacks.size(); i++) {
		NodeResolver *nr = m_pending_resolve_callbacks[i];
		nr->nodeResolveInternal();
	}

	m_pending_resolve_callbacks.clear();
}


void NodeDefManager::resetNodeResolveState()
{
	m_node_registration_complete = false;
	m_pending_resolve_callbacks.clear();
}

void NodeDefManager::resolveCrossrefs()
{
	for (ContentFeatures &f : m_content_features) {
		if (f.isLiquid() || f.isLiquidRender()) {
			f.liquid_alternative_flowing_id = getId(f.liquid_alternative_flowing);
			f.liquid_alternative_source_id = getId(f.liquid_alternative_source);
			continue;
		}
		if (f.drawtype != NDT_NODEBOX || f.node_box.type != NODEBOX_CONNECTED)
			continue;

		for (const std::string &name : f.connects_to) {
			getIds(name, f.connects_to_ids);
		}
		SORT_AND_UNIQUE(f.connects_to_ids);
	}
}

bool NodeDefManager::nodeboxConnects(MapNode from, MapNode to,
	u8 connect_face) const
{
	const ContentFeatures &f1 = get(from);

	if ((f1.drawtype != NDT_NODEBOX) || (f1.node_box.type != NODEBOX_CONNECTED))
		return false;

	// lookup target in connected set
	if (!CONTAINS(f1.connects_to_ids, to.param0))
		return false;

	const ContentFeatures &f2 = get(to);

	if ((f2.drawtype == NDT_NODEBOX) && (f2.node_box.type == NODEBOX_CONNECTED))
		// ignores actually looking if back connection exists
		return CONTAINS(f2.connects_to_ids, from.param0);

	// does to node declare usable faces?
	if (f2.connect_sides > 0) {
		if ((f2.param_type_2 == CPT2_FACEDIR ||
				f2.param_type_2 == CPT2_COLORED_FACEDIR ||
				f2.param_type_2 == CPT2_4DIR ||
				f2.param_type_2 == CPT2_COLORED_4DIR)
				&& (connect_face >= 4)) {
			static const u8 rot[33 * 4] = {
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				4, 32, 16, 8, // 4 - back
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				8, 4, 32, 16, // 8 - right
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				16, 8, 4, 32, // 16 - front
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				0, 0, 0, 0,
				32, 16, 8, 4 // 32 - left
			};
			if (f2.param_type_2 == CPT2_FACEDIR ||
					f2.param_type_2 == CPT2_COLORED_FACEDIR) {
				// FIXME: support arbitrary rotations (to.param2 & 0x1F) (#7696)
				return (f2.connect_sides
					& rot[(connect_face * 4) + (to.param2 & 0x03)]);
			} else if (f2.param_type_2 == CPT2_4DIR ||
					f2.param_type_2 == CPT2_COLORED_4DIR) {
				return (f2.connect_sides
					& rot[(connect_face * 4) + (to.param2 & 0x03)]);
			}
		}
		return (f2.connect_sides & connect_face);
	}
	// the target is just a regular node, so connect no matter back connection
	return true;
}

////
//// NodeResolver
////

NodeResolver::NodeResolver()
{
	reset();
}


NodeResolver::~NodeResolver()
{
	if (!m_resolve_done && m_ndef)
		m_ndef->cancelNodeResolveCallback(this);
}


void NodeResolver::cloneTo(NodeResolver *res) const
{
	FATAL_ERROR_IF(!m_resolve_done, "NodeResolver can only be cloned"
		" after resolving has completed");
	/* We don't actually do anything significant. Since the node resolving has
	 * already completed, the class that called us will already have the
	 * resolved IDs in its data structures (which it copies on its own) */
	res->m_ndef = m_ndef;
	res->m_resolve_done = true;
}


void NodeResolver::nodeResolveInternal()
{
	m_nodenames_idx   = 0;
	m_nnlistsizes_idx = 0;

	resolveNodeNames();
	m_resolve_done = true;

	m_nodenames.clear();
	m_nnlistsizes.clear();
}


bool NodeResolver::getIdFromNrBacklog(content_t *result_out,
	const std::string &node_alt, content_t c_fallback, bool error_on_fallback)
{
	if (m_nodenames_idx == m_nodenames.size()) {
		*result_out = c_fallback;
		errorstream << "NodeResolver: no more nodes in list" << std::endl;
		return false;
	}

	content_t c;
	std::string name = m_nodenames[m_nodenames_idx++];

	bool success = m_ndef->getId(name, c);
	if (!success && !node_alt.empty()) {
		name = node_alt;
		success = m_ndef->getId(name, c);
	}

	if (!success) {
		if (error_on_fallback)
			errorstream << "NodeResolver: failed to resolve node name '" << name
				<< "'." << std::endl;
		c = c_fallback;
	}

	*result_out = c;
	return success;
}


bool NodeResolver::getIdsFromNrBacklog(std::vector<content_t> *result_out,
	bool all_required, content_t c_fallback)
{
	bool success = true;

	if (m_nnlistsizes_idx == m_nnlistsizes.size()) {
		errorstream << "NodeResolver: no more node lists" << std::endl;
		return false;
	}

	size_t length = m_nnlistsizes[m_nnlistsizes_idx++];

	while (length--) {
		if (m_nodenames_idx == m_nodenames.size()) {
			errorstream << "NodeResolver: no more nodes in list" << std::endl;
			return false;
		}

		content_t c;
		std::string &name = m_nodenames[m_nodenames_idx++];

		if (!str_starts_with(name, "group:")) {
			if (m_ndef->getId(name, c)) {
				result_out->push_back(c);
			} else if (all_required) {
				errorstream << "NodeResolver: failed to resolve node name '"
					<< name << "'." << std::endl;
				result_out->push_back(c_fallback);
				success = false;
			}
		} else {
			m_ndef->getIds(name, *result_out);
		}
	}

	return success;
}

void NodeResolver::reset(bool resolve_done)
{
	m_nodenames.clear();
	m_nodenames_idx = 0;
	m_nnlistsizes.clear();
	m_nnlistsizes_idx = 0;

	m_resolve_done = resolve_done;

	m_nodenames.reserve(16);
	m_nnlistsizes.reserve(4);
}
