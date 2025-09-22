// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "object_properties.h"
#include "irrlicht_changes/printing.h"
#include "irrlichttypes_bloated.h"
#include "exceptions.h"
#include "log.h"
#include "util/serialize.h"
#include "util/enum_string.h"
#include <sstream>
#include <tuple>

static const video::SColor NULL_BGCOLOR{0, 1, 1, 1};

const struct EnumString es_ObjectVisual[] =
{
	{OBJECTVISUAL_UNKNOWN, "unknown"},
	{OBJECTVISUAL_SPRITE, "sprite"},
	{OBJECTVISUAL_UPRIGHT_SPRITE, "upright_sprite"},
	{OBJECTVISUAL_CUBE, "cube"},
	{OBJECTVISUAL_MESH, "mesh"},
	{OBJECTVISUAL_ITEM, "item"},
	{OBJECTVISUAL_WIELDITEM, "wielditem"},
	{OBJECTVISUAL_NODE, "node"},
	{0, nullptr},
};

ObjectProperties::ObjectProperties()
{
	textures.emplace_back("no_texture.png");
}

std::string ObjectProperties::dump() const
{
	const auto &put_color = [] (std::ostream &os, video::SColor color) {
		os << "\"" << color.getAlpha() << "," << color.getRed() << ","
			<< color.getGreen() << "," << color.getBlue() << "\" ";
	};

	std::ostringstream os(std::ios::binary);
	os << "hp_max=" << hp_max;
	os << ", breath_max=" << breath_max;
	os << ", physical=" << physical;
	os << ", collideWithObjects=" << collideWithObjects;
	os << ", collisionbox=" << collisionbox.MinEdge << "," << collisionbox.MaxEdge;
	os << ", visual=" << enum_to_string(es_ObjectVisual, visual);
	os << ", mesh=" << mesh;
	os << ", visual_size=" << visual_size;
	os << ", textures=[";
	for (const std::string &texture : textures) {
		os << "\"" << texture << "\" ";
	}
	os << "]";
	os << ", colors=[";
	for (const video::SColor &color : colors)
		put_color(os, color);
	os << "]";
	os << ", spritediv=" << spritediv;
	os << ", initial_sprite_basepos=" << initial_sprite_basepos;
	os << ", is_visible=" << is_visible;
	os << ", makes_footstep_sound=" << makes_footstep_sound;
	os << ", automatic_rotate="<< automatic_rotate;
	os << ", backface_culling="<< backface_culling;
	os << ", glow=" << glow;
	os << ", nametag=" << nametag;
	os << ", nametag_color=";
	put_color(os, nametag_color);
	os << ", nametag_bgcolor=";
	if (nametag_bgcolor)
		put_color(os, nametag_bgcolor.value());
	else
		os << "=null ";
	os << ", nametag_fontsize=";
	if (nametag_fontsize)
		os << "=" << nametag_fontsize.value() << " ";
	else
		os << "=null ";
	os << ", selectionbox=" << selectionbox.MinEdge << "," << selectionbox.MaxEdge;
	os << ", rotate_selectionbox=" << rotate_selectionbox;
	os << ", pointable=" << Pointabilities::toStringPointabilityType(pointable);
	os << ", static_save=" << static_save;
	os << ", eye_height=" << eye_height;
	os << ", zoom_fov=" << zoom_fov;
	os << ", node=(" << (int)node.getContent() << ", " << (int)node.getParam1()
		<< ", " << (int)node.getParam2() << ")";
	os << ", use_texture_alpha=" << use_texture_alpha;
	os << ", damage_texture_modifier=" << damage_texture_modifier;
	os << ", shaded=" << shaded;
	os << ", show_on_minimap=" << show_on_minimap;
	os << ", nametag_scale_z=" << nametag_scale_z;
	return os.str();
}

