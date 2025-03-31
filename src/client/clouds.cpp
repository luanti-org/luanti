// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "client/renderingengine.h"
#include "client/shader.h"
#include "clouds.h"
#include "constants.h"
#include "debug.h"
#include "irrlicht_changes/printing.h"
#include "noise.h"
#include "profiler.h"
#include "settings.h"
#include <cmath>

class Clouds;
scene::ISceneManager *g_menucloudsmgr = nullptr;
Clouds *g_menuclouds = nullptr;

// Constant for now
static constexpr const float cloud_size = BS * 64.0f;

static void cloud_3d_setting_changed(const std::string &settingname, void *data)
{
	((Clouds *)data)->readSettings();
}

Clouds::Clouds(scene::ISceneManager* mgr, IShaderSource *ssrc,
		s32 id,
		u32 seed
):
	scene::ISceneNode(mgr->getRootSceneNode(), mgr, id),
	m_seed(seed)
{
	assert(ssrc);

	m_material.BackfaceCulling = true;
	m_material.FogEnable = true;
	m_material.AntiAliasing = video::EAAM_SIMPLE;
	{
		auto sid = ssrc->getShaderRaw("cloud_shader", true);
		m_material.MaterialType = ssrc->getShaderInfo(sid).material;
	}

	m_params = SkyboxDefaults::getCloudDefaults();

	readSettings();
	g_settings->registerChangedCallback("enable_3d_clouds",
		&cloud_3d_setting_changed, this);
	g_settings->registerChangedCallback("soft_clouds",
		&cloud_3d_setting_changed, this);

	updateBox();

	// Neither EAC_BOX (the default) nor EAC_FRUSTUM_BOX will correctly cull
	// the clouds.
	// And yes, the bounding box is correct. You can check using the #if 0'd
	// code in render() and see for yourself.
	// So I give up and let's disable culling.
	setAutomaticCulling(scene::EAC_OFF);

	m_meshbuffer.reset(new scene::SMeshBuffer());
	m_meshbuffer->setHardwareMappingHint(scene::EHM_DYNAMIC);
}

Clouds::~Clouds()
{
	g_settings->deregisterAllChangedCallbacks(this);
}

void Clouds::OnRegisterSceneNode()
{
	if(IsVisible)
	{
		SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);
	}

	ISceneNode::OnRegisterSceneNode();
}

