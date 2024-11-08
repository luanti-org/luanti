// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "itemgroup.h"
#include "json-forwards.h"
#include "common/c_types.h"
#include <SColor.h>

#include <string>
#include <iostream>
#include <map>
#include <unordered_map>
#include <optional>

struct ItemDefinition;
class IItemDefManager;

struct ToolGroupCap
{
	std::unordered_map<int, float> times;
	int maxlevel = 1;
	int uses = 20;

	ToolGroupCap() = default;

	std::optional<float> getTime(int rating) const {
		auto i = times.find(rating);
		if (i == times.end())
			return std::nullopt;
		return i->second;
	}

	void toJson(Json::Value &object) const;
	void fromJson(const Json::Value &json);
};


typedef std::unordered_map<std::string, struct ToolGroupCap> ToolGCMap;
typedef std::unordered_map<std::string, s16> DamageGroup;

struct ToolCapabilities
{
	float full_punch_interval;
	int max_drop_level;
	ToolGCMap groupcaps;
	DamageGroup damageGroups;
	int punch_attack_uses;

	ToolCapabilities(
			float full_punch_interval_ = 1.4f,
			int max_drop_level_ = 1,
			const ToolGCMap &groupcaps_ = ToolGCMap(),
			const DamageGroup &damageGroups_ = DamageGroup(),
			int punch_attack_uses_ = 0
	):
		full_punch_interval(full_punch_interval_),
		max_drop_level(max_drop_level_),
		groupcaps(groupcaps_),
		damageGroups(damageGroups_),
		punch_attack_uses(punch_attack_uses_)
	{}

	void serialize(std::ostream &os, u16 version) const;
	void deSerialize(std::istream &is);
	void serializeJson(std::ostream &os) const;
	void deserializeJson(std::istream &is);

private:
	void deserializeJsonGroupcaps(Json::Value &json);
	void deserializeJsonDamageGroups(Json::Value &json);
};

struct WearBarParams
{
	std::map<f32, video::SColor> colorStops;
	enum BlendMode : u8 {
	    BLEND_MODE_CONSTANT,
	    BLEND_MODE_LINEAR,
	    BlendMode_END // Dummy for validity check
	};
	constexpr const static EnumString es_BlendMode[3] = {
		{WearBarParams::BLEND_MODE_CONSTANT, "constant"},
		{WearBarParams::BLEND_MODE_LINEAR, "linear"},
		{0, nullptr}
	};
	BlendMode blend;

	WearBarParams(const std::map<f32, video::SColor> &colorStops, BlendMode blend):
		colorStops(colorStops),
		blend(blend)
	{}

	WearBarParams(const video::SColor color):
		WearBarParams({{0.0f, color}}, WearBarParams::BLEND_MODE_CONSTANT)
	{};

	void serialize(std::ostream &os) const;
	static WearBarParams deserialize(std::istream &is);
	void serializeJson(std::ostream &os) const;
	static std::optional<WearBarParams> deserializeJson(std::istream &is);
	video::SColor getWearBarColor(f32 durabilityPercent);
};

struct DigParams
{
	bool diggable;
	// Digging time in seconds
	float time;
	// Caused wear
	u32 wear; // u32 because wear could be 65536 (single-use tool)
	std::string main_group;

	DigParams(bool a_diggable = false, float a_time = 0.0f, u32 a_wear = 0,
			const std::string &a_main_group = ""):
		diggable(a_diggable),
		time(a_time),
		wear(a_wear),
		main_group(a_main_group)
	{}
};

DigParams getDigParams(const ItemGroupList &groups,
		const ToolCapabilities *tp,
		const u16 initial_wear = 0);

struct HitParams
{
	s32 hp;
	// Caused wear
	u32 wear; // u32 because wear could be 65536 (single-use weapon)

	HitParams(s32 hp_ = 0, u32 wear_ = 0):
		hp(hp_),
		wear(wear_)
	{}
};

HitParams getHitParams(const ItemGroupList &armor_groups,
		const ToolCapabilities *tp, float time_from_last_punch,
		u16 initial_wear = 0);

HitParams getHitParams(const ItemGroupList &armor_groups,
		const ToolCapabilities *tp);

struct PunchDamageResult
{
	bool did_punch = false;
	int damage = 0;
	int wear = 0;

	PunchDamageResult() = default;
};

struct ItemStack;

PunchDamageResult getPunchDamage(
		const ItemGroupList &armor_groups,
		const ToolCapabilities *toolcap,
		const ItemStack *punchitem,
		float time_from_last_punch,
		u16 initial_wear = 0
);

u32 calculateResultWear(const u32 uses, const u16 initial_wear);
f32 getToolRange(const ItemStack &wielded_item, const ItemStack &hand_item,
		const IItemDefManager *itemdef_manager);
