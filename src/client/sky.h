// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include <ISceneNode.h>
#include <CMeshBuffer.h>
#include <array>
#include "camera.h" // CameraMode
#include "irr_ptr.h"
#include "skyparams.h"

#define SKY_MATERIAL_COUNT 12

namespace video
{
	class IVideoDriver;
	class IImage;
}

class IShaderSource;
class ITextureSource;

// Skybox, rendered with zbuffer turned off, before all other nodes.
class Sky : public scene::ISceneNode
{
public:
	//! constructor
	Sky(s32 id, RenderingEngine *rendering_engine, ITextureSource *tsrc, IShaderSource *ssrc);

	virtual void OnRegisterSceneNode();

	//! renders the node.
	virtual void render();

	virtual const aabb3f &getBoundingBox() const { return m_box; }

	// Used by Irrlicht for optimizing rendering
	virtual video::SMaterial &getMaterial(u32 i) { return m_materials[i]; }
	virtual u32 getMaterialCount() const { return SKY_MATERIAL_COUNT; }

	void update(float m_time_of_day, float time_brightness, float direct_brightness,
			bool sunlight_seen, CameraMode cam_mode, float yaw, float pitch);

	float getBrightness() { return m_brightness; }

	video::SColor getBgColor() const
	{
		return m_visible ? m_bgcolor : m_fallback_bg_color;
	}

	video::SColor getSkyColor() const
	{
		return m_visible ? m_skycolor : m_fallback_bg_color;
	}

	void setSunVisible(bool sun_visible) { m_sun_params.visible = sun_visible; }
	bool getSunVisible() const { return m_sun_params.visible; }
	void setSunTexture(const std::string &sun_texture,
		const std::string &sun_tonemap, ITextureSource *tsrc);
	void setSunScale(f32 sun_scale) { m_sun_params.scale = sun_scale; }
	void setSunriseVisible(bool glow_visible) { m_sun_params.sunrise_visible = glow_visible; }
	void setSunriseTexture(const std::string &sunglow_texture, ITextureSource* tsrc);
	v3f getSunDirection();

	void setMoonVisible(bool moon_visible) { m_moon_params.visible = moon_visible; }
	bool getMoonVisible() const { return m_moon_params.visible; }
	void setMoonTexture(const std::string &moon_texture,
		const std::string &moon_tonemap, ITextureSource *tsrc);
	void setMoonScale(f32 moon_scale) { m_moon_params.scale = moon_scale; }
	v3f getMoonDirection();

	void setStarsVisible(bool stars_visible) { m_star_params.visible = stars_visible; }
	void setStarCount(u16 star_count);
	void setStarColor(video::SColor star_color) { m_star_params.starcolor = star_color; }
	void setStarScale(f32 star_scale) { m_star_params.scale = star_scale; updateStars(); }
	void setStarDayOpacity(f32 day_opacity) { m_star_params.day_opacity = day_opacity; }

	bool getCloudsVisible() const { return m_clouds_visible && m_clouds_enabled; }
	const video::SColorf &getCloudColor() const { return m_cloudcolor_f; }

	void setVisible(bool visible) { m_visible = visible; }

	// Set only from set_sky API
	void setCloudsEnabled(bool clouds_enabled) { m_clouds_enabled = clouds_enabled; }
	void setFallbackBgColor(video::SColor fallback_bg_color)
	{
		m_fallback_bg_color = fallback_bg_color;
	}
	void setBodyOrbitTilt(float body_orbit_tilt)
	{
		if (body_orbit_tilt != SkyboxParams::INVALID_SKYBOX_TILT)
			m_sky_params.body_orbit_tilt = rangelim(body_orbit_tilt, -90.f, 90.f);
	}
	void overrideColors(video::SColor bgcolor, video::SColor skycolor)
	{
		m_bgcolor = bgcolor;
		m_skycolor = skycolor;
	}
	void setSkyColors(const SkyColor &sky_color);
	void setHorizonTint(video::SColor sun_tint, video::SColor moon_tint,
		const std::string &use_sun_tint);
	void setInClouds(bool clouds) { m_in_clouds = clouds; }
	void clearSkyboxTextures() { m_sky_params.textures.clear(); }
	void addTextureToSkybox(const std::string &texture, int material_id,
		ITextureSource *tsrc);

	// Note: the Sky class doesn't use these values. It just stores them.
	void setFogDistance(s16 fog_distance) { m_sky_params.fog_distance = fog_distance; }
	s16 getFogDistance() const { return m_sky_params.fog_distance; }

	void setFogStart(float fog_start) { m_sky_params.fog_start = fog_start; }
	float getFogStart() const { return m_sky_params.fog_start; }

