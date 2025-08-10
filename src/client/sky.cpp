// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2020 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include <cmath>
#include "sky.h"
#include <ITexture.h>
#include <IVideoDriver.h>
#include <ISceneManager.h>
#include <ICameraSceneNode.h>
#include <S3DVertex.h>
#include "client/mesh.h"
#include "client/tile.h"
#include "noise.h" // easeCurve
#include "profiler.h"
#include "util/numeric.h"
#include "client/renderingengine.h"
#include "client/texturesource.h"
#include "settings.h"
#include "camera.h" // CameraModes

using namespace core;

static video::SMaterial baseMaterial()
{
	video::SMaterial mat;
	mat.ZBuffer = video::ECFN_DISABLED;
	mat.ZWriteEnable = video::EZW_OFF;
	mat.AntiAliasing = video::EAAM_OFF;
	mat.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
	mat.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	mat.BackfaceCulling = false;
	return mat;
}

static inline void disableTextureFiltering(video::SMaterial &mat)
{
	mat.forEachTexture([] (auto &tex) {
		tex.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
		tex.MagFilter = video::ETMAGF_NEAREST;
		tex.AnisotropicFilter = 0;
	});
}

Sky::Sky(s32 id, RenderingEngine *rendering_engine, ITextureSource *tsrc, IShaderSource *ssrc) :
		scene::ISceneNode(rendering_engine->get_scene_manager()->getRootSceneNode(),
			rendering_engine->get_scene_manager(), id)
{
	m_seed = (u64)myrand() << 32 | myrand();

	setAutomaticCulling(scene::EAC_OFF);

	m_sky_params = SkyboxDefaults::getSkyDefaults();
	m_sun_params = SkyboxDefaults::getSunDefaults();
	m_moon_params = SkyboxDefaults::getMoonDefaults();
	m_star_params = SkyboxDefaults::getStarDefaults();

	// Create materials

	m_materials[0] = baseMaterial();
	m_materials[0].MaterialType =
			ssrc->getShaderInfo(ssrc->getShaderRaw("stars_shader", true)).material;

	m_materials[1] = baseMaterial();
	m_materials[1].MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;

	m_materials[2] = baseMaterial();
	m_materials[2].setTexture(0, tsrc->getTextureForMesh("sunrisebg.png"));
	m_materials[2].MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;

	setSunTexture(m_sun_params.texture, m_sun_params.tonemap, tsrc);

	setMoonTexture(m_moon_params.texture, m_moon_params.tonemap, tsrc);

	for (int i = 5; i < 11; i++) {
		m_materials[i] = baseMaterial();
		m_materials[i].MaterialType = video::EMT_SOLID;
	}

	m_directional_colored_fog = g_settings->getBool("directional_colored_fog");
	m_sky_params.body_orbit_tilt = g_settings->getFloat("shadow_sky_body_orbit_tilt", -60., 60.);
	m_sky_params.fog_start = rangelim(g_settings->getFloat("fog_start"), 0.0f, 0.99f);

	setStarCount(1000);
}

void Sky::OnRegisterSceneNode()
{
	if (IsVisible)
		SceneManager->registerNodeForRendering(this, scene::ESNRP_SKY_BOX);

	scene::ISceneNode::OnRegisterSceneNode();
}

