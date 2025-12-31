// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "wieldmesh.h"
#include "settings.h"
#include "shader.h"
#include "inventory.h"
#include "client.h"
#include "itemdef.h"
#include "node_visuals.h"
#include "nodedef.h"
#include "mesh.h"
#include "content_mapblock.h"
#include "mapblock_mesh.h"
#include "client/meshgen/collector.h"
#include "client/tile.h"
#include "client/texturesource.h"
#include "util/numeric.h"
#include <map>
#include <IMeshManipulator.h>
#include "client/renderingengine.h"
#include <SMesh.h>
#include <IMeshBuffer.h>
#include <CMeshBuffer.h>
#include "item_visuals_manager.h"

#define WIELD_SCALE_FACTOR 30.0f
#define WIELD_SCALE_FACTOR_EXTRUDED 40.0f

#define MIN_EXTRUSION_MESH_RESOLUTION 16
#define MAX_EXTRUSION_MESH_RESOLUTION 512

ItemMeshBufferInfo::ItemMeshBufferInfo(int layer_num, const TileLayer &layer) :
		override_color(layer.color),
		override_color_set(layer.has_color),
		layer(layer_num),
		animation_info((layer.material_flags & MATERIAL_FLAG_ANIMATION) ?
			std::make_unique<AnimationInfo>(layer) :
			nullptr)
{}

static scene::IMesh *createExtrusionMesh(int resolution_x, int resolution_y)
{
	const f32 r = 0.5;

	scene::IMeshBuffer *buf = new scene::SMeshBuffer();
	video::SColor c(255,255,255,255);
	v3f scale(1.0, 1.0, 0.1);

	// Front and back
	{
		video::S3DVertex vertices[8] = {
			// z-
			video::S3DVertex(-r,+r,-r, 0,0,-1, c, 0,0),
			video::S3DVertex(+r,+r,-r, 0,0,-1, c, 1,0),
			video::S3DVertex(+r,-r,-r, 0,0,-1, c, 1,1),
			video::S3DVertex(-r,-r,-r, 0,0,-1, c, 0,1),
			// z+
			video::S3DVertex(-r,+r,+r, 0,0,+1, c, 0,0),
			video::S3DVertex(-r,-r,+r, 0,0,+1, c, 0,1),
			video::S3DVertex(+r,-r,+r, 0,0,+1, c, 1,1),
			video::S3DVertex(+r,+r,+r, 0,0,+1, c, 1,0),
		};
		u16 indices[12] = {0,1,2,2,3,0,4,5,6,6,7,4};
		buf->append(vertices, 8, indices, 12);
	}

	f32 pixelsize_x = 1 / (f32) resolution_x;
	f32 pixelsize_y = 1 / (f32) resolution_y;

	for (int i = 0; i < resolution_x; ++i) {
		f32 pixelpos_x = i * pixelsize_x - 0.5;
		f32 x0 = pixelpos_x;
		f32 x1 = pixelpos_x + pixelsize_x;
		f32 tex0 = (i + 0.1) * pixelsize_x;
		f32 tex1 = (i + 0.9) * pixelsize_x;
		video::S3DVertex vertices[8] = {
			// x-
			video::S3DVertex(x0,-r,-r, -1,0,0, c, tex0,1),
			video::S3DVertex(x0,-r,+r, -1,0,0, c, tex1,1),
			video::S3DVertex(x0,+r,+r, -1,0,0, c, tex1,0),
			video::S3DVertex(x0,+r,-r, -1,0,0, c, tex0,0),
			// x+
			video::S3DVertex(x1,-r,-r, +1,0,0, c, tex0,1),
			video::S3DVertex(x1,+r,-r, +1,0,0, c, tex0,0),
			video::S3DVertex(x1,+r,+r, +1,0,0, c, tex1,0),
			video::S3DVertex(x1,-r,+r, +1,0,0, c, tex1,1),
		};
		u16 indices[12] = {0,1,2,2,3,0,4,5,6,6,7,4};
		buf->append(vertices, 8, indices, 12);
	}
	for (int i = 0; i < resolution_y; ++i) {
		f32 pixelpos_y = i * pixelsize_y - 0.5;
		f32 y0 = -pixelpos_y - pixelsize_y;
		f32 y1 = -pixelpos_y;
		f32 tex0 = (i + 0.1) * pixelsize_y;
		f32 tex1 = (i + 0.9) * pixelsize_y;
		video::S3DVertex vertices[8] = {
			// y-
			video::S3DVertex(-r,y0,-r, 0,-1,0, c, 0,tex0),
			video::S3DVertex(+r,y0,-r, 0,-1,0, c, 1,tex0),
			video::S3DVertex(+r,y0,+r, 0,-1,0, c, 1,tex1),
			video::S3DVertex(-r,y0,+r, 0,-1,0, c, 0,tex1),
			// y+
			video::S3DVertex(-r,y1,-r, 0,+1,0, c, 0,tex0),
			video::S3DVertex(-r,y1,+r, 0,+1,0, c, 0,tex1),
			video::S3DVertex(+r,y1,+r, 0,+1,0, c, 1,tex1),
			video::S3DVertex(+r,y1,-r, 0,+1,0, c, 1,tex0),
		};
		u16 indices[12] = {0,1,2,2,3,0,4,5,6,6,7,4};
		buf->append(vertices, 8, indices, 12);
	}

	// Create mesh object
	scene::SMesh *mesh = new scene::SMesh();
	mesh->addMeshBuffer(buf);
	buf->drop();
	scaleMesh(mesh, scale);  // also recalculates bounding box
	return mesh;
}