void Clouds::updateMesh()
{
	// Clouds move from Z+ towards Z-

	v2f camera_pos_2d(m_camera_pos.X, m_camera_pos.Z);
	// Position of cloud noise origin from the camera
	v2f cloud_origin_from_camera_f = m_origin - camera_pos_2d;
	// The center point of drawing in the noise
	v2f center_of_drawing_in_noise_f = -cloud_origin_from_camera_f;
	// The integer center point of drawing in the noise
	v2s16 center_of_drawing_in_noise_i(
		std::floor(center_of_drawing_in_noise_f.X / cloud_size),
		std::floor(center_of_drawing_in_noise_f.Y / cloud_size)
	);

	// Only update mesh if it has moved enough, this saves lots of GPU buffer uploads.
	constexpr float max_d = 5 * BS;

	if (!m_mesh_valid) {
		// mesh was never created or invalidated
	} else if (m_mesh_origin.getDistanceFrom(m_origin) >= max_d) {
		// clouds moved
	} else if (center_of_drawing_in_noise_i != m_last_noise_center) {
		// noise offset changed
		// I think in practice this never happens due to the camera offset
		// being smaller than the cloud size.(?)
	} else {
		return;
	}

	ScopeProfiler sp(g_profiler, "Clouds::updateMesh()", SPT_AVG);
	m_mesh_origin = m_origin;
	m_last_noise_center = center_of_drawing_in_noise_i;
	m_mesh_valid = true;

	const u32 num_faces_to_draw = is3D() ? 6 : 1;

	// The world position of the integer center point of drawing in the noise
	v2f world_center_of_drawing_in_noise_f = v2f(
		center_of_drawing_in_noise_i.X * cloud_size,
		center_of_drawing_in_noise_i.Y * cloud_size
	) + m_origin;

	// Colors with primitive shading

	video::SColorf c_top_f(1, 1, 1, 1);
	video::SColorf c_side_1_f(1, 1, 1, 1);
	video::SColorf c_side_2_f(1, 1, 1, 1);
	video::SColorf c_bottom_f(1, 1, 1, 1);
	const video::SColorf shadow = m_params.color_shadow;

	c_side_1_f.r *= shadow.r * 0.25f + 0.75f;
	c_side_1_f.g *= shadow.g * 0.25f + 0.75f;
	c_side_1_f.b *= shadow.b * 0.25f + 0.75f;
	c_side_2_f.r *= shadow.r * 0.5f + 0.5f;
	c_side_2_f.g *= shadow.g * 0.5f + 0.5f;
	c_side_2_f.b *= shadow.b * 0.5f + 0.5f;
	c_bottom_f.r *= shadow.r;
	c_bottom_f.g *= shadow.g;
	c_bottom_f.b *= shadow.b;

	video::SColor c_top = c_top_f.toSColor();
	video::SColor c_side_1 = c_side_1_f.toSColor();
	video::SColor c_side_2 = c_side_2_f.toSColor();
	video::SColor c_bottom = c_bottom_f.toSColor();

	// Read noise

	std::vector<bool> grid(m_cloud_radius_i * 2 * m_cloud_radius_i * 2);

	for(s16 zi = -m_cloud_radius_i; zi < m_cloud_radius_i; zi++) {
		u32 si = (zi + m_cloud_radius_i) * m_cloud_radius_i * 2 + m_cloud_radius_i;

		for (s16 xi = -m_cloud_radius_i; xi < m_cloud_radius_i; xi++) {
			u32 i = si + xi;

			grid[i] = gridFilled(
				xi + center_of_drawing_in_noise_i.X,
				zi + center_of_drawing_in_noise_i.Y
			);
		}
	}


	auto *mb = m_meshbuffer.get();
	auto &vertices = mb->Vertices->Data;
	auto &indices = mb->Indices->Data;
	{
		const u32 vertex_count = num_faces_to_draw * 16 * m_cloud_radius_i * m_cloud_radius_i;
		const u32 quad_count = vertex_count / 4;
		const u32 index_count = quad_count * 6;

		// reserve memory
		vertices.reserve(vertex_count);
		indices.reserve(index_count);
	}

#define GETINDEX(x, z, radius) (((z)+(radius))*(radius)*2 + (x)+(radius))
#define INAREA(x, z, radius) \
	((x) >= -(radius) && (x) < (radius) && (z) >= -(radius) && (z) < (radius))

	vertices.clear();
	for (s16 zi0= -m_cloud_radius_i; zi0 < m_cloud_radius_i; zi0++)
	for (s16 xi0= -m_cloud_radius_i; xi0 < m_cloud_radius_i; xi0++)
	{
		s16 zi = zi0;
		s16 xi = xi0;
		// Draw from back to front for proper transparency
		if(zi >= 0)
			zi = m_cloud_radius_i - zi - 1;
		if(xi >= 0)
			xi = m_cloud_radius_i - xi - 1;

		u32 i = GETINDEX(xi, zi, m_cloud_radius_i);

		if (!grid[i])
			continue;

		v2f p0 = v2f(xi,zi)*cloud_size + world_center_of_drawing_in_noise_f;

		video::S3DVertex v[4] = {
			video::S3DVertex(0,0,0, 0,0,0, c_top, 0, 1),
			video::S3DVertex(0,0,0, 0,0,0, c_top, 1, 1),
			video::S3DVertex(0,0,0, 0,0,0, c_top, 1, 0),
			video::S3DVertex(0,0,0, 0,0,0, c_top, 0, 0)
		};

		const f32 rx = cloud_size / 2.0f;
		// if clouds are flat, the top layer should be at the given height
		const f32 ry = is3D() ? m_params.thickness * BS : 0.0f;
		const f32 rz = cloud_size / 2;

		bool soft_clouds_enabled = g_settings->getBool("soft_clouds");
		for (u32 i = 0; i < num_faces_to_draw; i++)
		{
			switch (i)
			{
			case 0:	// top
				for (video::S3DVertex& vertex : v) {
					vertex.Normal.set(0, 1, 0);
				}
				v[0].Pos.set(-rx, ry,-rz);
				v[1].Pos.set(-rx, ry, rz);
				v[2].Pos.set( rx, ry, rz);
				v[3].Pos.set( rx, ry,-rz);
				break;
			case 1: // back
				if (INAREA(xi, zi - 1, m_cloud_radius_i)) {
					u32 j = GETINDEX(xi, zi - 1, m_cloud_radius_i);
					if (grid[j])
						continue;
				}
				if (soft_clouds_enabled) {
					for (video::S3DVertex& vertex : v) {
						vertex.Normal.set(0, 0, -1);
					}
					v[2].Color = c_bottom;
					v[3].Color = c_bottom;
				} else {
					for (video::S3DVertex& vertex : v) {
						vertex.Color = c_side_1;
						vertex.Normal.set(0, 0, -1);
					}
				}
				v[0].Pos.set(-rx, ry,-rz);
				v[1].Pos.set( rx, ry,-rz);
				v[2].Pos.set( rx,  0,-rz);
				v[3].Pos.set(-rx,  0,-rz);
				break;
			case 2: //right
				if (INAREA(xi + 1, zi, m_cloud_radius_i)) {
					u32 j = GETINDEX(xi + 1, zi, m_cloud_radius_i);
					if (grid[j])
						continue;
				}
				if (soft_clouds_enabled) {
					for (video::S3DVertex& vertex : v) {
						vertex.Normal.set(1, 0, 0);
					}
					v[2].Color = c_bottom;
					v[3].Color = c_bottom;
				}
				else {
					for (video::S3DVertex& vertex : v) {
						vertex.Color = c_side_2;
						vertex.Normal.set(1, 0, 0);
					}
				}
				v[0].Pos.set(rx, ry,-rz);
				v[1].Pos.set(rx, ry, rz);
				v[2].Pos.set(rx,  0, rz);
				v[3].Pos.set(rx,  0,-rz);
				break;
			case 3: // front
				if (INAREA(xi, zi + 1, m_cloud_radius_i)) {
					u32 j = GETINDEX(xi, zi + 1, m_cloud_radius_i);
					if (grid[j])
						continue;
				}
				if (soft_clouds_enabled) {
					for (video::S3DVertex& vertex : v) {
						vertex.Normal.set(0, 0, -1);
					}
					v[2].Color = c_bottom;
					v[3].Color = c_bottom;
				} else {
					for (video::S3DVertex& vertex : v) {
						vertex.Color = c_side_1;
						vertex.Normal.set(0, 0, -1);
					}
				}
				v[0].Pos.set( rx, ry, rz);
				v[1].Pos.set(-rx, ry, rz);
				v[2].Pos.set(-rx,  0, rz);
				v[3].Pos.set( rx,  0, rz);
				break;
			case 4: // left
				if (INAREA(xi - 1, zi, m_cloud_radius_i)) {
					u32 j = GETINDEX(xi - 1, zi, m_cloud_radius_i);
					if (grid[j])
						continue;
				}
				if (soft_clouds_enabled) {
					for (video::S3DVertex& vertex : v) {
						vertex.Normal.set(-1, 0, 0);
					}
					v[2].Color = c_bottom;
					v[3].Color = c_bottom;
				} else {
					for (video::S3DVertex& vertex : v) {
						vertex.Color = c_side_2;
						vertex.Normal.set(-1, 0, 0);
					}
				}
				v[0].Pos.set(-rx, ry, rz);
				v[1].Pos.set(-rx, ry,-rz);
				v[2].Pos.set(-rx,  0,-rz);
				v[3].Pos.set(-rx,  0, rz);
				break;
			case 5: // bottom
				for (video::S3DVertex& vertex : v) {
					vertex.Color = c_bottom;
					vertex.Normal.set(0, -1, 0);
				}
				v[0].Pos.set( rx,  0, rz);
				v[1].Pos.set(-rx,  0, rz);
				v[2].Pos.set(-rx,  0,-rz);
				v[3].Pos.set( rx,  0,-rz);
				break;
			}

			v3f pos(p0.X, m_params.height * BS, p0.Y);

			for (video::S3DVertex &vertex : v) {
				vertex.Pos += pos;
				vertices.push_back(vertex);
			}
		}
	}
	mb->setDirty(scene::EBT_VERTEX);

	const u32 quad_count = mb->getVertexCount() / 4;
	const u32 index_count = quad_count * 6;
	// rewrite index array as needed
	if (mb->getIndexCount() > index_count) {
		indices.resize(index_count);
		mb->setDirty(scene::EBT_INDEX);
	} else if (mb->getIndexCount() < index_count) {
		const u32 start = mb->getIndexCount() / 6;
		assert(start * 6 == mb->getIndexCount());
		for (u32 k = start; k < quad_count; k++) {
			indices.push_back(4 * k + 0);
			indices.push_back(4 * k + 1);
			indices.push_back(4 * k + 2);
			indices.push_back(4 * k + 2);
			indices.push_back(4 * k + 3);
			indices.push_back(4 * k + 0);
		}
		mb->setDirty(scene::EBT_INDEX);
	}

	tracestream << "Cloud::updateMesh(): " << mb->getVertexCount() << " vertices"
		<< std::endl;
}

