// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 cx384

#include "node_visuals.h"

#include "mesh.h"
#include "shader.h"
#include "client.h"
#include "renderingengine.h"
#include "texturesource.h"
#include "wieldmesh.h" // createAnimationFrames
#include "tile.h"
#include <IMeshManipulator.h>
#include <SMesh.h>
#include <SkinnedMesh.h>

struct TileAttribContext {
	ITextureSource *tsrc;
	PreLoadedTextures *texture_pool;
	video::SColor base_color;
	const TextureSettings &tsettings;
};

using GetShaderCallback = std::function<u32 /* shader id */ (bool /* array_texture */)>;

/*
	Texture pool and related
*/

struct PreLoadedTexture {
	video::ITexture *texture = nullptr;
	u32 texture_id = 0;
	u16 texture_layer_idx = 0;
	bool used = false; // For debugging
};

struct PreLoadedTextures {
	std::unordered_map<std::string, PreLoadedTexture> pool;
	std::unordered_set<std::string> missed; // For debugging

	PreLoadedTexture find(const std::string &name);
	void add(const std::string &name, const PreLoadedTexture &t);

	void printStats(std::ostream &to) const;
};

PreLoadedTexture PreLoadedTextures::find(const std::string &name)
{
	auto it = pool.find(name);
	if (it == pool.end()) {
		missed.emplace(name);
		return {};
	}
	it->second.used = true;
	return it->second;
}

void PreLoadedTextures::add(const std::string &name, const PreLoadedTexture &t)
{
	assert(pool.find(name) == pool.end());
	pool[name] = t;
}

void PreLoadedTextures::printStats(std::ostream &to) const
{
	size_t unused = 0;
	for (auto &it : pool)
		unused += it.second.used ? 0 : 1;
	to << "PreLoadedTextures: " << pool.size() << "\n  wasted: " << unused
		<< " missed: " << missed.size() << std::endl;
}


static void fillTileAttribs(TileLayer *layer, TileAttribContext context,
		const TileSpec &tile, const TileDef &tiledef,
		MaterialType material_type, GetShaderCallback get_shader)
{
	auto *tsrc = context.tsrc;
	const auto &tsettings = context.tsettings;

	std::string texture_image;
	if (!tiledef.name.empty()) {
		texture_image = tiledef.name;
		if (tsrc->needFilterForMesh())
			texture_image += tsrc->FILTER_FOR_MESH;
	} else {
		// Tile is empty, nothing to do.
		return;
	}

	core::dimension2du texture_size;
	if (!texture_image.empty())
		texture_size = tsrc->getTextureDimensions(texture_image);
	if (!texture_size.Width || !texture_size.Height)
		texture_size = {1, 1}; // dummy if there's an error

	// Scale
	bool has_scale = tiledef.scale > 0;
	bool use_autoscale = tsettings.autoscale_mode == AUTOSCALE_FORCE ||
		(tsettings.autoscale_mode == AUTOSCALE_ENABLE && !has_scale);
	if (use_autoscale) {
		float base_size = tsettings.node_texture_size;
		float size = std::fmin(texture_size.Width, texture_size.Height);
		layer->scale = std::fmax(base_size, size) / base_size;
	} else if (has_scale) {
		layer->scale = tiledef.scale;
	} else {
		layer->scale = 1;
	}
	if (!tile.world_aligned)
		layer->scale = 1;

	// Material
	layer->material_type = material_type;
	layer->material_flags = 0;
	if (tiledef.backface_culling)
		layer->material_flags |= MATERIAL_FLAG_BACKFACE_CULLING;
	if (tiledef.animation.type != TAT_NONE)
		layer->material_flags |= MATERIAL_FLAG_ANIMATION;
	if (tiledef.tileable_horizontal)
		layer->material_flags |= MATERIAL_FLAG_TILEABLE_HORIZONTAL;
	if (tiledef.tileable_vertical)
		layer->material_flags |= MATERIAL_FLAG_TILEABLE_VERTICAL;

	// Color
	layer->has_color = tiledef.has_color;
	if (tiledef.has_color)
		layer->color = tiledef.color;
	else
		layer->color = context.base_color;

	// Animation
	if (layer->material_flags & MATERIAL_FLAG_ANIMATION) {
		int frame_length_ms = 0;
		std::vector<FrameSpec> frames = createAnimationFrames(
				tsrc, tiledef.name, tiledef.animation, frame_length_ms);
		if (frames.size() > 1) {
			layer->frames = new std::vector<FrameSpec>(frames);
			layer->animation_frame_count = layer->frames->size();
			layer->animation_frame_length_ms = frame_length_ms;

			// Set default texture to first frame (not used practice)
			layer->texture = (*layer->frames)[0].texture;
			layer->texture_id = (*layer->frames)[0].texture_id;
		} else {
			layer->material_flags &= ~MATERIAL_FLAG_ANIMATION;
		}
	}

	if (!(layer->material_flags & MATERIAL_FLAG_ANIMATION)) {
		// Grab texture
		auto tex = context.texture_pool->find(texture_image);
		if (!tex.texture) {
			// wasn't pre-loaded: create standard texture on the fly
			layer->texture = tsrc->getTexture(texture_image, &layer->texture_id);
		} else {
			layer->texture = tex.texture;
			layer->texture_id = tex.texture_id;
			layer->texture_layer_idx = tex.texture_layer_idx;
		}
	}

	// Decide on shader to use
	if (layer->texture) {
		layer->shader_id = get_shader(layer->texture->getType() == video::ETT_2D_ARRAY);
	}
}