static video::ITexture *extractTexture(const TileDef &def, const TileLayer &layer,
	ITextureSource *tsrc)
{
	// If animated take first frame from tile layer (so we don't have to handle
	// that manually), otherwise look up by name.
	if (!layer.empty() && (layer.material_flags & MATERIAL_FLAG_ANIMATION)) {
		auto *ret = (*layer.frames)[0].texture;
		assert(ret->getType() == video::ETT_2D);
		return ret;
	}
	return tsrc->getTextureForMesh(def.name.empty() ? "no_texture.png" : def.name);
}

void getAdHocNodeShader(video::SMaterial &mat, IShaderSource *shdsrc,
		const char *shader, AlphaMode mode, int layer)
{
	assert(shdsrc);
	MaterialType type = alpha_mode_to_material_type(mode);
	if (layer == 1)
		type = material_type_with_alpha(type);

	// Note: logic wise this duplicates what `ContentFeatures::updateTextures`
	// and related functions do.

	bool array_texture = false;
	if (mat.getTexture(0))
		array_texture = mat.getTexture(0)->getType() == video::ETT_2D_ARRAY;

	u32 shader_id = shdsrc->getShader(shader, type, NDT_NORMAL, array_texture);
	mat.MaterialType = shdsrc->getShaderInfo(shader_id).material;
}

/*
	Caches extrusion meshes so that only one of them per resolution
	is needed. Also caches one cube (for convenience).

	E.g. there is a single extrusion mesh that is used for all
	16x16 px images, another for all 256x256 px images, and so on.

	WARNING: Not thread safe. This should not be a problem since
	rendering related classes (such as WieldMeshSceneNode) will be
	used from the rendering thread only.
*/
class ExtrusionMeshCache: public IReferenceCounted
{
public:
	// Constructor
	ExtrusionMeshCache()
	{
		for (int resolution = MIN_EXTRUSION_MESH_RESOLUTION;
				resolution <= MAX_EXTRUSION_MESH_RESOLUTION;
				resolution *= 2) {
			m_extrusion_meshes[resolution] =
				createExtrusionMesh(resolution, resolution);
		}
		m_cube = createCubeMesh(v3f(1.0, 1.0, 1.0));
	}
	// Destructor
	virtual ~ExtrusionMeshCache()
	{
		for (auto &extrusion_meshe : m_extrusion_meshes) {
			extrusion_meshe.second->drop();
		}
		m_cube->drop();
	}
	// Get closest extrusion mesh for given image dimensions
	// Caller must drop the returned pointer
	scene::IMesh* create(core::dimension2d<u32> dim)
	{
		// handle non-power of two textures inefficiently without cache
		if (!is_power_of_two(dim.Width) || !is_power_of_two(dim.Height)) {
			return createExtrusionMesh(dim.Width, dim.Height);
		}

		int maxdim = MYMAX(dim.Width, dim.Height);

		auto it = m_extrusion_meshes.lower_bound(maxdim);

		if (it == m_extrusion_meshes.end()) {
			// no viable resolution found; use largest one
			it = m_extrusion_meshes.find(MAX_EXTRUSION_MESH_RESOLUTION);
			sanity_check(it != m_extrusion_meshes.end());
		}

		scene::IMesh *mesh = it->second;
		mesh->grab();
		return mesh;
	}
	// Returns a 1x1x1 cube mesh with one meshbuffer (material) per face
	// Caller must drop the returned pointer
	scene::IMesh* createCube()
	{
		m_cube->grab();
		return m_cube;
	}

private:
	std::map<int, scene::IMesh*> m_extrusion_meshes;
	scene::IMesh *m_cube;
};