void Sky::render()
{
	video::IVideoDriver *driver = SceneManager->getVideoDriver();
	scene::ICameraSceneNode *camera = SceneManager->getActiveCamera();

	if (!camera || !driver)
		return;

	ScopeProfiler sp(g_profiler, "Sky::render()", SPT_AVG, PRECISION_MICRO);

	// Draw perspective skybox

	core::matrix4 translate(AbsoluteTransformation);
	translate.setTranslation(camera->getAbsolutePosition());

	// Draw the sky box between the near and far clip plane
	const f32 viewDistance = (camera->getNearValue() + camera->getFarValue()) * 0.5f;
	core::matrix4 scale;
	scale.setScale(core::vector3df(viewDistance, viewDistance, viewDistance));

	driver->setTransform(video::ETS_WORLD, translate * scale);

	if (m_sunlight_seen) {
		video::SColorf suncolor_f(1, 1, 0, 1);
		//suncolor_f.r = 1;
		//suncolor_f.g = MYMAX(0.3, MYMIN(1.0, 0.7 + m_time_brightness * 0.5));
		//suncolor_f.b = MYMAX(0.0, m_brightness * 0.95);
		video::SColorf suncolor2_f(1, 1, 1, 1);
		// The values below were probably meant to be suncolor2_f instead of a
		// reassignment of suncolor_f. However, the resulting colour was chosen
		// and is our long-running classic colour. So preserve, but comment-out
		// the unnecessary first assignments above.
		suncolor_f.r = 1;
		suncolor_f.g = MYMAX(0.3, MYMIN(1.0, 0.85 + m_time_brightness * 0.5));
		suncolor_f.b = MYMAX(0.0, m_brightness);

		video::SColorf mooncolor_f(0.50, 0.57, 0.65, 1);
		video::SColorf mooncolor2_f(0.85, 0.875, 0.9, 1);

		float wicked_time_of_day = getWickedTimeOfDay(m_time_of_day);

		video::SColor suncolor = suncolor_f.toSColor();
		video::SColor suncolor2 = suncolor2_f.toSColor();
		video::SColor mooncolor = mooncolor_f.toSColor();
		video::SColor mooncolor2 = mooncolor2_f.toSColor();

		// Calculate offset normalized to the X dimension of a 512x1 px tonemap
		float offset = (1.0 - fabs(sin((m_time_of_day - 0.5) * core::PI))) * 511;

		if (m_sun_tonemap) {
			auto texel_color = m_sun_tonemap->getPixel(offset, 0);
			texel_color.setAlpha(255);
			// Only accessed by our code later, not used by a shader
			m_materials[3].ColorParam = texel_color;
		}

		if (m_moon_tonemap) {
			auto texel_color = m_moon_tonemap->getPixel(offset, 0);
			texel_color.setAlpha(255);
			// Only accessed by our code later, not used by a shader
			m_materials[4].ColorParam = texel_color;
		}

		const f32 t = 1.0f;
		const f32 o = 0.0f;
		static const u16 indices[6] = {0, 1, 2, 0, 2, 3};
		video::S3DVertex vertices[4];

		driver->setMaterial(m_materials[1]);

		video::SColor cloudyfogcolor = m_bgcolor;

		// Abort rendering if we're in the clouds.
		// Stops rendering a pure white hole in the bottom of the skybox.
		if (m_in_clouds)
			return;

		// Draw the six sided skybox,
		if (m_sky_params.textures.size() == 6) {
			for (u32 j = 5; j < 11; j++) {
				video::SColor c(255, 255, 255, 255);
				driver->setMaterial(m_materials[j]);
				// Use 1.05 rather than 1.0 to avoid colliding with the
				// sun, moon and stars, as this is a background skybox.
				vertices[0] = video::S3DVertex(-1.05, -1.05, -1.05, 0, 0, 1, c, t, t);
				vertices[1] = video::S3DVertex( 1.05, -1.05, -1.05, 0, 0, 1, c, o, t);
				vertices[2] = video::S3DVertex( 1.05,  1.05, -1.05, 0, 0, 1, c, o, o);
				vertices[3] = video::S3DVertex(-1.05,  1.05, -1.05, 0, 0, 1, c, t, o);
				for (video::S3DVertex &vertex : vertices) {
					if (j == 5) { // Top texture
						vertex.Pos.rotateYZBy(90);
						vertex.Pos.rotateXZBy(90);
					} else if (j == 6) { // Bottom texture
						vertex.Pos.rotateYZBy(-90);
						vertex.Pos.rotateXZBy(90);
					} else if (j == 7) { // Left texture
						vertex.Pos.rotateXZBy(90);
					} else if (j == 8) { // Right texture
						vertex.Pos.rotateXZBy(-90);
					} else if (j == 9) { // Front texture, do nothing
						// Irrlicht doesn't like it when vertexes are left
						// alone and not rotated for some reason.
						vertex.Pos.rotateXZBy(0);
					} else {// Back texture
						vertex.Pos.rotateXZBy(180);
					}
				}
				driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
			}
		}

		// Draw far cloudy fog thing blended with skycolor
		if (m_visible) {
			driver->setMaterial(m_materials[1]);
			for (u32 j = 0; j < 4; j++) {
				vertices[0] = video::S3DVertex(-1, -0.02, -1, 0, 0, 1, m_bgcolor, t, t);
				vertices[1] = video::S3DVertex( 1, -0.02, -1, 0, 0, 1, m_bgcolor, o, t);
				vertices[2] = video::S3DVertex( 1, 0.45, -1, 0, 0, 1, m_skycolor, o, o);
				vertices[3] = video::S3DVertex(-1, 0.45, -1, 0, 0, 1, m_skycolor, t, o);
				for (video::S3DVertex &vertex : vertices) {
					if (j == 0)
						// Don't switch
						{}
					else if (j == 1)
						// Switch from -Z (south) to +X (east)
						vertex.Pos.rotateXZBy(90);
					else if (j == 2)
						// Switch from -Z (south) to -X (west)
						vertex.Pos.rotateXZBy(-90);
					else
						// Switch from -Z (south) to +Z (north)
						vertex.Pos.rotateXZBy(-180);
				}
				driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
			}
		}

		// Draw stars before sun and moon to be behind them
		if (m_star_params.visible)
			draw_stars(driver, wicked_time_of_day);

		// Draw sunrise/sunset horizon glow texture
		// (textures/base/pack/sunrisebg.png)
		if (m_sun_params.sunrise_visible) {
			driver->setMaterial(m_materials[2]);
			float mid1 = 0.25;
			float mid = wicked_time_of_day < 0.5 ? mid1 : (1.0 - mid1);
			float a_ = 1.0f - std::fabs(wicked_time_of_day - mid) * 35.0f;
			float a = easeCurve(MYMAX(0, MYMIN(1, a_)));
			//std::cerr<<"a_="<<a_<<" a="<<a<<std::endl;
			video::SColor c(255, 255, 255, 255);
			float y = -(1.0 - a) * 0.22;
			vertices[0] = video::S3DVertex(-1, -0.05 + y, -1, 0, 0, 1, c, t, t);
			vertices[1] = video::S3DVertex( 1, -0.05 + y, -1, 0, 0, 1, c, o, t);
			vertices[2] = video::S3DVertex( 1,   0.2 + y, -1, 0, 0, 1, c, o, o);
			vertices[3] = video::S3DVertex(-1,   0.2 + y, -1, 0, 0, 1, c, t, o);
			for (video::S3DVertex &vertex : vertices) {
				if (wicked_time_of_day < 0.5)
					// Switch from -Z (south) to +X (east)
					vertex.Pos.rotateXZBy(90);
				else
					// Switch from -Z (south) to -X (west)
					vertex.Pos.rotateXZBy(-90);
			}
			driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
		}

		// Draw sun
		if (m_sun_params.visible)
			draw_sun(driver, suncolor, suncolor2, wicked_time_of_day);

		// Draw moon
		if (m_moon_params.visible)
			draw_moon(driver, mooncolor, mooncolor2, wicked_time_of_day);

		// Draw far cloudy fog thing below all horizons in front of sun, moon
		// and stars.
		if (m_visible) {
			driver->setMaterial(m_materials[1]);

			for (u32 j = 0; j < 4; j++) {
				video::SColor c = cloudyfogcolor;
				vertices[0] = video::S3DVertex(-1, -1.0,  -1, 0, 0, 1, c, t, t);
				vertices[1] = video::S3DVertex( 1, -1.0,  -1, 0, 0, 1, c, o, t);
				vertices[2] = video::S3DVertex( 1, -0.02, -1, 0, 0, 1, c, o, o);
				vertices[3] = video::S3DVertex(-1, -0.02, -1, 0, 0, 1, c, t, o);
				for (video::S3DVertex &vertex : vertices) {
					if (j == 0)
						// Don't switch
						{}
					else if (j == 1)
						// Switch from -Z (south) to +X (east)
						vertex.Pos.rotateXZBy(90);
					else if (j == 2)
						// Switch from -Z (south) to -X (west)
						vertex.Pos.rotateXZBy(-90);
					else
						// Switch from -Z (south) to +Z (north)
						vertex.Pos.rotateXZBy(-180);
				}
				driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
			}

			// Draw bottom far cloudy fog thing in front of sun, moon and stars
			video::SColor c = cloudyfogcolor;
			vertices[0] = video::S3DVertex(-1, -1.0, -1, 0, 1, 0, c, t, t);
			vertices[1] = video::S3DVertex( 1, -1.0, -1, 0, 1, 0, c, o, t);
			vertices[2] = video::S3DVertex( 1, -1.0, 1, 0, 1, 0, c, o, o);
			vertices[3] = video::S3DVertex(-1, -1.0, 1, 0, 1, 0, c, t, o);
			driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
		}
	}
}