static bool isWorldAligned(AlignStyle style, WorldAlignMode mode, NodeDrawType drawtype)
{
	if (style == ALIGN_STYLE_WORLD)
		return true;
	if (mode == WORLDALIGN_DISABLE)
		return false;
	if (style == ALIGN_STYLE_USER_DEFINED)
		return true;
	if (drawtype == NDT_NORMAL)
		return mode >= WORLDALIGN_FORCE;
	if (drawtype == NDT_NODEBOX)
		return mode >= WORLDALIGN_FORCE_NODEBOX;
	return false;
}

/// @return maximum number of layers in array textures we can use (0 if unsupported)
static size_t getArrayTextureMax(IShaderSource *shdsrc)
{
	auto *driver = RenderingEngine::get_video_driver();
	// needs to support creating array textures
	if (!driver->queryFeature(video::EVDF_TEXTURE_2D_ARRAY))
		return 0;
	// must support sampling from them
	if (!shdsrc->supportsSampler2DArray())
		return 0;

	u32 n = driver->getLimits().MaxArrayTextureImages;
	constexpr u32 type_max = std::numeric_limits<decltype(TileLayer::texture_layer_idx)>::max();
	n = std::min(n, type_max);
	n = std::min(n, g_settings->getU32("array_texture_max"));
	return n;
}


//// NodeVisuals

NodeVisuals::~NodeVisuals()
{
	for (u16 j = 0; j < 6; j++) {
		delete tiles[j].layers[0].frames;
		delete tiles[j].layers[1].frames;
	}
	for (u16 j = 0; j < CF_SPECIAL_COUNT; j++) {
		delete special_tiles[j].layers[0].frames;
		delete special_tiles[j].layers[1].frames;
	}
	if (mesh_ptr)
		mesh_ptr->drop();
}