static ExtrusionMeshCache *g_extrusion_mesh_cache = nullptr;


WieldMeshSceneNode::WieldMeshSceneNode(scene::ISceneManager *mgr, s32 id):
	scene::ISceneNode(mgr->getRootSceneNode(), mgr, id),
	m_material_type(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF)
{
	m_anisotropic_filter = g_settings->getBool("anisotropic_filter");
	m_bilinear_filter = g_settings->getBool("bilinear_filter");
	m_trilinear_filter = g_settings->getBool("trilinear_filter");

	// If this is the first wield mesh scene node, create a cache
	// for extrusion meshes (and a cube mesh), otherwise reuse it
	if (!g_extrusion_mesh_cache)
		g_extrusion_mesh_cache = new ExtrusionMeshCache();
	else
		g_extrusion_mesh_cache->grab();

	// This class doesn't render anything, so disable culling.
	setAutomaticCulling(scene::EAC_OFF);

	// Create the child scene node
	scene::IMesh *dummymesh = g_extrusion_mesh_cache->createCube();
	m_meshnode = SceneManager->addMeshSceneNode(dummymesh, this, -1);
	m_meshnode->setVisible(false);
	dummymesh->drop(); // m_meshnode grabbed it

	m_shadow = RenderingEngine::get_shadow_renderer();

	if (m_shadow) {
		// Add mesh to shadow caster
		m_shadow->addNodeToShadowList(m_meshnode);
	}
}

WieldMeshSceneNode::~WieldMeshSceneNode()
{
	sanity_check(g_extrusion_mesh_cache);

	// Remove node from shadow casters. m_shadow might be an invalid pointer!
	if (m_shadow)
		m_shadow->removeNodeFromShadowList(m_meshnode);

	if (g_extrusion_mesh_cache->drop())
		g_extrusion_mesh_cache = nullptr;
}

void WieldMeshSceneNode::setExtruded(const TileDef &d0, const TileLayer &l0,
		const TileDef &d1, const TileLayer &l1,
		v3f wield_scale, ITextureSource *tsrc)
{
	setExtruded(extractTexture(d0, l0, tsrc),
		extractTexture(d1, l1, tsrc), wield_scale);
	// Add color
	m_buffer_info.clear();
	m_buffer_info.emplace_back(0, l0);
	m_buffer_info.emplace_back(1, l1);
}

// This does not set m_buffer_info
void WieldMeshSceneNode::setExtruded(video::ITexture *texture,
		video::ITexture *overlay_texture, v3f wield_scale)
{
	if (!texture) {
		changeToMesh(nullptr);
		return;
	}

	// Get mesh from cache
	core::dimension2d<u32> dim = texture->getSize();
	scene::IMesh *original = g_extrusion_mesh_cache->create(dim);
	scene::SMesh *mesh = cloneStaticMesh(original);
	original->drop();

	// Set texture
	mesh->getMeshBuffer(0)->getMaterial().setTexture(0, texture);
	if (overlay_texture) {
		// duplicate the extruded mesh for the overlay
		scene::IMeshBuffer *copy = cloneMeshBuffer(mesh->getMeshBuffer(0));
		copy->getMaterial().setTexture(0, overlay_texture);
		mesh->addMeshBuffer(copy);
		copy->drop();
	}
	mesh->recalculateBoundingBox();
	changeToMesh(mesh);
	mesh->drop();

	m_meshnode->setScale(wield_scale * WIELD_SCALE_FACTOR_EXTRUDED);

	// Customize materials
	for (u32 layer = 0; layer < m_meshnode->getMaterialCount(); layer++) {
		video::SMaterial &material = m_meshnode->getMaterial(layer);
		material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
		material.MaterialType = m_material_type;
		material.MaterialTypeParam = 0.5f;
		material.BackfaceCulling = true;
		// don't filter low-res textures, makes them look blurry
		material.forEachTexture([=] (auto &tex) {
			video::ITexture *t = tex.Texture;
			if (!t)
				return;
			core::dimension2d<u32> d = t->getSize();
			bool f_ok = std::min(d.Width, d.Height) >= TEXTURE_FILTER_MIN_SIZE;
			setMaterialFilters(tex, m_bilinear_filter && f_ok,
				m_trilinear_filter && f_ok, m_anisotropic_filter);
		});
		// mipmaps cause "thin black line" artifacts
		material.UseMipMaps = false;
	}
}