static inline auto tie(const ObjectProperties &o)
{
	// Make sure to add new members to this list!
	return std::tie(
	o.textures, o.colors, o.collisionbox, o.selectionbox, o.visual, o.mesh,
	o.damage_texture_modifier, o.nametag, o.infotext, o.wield_item, o.visual_size,
	o.nametag_color, o.nametag_bgcolor, o.nametag_fontsize, o.spritediv,
	o.initial_sprite_basepos,
	o.stepheight, o.automatic_rotate, o.automatic_face_movement_dir_offset,
	o.automatic_face_movement_max_rotation_per_sec, o.eye_height, o.zoom_fov,
	o.node, o.hp_max, o.breath_max, o.glow, o.pointable, o.physical,
	o.collideWithObjects, o.rotate_selectionbox, o.is_visible, o.makes_footstep_sound,
	o.automatic_face_movement_dir, o.backface_culling, o.static_save, o.use_texture_alpha,
	o.shaded, o.show_on_minimap, o.nametag_scale_z
	);
}

bool ObjectProperties::operator==(const ObjectProperties &other) const
{
	return tie(*this) == tie(other);
}

bool ObjectProperties::validate()
{
	const char *func = "ObjectProperties::validate(): ";
	bool ret = true;

	// cf. where serializeString16 is used below
	for (u32 i = 0; i < textures.size(); i++) {
		if (textures[i].size() > U16_MAX) {
			warningstream << func << "texture " << (i+1) << " has excessive length, "
				"clearing it." << std::endl;
			textures[i].clear();
			ret = false;
		}
	}
	if (nametag.length() > U16_MAX) {
		warningstream << func << "nametag has excessive length, clearing it." << std::endl;
		nametag.clear();
		ret = false;
	}
	if (infotext.length() > U16_MAX) {
		warningstream << func << "infotext has excessive length, clearing it." << std::endl;
		infotext.clear();
		ret = false;
	}
	if (wield_item.length() > U16_MAX) {
		warningstream << func << "wield_item has excessive length, clearing it." << std::endl;
		wield_item.clear();
		ret = false;
	}

	return ret;
}

void ObjectProperties::serialize(std::ostream &os) const
{
	writeU8(os, 4); // PROTOCOL_VERSION >= 37
	writeU16(os, hp_max);
	writeU8(os, physical);
	writeF32(os, 0.f); // Removed property (weight)
	writeV3F32(os, collisionbox.MinEdge);
	writeV3F32(os, collisionbox.MaxEdge);
	writeV3F32(os, selectionbox.MinEdge);
	writeV3F32(os, selectionbox.MaxEdge);
	Pointabilities::serializePointabilityType(os, pointable);

	// Convert to string for compatibility
	os << serializeString16(enum_to_string(es_ObjectVisual, visual));

	writeV3F32(os, visual_size);
	writeU16(os, textures.size());
	for (const std::string &texture : textures) {
		os << serializeString16(texture);
	}
	writeV2S16(os, spritediv);
	writeV2S16(os, initial_sprite_basepos);
	writeU8(os, is_visible);
	writeU8(os, makes_footstep_sound);
	writeF32(os, automatic_rotate);
	os << serializeString16(mesh);
	writeU16(os, colors.size());
	for (video::SColor color : colors) {
		writeARGB8(os, color);
	}
	writeU8(os, collideWithObjects);
	writeF32(os, stepheight);
	writeU8(os, automatic_face_movement_dir);
	writeF32(os, automatic_face_movement_dir_offset);
	writeU8(os, backface_culling);
	os << serializeString16(nametag);
	writeARGB8(os, nametag_color);
	writeF32(os, automatic_face_movement_max_rotation_per_sec);
	os << serializeString16(infotext);
	os << serializeString16(wield_item);
	writeS8(os, glow);
	writeU16(os, breath_max);
	writeF32(os, eye_height);
	writeF32(os, zoom_fov);
	writeU8(os, use_texture_alpha);
	os << serializeString16(damage_texture_modifier);
	writeU8(os, shaded);
	writeU8(os, show_on_minimap);

	// use special value to tell apart nil, fully transparent and other colors
	if (!nametag_bgcolor)
		writeARGB8(os, NULL_BGCOLOR);
	else if (nametag_bgcolor.value().getAlpha() == 0)
		writeARGB8(os, video::SColor(0, 0, 0, 0));
	else
		writeARGB8(os, nametag_bgcolor.value());

	writeU8(os, rotate_selectionbox);
	writeU16(os, node.getContent());
	writeU8(os, node.getParam1());
	writeU8(os, node.getParam2());

	if (!nametag_fontsize)
		writeU32(os, U32_MAX); // null placeholder
	else
		writeU32(os, nametag_fontsize.value());

	writeU8(os, nametag_scale_z);

	// Add stuff only at the bottom.
	// Never remove anything, because we don't want new versions of this!
}