void NodeVisuals::preUpdateTextures(ITextureSource *tsrc,
		std::unordered_set<std::string> &pool, const TextureSettings &tsettings)
{
	// Find out the exact texture strings this node might use, and put them into the pool
	// (this should match updateTextures, but it's not the end of the world if
	// a mismatch occurs)
	std::string append;
	if (tsrc->needFilterForMesh())
		append = ITextureSource::FILTER_FOR_MESH;
	std::string append_overlay = append, append_special = append;
	bool use = true, use_overlay = true, use_special = true;

	if (f->drawtype == NDT_ALLFACES_OPTIONAL) {
		use_special = (tsettings.leaves_style == LEAVES_SIMPLE);
		use = !use_special;
		if (tsettings.leaves_style == LEAVES_OPAQUE)
			append.insert(0, "^[noalpha");
	}

	const auto &consider_tile = [&] (const TileDef &def, const std::string &append) {
		// Animations are chopped into frames later, so we won't actually need
		// the source texture
		if (!def.name.empty() && def.animation.type == TAT_NONE) {
			pool.insert(def.name + append);
		}
	};

	for (u32 j = 0; j < 6; j++) {
		if (use)
			consider_tile(f->tiledef[j], append);
	}
	for (u32 j = 0; j < 6; j++) {
		if (use_overlay)
			consider_tile(f->tiledef_overlay[j], append_overlay);
	}
	for (u32 j = 0; j < CF_SPECIAL_COUNT; j++) {
		if (use_special)
			consider_tile(f->tiledef_special[j], append_special);
	}
}