static scene::SMesh *createGenericNodeMesh(Client *client, MapNode n,
	std::vector<ItemMeshBufferInfo> *buffer_info, const ContentFeatures &f)
{
	n.setParam1(0xff);
	if (n.getParam2()) {
		// keep it
	} else if (f.param_type_2 == CPT2_WALLMOUNTED ||
			f.param_type_2 == CPT2_COLORED_WALLMOUNTED) {
		if (f.drawtype == NDT_TORCHLIKE ||
				f.drawtype == NDT_SIGNLIKE ||
				f.drawtype == NDT_NODEBOX ||
				f.drawtype == NDT_MESH) {
			n.setParam2(4);
		}
	} else if (f.drawtype == NDT_SIGNLIKE || f.drawtype == NDT_TORCHLIKE) {
		n.setParam2(1);
	}

	MeshCollector collector(v3f(0), v3f());
	{
		MeshMakeData mmd(client->ndef(), 1, MeshGrid{1});
		mmd.fillSingleNode(n);
		MapblockMeshGenerator(&mmd, &collector).generate();
	}

	buffer_info->clear();
	scene::SMesh *mesh = new scene::SMesh();
	for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
		auto &prebuffers = collector.prebuffers[layer];
		for (PreMeshBuffer &p : prebuffers) {
			for (video::S3DVertex &v : p.vertices)
				v.Color.setAlpha(255);

			auto buf = make_irr<scene::SMeshBuffer>();
			buf->append(&p.vertices[0], p.vertices.size(),
					&p.indices[0], p.indices.size());

			// note: material type is left unset, overriden later
			p.layer.applyMaterialOptions(buf->Material, layer);

			mesh->addMeshBuffer(buf.get());
			buffer_info->emplace_back(layer, p.layer);
		}
	}
	mesh->recalculateBoundingBox();
	return mesh;
}

std::vector<FrameSpec> createAnimationFrames(ITextureSource *tsrc,
		const std::string &image_name, const TileAnimationParams &animation,
		int &result_frame_length_ms)
{
	result_frame_length_ms = 0;

	if (image_name.empty())
		return {};

	// Still create texture if not animated
	if (animation.type == TileAnimationType::TAT_NONE) {
		u32 id;
		video::ITexture *texture = tsrc->getTextureForMesh(image_name, &id);
		return {{id, texture}};
	}

	auto texture_size = tsrc->getTextureDimensions(image_name);
	if (!texture_size.Width || !texture_size.Height)
		return {};

	int frame_count = 1;
	animation.determineParams(texture_size, &frame_count, &result_frame_length_ms, nullptr);

	std::vector<FrameSpec> frames(frame_count);
	std::ostringstream os(std::ios::binary);
	for (int i = 0; i < frame_count; i++) {
		os.str("");
		os << image_name;
		animation.getTextureModifer(os, texture_size, i);

		u32 id;
		frames[i].texture = tsrc->getTextureForMesh(os.str(), &id);
		frames[i].texture_id = id;
	}

	return frames;
}