void Sky::update(float time_of_day, float time_brightness,
	float direct_brightness, bool sunlight_seen,
	CameraMode cam_mode, float yaw, float pitch)
{
	// Stabilize initial brightness and color values by flooding updates
	if (m_first_update) {
		/*dstream<<"First update with time_of_day="<<time_of_day
				<<" time_brightness="<<time_brightness
				<<" direct_brightness="<<direct_brightness
				<<" sunlight_seen="<<sunlight_seen<<std::endl;*/
		m_first_update = false;
		for (u32 i = 0; i < 100; i++) {
			update(time_of_day, time_brightness, direct_brightness,
					sunlight_seen, cam_mode, yaw, pitch);
		}
		return;
	}

	m_time_of_day = time_of_day;
	m_time_brightness = time_brightness;
	m_sunlight_seen = sunlight_seen;
	m_in_clouds = false;

	bool is_dawn = (time_brightness >= 0.20 && time_brightness < 0.35);

	video::SColorf bgcolor_bright_normal_f = m_sky_params.sky_color.day_horizon;
	video::SColorf bgcolor_bright_indoor_f = m_sky_params.sky_color.indoors;
	video::SColorf bgcolor_bright_dawn_f = m_sky_params.sky_color.dawn_horizon;
	video::SColorf bgcolor_bright_night_f = m_sky_params.sky_color.night_horizon;

	video::SColorf skycolor_bright_normal_f = m_sky_params.sky_color.day_sky;
	video::SColorf skycolor_bright_dawn_f = m_sky_params.sky_color.dawn_sky;
	video::SColorf skycolor_bright_night_f = m_sky_params.sky_color.night_sky;

	video::SColorf cloudcolor_bright_normal_f = m_cloudcolor_day_f;
	video::SColorf cloudcolor_bright_dawn_f = m_cloudcolor_dawn_f;

	float cloud_color_change_fraction = 0.95;
	if (sunlight_seen) {
		if (std::fabs(time_brightness - m_brightness) < 0.2f) {
			m_brightness = m_brightness * 0.95 + time_brightness * 0.05;
		} else {
			m_brightness = m_brightness * 0.80 + time_brightness * 0.20;
			cloud_color_change_fraction = 0.0;
		}
	} else {
		if (direct_brightness < m_brightness)
			m_brightness = m_brightness * 0.95 + direct_brightness * 0.05;
		else
			m_brightness = m_brightness * 0.98 + direct_brightness * 0.02;
	}

	m_clouds_visible = true;
	float color_change_fraction = 0.98f;
	if (sunlight_seen) {
		if (is_dawn) { // Dawn
			m_bgcolor_bright_f = m_bgcolor_bright_f.getInterpolated(
				bgcolor_bright_dawn_f, color_change_fraction);
			m_skycolor_bright_f = m_skycolor_bright_f.getInterpolated(
				skycolor_bright_dawn_f, color_change_fraction);
			m_cloudcolor_bright_f = m_cloudcolor_bright_f.getInterpolated(
				cloudcolor_bright_dawn_f, color_change_fraction);
		} else {
			if (time_brightness < 0.13f) { // Night
				m_bgcolor_bright_f = m_bgcolor_bright_f.getInterpolated(
					bgcolor_bright_night_f, color_change_fraction);
				m_skycolor_bright_f = m_skycolor_bright_f.getInterpolated(
					skycolor_bright_night_f, color_change_fraction);
			} else { // Day
				m_bgcolor_bright_f = m_bgcolor_bright_f.getInterpolated(
					bgcolor_bright_normal_f, color_change_fraction);
				m_skycolor_bright_f = m_skycolor_bright_f.getInterpolated(
					skycolor_bright_normal_f, color_change_fraction);
			}

			m_cloudcolor_bright_f = m_cloudcolor_bright_f.getInterpolated(
				cloudcolor_bright_normal_f, color_change_fraction);
		}
	} else {
		m_bgcolor_bright_f = m_bgcolor_bright_f.getInterpolated(
			bgcolor_bright_indoor_f, color_change_fraction);
		m_skycolor_bright_f = m_skycolor_bright_f.getInterpolated(
			bgcolor_bright_indoor_f, color_change_fraction);
		m_cloudcolor_bright_f = m_cloudcolor_bright_f.getInterpolated(
			cloudcolor_bright_normal_f, color_change_fraction);
		m_clouds_visible = false;
	}

	video::SColor bgcolor_bright = m_bgcolor_bright_f.toSColor();
	m_bgcolor = video::SColor(
		255,
		bgcolor_bright.getRed() * m_brightness,
		bgcolor_bright.getGreen() * m_brightness,
		bgcolor_bright.getBlue() * m_brightness
	);

	video::SColor skycolor_bright = m_skycolor_bright_f.toSColor();
	m_skycolor = video::SColor(
		255,
		skycolor_bright.getRed() * m_brightness,
		skycolor_bright.getGreen() * m_brightness,
		skycolor_bright.getBlue() * m_brightness
	);

	// Horizon coloring based on sun and moon direction during sunset and sunrise
	video::SColor pointcolor = video::SColor(m_bgcolor.getAlpha(), 255, 255, 255);
	if (m_directional_colored_fog) {
		if (m_horizon_blend() != 0) {
			// Calculate hemisphere value from yaw, (inverted in third person front view)
			s8 dir_factor = 1;
			if (cam_mode > CAMERA_MODE_THIRD)
				dir_factor = -1;
			f32 pointcolor_blend = wrapDegrees_0_360(yaw * dir_factor + 90);
			if (pointcolor_blend > 180)
				pointcolor_blend = 360 - pointcolor_blend;
			pointcolor_blend /= 180;
			// Bound view angle to determine where transition starts and ends
			pointcolor_blend = rangelim(1 - pointcolor_blend * 1.375, 0, 1 / 1.375) *
				1.375;
			// Combine the colors when looking up or down, otherwise turning looks weird
			pointcolor_blend += (0.5 - pointcolor_blend) *
				(1 - MYMIN((90 - std::fabs(pitch)) / 90 * 1.5, 1));
			// Invert direction to match where the sun and moon are rising
			if (m_time_of_day > 0.5)
				pointcolor_blend = 1 - pointcolor_blend;
			// Horizon colors of sun and moon
			f32 pointcolor_light = rangelim(m_time_brightness * 3, 0.2, 1);

			video::SColorf pointcolor_sun_f(1, 1, 1, 1);
			// Use tonemap only if default sun/moon tinting is used
			// which keeps previous behavior.
			if (m_sun_tonemap && m_default_tint) {
				pointcolor_sun_f.r = pointcolor_light *
					(float)m_materials[3].ColorParam.getRed() / 255;
				pointcolor_sun_f.b = pointcolor_light *
					(float)m_materials[3].ColorParam.getBlue() / 255;
				pointcolor_sun_f.g = pointcolor_light *
					(float)m_materials[3].ColorParam.getGreen() / 255;
			} else if (!m_default_tint) {
				pointcolor_sun_f = m_sky_params.fog_sun_tint;
			} else {
				pointcolor_sun_f.r = pointcolor_light * 1;
				pointcolor_sun_f.b = pointcolor_light *
					(0.25 + (rangelim(m_time_brightness, 0.25, 0.75) - 0.25) * 2 * 0.75);
				pointcolor_sun_f.g = pointcolor_light * (pointcolor_sun_f.b * 0.375 +
					(rangelim(m_time_brightness, 0.05, 0.15) - 0.05) * 10 * 0.625);
			}

			video::SColorf pointcolor_moon_f;
			if (m_default_tint) {
				pointcolor_moon_f = video::SColorf(
					0.5 * pointcolor_light,
					0.6 * pointcolor_light,
					0.8 * pointcolor_light,
					1
				);
			} else {
				pointcolor_moon_f = video::SColorf(
					(m_sky_params.fog_moon_tint.getRed() / 255.0f) * pointcolor_light,
					(m_sky_params.fog_moon_tint.getGreen() / 255.0f) * pointcolor_light,
					(m_sky_params.fog_moon_tint.getBlue() / 255.0f) * pointcolor_light,
					1
				);
			}
			if (m_moon_tonemap && m_default_tint) {
				pointcolor_moon_f.r = pointcolor_light *
					(float)m_materials[4].ColorParam.getRed() / 255;
				pointcolor_moon_f.b = pointcolor_light *
					(float)m_materials[4].ColorParam.getBlue() / 255;
				pointcolor_moon_f.g = pointcolor_light *
					(float)m_materials[4].ColorParam.getGreen() / 255;
			}

			video::SColor pointcolor_sun = pointcolor_sun_f.toSColor();
			video::SColor pointcolor_moon = pointcolor_moon_f.toSColor();
			// Calculate the blend color
			pointcolor = m_mix_scolor(pointcolor_moon, pointcolor_sun, pointcolor_blend);
		}
		m_bgcolor = m_mix_scolor(m_bgcolor, pointcolor, m_horizon_blend() * 0.5);
		m_skycolor = m_mix_scolor(m_skycolor, pointcolor, m_horizon_blend() * 0.25);
	}

	float cloud_direct_brightness = 0.0f;
	if (sunlight_seen) {
		if (!m_directional_colored_fog) {
			cloud_direct_brightness = time_brightness;
			// Boost cloud brightness relative to sky, at dawn, dusk and at night
			if (time_brightness < 0.7f)
				cloud_direct_brightness *= 1.3f;
		} else {
			cloud_direct_brightness = std::fmin(m_horizon_blend() * 0.15f +
				m_time_brightness, 1.0f);
			// Set the same minimum cloud brightness at night
			if (time_brightness < 0.5f)
				cloud_direct_brightness = std::fmax(cloud_direct_brightness,
					time_brightness * 1.3f);
		}
	} else {
		cloud_direct_brightness = direct_brightness;
	}

	m_cloud_brightness = m_cloud_brightness * cloud_color_change_fraction +
		cloud_direct_brightness * (1.0 - cloud_color_change_fraction);
	m_cloudcolor_f = video::SColorf(
		m_cloudcolor_bright_f.r * m_cloud_brightness,
		m_cloudcolor_bright_f.g * m_cloud_brightness,
		m_cloudcolor_bright_f.b * m_cloud_brightness,
		1.0
	);
	if (m_directional_colored_fog) {
		m_cloudcolor_f = m_mix_scolorf(m_cloudcolor_f,
			video::SColorf(pointcolor), m_horizon_blend() * 0.25);
	}
}

