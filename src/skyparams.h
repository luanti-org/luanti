// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2019 Jordach, Jordan Snelling <jordach.snelling@gmail.com>

#pragma once

#include <vector>
#include <string>
#include "SColor.h"
#include "irr_v2d.h"


struct SkyColor
{
	video::SColor day_sky;
	video::SColor day_horizon;
	video::SColor dawn_sky;
	video::SColor dawn_horizon;
	video::SColor night_sky;
	video::SColor night_horizon;
	video::SColor indoors;
};

struct SkyboxParams
{
	static constexpr float INVALID_SKYBOX_TILT = -1024.f;

	video::SColor bgcolor;
	std::string type;
	std::vector<std::string> textures;
	bool clouds;
	SkyColor sky_color;
	video::SColor fog_sun_tint;
	video::SColor fog_moon_tint;
	std::string fog_tint_type;
	float body_orbit_tilt { INVALID_SKYBOX_TILT };
	s16 fog_distance { -1 };
	float fog_start { -1.0f };
	video::SColor fog_color { 0 }; // override, only used if alpha > 0
};

struct SunParams
{
	bool visible;
	std::string texture;
	std::string tonemap;
	std::string sunrise;
	bool sunrise_visible;
	f32 scale;
};

struct MoonParams
{
	bool visible;
	std::string texture;
	std::string tonemap;
	f32 scale;
};

struct StarParams
{
	bool visible;
	u32 count;
	video::SColor starcolor;
	f32 scale;
	f32 day_opacity;
};

struct CloudParams
{
	float density;
	video::SColor color_bright;
	video::SColor color_ambient;
	video::SColor color_shadow;
	float thickness;
	float height;
	v2f speed;
};

// Utility class for setting default sky, sun, moon, stars values:
class SkyboxDefaults
{
public:
	SkyboxDefaults() = delete;

	static const SkyboxParams getSkyDefaults()
	{
		SkyboxParams sky;
		sky.bgcolor = video::SColor(255, 255, 255, 255);
		sky.type = "regular";
		sky.clouds = true;
		sky.sky_color = getSkyColorDefaults();
		sky.fog_sun_tint = video::SColor(255, 244, 125, 29);
		sky.fog_moon_tint = video::SColorf(0.5, 0.6, 0.8, 1).toSColor();
		sky.fog_tint_type = "default";
		sky.fog_color = video::SColor(0);
		return sky;
	}

	static const SkyColor getSkyColorDefaults()
	{
		SkyColor sky;
		// Horizon colors
		sky.day_horizon = video::SColor(255, 144, 211, 246);
		sky.indoors = video::SColor(255, 100, 100, 100);
		sky.dawn_horizon = video::SColor(255, 186, 193, 240);
		sky.night_horizon = video::SColor(255, 64, 144, 255);
		// Sky colors
		sky.day_sky = video::SColor(255, 97, 181, 245);
		sky.dawn_sky = video::SColor(255, 180, 186, 250);
		sky.night_sky = video::SColor(255, 0, 107, 255);
		return sky;
	}

	static const SunParams getSunDefaults()
	{
		SunParams sun;
		sun.visible = true;
		sun.sunrise_visible = true;
		sun.texture = "sun.png";
		sun.tonemap = "sun_tonemap.png";
		sun.sunrise = "sunrisebg.png";
		sun.scale = 1;
		return sun;
	}

	static const MoonParams getMoonDefaults()
	{
		MoonParams moon;
		moon.visible = true;
		moon.texture = "moon.png";
		moon.tonemap = "moon_tonemap.png";
		moon.scale = 1;
		return moon;
	}

	static const StarParams getStarDefaults()
	{
		StarParams stars;
		stars.visible = true;
		stars.count = 1000;
		stars.starcolor = video::SColor(105, 235, 235, 255);
		stars.scale = 1;
		stars.day_opacity = 0;
		return stars;
	}

	static const CloudParams getCloudDefaults()
	{
		CloudParams clouds;
		clouds.density = 0.4f;
		clouds.color_bright = video::SColor(229, 240, 240, 255);
		clouds.color_ambient = video::SColor(255, 0, 0, 0);
		clouds.color_shadow = video::SColor(255, 204, 204, 204);
		clouds.thickness = 16.0f;
		clouds.height = 120;
		clouds.speed = v2f(0.0f, -2.0f);
		return clouds;
	}
};