void WieldMeshSceneNode::setItem(const ItemStack &item, Client *client, bool check_wield_image)
{
	ITextureSource *tsrc = client->getTextureSource();
	IItemDefManager *idef = client->getItemDefManager();
	ItemVisualsManager *item_visuals = client->getItemVisualsManager();
	IShaderSource *shdsrc = client->getShaderSource();
	const NodeDefManager *ndef = client->getNodeDefManager();
	const ItemDefinition &def = item.getDefinition(idef);
	const ContentFeatures &f = ndef->get(def.name);
	const NodeVisuals &v = *(f.visuals);

	{
		// Initialize material type used by setExtruded
		u32 shader_id = shdsrc->getShader("object_shader", TILE_MATERIAL_BASIC, NDT_NORMAL);
		m_material_type = shdsrc->getShaderInfo(shader_id).material;
	}

	scene::SMesh *mesh = nullptr;

	// Color-related
	m_buffer_info.clear();
	m_base_color = item_visuals->getItemstackColor(item, client);

	const ItemImageDef wield_image = item.getWieldImage(idef);
	const ItemImageDef wield_overlay = item.getWieldOverlay(idef);
	const v3f wield_scale = item.getWieldScale(idef);

	// If wield_image needs to be checked and is defined, it overrides everything else
	if (!wield_image.name.empty() && check_wield_image) {
		video::ITexture *wield_texture;
		video::ITexture *wield_overlay_texture = nullptr;

		int frame_length_ms;
		m_wield_image_frames = createAnimationFrames(tsrc,
				wield_image.name, wield_image.animation, frame_length_ms);

		auto &l0 = m_buffer_info.emplace_back(0);
		if (m_wield_image_frames.empty()) {
			wield_texture = tsrc->getTexture(wield_image.name);
		} else {
			wield_texture = m_wield_image_frames[0].texture;
			l0.animation_info = std::make_unique<AnimationInfo>(
				&m_wield_image_frames, frame_length_ms);
		}

		// Overlay
		if (!wield_overlay.name.empty()) {
			int overlay_frame_length_ms;
			m_wield_overlay_frames = createAnimationFrames(tsrc,
					wield_overlay.name, wield_overlay.animation, overlay_frame_length_ms);

			// overlay is white, if present
			auto &l1 = m_buffer_info.emplace_back(1, true, video::SColor(0xFFFFFFFF));
			if (m_wield_overlay_frames.empty()) {
				wield_overlay_texture = tsrc->getTexture(wield_overlay.name);
			} else {
				wield_overlay_texture = m_wield_overlay_frames[0].texture;
				l1.animation_info = std::make_unique<AnimationInfo>(
					&m_wield_overlay_frames, overlay_frame_length_ms);
			}
		}

		setExtruded(wield_texture, wield_overlay_texture, wield_scale);
		// initialize the color
		setColor(video::SColor(0xFFFFFFFF));
		return;
	}

	// Handle nodes
	if (def.type == ITEM_NODE) {
		switch (f.drawtype) {
		case NDT_AIRLIKE:
			setExtruded(tsrc->getTexture("no_texture_airlike.png"), nullptr, v3f(1));
			m_buffer_info.emplace_back(0);
			setColor(video::SColor(0xFFFFFFFF));
			return;
		case NDT_SIGNLIKE:
		case NDT_TORCHLIKE:
		case NDT_RAILLIKE:
		case NDT_PLANTLIKE:
		case NDT_FLOWINGLIQUID: {
			v3f wscale = wield_scale;
			if (f.drawtype == NDT_FLOWINGLIQUID)
				wscale.Z *= 0.1f;
			setExtruded(f.tiledef[0], v.tiles[0].layers[0],
				f.tiledef_overlay[0], v.tiles[0].layers[1], wscale, tsrc);
			break;
		}
		case NDT_PLANTLIKE_ROOTED: {
			// use the plant tile
			setExtruded(f.tiledef_special[0], v.special_tiles[0].layers[0],
				TileDef(), TileLayer(), wield_scale, tsrc);
			break;
		}
		default: {
			// Render all other drawtypes like the actual node
			MapNode n(ndef->getId(def.name));
			if (def.place_param2)
				n.setParam2(*def.place_param2);

			mesh = createGenericNodeMesh(client, n, &m_buffer_info, f);
			changeToMesh(mesh);
			mesh->drop();
			m_meshnode->setScale(
				wield_scale * WIELD_SCALE_FACTOR
				/ (BS * f.visual_scale));
			break;
		}
		}

		u32 material_count = m_meshnode->getMaterialCount();
		for (u32 i = 0; i < material_count; ++i) {
			video::SMaterial &material = m_meshnode->getMaterial(i);
			// apply node's alpha mode
			getAdHocNodeShader(material, shdsrc, "object_shader", f.alpha,
				m_buffer_info[i].layer == 1);
			material.forEachTexture([this] (auto &tex) {
				setMaterialFilters(tex, m_bilinear_filter, m_trilinear_filter,
						m_anisotropic_filter);
			});
		}

		// initialize the color
		setColor(video::SColor(0xFFFFFFFF));
		return;
	} else {
		video::ITexture* inventory_texture = item_visuals->getInventoryTexture(item, client);
		if (inventory_texture) {
			video::ITexture* inventory_overlay = item_visuals->getInventoryOverlayTexture(item,
					client);
			setExtruded(inventory_texture, inventory_overlay, wield_scale);
		} else {
			setExtruded(tsrc->getTexture("no_texture.png"), nullptr, wield_scale);
		}

		m_buffer_info.emplace_back(0, item_visuals->getInventoryAnimation(item, client));
		// overlay is white, if present
		m_buffer_info.emplace_back(1, item_visuals->getInventoryOverlayAnimation(item, client),
				true, video::SColor(0xFFFFFFFF));

		// initialize the color
		setColor(video::SColor(0xFFFFFFFF));
		return;
	}

	// no wield mesh found
	changeToMesh(nullptr);
}