static v3f getSkyBodyPosition(float horizon_position, float day_position, float orbit_tilt)
{
	v3f result = v3f(0, 0, -1);
	result.rotateXZBy(horizon_position);
	result.rotateXYBy(day_position);
	result.rotateYZBy(orbit_tilt);
	return result;
}

v3f Sky::getSunDirection()
{
	return getSkyBodyPosition(90, getWickedTimeOfDay(m_time_of_day) * 360 - 90, m_sky_params.body_orbit_tilt);
}

v3f Sky::getMoonDirection()
{
	return getSkyBodyPosition(270, getWickedTimeOfDay(m_time_of_day) * 360 - 90, m_sky_params.body_orbit_tilt);
}

void Sky::draw_sun(video::IVideoDriver *driver, const video::SColor &suncolor,
	const video::SColor &suncolor2, float wicked_time_of_day)
	/* Draw sun in the sky.
	 * driver: Video driver object used to draw
	 * suncolor: main sun color
	 * suncolor2: second sun color
	 * wicked_time_of_day: current time of day, to know where should be the sun in the sky
	 */
{
	// A magic number that contributes to the ratio 1.57 sun/moon size difference.
	constexpr float sunsize = 0.07;

	static const u16 indices[] = {0, 1, 2, 0, 2, 3};
	std::array<video::S3DVertex, 4> vertices;
	if (!m_sun_texture) {
		driver->setMaterial(m_materials[1]);
		const float sunsizes[4] = {
			(sunsize * 1.7f) * m_sun_params.scale,
			(sunsize * 1.2f) * m_sun_params.scale,
			(sunsize) * m_sun_params.scale,
			(sunsize * 0.7f) * m_sun_params.scale
		};
		video::SColor c1 = suncolor;
		video::SColor c2 = suncolor;
		c1.setAlpha(0.05 * 255);
		c2.setAlpha(0.15 * 255);
		const video::SColor colors[4] = {c1, c2, suncolor, suncolor2};
		for (int i = 0; i < 4; i++) {
			draw_sky_body(vertices, -sunsizes[i], sunsizes[i], colors[i]);
			place_sky_body(vertices, 90, wicked_time_of_day * 360 - 90);
			driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
		}
	} else {
		driver->setMaterial(m_materials[3]);
		// Another magic number that contributes to the ratio 1.57 sun/moon size
		// difference.
		float d = (sunsize * 1.7) * m_sun_params.scale;
		video::SColor c = m_sun_tonemap ? m_materials[3].ColorParam :
				video::SColor(255, 255, 255, 255);
		draw_sky_body(vertices, -d, d, c);
		place_sky_body(vertices, 90, wicked_time_of_day * 360 - 90);
		driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
	}
}