void NodeVisuals::updateTextures(ITextureSource *tsrc, IShaderSource *shdsrc, Client *client,
		PreLoadedTextures *texture_pool, const TextureSettings &tsettings)
{
	// Things needed form ContentFeatures
	auto &alpha = f->alpha;
	auto &drawtype = f->drawtype;
	const auto &tiledef = f->tiledef;
	const auto &tiledef_overlay = f->tiledef_overlay;
	const auto &tiledef_special = f->tiledef_special;
	const auto &waving = f->waving;
	const auto &color = f->color;
	const auto &param_type_2 = f->param_type_2;
	const auto &palette_name = f->palette_name;

	// Figure out the actual tiles to use
	TileDef tdef[6];
	for (u32 j = 0; j < 6; j++) {
		tdef[j] = tiledef[j];
		if (tdef[j].name.empty()) {
			tdef[j].name = "no_texture.png";
			tdef[j].backface_culling = false;
		}
	}
	// also the overlay tiles
	TileDef tdef_overlay[6];
	for (u32 j = 0; j < 6; j++)
		tdef_overlay[j] = tiledef_overlay[j];
	// also the special tiles
	TileDef tdef_spec[6];
	for (u32 j = 0; j < CF_SPECIAL_COUNT; j++) {
		tdef_spec[j] = tiledef_special[j];
	}

	bool is_liquid = false;

	MaterialType material_type = alpha_mode_to_material_type(alpha);

	switch (drawtype) {
	default:
	case NDT_NORMAL:
		solidness = 2;
		break;
	case NDT_AIRLIKE:
		solidness = 0;
		break;
	case NDT_LIQUID:
		if (!tsettings.translucent_liquids)
			alpha = ALPHAMODE_OPAQUE;
		solidness = 1;
		is_liquid = true;
		break;
	case NDT_FLOWINGLIQUID:
		solidness = 0;
		if (!tsettings.translucent_liquids)
			alpha = ALPHAMODE_OPAQUE;
		is_liquid = true;
		break;
	case NDT_GLASSLIKE:
		solidness = 0;
		visual_solidness = 1;
		break;
	case NDT_GLASSLIKE_FRAMED:
		solidness = 0;
		visual_solidness = 1;
		break;
	case NDT_GLASSLIKE_FRAMED_OPTIONAL:
		solidness = 0;
		visual_solidness = 1;
		drawtype = tsettings.connected_glass ? NDT_GLASSLIKE_FRAMED : NDT_GLASSLIKE;
		break;
	case NDT_ALLFACES:
		solidness = 0;
		visual_solidness = 1;
		break;
	case NDT_ALLFACES_OPTIONAL:
		if (tsettings.leaves_style == LEAVES_FANCY) {
			drawtype = NDT_ALLFACES;
			solidness = 0;
			visual_solidness = 1;
		} else if (tsettings.leaves_style == LEAVES_SIMPLE) {
			for (u32 j = 0; j < 6; j++) {
				if (!tdef_spec[j].name.empty())
					tdef[j].name = tdef_spec[j].name;
			}
			drawtype = NDT_GLASSLIKE;
			solidness = 0;
			visual_solidness = 1;
		} else {
			if (waving >= 1) {
				// waving nodes must make faces so there are no gaps
				drawtype = NDT_ALLFACES;
				solidness = 0;
				visual_solidness = 1;
			} else {
				drawtype = NDT_NORMAL;
				solidness = 2;
			}
			for (TileDef &td : tdef)
				td.name += std::string("^[noalpha");
		}
		if (waving >= 1)
			material_type = TILE_MATERIAL_WAVING_LEAVES;
		break;
	case NDT_PLANTLIKE:
		solidness = 0;
		if (waving >= 1)
			material_type = TILE_MATERIAL_WAVING_PLANTS;
		break;
	case NDT_FIRELIKE:
		solidness = 0;
		break;
	case NDT_MESH:
	case NDT_NODEBOX:
		solidness = 0;
		if (waving == 1) {
			material_type = TILE_MATERIAL_WAVING_PLANTS;
		} else if (waving == 2) {
			material_type = TILE_MATERIAL_WAVING_LEAVES;
		} else if (waving == 3) {
			material_type = alpha == ALPHAMODE_OPAQUE ?
				TILE_MATERIAL_WAVING_LIQUID_OPAQUE : (alpha == ALPHAMODE_CLIP ?
				TILE_MATERIAL_WAVING_LIQUID_BASIC : TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT);
		}
		break;
	case NDT_TORCHLIKE:
	case NDT_SIGNLIKE:
	case NDT_FENCELIKE:
	case NDT_RAILLIKE:
		solidness = 0;
		break;
	case NDT_PLANTLIKE_ROOTED:
		solidness = 2;
		break;
	}

	if (is_liquid) {
		if (waving == 3) {
			material_type = alpha == ALPHAMODE_OPAQUE ?
				TILE_MATERIAL_WAVING_LIQUID_OPAQUE : (alpha == ALPHAMODE_CLIP ?
				TILE_MATERIAL_WAVING_LIQUID_BASIC : TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT);
		} else {
			material_type = alpha == ALPHAMODE_OPAQUE ? TILE_MATERIAL_LIQUID_OPAQUE :
				TILE_MATERIAL_LIQUID_TRANSPARENT;
		}
	}

	GetShaderCallback tile_shader = [&] (bool array_texture) {
		return shdsrc->getShader("nodes_shader", material_type, drawtype, array_texture);
	};

	MaterialType overlay_material = material_type_with_alpha(material_type);

	GetShaderCallback overlay_shader = [&] (bool array_texture) {
		return shdsrc->getShader("nodes_shader", overlay_material, drawtype, array_texture);
	};

	// minimap pixel color = average color of top tile
	if (tsettings.enable_minimap && drawtype != NDT_AIRLIKE && !tdef[0].name.empty())
	{
		if (!tdef_overlay[0].name.empty()) {
			// Merge overlay and base texture
			std::string combined = tdef[0].name + "^(" + tdef_overlay[0].name + ")";
			minimap_color = tsrc->getTextureAverageColor(combined);
		} else {
			minimap_color = tsrc->getTextureAverageColor(tdef[0].name);
		}
	}

	// Tiles (fill in f->tiles[])
	bool any_polygon_offset = false;
	TileAttribContext tac{tsrc, texture_pool, color, tsettings};

	for (u16 j = 0; j < 6; j++) {
		tiles[j].world_aligned = isWorldAligned(tdef[j].align_style,
				tsettings.world_aligned_mode, drawtype);
		fillTileAttribs(&tiles[j].layers[0], tac, tiles[j], tdef[j],
				material_type, tile_shader);
		if (!tdef_overlay[j].name.empty()) {
			tdef_overlay[j].backface_culling = tdef[j].backface_culling;
			fillTileAttribs(&tiles[j].layers[1], tac, tiles[j], tdef_overlay[j],
					overlay_material, overlay_shader);
		}

		tiles[j].layers[0].need_polygon_offset = !tiles[j].layers[1].empty();
		any_polygon_offset |= tiles[j].layers[0].need_polygon_offset;
	}

	if (drawtype == NDT_MESH && any_polygon_offset) {
		// Our per-tile polygon offset enablement workaround works fine for normal
		// nodes and anything else, where we know that different tiles are different
		// faces that couldn't possibly conflict with each other.
		// We can't assume this for mesh nodes, so apply it to all tiles (= materials)
		// then.
		for (u16 j = 0; j < 6; j++)
			tiles[j].layers[0].need_polygon_offset = true;
	}

	MaterialType special_material = material_type;
	if (drawtype == NDT_PLANTLIKE_ROOTED) {
		if (waving == 1)
			special_material = TILE_MATERIAL_WAVING_PLANTS;
		else if (waving == 2)
			special_material = TILE_MATERIAL_WAVING_LEAVES;
	}

	GetShaderCallback special_shader = [&] (bool array_texture) {
		return shdsrc->getShader("nodes_shader", special_material, drawtype, array_texture);
	};

	// Special tiles (fill in f->special_tiles[])
	for (u16 j = 0; j < CF_SPECIAL_COUNT; j++) {
		fillTileAttribs(&special_tiles[j].layers[0], tac,
				special_tiles[j], tdef_spec[j], special_material, special_shader);
	}

	if (param_type_2 == CPT2_COLOR ||
			param_type_2 == CPT2_COLORED_FACEDIR ||
			param_type_2 == CPT2_COLORED_4DIR ||
			param_type_2 == CPT2_COLORED_WALLMOUNTED ||
			param_type_2 == CPT2_COLORED_DEGROTATE)
		palette = tsrc->getPalette(palette_name);
}

