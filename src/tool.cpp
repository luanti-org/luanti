// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "tool.h"
#include "itemdef.h"
#include "itemgroup.h"
#include "log.h"
#include "inventory.h"
#include "exceptions.h"
#include "convert_json.h"
#include "util/serialize.h"
#include "util/numeric.h"
#include "util/hex.h"
#include <json/json.h>


void ToolGroupCap::toJson(Json::Value &object) const
{
	object["maxlevel"] = maxlevel;
	object["uses"] = uses;

	Json::Value times_object;
	for (auto time : times)
		times_object[time.first] = time.second;
	object["times"] = std::move(times_object);
}

void ToolGroupCap::fromJson(const Json::Value &json)
{
	if (!json.isObject())
		return;

	if (json["maxlevel"].isInt())
		maxlevel = json["maxlevel"].asInt();

	if (json["uses"].isInt())
		uses = json["uses"].asInt();

	const Json::Value &times_object = json["times"];

	if (!times_object.isArray())
		return;

	Json::ArrayIndex size = times_object.size();

	for (Json::ArrayIndex i = 0; i < size; ++i) {
		if (times_object[i].isDouble())
			times.emplace_back(i, times_object[i].asFloat());
	}
}

void ToolCapabilities::serialize(std::ostream &os, u16 protocol_version) const
{
	if (protocol_version >= 38)
		writeU8(os, 5);
	else
		writeU8(os, 4); // proto == 37

	writeF32(os, full_punch_interval);
	writeS16(os, max_drop_level);
	writeU32(os, groupcaps.size());

	for (const auto &groupcap : groupcaps) {
		const std::string *name = &groupcap.first;
		const ToolGroupCap *cap = &groupcap.second;
		os << serializeString16(*name);
		writeS16(os, cap->uses);
		writeS16(os, cap->maxlevel);
		writeU32(os, cap->times.size());
		for (const auto &time : cap->times) {
			writeS16(os, time.first);
			writeF32(os, time.second);
		}
	}

	writeU32(os, damageGroups.size());

	for (const auto &damageGroup : damageGroups) {
		os << serializeString16(damageGroup.first);
		writeS16(os, damageGroup.second);
	}

	if (protocol_version >= 38)
		writeU16(os, rangelim(punch_attack_uses, 0, U16_MAX));
}

void ToolCapabilities::deSerialize(std::istream &is)
{
	int version = readU8(is);
	if (version < 4)
		throw SerializationError("unsupported ToolCapabilities version");

	full_punch_interval = readF32(is);
	max_drop_level = readS16(is);
	groupcaps.clear();
	u32 groupcaps_size = readU32(is);

	for (u32 i = 0; i < groupcaps_size; i++) {
		std::string name = deSerializeString16(is);
		ToolGroupCap cap;
		cap.uses = readS16(is);
		cap.maxlevel = readS16(is);
		u32 times_size = readU32(is);
		for(u32 i = 0; i < times_size; i++) {
			int level = readS16(is);
			float time = readF32(is);
			cap.times.emplace_back(level, time);
		}
		groupcaps[name] = cap;
	}

	u32 damage_groups_size = readU32(is);
	for (u32 i = 0; i < damage_groups_size; i++) {
		std::string name = deSerializeString16(is);
		s16 rating = readS16(is);
		damageGroups[name] = rating;
	}

	if (version >= 5)
		punch_attack_uses = readU16(is);
}

void ToolCapabilities::serializeJson(std::ostream &os) const
{
	Json::Value root;
	root["full_punch_interval"] = full_punch_interval;
	root["max_drop_level"] = max_drop_level;
	root["punch_attack_uses"] = punch_attack_uses;

	Json::Value groupcaps_object;

	for (const auto &groupcap : groupcaps) {
		groupcap.second.toJson(groupcaps_object[groupcap.first]);
	}

	root["groupcaps"] = std::move(groupcaps_object);

	Json::Value damage_groups_object;

	for (const auto &damagegroup : damageGroups) {
		damage_groups_object[damagegroup.first] = damagegroup.second;
	}

	root["damage_groups"] = std::move(damage_groups_object);

	fastWriteJson(root, os);
}

