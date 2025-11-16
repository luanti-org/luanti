// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2019 rubenwardy

#pragma once

#include "client/texturesource.h"
#include "client/fontengine.h"
#include "debug.h"
#include "irrlichttypes_bloated.h"
#include "util/string.h"
#include <algorithm>
#include <array>
#include <vector>


class StyleSpec
{
public:
	enum Property
	{
		TEXTCOLOR,
		BGCOLOR,
		BGCOLOR_HOVERED, // Note: Deprecated property
		BGCOLOR_PRESSED, // Note: Deprecated property
		NOCLIP,
		BORDER,
		BGIMG,
		BGIMG_HOVERED, // Note: Deprecated property
		BGIMG_MIDDLE,
		BGIMG_PRESSED, // Note: Deprecated property
		FGIMG,
		FGIMG_HOVERED, // Note: Deprecated property
		FGIMG_MIDDLE,
		FGIMG_PRESSED, // Note: Deprecated property
		ALPHA,
		CONTENT_OFFSET,
		PADDING,
		FONT,
		FONT_SIZE,
		COLORS,
		BORDERCOLORS,
		BORDERWIDTHS,
		SOUND,
		SPACING,
		SIZE,
		NUM_PROPERTIES,
		NONE
	};

	// State is a bitfield, it's possible to have multiple of these at once
	enum State : u8
	{
		STATE_DEFAULT = 0,
		STATE_FOCUSED = 1 << 0,
		STATE_HOVERED = 1 << 1,
		STATE_PRESSED = 1 << 2,
		NUM_STATES = 1 << 3, // This includes all permutations
		STATE_INVALID = 1 << 4,
	};

private:
	std::array<bool, NUM_PROPERTIES> property_set{};
	std::array<std::string, NUM_PROPERTIES> properties;
	State state_map = STATE_DEFAULT;

public:
	static Property GetPropertyByName(const std::string &name)
	{
		if (name == "textcolor") {
			return TEXTCOLOR;
		} else if (name == "bgcolor") {
			return BGCOLOR;
		} else if (name == "bgcolor_hovered") {
			return BGCOLOR_HOVERED;
		} else if (name == "bgcolor_pressed") {
			return BGCOLOR_PRESSED;
		} else if (name == "noclip") {
			return NOCLIP;
		} else if (name == "border") {
			return BORDER;
		} else if (name == "bgimg") {
			return BGIMG;
		} else if (name == "bgimg_hovered") {
			return BGIMG_HOVERED;
		} else if (name == "bgimg_middle") {
			return BGIMG_MIDDLE;
		} else if (name == "bgimg_pressed") {
			return BGIMG_PRESSED;
		} else if (name == "fgimg") {
			return FGIMG;
		} else if (name == "fgimg_hovered") {
			return FGIMG_HOVERED;
		} else if (name == "fgimg_middle") {
			return FGIMG_MIDDLE;
		} else if (name == "fgimg_pressed") {
			return FGIMG_PRESSED;
		} else if (name == "alpha") {
			return ALPHA;
		} else if (name == "content_offset") {
			return CONTENT_OFFSET;
		} else if (name == "padding") {
			return PADDING;
		} else if (name == "font") {
			return FONT;
		} else if (name == "font_size") {
			return FONT_SIZE;
		} else if (name == "colors") {
			return COLORS;
		} else if (name == "bordercolors") {
			return BORDERCOLORS;
		} else if (name == "borderwidths") {
			return BORDERWIDTHS;
		} else if (name == "sound") {
			return SOUND;
		} else if (name == "spacing") {
			return SPACING;
		} else if (name == "size") {
			return SIZE;
		} else {
			return NONE;
		}
	}

	std::string get(Property prop, std::string def) const
	{
		const auto &val = properties[prop];
		return val.empty() ? def : val;
	}

	void set(Property prop, const std::string &value)
	{
		properties[prop] = value;
		property_set[prop] = true;
	}

	//! Parses a name and returns the corresponding state enum
	static State getStateByName(const std::string &name)
	{
		if (name == "default") {
			return STATE_DEFAULT;
		} else if (name == "focused") {
			return STATE_FOCUSED;
		} else if (name == "hovered") {
			return STATE_HOVERED;
		} else if (name == "pressed") {
			return STATE_PRESSED;
		} else {
			return STATE_INVALID;
		}
	}