void WieldMeshSceneNode::setColor(video::SColor c)
{
	scene::IMesh *mesh = m_meshnode->getMesh();
	if (!mesh)
		return;

	u8 red = c.getRed();
	u8 green = c.getGreen();
	u8 blue = c.getBlue();

	u32 mc = mesh->getMeshBufferCount();
	assert(mc <= m_buffer_info.size());
	mc = std::min<u32>(mc, m_buffer_info.size());
	for (u32 j = 0; j < mc; j++) {
		video::SColor bc(m_base_color);
		m_buffer_info[j].applyOverride(bc);
		video::SColor buffercolor(255,
			bc.getRed() * red / 255,
			bc.getGreen() * green / 255,
			bc.getBlue() * blue / 255);
		scene::IMeshBuffer *buf = mesh->getMeshBuffer(j);

		if (m_buffer_info[j].needColorize(buffercolor)) {
			buf->setDirty(scene::EBT_VERTEX);
			setMeshBufferColor(buf, buffercolor);
		}
	}
}

void WieldMeshSceneNode::setLightColorAndAnimation(video::SColor color, float animation_time)
{
	if (!m_meshnode)
		return;

	for (u32 i = 0; i < m_meshnode->getMaterialCount(); ++i) {
		// Color
		video::SMaterial &material = m_meshnode->getMaterial(i);
		material.ColorParam = color;

		// Animation
		const ItemMeshBufferInfo &buf_info = m_buffer_info[i];
		if (buf_info.animation_info) {
			buf_info.animation_info->updateTexture(material, animation_time);
		}
	}
}

void WieldMeshSceneNode::render()
{
	// note: if this method is changed to actually do something,
	// you probably should implement OnRegisterSceneNode as well
}

void WieldMeshSceneNode::changeToMesh(scene::IMesh *mesh)
{
	if (!mesh) {
		scene::IMesh *dummymesh = g_extrusion_mesh_cache->createCube();
		m_meshnode->setVisible(false);
		m_meshnode->setMesh(dummymesh);
		dummymesh->drop();  // m_meshnode grabbed it
	} else {
		m_meshnode->setMesh(mesh);
		mesh->setHardwareMappingHint(scene::EHM_STATIC);
	}

	m_meshnode->setVisible(true);
}