void ToolCapabilities::deserializeJson(std::istream &is)
{
	Json::Value root;
	is >> root;

	if (!root.isObject())
		return;

	if (root["full_punch_interval"].isDouble())
		full_punch_interval = root["full_punch_interval"].asFloat();

	if (root["max_drop_level"].isInt())
		max_drop_level = root["max_drop_level"].asInt();

	if (root["punch_attack_uses"].isInt())
		punch_attack_uses = root["punch_attack_uses"].asInt();

	deserializeJsonGroupcaps(root["groupcaps"]);
	deserializeJsonDamageGroups(root["damage_groups"]);
}

void ToolCapabilities::deserializeJsonGroupcaps(Json::Value &json)
{
	if (!json.isObject())
		return;

	for (Json::ValueIterator iter = json.begin(); iter != json.end(); ++iter) {
		ToolGroupCap value;
		value.fromJson(*iter);
		groupcaps[iter.key().asString()] = value;
	}
}

void ToolCapabilities::deserializeJsonDamageGroups(Json::Value &json)
{
	if (!json.isObject())
		return;

	for (Json::ValueIterator iter = json.begin(); iter != json.end(); ++iter) {
		Json::Value &value = *iter;
		if (value.isInt())
			damageGroups[iter.key().asString()] = value.asInt();
	}
}

void WearBarParams::serialize(std::ostream &os) const
{
	writeU8(os, 1); // Version for future-proofing
	writeU8(os, blend);
	writeU16(os, colorStops.size());
	for (const std::pair<f32, video::SColor> item : colorStops) {
		writeF32(os, item.first);
		writeARGB8(os, item.second);
	}
}

WearBarParams WearBarParams::deserialize(std::istream &is)
{
	u8 version = readU8(is);
	if (version > 1)
		throw SerializationError("unsupported WearBarParams version");

	auto blend = static_cast<WearBarParams::BlendMode>(readU8(is));
	if (blend >= BlendMode_END)
		throw SerializationError("invalid blend mode");
	u16 count = readU16(is);
	if (count == 0)
		throw SerializationError("no stops");
	std::map<f32, video::SColor> colorStops;
	for (u16 i = 0; i < count; i++) {
		f32 key = readF32(is);
		if (key < 0 || key > 1)
			throw SerializationError("key out of range");
		video::SColor color = readARGB8(is);
		colorStops.emplace(key, color);
	}
	return WearBarParams(colorStops, blend);
}

void WearBarParams::serializeJson(std::ostream &os) const
{
	Json::Value root;
	Json::Value color_stops;
	for (const std::pair<f32, video::SColor> item : colorStops) {
		color_stops[ftos(item.first)] = encodeHexColorString(item.second);
	}
	root["color_stops"] = color_stops;
	root["blend"] = enum_to_string(WearBarParams::es_BlendMode, blend);

	fastWriteJson(root, os);
}

std::optional<WearBarParams> WearBarParams::deserializeJson(std::istream &is)
{
	Json::Value root;
	is >> root;
	if (!root.isObject() || !root["color_stops"].isObject() || !root["blend"].isString())
		return std::nullopt;

	int blendInt;
	WearBarParams::BlendMode blend;
	if (string_to_enum(WearBarParams::es_BlendMode, blendInt, root["blend"].asString()))
		blend = static_cast<WearBarParams::BlendMode>(blendInt);
	else
		return std::nullopt;

	const Json::Value &color_stops_object = root["color_stops"];
	std::map<f32, video::SColor> colorStops;
	for (const std::string &key : color_stops_object.getMemberNames()) {
		f32 stop = stof(key);
		if (stop < 0 || stop > 1)
			return std::nullopt;
		const Json::Value &value = color_stops_object[key];
		if (value.isString()) {
			video::SColor color;
			parseColorString(value.asString(), color, false);
			colorStops.emplace(stop, color);
		}
	}
	if (colorStops.empty())
		return std::nullopt;
	return WearBarParams(colorStops, blend);
}

