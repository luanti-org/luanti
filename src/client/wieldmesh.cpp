// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "wieldmesh.h"
#include "settings.h"
#include "shader.h"
#include "inventory.h"
#include "client.h"
#include "itemdef.h"
#include "nodedef.h"
#include "mesh.h"
#include "content_mapblock.h"
#include "mapblock_mesh.h"
#include "client/meshgen/collector.h"
#include "client/tile.h"
#include "client/texturesource.h"
#include "log.h"
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

ItemMeshBufferInfo::ItemMeshBufferInfo(const TileLayer &layer) :
		override_color(layer.color),
		override_color_set(layer.has_color),
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

void WieldMeshSceneNode::setExtruded(const std::string &imagename,
	const std::string &overlay_name, v3f wield_scale, ITextureSource *tsrc,
	u8 num_frames)
{
	video::ITexture *texture = tsrc->getTexture(imagename);
	if (!texture) {
		changeToMesh(nullptr);
		return;
	}
	video::ITexture *overlay_texture =
		overlay_name.empty() ? NULL : tsrc->getTexture(overlay_name);

	core::dimension2d<u32> dim = texture->getSize();
	// Detect animation texture and pull off top frame instead of using entire thing
	// FIXME: this is quite unportable, we should be working with the original TileLayer if there's one
	if (num_frames > 1) {
		u32 frame_height = dim.Height / num_frames;
		dim = core::dimension2d<u32>(dim.Width, frame_height);
	}
	scene::IMesh *original = g_extrusion_mesh_cache->create(dim);
	scene::SMesh *mesh = cloneStaticMesh(original);
	original->drop();
	//set texture
	mesh->getMeshBuffer(0)->getMaterial().setTexture(0,
		tsrc->getTexture(imagename));
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
		bool f_ok = std::min(dim.Width, dim.Height) >= TEXTURE_FILTER_MIN_SIZE;
		material.forEachTexture([=] (auto &tex) {
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
			if (p.layer.material_flags & MATERIAL_FLAG_ANIMATION) {
				const FrameSpec &frame = (*p.layer.frames)[0];
				p.layer.texture = frame.texture;
			}
			for (video::S3DVertex &v : p.vertices)
				v.Color.setAlpha(255);

			auto buf = make_irr<scene::SMeshBuffer>();
			buf->append(&p.vertices[0], p.vertices.size(),
					&p.indices[0], p.indices.size());

			// note: material type is left unset, overriden later
			p.layer.applyMaterialOptions(buf->Material, layer);

			mesh->addMeshBuffer(buf.get());
			buffer_info->emplace_back(p.layer);
		}
	}
	mesh->recalculateBoundingBox();
	return mesh;
}