namespace {
	// Type-safe wrapper for bools as u8
	inline bool readBool(std::istream &is)
	{
		return readU8(is) != 0;
	}

	// Wrapper for primitive reading functions that don't throw (awful)
	template <typename T, T (reader)(std::istream& is)>
	bool tryRead(T& val, std::istream& is)
	{
		T tmp = reader(is);
		if (is.eof())
			return false;
		val = tmp;
		return true;
	}
}

void ObjectProperties::deSerialize(std::istream &is)
{
	int version = readU8(is);
	if (version != 4)
		throw SerializationError("unsupported ObjectProperties version");

	hp_max = readU16(is);
	physical = readU8(is);
	readU32(is); // removed property (weight)
	collisionbox.MinEdge = readV3F32(is);
	collisionbox.MaxEdge = readV3F32(is);
	selectionbox.MinEdge = readV3F32(is);
	selectionbox.MaxEdge = readV3F32(is);
	pointable = Pointabilities::deSerializePointabilityType(is);

	std::string visual_string{deSerializeString16(is)};
	if (!string_to_enum(es_ObjectVisual, visual, visual_string)) {
		infostream << "ObjectProperties::deSerialize(): visual \"" << visual_string
				<< "\" not supported" << std::endl;
		visual = OBJECTVISUAL_UNKNOWN;
	}

	visual_size = readV3F32(is);
	textures.clear();
	u32 texture_count = readU16(is);
	for (u32 i = 0; i < texture_count; i++){
		textures.push_back(deSerializeString16(is));
	}
	spritediv = readV2S16(is);
	initial_sprite_basepos = readV2S16(is);
	is_visible = readU8(is);
	makes_footstep_sound = readU8(is);
	automatic_rotate = readF32(is);
	mesh = deSerializeString16(is);
	colors.clear();
	u32 color_count = readU16(is);
	for (u32 i = 0; i < color_count; i++){
		colors.push_back(readARGB8(is));
	}
	collideWithObjects = readU8(is);
	stepheight = readF32(is);
	automatic_face_movement_dir = readU8(is);
	automatic_face_movement_dir_offset = readF32(is);
	backface_culling = readU8(is);
	nametag = deSerializeString16(is);
	nametag_color = readARGB8(is);
	automatic_face_movement_max_rotation_per_sec = readF32(is);
	infotext = deSerializeString16(is);
	wield_item = deSerializeString16(is);
	glow = readS8(is);
	breath_max = readU16(is);
	eye_height = readF32(is);
	zoom_fov = readF32(is);
	use_texture_alpha = readU8(is);

	try {
		damage_texture_modifier = deSerializeString16(is);
	} catch (SerializationError &e) {
		return;
	}

	if (!tryRead<bool, readBool>(shaded, is))
		return;

	if (!tryRead<bool, readBool>(show_on_minimap, is))
		return;

	auto bgcolor = readARGB8(is);
	if (bgcolor != NULL_BGCOLOR)
		nametag_bgcolor = bgcolor;
	else
		nametag_bgcolor = std::nullopt;

	if (!tryRead<bool, readBool>(rotate_selectionbox, is))
		return;

	if (!tryRead<content_t, readU16>(node.param0, is))
		return;
	node.param1 = readU8(is);
	node.param2 = readU8(is);

	u32 fontsize;
	if (!tryRead<u32, readU32>(fontsize, is))
		return;
	if (fontsize != U32_MAX)
		nametag_fontsize = fontsize;
	else
		nametag_fontsize = std::nullopt;
	nametag_scale_z = readU8(is);

	// Add new properties down here and remember to use either tryRead<> or a try-catch.
}