void createItemMesh(Client *client, const ItemDefinition &def,
		const AnimationInfo &animation_normal,
		const AnimationInfo &animation_overlay,
		ItemMesh *result)
{
	ITextureSource *tsrc = client->getTextureSource();
	IShaderSource *shdsrc = client->getShaderSource();
	const NodeDefManager *ndef = client->getNodeDefManager();
	const ContentFeatures &f = ndef->get(def.name);
	const NodeVisuals &v = *(f.visuals);
	assert(result);

	FATAL_ERROR_IF(!g_extrusion_mesh_cache, "Extrusion mesh cache is not yet initialized");

	scene::SMesh *mesh = nullptr;

	// Shading is off by default
	result->needs_shading = false;

	video::ITexture *inventory_texture = animation_normal.getTexture(0.0f),
		*inventory_overlay_texture = animation_overlay.getTexture(0.0f);

	// If inventory_image is defined, it overrides everything else
	if (inventory_texture) {
		mesh = getExtrudedMesh(inventory_texture, inventory_overlay_texture);

		result->buffer_info.emplace_back(0, &animation_normal);

		// overlay is white, if present
		result->buffer_info.emplace_back(1, &animation_overlay,
				true, video::SColor(0xFFFFFFFF));
	} else if (def.type == ITEM_NODE && f.drawtype == NDT_AIRLIKE) {
		// Fallback image for airlike node
		mesh = getExtrudedMesh(tsrc->getTexture("no_texture_airlike.png"),
				inventory_overlay_texture);
		result->buffer_info.emplace_back(0);

		// overlay is white, if present
		result->buffer_info.emplace_back(1, true, video::SColor(0xFFFFFFFF));
	} else if (def.type == ITEM_NODE) {
		switch (f.drawtype) {
		case NDT_PLANTLIKE: {
			const TileLayer &l0 = v.tiles[0].layers[0];
			const TileLayer &l1 = v.tiles[0].layers[1];
			mesh = getExtrudedMesh(
				extractTexture(f.tiledef[0], l0, tsrc),
				extractTexture(f.tiledef[1], l1, tsrc));
			// Add color
			result->buffer_info.emplace_back(0, l0);
			result->buffer_info.emplace_back(1, l1);
			break;
		}
		case NDT_PLANTLIKE_ROOTED: {
			// Use the plant tile
			const TileLayer &l0 = v.special_tiles[0].layers[0];
			mesh = getExtrudedMesh(
				extractTexture(f.tiledef_special[0], l0, tsrc)
			);
			result->buffer_info.emplace_back(0, l0);
			break;
		}
		default: {
			// Render all other drawtypes like the actual node
			MapNode n(ndef->getId(def.name));
			if (def.place_param2)
				n.setParam2(*def.place_param2);

			mesh = createGenericNodeMesh(client, n, &result->buffer_info, f);
			scaleMesh(mesh, v3f(0.12f));
			result->needs_shading = true;
			break;
		}
		}
		FATAL_ERROR_IF(!mesh, ("mesh creation failed for " + def.name).c_str());

		for (u32 i = 0; i < mesh->getMeshBufferCount(); ++i) {
			scene::IMeshBuffer *buf = mesh->getMeshBuffer(i);
			video::SMaterial &material = buf->getMaterial();
			// apply node's alpha mode
			getAdHocNodeShader(material, shdsrc, "inventory_shader", f.alpha,
				result->buffer_info[i].layer == 1);
			material.forEachTexture([] (auto &tex) {
				tex.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
				tex.MagFilter = video::ETMAGF_NEAREST;
			});
		}

		rotateMeshXZby(mesh, -45);
		rotateMeshYZby(mesh, -30);
	}

	// might need to be re-colorized, this is done only when needed
	if (mesh) {
		mesh->setHardwareMappingHint(scene::EHM_DYNAMIC, scene::EBT_VERTEX);
		mesh->setHardwareMappingHint(scene::EHM_STATIC, scene::EBT_INDEX);
	}
	result->mesh = mesh;
}

scene::SMesh *getExtrudedMesh(video::ITexture *texture,
	video::ITexture *overlay_texture)
{
	if (!texture)
		return nullptr;

	// Get mesh
	core::dimension2d<u32> dim = texture->getSize();
	scene::IMesh *original = g_extrusion_mesh_cache->create(dim);
	scene::SMesh *mesh = cloneStaticMesh(original);
	original->drop();

	// Set texture
	mesh->getMeshBuffer(0)->getMaterial().setTexture(0, texture);
	if (overlay_texture) {
		scene::IMeshBuffer *copy = cloneMeshBuffer(mesh->getMeshBuffer(0));
		copy->getMaterial().setTexture(0, overlay_texture);
		mesh->addMeshBuffer(copy);
		copy->drop();
	}

	// Customize materials
	for (u32 layer = 0; layer < mesh->getMeshBufferCount(); layer++) {
		video::SMaterial &material = mesh->getMeshBuffer(layer)->getMaterial();
		material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
		material.forEachTexture([] (auto &tex) {
			tex.MinFilter = video::ETMINF_NEAREST_MIPMAP_NEAREST;
			tex.MagFilter = video::ETMAGF_NEAREST;
		});
		material.BackfaceCulling = true;
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		material.MaterialTypeParam = 0.5f;
	}
	scaleMesh(mesh, v3f(2));

	return mesh;
}