	//! Gets the state that this style is intended for
	State getState() const
	{
		return state_map;
	}

	//! Set the given state on this style
	void addState(State state)
	{
		FATAL_ERROR_IF(state >= NUM_STATES, "Out-of-bounds state received");

		state_map = static_cast<State>(state_map | state);
	}

	//! Using a list of styles mapped to state values, calculate the final
	//  combined style for a state by propagating values in its component states
	static StyleSpec getStyleFromStatePropagation(const std::array<StyleSpec, NUM_STATES> &styles, State state)
	{
		StyleSpec temp = styles[StyleSpec::STATE_DEFAULT];
		temp.state_map = state;
		for (int i = StyleSpec::STATE_DEFAULT + 1; i <= state; i++) {
			if ((state & i) != 0) {
				temp = temp | styles[i];
			}
		}

		return temp;
	}

	video::SColor getColor(Property prop, video::SColor def) const
	{
		const auto &val = properties[prop];
		if (val.empty()) {
			return def;
		}

		parseColorString(val, def, false, 0xFF);
		return def;
	}

	video::SColor getColor(Property prop) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		video::SColor color;
		parseColorString(val, color, false, 0xFF);
		return color;
	}

	std::array<video::SColor, 4> getColorArray(Property prop,
		std::array<video::SColor, 4> def) const
	{
		const auto &val = properties[prop];
		if (val.empty())
			return def;

		std::vector<std::string> strs;
		if (!parseArray(val, strs))
			return def;

		for (size_t i = 0; i <= 3; i++) {
			video::SColor color;
			if (parseColorString(strs[i], color, false, 0xff))
				def[i] = color;
		}

		return def;
	}

	std::array<s32, 4> getIntArray(Property prop, std::array<s32, 4> def) const
	{
		const auto &val = properties[prop];
		if (val.empty())
			return def;

		std::vector<std::string> strs;
		if (!parseArray(val, strs))
			return def;

		for (size_t i = 0; i <= 3; i++)
			def[i] = stoi(strs[i]);

		return def;
	}

	core::rect<s32> getRect(Property prop, core::rect<s32> def) const
	{
		const auto &val = properties[prop];
		if (val.empty())
			return def;

		core::rect<s32> rect;
		if (!parseRect(val, &rect))
			return def;

		return rect;
	}

	core::rect<s32> getRect(Property prop) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		core::rect<s32> rect;
		parseRect(val, &rect);
		return rect;
	}

	v2f32 getVector2f(Property prop, v2f32 def) const
	{
		const auto &val = properties[prop];
		if (val.empty())
			return def;

		v2f32 vec;
		if (!parseVector2f(val, &vec))
			return def;

		return vec;
	}

	v2s32 getVector2i(Property prop, v2s32 def) const
	{
		const auto &val = properties[prop];
		if (val.empty())
			return def;

		v2f32 vec;
		if (!parseVector2f(val, &vec))
			return def;

		return v2s32(vec.X, vec.Y);
	}

	v2s32 getVector2i(Property prop) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		v2f32 vec;
		parseVector2f(val, &vec);
		return v2s32(vec.X, vec.Y);
	}

	gui::IGUIFont *getFont() const
	{
		FontSpec spec(FONT_SIZE_UNSPECIFIED, FM_Standard, false, false);

		const std::string &font = properties[FONT];
		const std::string &size = properties[FONT_SIZE];

		if (font.empty() && size.empty())
			return nullptr;

		std::vector<std::string> modes = split(font, ',');

		for (size_t i = 0; i < modes.size(); i++) {
			if (modes[i] == "normal")
				spec.mode = FM_Standard;
			else if (modes[i] == "mono")
				spec.mode = FM_Mono;
			else if (modes[i] == "bold")
				spec.bold = true;
			else if (modes[i] == "italic")
				spec.italic = true;
			else if (modes[i] == "_no_server_media") // for internal use only
				spec.allow_server_media = false;
		}

		if (!size.empty()) {
			int calc_size = 1;

			if (size[0] == '*') {
				std::string new_size = size.substr(1); // Remove '*' (invalid for stof)
				calc_size = stof(new_size) * g_fontengine->getFontSize(spec.mode);
			} else if (size[0] == '+' || size[0] == '-') {
				calc_size = stoi(size) + g_fontengine->getFontSize(spec.mode);
			} else {
				calc_size = stoi(size);
			}

			spec.size = (unsigned)std::min(std::max(calc_size, 1), 999);
		}

		return g_fontengine->getFont(spec);
	}

	video::ITexture *getTexture(Property prop, ISimpleTextureSource *tsrc,
			video::ITexture *def) const
	{
		const auto &val = properties[prop];
		if (val.empty()) {
			return def;
		}

		video::ITexture *texture = tsrc->getTexture(val);

		return texture;
	}

	video::ITexture *getTexture(Property prop, ISimpleTextureSource *tsrc) const
	{
		const auto &val = properties[prop];
		FATAL_ERROR_IF(val.empty(), "Unexpected missing property");

		video::ITexture *texture = tsrc->getTexture(val);

		return texture;
	}

	bool getBool(Property prop, bool def) const
	{
		const auto &val = properties[prop];
		if (val.empty()) {
			return def;
		}

		return is_yes(val);
	}

	inline bool isNotDefault(Property prop) const
	{
		return !properties[prop].empty();
	}

	inline bool hasProperty(Property prop) const { return property_set[prop]; }

	StyleSpec &operator|=(const StyleSpec &other)
	{
		for (size_t i = 0; i < NUM_PROPERTIES; i++) {
			auto prop = (Property)i;
			if (other.hasProperty(prop)) {
				set(prop, other.get(prop, ""));
			}
		}

		return *this;
	}

	StyleSpec operator|(const StyleSpec &other) const
	{
		StyleSpec newspec = *this;
		newspec |= other;
		return newspec;
	}