void Sky::draw_moon(video::IVideoDriver *driver, const video::SColor &mooncolor,
	const video::SColor &mooncolor2, float wicked_time_of_day)
/*
	* Draw moon in the sky.
	* driver: Video driver object used to draw
	* mooncolor: main moon color
	* mooncolor2: second moon color
	* wicked_time_of_day: current time of day, to know where should be the moon in
	* the sky
	*/
{
	// A magic number that contributes to the ratio 1.57 sun/moon size difference.
	constexpr float moonsize = 0.04;

	static const u16 indices[] = {0, 1, 2, 0, 2, 3};
	std::array<video::S3DVertex, 4> vertices;
	if (!m_moon_texture) {
		driver->setMaterial(m_materials[1]);
		const float moonsizes_1[4] = {
			(-moonsize * 1.9f) * m_moon_params.scale,
			(-moonsize * 1.3f) * m_moon_params.scale,
			(-moonsize) * m_moon_params.scale,
			(-moonsize) * m_moon_params.scale
		};
		const float moonsizes_2[4] = {
			(moonsize * 1.9f) * m_moon_params.scale,
			(moonsize * 1.3f) * m_moon_params.scale,
			(moonsize) *m_moon_params.scale,
			(moonsize * 0.6f) * m_moon_params.scale
		};
		video::SColor c1 = mooncolor;
		video::SColor c2 = mooncolor;
		c1.setAlpha(0.05 * 255);
		c2.setAlpha(0.15 * 255);
		const video::SColor colors[4] = {c1, c2, mooncolor, mooncolor2};
		for (int i = 0; i < 4; i++) {
			draw_sky_body(vertices, moonsizes_1[i], moonsizes_2[i], colors[i]);
			place_sky_body(vertices, -90, wicked_time_of_day * 360 - 90);
			driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
		}
	} else {
		driver->setMaterial(m_materials[4]);
		// Another magic number that contributes to the ratio 1.57 sun/moon size
		// difference.
		float d = (moonsize * 1.9) * m_moon_params.scale;
		video::SColor c = m_sun_tonemap ? m_materials[4].ColorParam :
				video::SColor(255, 255, 255, 255);
		draw_sky_body(vertices, -d, d, c);
		place_sky_body(vertices, -90, wicked_time_of_day * 360 - 90);
		driver->drawIndexedTriangleList(&vertices[0], 4, indices, 2);
	}
}