	void setFogColor(video::SColor v) { m_sky_params.fog_color = v; }
	video::SColor getFogColor() const {
		if (m_sky_params.fog_color.getAlpha() > 0)
			return m_sky_params.fog_color;
		return getBgColor();
	}

private:
	aabb3f m_box{{0.0f, 0.0f, 0.0f}};
	video::SMaterial m_materials[SKY_MATERIAL_COUNT];
	// How much sun & moon transition should affect horizon color
	float m_horizon_blend()
	{
		if (!m_sunlight_seen)
			return 0;
		float x = m_time_of_day >= 0.5 ? (1 - m_time_of_day) * 2
					       : m_time_of_day * 2;

		if (x <= 0.3)
			return 0;
		if (x <= 0.4) // when the sun and moon are aligned
			return (x - 0.3) * 10;
		if (x <= 0.5)
			return (0.5 - x) * 10;
		return 0;
	}

	// Mix two colors by a given amount
	static video::SColor m_mix_scolor(video::SColor col1, video::SColor col2, f32 factor)
	{
		video::SColor result = video::SColor(
				col1.getAlpha() * (1 - factor) + col2.getAlpha() * factor,
				col1.getRed() * (1 - factor) + col2.getRed() * factor,
				col1.getGreen() * (1 - factor) + col2.getGreen() * factor,
				col1.getBlue() * (1 - factor) + col2.getBlue() * factor);
		return result;
	}
	static video::SColorf m_mix_scolorf(video::SColorf col1, video::SColorf col2, f32 factor)
	{
		video::SColorf result =
				video::SColorf(col1.r * (1 - factor) + col2.r * factor,
						col1.g * (1 - factor) + col2.g * factor,
						col1.b * (1 - factor) + col2.b * factor,
						col1.a * (1 - factor) + col2.a * factor);
		return result;
	}

	bool m_visible = true;
	// Used when m_visible=false
	video::SColor m_fallback_bg_color = video::SColor(255, 255, 255, 255);
	bool m_first_update = true; // Set before the sky is updated for the first time
	float m_time_of_day;
	float m_time_brightness;
	bool m_sunlight_seen;
	float m_brightness = 0.5f;
	float m_cloud_brightness = 0.5f;
	bool m_clouds_visible; // Whether clouds are disabled due to player underground
	bool m_clouds_enabled = true; // Initialised to true, reset only by set_sky API
	bool m_directional_colored_fog;
	bool m_in_clouds = true; // Prevent duplicating bools to remember old values

	video::SColorf m_bgcolor_bright_f = video::SColorf(1.0f, 1.0f, 1.0f, 1.0f);
	video::SColorf m_skycolor_bright_f = video::SColorf(1.0f, 1.0f, 1.0f, 1.0f);
	video::SColorf m_cloudcolor_bright_f = video::SColorf(1.0f, 1.0f, 1.0f, 1.0f);
	video::SColor m_bgcolor;
	video::SColor m_skycolor;
	video::SColorf m_cloudcolor_f;

	// pure white: becomes "diffuse light component" for clouds
	video::SColorf m_cloudcolor_day_f = video::SColorf(1, 1, 1, 1);
	// dawn-factoring version of pure white (note: R is above 1.0)
	video::SColorf m_cloudcolor_dawn_f = video::SColorf(
		255.0f/240.0f,
		223.0f/240.0f,
		191.0f/255.0f
	);

	SkyboxParams m_sky_params;
	SunParams m_sun_params;
	MoonParams m_moon_params;
	StarParams m_star_params;

	bool m_default_tint = true;

	u64 m_seed = 0;
	irr_ptr<scene::SMeshBuffer> m_stars;

	video::ITexture *m_sun_texture = nullptr;
	video::ITexture *m_moon_texture = nullptr;
	video::IImage *m_sun_tonemap = nullptr;
	video::IImage *m_moon_tonemap = nullptr;

	void updateStars();

	void draw_sun(video::IVideoDriver *driver, const video::SColor &suncolor,
		const video::SColor &suncolor2, float wicked_time_of_day);
	void draw_moon(video::IVideoDriver *driver, const video::SColor &mooncolor,
		const video::SColor &mooncolor2, float wicked_time_of_day);
	void draw_sky_body(std::array<video::S3DVertex, 4> &vertices,
		float pos_1, float pos_2, const video::SColor &c);
	void draw_stars(video::IVideoDriver *driver, float wicked_time_of_day);
	void place_sky_body(std::array<video::S3DVertex, 4> &vertices,
		float horizon_position,	float day_position);
};

// calculates value for sky body positions for the given observed time of day
// this is used to draw both Sun/Moon and shadows
float getWickedTimeOfDay(float time_of_day);