private:
	bool parseArray(const std::string &value, std::vector<std::string> &arr) const
	{
		std::vector<std::string> strs = split(value, ',');

		if (strs.size() == 1) {
			arr = {strs[0], strs[0], strs[0], strs[0]};
		} else if (strs.size() == 2) {
			arr = {strs[0], strs[1], strs[0], strs[1]};
		} else if (strs.size() == 4) {
			arr = strs;
		} else {
			warningstream << "Invalid array size (" << strs.size()
					<< " arguments): \"" << value << "\"" << std::endl;
			return false;
		}
		return true;
	}

	bool parseRect(const std::string &value, core::rect<s32> *parsed_rect) const
	{
		core::rect<s32> rect;
		std::vector<std::string> v_rect = split(value, ',');

		if (v_rect.size() == 1) {
			s32 x = stoi(v_rect[0]);
			rect.UpperLeftCorner = core::vector2di(x, x);
			rect.LowerRightCorner = core::vector2di(-x, -x);
		} else if (v_rect.size() == 2) {
			s32 x = stoi(v_rect[0]);
			s32 y =	stoi(v_rect[1]);
			rect.UpperLeftCorner = core::vector2di(x, y);
			rect.LowerRightCorner = core::vector2di(-x, -y);
			// `-x` is interpreted as `w - x`
		} else if (v_rect.size() == 4) {
			rect.UpperLeftCorner = core::vector2di(
					stoi(v_rect[0]), stoi(v_rect[1]));
			rect.LowerRightCorner = core::vector2di(
					stoi(v_rect[2]), stoi(v_rect[3]));
		} else {
			warningstream << "Invalid rectangle string format: \"" << value
					<< "\"" << std::endl;
			return false;
		}

		*parsed_rect = rect;

		return true;
	}

	bool parseVector2f(const std::string &value, v2f32 *parsed_vec) const
	{
		v2f32 vec;
		std::vector<std::string> v_vector = split(value, ',');

		if (v_vector.size() == 1) {
			f32 x = stof(v_vector[0]);
			vec.X = x;
			vec.Y = x;
		} else if (v_vector.size() == 2) {
			vec.X = stof(v_vector[0]);
			vec.Y =	stof(v_vector[1]);
		} else {
			warningstream << "Invalid 2d vector string format: \"" << value
					<< "\"" << std::endl;
			return false;
		}

		*parsed_vec = vec;

		return true;
	}
};