void Sky::draw_stars(video::IVideoDriver * driver, float wicked_time_of_day)
{
	// Tune values so that stars first appear just after the sun
	// disappears over the horizon, and disappear just before the sun
	// appears over the horizon.
	// Also tune so that stars are at full brightness from time 20000
	// to time 4000.

	float tod = wicked_time_of_day < 0.5f ? wicked_time_of_day : (1.0f - wicked_time_of_day);
	float day_opacity = clamp(m_star_params.day_opacity, 0.0f, 1.0f);
	float starbrightness = (0.25f - std::abs(tod)) * 20.0f;
	float alpha = clamp(starbrightness, day_opacity, 1.0f);

	video::SColorf color(m_star_params.starcolor);
	color.a *= alpha;
	if (color.a <= 0.0f) // Stars are only drawn when not fully transparent
		return;
	m_materials[0].ColorParam = color.toSColor();

	auto day_rotation = core::matrix4().setRotationAxisRadians(2.0f * M_PI * (wicked_time_of_day - 0.25f), v3f(0.0f, 0.0f, 1.0f));
	auto orbit_rotation = core::matrix4().setRotationAxisRadians(m_sky_params.body_orbit_tilt * M_PI / 180.0, v3f(1.0f, 0.0f, 0.0f));
	auto sky_rotation = orbit_rotation * day_rotation;
	auto world_matrix = driver->getTransform(video::ETS_WORLD);
	driver->setTransform(video::ETS_WORLD, world_matrix * sky_rotation);
	driver->setMaterial(m_materials[0]);
	driver->drawMeshBuffer(m_stars.get());
	driver->setTransform(video::ETS_WORLD, world_matrix);
}