video::SColor WearBarParams::getWearBarColor(f32 durabilityPercent)
{
	if (colorStops.empty())
		return video::SColor();

	auto upper = colorStops.upper_bound(durabilityPercent);

	if (upper == colorStops.end()) // durability is >= the highest defined color stop
		return std::prev(colorStops.end())->second; // return last element of the map

	if (upper == colorStops.begin()) // durability is <= the lowest defined color stop
		return upper->second;

	// between two values, interpolate
	auto lower = std::prev(upper);
	f32 lower_bound = lower->first;
	video::SColor lower_color = lower->second;
	f32 upper_bound = upper->first;
	video::SColor upper_color = upper->second;

	f32 progress = (durabilityPercent - lower_bound) / (upper_bound - lower_bound);

	switch (blend) {
		case BLEND_MODE_CONSTANT:
			return lower_color;
		case BLEND_MODE_LINEAR:
			return upper_color.getInterpolated(lower_color, progress);
		case BlendMode_END:
			throw std::logic_error("dummy value");
	}
	throw std::logic_error("invalid blend value");
}

u32 calculateResultWear(const u32 uses, const u16 initial_wear)
{
	if (uses == 0) {
		// Trivial case: Infinite uses
		return 0;
	}
	/* Finite uses. This is not trivial,
	as the maximum wear is not neatly evenly divisible by
	most possible uses numbers. For example, for 128
	uses, the calculation of wear is trivial, as
	65536 / 128 uses = 512 wear,
	so the tool will get 512 wear 128 times in its lifetime.
	But for a number like 130, this does not work:
	65536 / 130 uses = 504.123... wear.
	Since wear must be an integer, we will get
	504*130 = 65520, which would lead to the wrong number
	of uses.

	Instead, we partition the "wear range" into blocks:
	A block represents a single use and can be
	of two possible sizes: normal and oversized.
	A normal block is equal to floor(65536 / uses).
	An oversized block is a normal block plus 1.
	Then we determine how many oversized and normal
	blocks we need and finally, whether we add
	the normal wear or the oversized wear.

	Example for 130 uses:
	* Normal wear = 504
	* Number of normal blocks = 114
	* Oversized wear = 505
	* Number of oversized blocks = 16

	If we add everything together, we get:
	  114*504 + 16*505 = 65536
	*/
	u32 result_wear;
	u32 wear_normal = ((U16_MAX+1) / uses);
	// Will be non-zero if its not evenly divisible
	u16 blocks_oversize = (U16_MAX+1) % uses;
	// Whether to add one extra wear point in case
	// of oversized wear.
	u16 wear_extra = 0;
	if (blocks_oversize > 0) {
		u16 blocks_normal = uses - blocks_oversize;
		/* When the wear has reached this value, we
		   know that wear_normal has been applied
		   for blocks_normal times, therefore,
		   only oversized blocks remain.
		   This also implies the raw tool wear number
		   increases a bit faster after this point,
		   but this should be barely noticeable by the
		   player.
		*/
		u16 wear_extra_at = blocks_normal * wear_normal;
		if (initial_wear >= wear_extra_at)
			wear_extra = 1;
	}
	result_wear = wear_normal + wear_extra;
	return result_wear;
}