void Clouds::render()
{
	if (m_params.density <= 0.0f)
		return; // no need to do anything

	video::IVideoDriver* driver = SceneManager->getVideoDriver();

	if (SceneManager->getSceneNodeRenderPass() != scene::ESNRP_TRANSPARENT)
		return;

#if 0
	{
		video::SMaterial tmp;
		tmp.Thickness = 1.f;
		driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
		driver->setMaterial(tmp);
		aabb3f tmpbox = m_box;
		tmpbox.MinEdge.X = tmpbox.MinEdge.Z = -1000 * BS;
		tmpbox.MaxEdge.X = tmpbox.MaxEdge.Z = 1000 * BS;
		driver->draw3DBox(tmpbox, video::SColor(255, 255, 0x4d, 0));
	}
#endif

	updateMesh();

	// Update position
	{
		v2f off_origin = m_origin - m_mesh_origin;
		v3f rel(off_origin.X, 0, off_origin.Y);
		rel -= intToFloat(m_camera_offset, BS);
		setPosition(rel);
		updateAbsolutePosition();
	}

	m_material.BackfaceCulling = is3D();
	m_material.ColorParam = m_color.toSColor();

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
	driver->setMaterial(m_material);

	const float cloud_full_radius = cloud_size * m_cloud_radius_i;

	// Get fog parameters for setting them back later
	video::SColor fog_color(0,0,0,0);
	video::E_FOG_TYPE fog_type = video::EFT_FOG_LINEAR;
	f32 fog_start = 0;
	f32 fog_end = 0;
	f32 fog_density = 0;
	bool fog_pixelfog = false;
	bool fog_rangefog = false;
	driver->getFog(fog_color, fog_type, fog_start, fog_end, fog_density,
			fog_pixelfog, fog_rangefog);

	// Set our own fog, unless it was already disabled
	if (fog_start < FOG_RANGE_ALL) {
		driver->setFog(fog_color, fog_type, cloud_full_radius * 0.5,
				cloud_full_radius*1.2, fog_density, fog_pixelfog, fog_rangefog);
	}

	driver->drawMeshBuffer(m_meshbuffer.get());

	// Restore fog settings
	driver->setFog(fog_color, fog_type, fog_start, fog_end, fog_density,
			fog_pixelfog, fog_rangefog);
}

