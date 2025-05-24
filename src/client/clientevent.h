// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include <string>
#include "irrlichttypes.h"
#include "client/hud.h" // HudElementStat

struct ParticleParameters;
struct ParticleSpawnerParameters;
struct SkyboxParams;
struct SunParams;
struct MoonParams;
struct StarParams;

enum ClientEventType : u8
{
	CE_NONE,
	CE_PLAYER_DAMAGE,
	CE_PLAYER_FORCE_MOVE,
	CE_DEATHSCREEN_LEGACY,
	CE_SHOW_FORMSPEC,
	CE_SHOW_CSM_FORMSPEC,
	CE_SHOW_PAUSE_MENU_FORMSPEC,
	CE_SPAWN_PARTICLE,
	CE_ADD_PARTICLESPAWNER,
	CE_DELETE_PARTICLESPAWNER,
	CE_HUDADD,
	CE_HUDRM,
	CE_HUDCHANGE,
	CE_SET_SKY,
	CE_SET_SUN,
	CE_SET_MOON,
	CE_SET_STARS,
	CE_OVERRIDE_DAY_NIGHT_RATIO,
	CE_CLOUD_PARAMS,
	CE_UPDATE_CAMERA,
	CLIENTEVENT_MAX,
};

struct ClientEventHudAdd
{
	u32 server_id;
	u8 type;
	v2f pos, scale;
	std::string name;
	std::string text, text2;
	u32 number, item, dir, style;
	v2f align, offset;
	v3f world_pos;
	v2s32 size;
	s16 z_index;
};

struct ClientEventHudChange
{
	u32 id;
	HudElementStat stat;
	v2f v2fdata;
	std::string sdata;
	u32 data;
	v3f v3fdata;
	v2s32 v2s32data;
};

struct ClientEvent
{
	// TODO: should get rid of this ctor
	ClientEvent() : type(CE_NONE) {}

	ClientEvent(ClientEventType type) : type(type) {}

	ClientEventType type;
	union
	{
		struct
		{
			u16 amount;
			bool effect;
		} player_damage;
		struct
		{
			f32 pitch;
			f32 yaw;
		} player_force_move;
		struct
		{
			std::string *formspec;
			std::string *formname;
		} show_formspec;
		ParticleParameters *spawn_particle;
		struct
		{
			ParticleSpawnerParameters *p;
			u16 attached_id;
			u64 id;
		} add_particlespawner;
		struct
		{
			u32 id;
		} delete_particlespawner;
		ClientEventHudAdd *hudadd;
		struct
		{
			u32 id;
		} hudrm;
		ClientEventHudChange *hudchange;
		SkyboxParams *set_sky;
		struct
		{
			bool do_override;
			float ratio_f;
		} override_day_night_ratio;
		struct
		{
			f32 density;
			u32 color_bright;
			u32 color_ambient;
			u32 color_shadow;
			f32 height;
			f32 thickness;
			f32 speed_x;
			f32 speed_y;
		} cloud_params;
		SunParams *sun_params;
		MoonParams *moon_params;
		StarParams *star_params;
	};
};