DigParams getDigParams(const ItemGroupList &groups,
		const ToolCapabilities *tp,
		const u16 initial_wear)
{

	// Group dig_immediate defaults to fixed time and no wear
	if (tp->groupcaps.find("dig_immediate") == tp->groupcaps.cend()) {
		switch (itemgroup_get(groups, "dig_immediate")) {
		case 2:
			return DigParams(true, 0.5, 0, "dig_immediate");
		case 3:
			return DigParams(true, 0, 0, "dig_immediate");
		default:
			break;
		}
	}

	// Values to be returned (with a bit of conversion)
	bool result_diggable = false;
	float result_time = 0.0;
	u32 result_wear = 0;
	std::string result_main_group;

	int level = itemgroup_get(groups, "level");
	for (const auto &groupcap : tp->groupcaps) {
		const ToolGroupCap &cap = groupcap.second;

		int leveldiff = cap.maxlevel - level;
		if (leveldiff < 0)
			continue;

		const std::string &groupname = groupcap.first;
		int rating = itemgroup_get(groups, groupname);
		const auto time_o = cap.getTime(rating);
		if (!time_o.has_value())
			continue;
		float time = *time_o;

		if (leveldiff > 1)
			time /= leveldiff;

		if (!result_diggable || time < result_time) {
			result_time = time;
			result_diggable = true;
			// The actual number of uses increases
			// exponentially with leveldiff.
			// If the levels are equal, real_uses equals cap.uses.
			const u32 real_uses = std::min<f64>(cap.uses * pow(3.0, leveldiff), U16_MAX);
			result_wear = calculateResultWear(real_uses, initial_wear);
			result_main_group = groupname;
		}
	}

	return DigParams(result_diggable, result_time, result_wear, result_main_group);
}

HitParams getHitParams(const ItemGroupList &armor_groups,
		const ToolCapabilities *tp, float time_from_last_punch,
		u16 initial_wear)
{
	s32 damage = 0;
	float result_wear = 0.0f;
	float punch_interval_multiplier =
			rangelim(time_from_last_punch / tp->full_punch_interval, 0.0f, 1.0f);

	for (const auto &damageGroup : tp->damageGroups) {
		s16 armor = itemgroup_get(armor_groups, damageGroup.first);
		damage += damageGroup.second * punch_interval_multiplier * armor / 100.0;
	}

	if (tp->punch_attack_uses > 0) {
		result_wear = calculateResultWear(tp->punch_attack_uses, initial_wear);
		result_wear *= punch_interval_multiplier;
	}
	// Keep damage in sane bounds for simplicity
	damage = rangelim(damage, -U16_MAX, U16_MAX);

	u32 wear_i = (u32) result_wear;
	return {damage, wear_i};
}

HitParams getHitParams(const ItemGroupList &armor_groups,
		const ToolCapabilities *tp)
{
	return getHitParams(armor_groups, tp, 1000000);
}

PunchDamageResult getPunchDamage(
		const ItemGroupList &armor_groups,
		const ToolCapabilities *toolcap,
		const ItemStack *punchitem,
		float time_from_last_punch,
		u16 initial_wear
){
	bool do_hit = true;
	{
		if (do_hit && punchitem) {
			if (itemgroup_get(armor_groups, "punch_operable") &&
					(toolcap == NULL || punchitem->name.empty()))
				do_hit = false;
		}

		if (do_hit) {
			if(itemgroup_get(armor_groups, "immortal"))
				do_hit = false;
		}
	}

	PunchDamageResult result;
	if(do_hit)
	{
		HitParams hitparams = getHitParams(armor_groups, toolcap,
				time_from_last_punch,
				punchitem ? punchitem->wear : 0);
		result.did_punch = true;
		result.wear = hitparams.wear;
		result.damage = hitparams.hp;
	}

	return result;
}

f32 getToolRange(const ItemStack &wielded_item, const ItemStack &hand_item,
		const IItemDefManager *itemdef_manager)
{
	const std::string &wielded_meta_range = wielded_item.metadata.getString("range");
	const std::string &hand_meta_range = hand_item.metadata.getString("range");

	f32 max_d = wielded_meta_range.empty() ? wielded_item.getDefinition(itemdef_manager).range :
			stof(wielded_meta_range);
	f32 max_d_hand = hand_meta_range.empty() ? hand_item.getDefinition(itemdef_manager).range :
			stof(hand_meta_range);

	if (max_d < 0 && max_d_hand >= 0)
		max_d = max_d_hand;
	else if (max_d < 0)
		max_d = 4.0f;

	return max_d;
}