void NodeVisuals::updateMesh(Client *client, const TextureSettings &tsettings)
{
	auto *manip = client->getSceneManager()->getMeshManipulator();
	(void)tsettings;

	const auto &mesh = f->mesh;
	if (f->drawtype != NDT_MESH || mesh.empty())
		return;

	// Note: By freshly reading, we get an unencumbered mesh.
	if (scene::IMesh *src_mesh = client->getMesh(mesh)) {
		bool apply_bs = false;
		if (auto *skinned_mesh = dynamic_cast<scene::SkinnedMesh *>(src_mesh)) {
			// Compatibility: Animated meshes, as well as static gltf meshes, are not scaled by BS.
			// See https://github.com/luanti-org/luanti/pull/16112#issuecomment-2881860329
			bool is_gltf = skinned_mesh->getSourceFormat() ==
					scene::SkinnedMesh::SourceFormat::GLTF;
			apply_bs = skinned_mesh->isStatic() && !is_gltf;
			// Nodes do not support mesh animation, so we clone the static pose.
			// This simplifies working with the mesh: We can just scale the vertices
			// as transformations have already been applied.
			mesh_ptr = cloneStaticMesh(src_mesh);
			src_mesh->drop();
		} else {
			auto *static_mesh = dynamic_cast<scene::SMesh *>(src_mesh);
			assert(static_mesh);
			mesh_ptr = static_mesh;
			// Compatibility: Apply BS scaling to static meshes (.obj). See #15811.
			apply_bs = true;
		}
		scaleMesh(mesh_ptr, v3f((apply_bs ? BS : 1.0f) * f->visual_scale));
		recalculateBoundingBox(mesh_ptr);
		if (!checkMeshNormals(mesh_ptr)) {
			// TODO this should be done consistently when the mesh is loaded
			infostream << "ContentFeatures: recalculating normals for mesh "
				<< mesh << std::endl;
			manip->recalculateNormals(mesh_ptr, true, false);
		}
	} else {
		mesh_ptr = nullptr;
	}
}