void Sky::draw_sky_body(std::array<video::S3DVertex, 4> &vertices, float pos_1, float pos_2, const video::SColor &c)
{
	/*
	* Create an array of vertices with the dimensions specified.
	* pos_1, pos_2: position of the body's vertices
	* c: color of the body
	*/

	const f32 t = 1.0f;
	const f32 o = 0.0f;
	vertices[0] = video::S3DVertex(pos_1, pos_1, -1, 0, 0, 1, c, t, t);
	vertices[1] = video::S3DVertex(pos_2, pos_1, -1, 0, 0, 1, c, o, t);
	vertices[2] = video::S3DVertex(pos_2, pos_2, -1, 0, 0, 1, c, o, o);
	vertices[3] = video::S3DVertex(pos_1, pos_2, -1, 0, 0, 1, c, t, o);
}


void Sky::place_sky_body(
	std::array<video::S3DVertex, 4> &vertices, float horizon_position, float day_position)
	/*
	* Place body in the sky.
	* vertices: The body as a rectangle of 4 vertices
	* horizon_position: turn the body around the Y axis
	* day_position: turn the body around the Z axis, to place it depending of the time of the day
	*/
{
	for (video::S3DVertex &vertex : vertices) {
		// Body is directed to -Z (south) by default
		vertex.Pos.rotateXZBy(horizon_position);
		vertex.Pos.rotateXYBy(day_position);
		vertex.Pos.rotateYZBy(m_sky_params.body_orbit_tilt);
	}
}

// FIXME: stupid helper that does a pointless texture upload/download
static void getTextureAsImage(video::IImage *&dst, const std::string &name, ITextureSource *tsrc)
{
	if (dst) {
		dst->drop();
		dst = nullptr;
	}
	if (tsrc->isKnownSourceImage(name)) {
		infostream << "Sky: loading image " << name << std::endl;
		auto *texture = tsrc->getTexture(name);
		assert(texture);
		auto *driver = RenderingEngine::get_video_driver();
		dst = driver->createImageFromData(
			texture->getColorFormat(), texture->getSize(),
			texture->lock(video::ETLM_READ_ONLY));
		texture->unlock();
	}
}

void Sky::setSunTexture(const std::string &sun_texture,
		const std::string &sun_tonemap, ITextureSource *tsrc)
{
	// Ignore matching textures (with modifiers) entirely,
	// but lets at least update the tonemap before hand.
	if (m_sun_params.tonemap != sun_tonemap || m_first_update) {
		m_sun_params.tonemap = sun_tonemap;
		getTextureAsImage(m_sun_tonemap, sun_tonemap, tsrc);
	}

	if (m_sun_params.texture == sun_texture && !m_first_update)
		return;
	m_sun_params.texture = sun_texture;

	m_sun_texture = nullptr;
	if (sun_texture == "sun.png") {
		// Dumb compatibility fix: sun.png transparently falls back to no texture
		m_sun_texture = tsrc->isKnownSourceImage(sun_texture) ?
			tsrc->getTexture(sun_texture) : nullptr;
	} else if (!sun_texture.empty()) {
		m_sun_texture = tsrc->getTextureForMesh(sun_texture);
	}

	if (m_sun_texture) {
		m_materials[3] = baseMaterial();
		m_materials[3].setTexture(0, m_sun_texture);
		m_materials[3].MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		disableTextureFiltering(m_materials[3]);
	}
}

void Sky::setSunriseTexture(const std::string &sunglow_texture,
		ITextureSource* tsrc)
{
	// Ignore matching textures (with modifiers) entirely.
	if (m_sun_params.sunrise == sunglow_texture && !m_first_update)
		return;
	m_sun_params.sunrise = sunglow_texture;
	m_materials[2].setTexture(0, tsrc->getTextureForMesh(
		sunglow_texture.empty() ? "sunrisebg.png" : sunglow_texture)
	);
}