void Clouds::step(float dtime)
{
	m_origin = m_origin + dtime * BS * m_params.speed;
}

void Clouds::update(const v3f &camera_p, const video::SColorf &color_diffuse)
{
	video::SColorf ambient(m_params.color_ambient);
	video::SColorf bright(m_params.color_bright);
	m_color.r = core::clamp(color_diffuse.r * bright.r, ambient.r, 1.0f);
	m_color.g = core::clamp(color_diffuse.g * bright.g, ambient.g, 1.0f);
	m_color.b = core::clamp(color_diffuse.b * bright.b, ambient.b, 1.0f);
	m_color.a = bright.a;

	// is the camera inside the cloud mesh?
	m_camera_pos = camera_p;
	m_camera_inside_cloud = false;
	if (is3D()) {
		float camera_height = camera_p.Y - BS * m_camera_offset.Y;
		if (camera_height >= m_box.MinEdge.Y &&
				camera_height <= m_box.MaxEdge.Y) {
			v2f camera_in_noise;
			camera_in_noise.X = floorf((camera_p.X - m_origin.X) / cloud_size + 0.5f);
			camera_in_noise.Y = floorf((camera_p.Z - m_origin.Y) / cloud_size + 0.5f);
			bool filled = gridFilled(camera_in_noise.X, camera_in_noise.Y);
			m_camera_inside_cloud = filled;
		}
	}
}

void Clouds::readSettings()
{
	// The code isn't designed to go over 64k vertices so the upper limits were
	// chosen to avoid exactly that.
	// refer to vertex_count in updateMesh()
	m_enable_3d = g_settings->getBool("enable_3d_clouds");
	const u16 maximum = !m_enable_3d ? 62 : 25;
	m_cloud_radius_i = rangelim(g_settings->getU16("cloud_radius"), 8, maximum);

	invalidateMesh();
}

bool Clouds::gridFilled(int x, int y) const
{
	float cloud_size_noise = cloud_size / (BS * 200.f);
	float noise = noise2d_fractal(
			(float)x * cloud_size_noise,
			(float)y * cloud_size_noise,
			m_seed, 3, 0.5);
	// normalize to 0..1 (given 3 octaves)
	static constexpr const float noise_bound = 1.0f + 0.5f + 0.25f;
	float density = noise / noise_bound * 0.5f + 0.5f;
	return (density < m_params.density);
}