void NodeVisuals::collectMaterials(std::vector<u32> &leaves_materials)
{
	if (f->drawtype == NDT_AIRLIKE)
		return;

	for (u16 j = 0; j < 6; j++) {
		auto &l = tiles[j].layers;
		if (!l[0].empty() && l[0].material_type == TILE_MATERIAL_WAVING_LEAVES)
			leaves_materials.push_back(l[0].shader_id);
		if (!l[1].empty() && l[1].material_type == TILE_MATERIAL_WAVING_LEAVES)
			leaves_materials.push_back(l[1].shader_id);
	}
}

void NodeVisuals::getColor(u8 param2, video::SColor *color) const
{
	if (palette) {
		*color = (*palette)[param2];
		return;
	}
	*color = f->color;
}

void NodeVisuals::fillNodeVisuals(NodeDefManager *ndef, Client *client, void *progress_callback_args)
{
	infostream << "fillNodeVisuals: Updating "
			"textures in node definitions" << std::endl;
	ITextureSource *tsrc = client->tsrc();
	IShaderSource *shdsrc = client->getShaderSource();
	TextureSettings tsettings;
	tsettings.readSettings();

	tsrc->setImageCaching(true);
	const u32 size = ndef->size();

	/* collect all textures we might use */
	std::unordered_set<std::string> pool;
	ndef->applyFunction([&](ContentFeatures &f) {
		assert(!f.visuals);
		f.visuals = new NodeVisuals(&f);
		f.visuals->preUpdateTextures(tsrc, pool, tsettings);
	});

	/* texture pre-loading stage */
	const size_t arraymax = getArrayTextureMax(shdsrc);
	// Group by size
	std::unordered_map<v2u32, std::vector<std::string_view>> sizes;
	if (arraymax > 1) {
		infostream << "Using array textures with " << arraymax << " layers" << std::endl;
		size_t i = 0;
		for (auto &image : pool) {
			core::dimension2du dim = tsrc->getTextureDimensions(image);
			client->showUpdateProgressTexture(progress_callback_args,
				0.33333f * ++i / pool.size());
			if (!dim.Width || !dim.Height) // error
				continue;
			sizes[v2u32(dim)].emplace_back(image);
		}
	}

	// create array textures as far as possible
	size_t num_preloadable = 0, preload_progress = 0;
	for (auto &it : sizes) {
		if (it.second.size() < 2)
			continue;
		num_preloadable += it.second.size();
	}
	PreLoadedTextures plt;
	const auto &doBunch = [&] (const std::vector<std::string> &bunch) {
		PreLoadedTexture t;
		t.texture = tsrc->addArrayTexture(bunch, &t.texture_id);
		preload_progress += bunch.size();
		client->showUpdateProgressTexture(progress_callback_args,
			0.33333f + 0.33333f * preload_progress / num_preloadable);
		if (t.texture) {
			// Success: all of the images in this bunch can now refer to this texture
			for (size_t idx = 0; idx < bunch.size(); idx++) {
				t.texture_layer_idx = idx;
				plt.add(bunch[idx], t);
			}
		}
	};
	for (auto &it : sizes) {
		if (it.second.size() < 2)
			continue;
		std::vector<std::string> bunch;
		for (auto &image : it.second) {
			bunch.emplace_back(image);
			if (bunch.size() == arraymax) {
				doBunch(bunch);
				bunch.clear();
			}
		}
		if (!bunch.empty())
			doBunch(bunch);
	}
	// note that standard textures aren't preloaded

	/* final step */
	u32 progress = 0;
	ndef->applyFunction([&](ContentFeatures &f) {
		auto *v = f.visuals;
		v->updateTextures(tsrc, shdsrc, client, &plt, tsettings);
		v->updateMesh(client, tsettings);
		v->collectMaterials(ndef->m_leaves_materials);

		client->showUpdateProgressTexture(progress_callback_args,
				0.66666f + 0.33333f * progress / size);
		progress++;
	});

	SORT_AND_UNIQUE(ndef->m_leaves_materials);
	verbosestream << "m_leaves_materials.size() = " << ndef->m_leaves_materials.size()
		<< std::endl;

	plt.printStats(infostream);
	tsrc->setImageCaching(false);
}