void Sky::setMoonTexture(const std::string &moon_texture,
	const std::string &moon_tonemap, ITextureSource *tsrc)
{
	// Ignore matching textures (with modifiers) entirely,
	// but lets at least update the tonemap before hand.
	if (m_moon_params.tonemap != moon_tonemap || m_first_update) {
		m_moon_params.tonemap = moon_tonemap;
		getTextureAsImage(m_moon_tonemap, moon_tonemap, tsrc);
	}

	if (m_moon_params.texture == moon_texture && !m_first_update)
		return;
	m_moon_params.texture = moon_texture;

	m_moon_texture = nullptr;
	if (moon_texture == "moon.png") {
		// Dumb compatibility fix: moon.png transparently falls back to no texture
		m_moon_texture = tsrc->isKnownSourceImage(moon_texture) ?
			tsrc->getTexture(moon_texture) : nullptr;
	} else if (!moon_texture.empty()) {
		m_moon_texture = tsrc->getTextureForMesh(moon_texture);
	}

	if (m_moon_texture) {
		m_materials[4] = baseMaterial();
		m_materials[4].setTexture(0, m_moon_texture);
		m_materials[4].MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		disableTextureFiltering(m_materials[4]);
	}
}

void Sky::setStarCount(u16 star_count)
{
	// Allow force updating star count at game init.
	if (m_star_params.count != star_count || m_first_update) {
		m_star_params.count = star_count;
		updateStars();
	}
}

void Sky::updateStars()
{
	m_stars.reset(new scene::SMeshBuffer());
	// Stupid IrrLicht doesn’t allow non-indexed rendering, and indexed quad
	// rendering is slow due to lack of hardware support. So as indices are
	// 16-bit and there are 4 vertices per star... the limit is 2^16/4 = 0x4000.
	// That should be well enough actually.
	if (m_star_params.count > 0x4000) {
		warningstream << "Requested " << m_star_params.count << " stars but " << 0x4000 << " is the max\n";
		m_star_params.count = 0x4000;
	}
	auto &vertices = m_stars->Vertices->Data;
	auto &indices = m_stars->Indices->Data;
	vertices.reserve(4 * m_star_params.count);
	indices.reserve(6 * m_star_params.count);

	PcgRandom rgen(m_seed);
	float d = (0.006 / 2) * m_star_params.scale;
	for (u16 i = 0; i < m_star_params.count; i++) {
		v3f r = v3f(
			rgen.range(-10000, 10000),
			rgen.range(-10000, 10000),
			rgen.range(-10000, 10000)
		);
		core::CMatrix4<f32> a;
		a.buildRotateFromTo(v3f(0, 1, 0), r);
		v3f p = a.rotateAndScaleVect(v3f(-d, 1, -d));
		v3f p1 = a.rotateAndScaleVect(v3f(d, 1, -d));
		v3f p2 = a.rotateAndScaleVect(v3f(d, 1, d));
		v3f p3 = a.rotateAndScaleVect(v3f(-d, 1, d));
		vertices.push_back(video::S3DVertex(p, {}, {}, {}));
		vertices.push_back(video::S3DVertex(p1, {}, {}, {}));
		vertices.push_back(video::S3DVertex(p2, {}, {}, {}));
		vertices.push_back(video::S3DVertex(p3, {}, {}, {}));
	}
	for (u16 i = 0; i < m_star_params.count; i++) {
		indices.push_back(i * 4 + 0);
		indices.push_back(i * 4 + 1);
		indices.push_back(i * 4 + 2);
		indices.push_back(i * 4 + 2);
		indices.push_back(i * 4 + 3);
		indices.push_back(i * 4 + 0);
	}
	m_stars->setHardwareMappingHint(scene::EHM_STATIC);
}

void Sky::setSkyColors(const SkyColor &sky_color)
{
	m_sky_params.sky_color = sky_color;
}

void Sky::setHorizonTint(video::SColor sun_tint, video::SColor moon_tint,
	const std::string &use_sun_tint)
{
	// Change sun and moon tinting:
	m_sky_params.fog_sun_tint = sun_tint;
	m_sky_params.fog_moon_tint = moon_tint;
	// Faster than comparing strings every rendering frame
	if (use_sun_tint == "default")
		m_default_tint = true;
	else if (use_sun_tint == "custom")
		m_default_tint = false;
	else
		m_default_tint = true;
}

void Sky::addTextureToSkybox(const std::string &texture, int material_id,
		ITextureSource *tsrc)
{
	// Sanity check for more than six textures.
	if (material_id + 5 >= SKY_MATERIAL_COUNT)
		return;
	// Keep a list of texture names handy.
	m_sky_params.textures.emplace_back(texture);
	video::ITexture *result = tsrc->getTextureForMesh(texture);
	m_materials[material_id+5] = baseMaterial();
	m_materials[material_id+5].setTexture(0, result);
	m_materials[material_id+5].MaterialType = video::EMT_SOLID;
}

float getWickedTimeOfDay(float time_of_day)
{
	float nightlength = 0.415f;
	float wn = nightlength / 2;
	float wicked_time_of_day = 0;
	if (time_of_day > wn && time_of_day < 1.0f - wn)
		wicked_time_of_day = (time_of_day - wn) / (1.0f - wn * 2) * 0.5f + 0.25f;
	else if (time_of_day < 0.5f)
		wicked_time_of_day = time_of_day / wn * 0.25f;
	else
		wicked_time_of_day = 1.0f - ((1.0f - time_of_day) / wn * 0.25f);
	return wicked_time_of_day;
}