void WieldMeshSceneNode::setItem(const ItemStack &item, Client *client, bool check_wield_image)
{
	ITextureSource *tsrc = client->getTextureSource();
	IItemDefManager *idef = client->getItemDefManager();
	ItemVisualsManager *item_visuals = client->getItemVisualsManager();
	IShaderSource *shdrsrc = client->getShaderSource();
	const NodeDefManager *ndef = client->getNodeDefManager();
	const ItemDefinition &def = item.getDefinition(idef);
	const ContentFeatures &f = ndef->get(def.name);
	content_t id = ndef->getId(def.name);

	scene::SMesh *mesh = nullptr;

	u32 shader_id = shdrsrc->getShader("object_shader", TILE_MATERIAL_BASIC, NDT_NORMAL);
	m_material_type = shdrsrc->getShaderInfo(shader_id).material;

	// Color-related
	m_buffer_info.clear();
	m_base_color = item_visuals->getItemstackColor(item, client);

	const std::string wield_image = item.getWieldImage(idef);
	const std::string wield_overlay = item.getWieldOverlay(idef);
	const v3f wield_scale = item.getWieldScale(idef);

	// If wield_image needs to be checked and is defined, it overrides everything else
	if (!wield_image.empty() && check_wield_image) {
		setExtruded(wield_image, wield_overlay, wield_scale, tsrc, 1);
		m_buffer_info.emplace_back();
		// overlay is white, if present
		m_buffer_info.emplace_back(true, video::SColor(0xFFFFFFFF));
		// initialize the color
		setColor(video::SColor(0xFFFFFFFF));
		return;
	}

	// Handle nodes
	if (def.type == ITEM_NODE) {

		switch (f.drawtype) {
		case NDT_AIRLIKE:
			setExtruded("no_texture_airlike.png", "",
				v3f(1), tsrc, 1);
			break;
		case NDT_SIGNLIKE:
		case NDT_TORCHLIKE:
		case NDT_RAILLIKE:
		case NDT_PLANTLIKE:
		case NDT_FLOWINGLIQUID: {
			v3f wscale = wield_scale;
			if (f.drawtype == NDT_FLOWINGLIQUID)
				wscale.Z *= 0.1f;
			const TileLayer &l0 = f.tiles[0].layers[0];
			const TileLayer &l1 = f.tiles[0].layers[1];
			setExtruded(tsrc->getTextureName(l0.texture_id),
				tsrc->getTextureName(l1.texture_id),
				wscale, tsrc,
				l0.animation_frame_count);
			// Add color
			m_buffer_info.emplace_back(l0.has_color, l0.color);
			m_buffer_info.emplace_back(l1.has_color, l1.color);
			break;
		}
		case NDT_PLANTLIKE_ROOTED: {
			// use the plant tile
			const TileLayer &l0 = f.special_tiles[0].layers[0];
			setExtruded(tsrc->getTextureName(l0.texture_id),
				"", wield_scale, tsrc,
				l0.animation_frame_count);
			m_buffer_info.emplace_back(l0.has_color, l0.color);
			break;
		}
		default: {
			// Render all other drawtypes like the actual node
			MapNode n(id);
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
			// FIXME: we should take different alpha modes of the mesh into account here
			material.MaterialType = m_material_type;
			material.MaterialTypeParam = 0.5f;
			material.forEachTexture([this] (auto &tex) {
				setMaterialFilters(tex, m_bilinear_filter, m_trilinear_filter,
						m_anisotropic_filter);
			});
		}

		// initialize the color
		setColor(video::SColor(0xFFFFFFFF));
		return;
	} else {
		const std::string inventory_image = item.getInventoryImage(idef);
		if (!inventory_image.empty()) {
			const std::string inventory_overlay = item.getInventoryOverlay(idef);
			setExtruded(inventory_image, inventory_overlay, def.wield_scale, tsrc, 1);
		} else {
			setExtruded("no_texture.png", "", def.wield_scale, tsrc, 1);
		}

		m_buffer_info.emplace_back();
		// overlay is white, if present
		m_buffer_info.emplace_back(true, video::SColor(0xFFFFFFFF));

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

	const u32 mc = mesh->getMeshBufferCount();
	if (mc > m_buffer_info.size())
		m_buffer_info.resize(mc);
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

void getItemMesh(Client *client, const ItemStack &item, ItemMesh *result)
{
	ITextureSource *tsrc = client->getTextureSource();
	IItemDefManager *idef = client->getItemDefManager();
	const NodeDefManager *ndef = client->getNodeDefManager();
	const ItemDefinition &def = item.getDefinition(idef);
	const ContentFeatures &f = ndef->get(def.name);
	content_t id = ndef->getId(def.name);

	FATAL_ERROR_IF(!g_extrusion_mesh_cache, "Extrusion mesh cache is not yet initialized");

	scene::SMesh *mesh = nullptr;

	// Shading is on by default
	result->needs_shading = true;

	// If inventory_image is defined, it overrides everything else
	const std::string inventory_image = item.getInventoryImage(idef);
	const std::string inventory_overlay = item.getInventoryOverlay(idef);
	if (!inventory_image.empty()) {
		mesh = getExtrudedMesh(tsrc, inventory_image, inventory_overlay);
		result->buffer_info.emplace_back();
		// overlay is white, if present
		result->buffer_info.emplace_back(true, video::SColor(0xFFFFFFFF));
		// Items with inventory images do not need shading
		result->needs_shading = false;
	} else if (def.type == ITEM_NODE && f.drawtype == NDT_AIRLIKE) {
		// Fallback image for airlike node
		mesh = getExtrudedMesh(tsrc, "no_texture_airlike.png", inventory_overlay);
		result->needs_shading = false;
	} else if (def.type == ITEM_NODE) {
		switch (f.drawtype) {
		case NDT_PLANTLIKE: {
			const TileLayer &l0 = f.tiles[0].layers[0];
			const TileLayer &l1 = f.tiles[0].layers[1];
			mesh = getExtrudedMesh(tsrc,
				tsrc->getTextureName(l0.texture_id),
				tsrc->getTextureName(l1.texture_id));
			// Add color
			result->buffer_info.emplace_back(l0.has_color, l0.color);
			result->buffer_info.emplace_back(l1.has_color, l1.color);
			break;
		}
		case NDT_PLANTLIKE_ROOTED: {
			// Use the plant tile
			const TileLayer &l0 = f.special_tiles[0].layers[0];
			mesh = getExtrudedMesh(tsrc,
				tsrc->getTextureName(l0.texture_id), "");
			result->buffer_info.emplace_back(l0.has_color, l0.color);
			break;
		}
		default: {
			// Render all other drawtypes like the actual node
			MapNode n(id);
			if (def.place_param2)
				n.setParam2(*def.place_param2);

			mesh = createGenericNodeMesh(client, n, &result->buffer_info, f);
			scaleMesh(mesh, v3f(0.12, 0.12, 0.12));
			break;
		}
		}

		for (u32 i = 0; i < mesh->getMeshBufferCount(); ++i) {
			scene::IMeshBuffer *buf = mesh->getMeshBuffer(i);
			video::SMaterial &material = buf->getMaterial();
			// FIXME: overriding this breaks different alpha modes the mesh may have
			material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			material.MaterialTypeParam = 0.5f;
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



scene::SMesh *getExtrudedMesh(ITextureSource *tsrc,
	const std::string &imagename, const std::string &overlay_name)
{
	// check textures
	video::ITexture *texture = tsrc->getTexture(imagename);
	if (!texture) {
		return NULL;
	}
	video::ITexture *overlay_texture =
		(overlay_name.empty()) ? NULL : tsrc->getTexture(overlay_name);

	// get mesh
	core::dimension2d<u32> dim = texture->getSize();
	scene::IMesh *original = g_extrusion_mesh_cache->create(dim);
	scene::SMesh *mesh = cloneStaticMesh(original);
	original->drop();

	//set texture
	mesh->getMeshBuffer(0)->getMaterial().setTexture(0,
		tsrc->getTexture(imagename));
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
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		material.MaterialTypeParam = 0.5f;
	}
	scaleMesh(mesh, v3f(2));

	return mesh;
}
