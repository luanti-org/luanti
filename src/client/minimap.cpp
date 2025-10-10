// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2015 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "minimap.h"
#include <cmath>
#include "client.h"
#include "clientmap.h"
#include "settings.h"
#include "shader.h"
#include "mapblock.h"
#include "client/renderingengine.h"
#include "client/texturesource.h"
#include "gettext.h"

////
//// MinimapUpdateThread
////

MinimapUpdateThread::~MinimapUpdateThread()
{
	for (auto &it : m_blocks_cache) {
		delete it.second;
	}

	for (auto &q : m_update_queue) {
		delete q.data;
	}
}

bool MinimapUpdateThread::pushBlockUpdate(v3s16 pos, MinimapMapblock *data)
{
	MutexAutoLock lock(m_queue_mutex);

	// Find if block is already in queue.
	// If it is, update the data and quit.
	for (QueuedMinimapUpdate &q : m_update_queue) {
		if (q.pos == pos) {
			delete q.data;
			q.data = data;
			return false;
		}
	}

	// Add the block
	QueuedMinimapUpdate q;
	q.pos  = pos;
	q.data = data;
	m_update_queue.push_back(q);

	return true;
}

bool MinimapUpdateThread::popBlockUpdate(QueuedMinimapUpdate *update)
{
	MutexAutoLock lock(m_queue_mutex);

	if (m_update_queue.empty())
		return false;

	*update = m_update_queue.front();
	m_update_queue.pop_front();

	return true;
}

void MinimapUpdateThread::enqueueBlock(v3s16 pos, MinimapMapblock *data)
{
	pushBlockUpdate(pos, data);
	deferUpdate();
}


void MinimapUpdateThread::doUpdate()
{
	QueuedMinimapUpdate update;

	while (popBlockUpdate(&update)) {
		if (update.data) {
			// Swap two values in the map using single lookup
			auto result = m_blocks_cache.insert(std::make_pair(update.pos, update.data));
			if (!result.second) {
				delete result.first->second;
				result.first->second = update.data;
			}
		} else {
			auto it = m_blocks_cache.find(update.pos);
			if (it != m_blocks_cache.end()) {
				delete it->second;
				m_blocks_cache.erase(it);
			}
		}
	}


	if (data->map_invalidated && (
				data->mode.type == MINIMAP_TYPE_RADAR ||
				data->mode.type == MINIMAP_TYPE_SURFACE)) {
		getMap(data->pos, data->mode.map_size, data->mode.scan_height);
		data->map_invalidated = false;
	}
}

void MinimapUpdateThread::getMap(v3s16 pos, s16 size, s16 height)
{
	v3s16 pos_min(pos.X - size / 2, pos.Y - height / 2, pos.Z - size / 2);
	v3s16 pos_max(pos_min.X + size - 1, pos.Y + height / 2, pos_min.Z + size - 1);
	v3s16 blockpos_min = getNodeBlockPos(pos_min);
	v3s16 blockpos_max = getNodeBlockPos(pos_max);

// clear the map
	for (int z = 0; z < size; z++)
	for (int x = 0; x < size; x++) {
		MinimapPixel &mmpixel = data->minimap_scan[x + z * size];
		mmpixel.air_count = 0;
		mmpixel.height = 0;
		mmpixel.n = MapNode(CONTENT_AIR);
	}

// draw the map
	v3s16 blockpos;
	for (blockpos.Z = blockpos_min.Z; blockpos.Z <= blockpos_max.Z; ++blockpos.Z)
	for (blockpos.Y = blockpos_min.Y; blockpos.Y <= blockpos_max.Y; ++blockpos.Y)
	for (blockpos.X = blockpos_min.X; blockpos.X <= blockpos_max.X; ++blockpos.X) {
		auto pblock = m_blocks_cache.find(blockpos);
		if (pblock == m_blocks_cache.end())
			continue;
		const MinimapMapblock &block = *pblock->second;

		v3s16 block_node_min(blockpos * MAP_BLOCKSIZE);
		v3s16 block_node_max(block_node_min + MAP_BLOCKSIZE - 1);
		// clip
		v3s16 range_min = componentwise_max(block_node_min, pos_min);
		v3s16 range_max = componentwise_min(block_node_max, pos_max);

		v3s16 pos;
		pos.Y = range_min.Y;
		for (pos.Z = range_min.Z; pos.Z <= range_max.Z; ++pos.Z)
		for (pos.X = range_min.X; pos.X <= range_max.X; ++pos.X) {
			v3s16 inblock_pos = pos - block_node_min;
			const MinimapPixel &in_pixel =
				block.data[inblock_pos.Z * MAP_BLOCKSIZE + inblock_pos.X];

			v3s16 inmap_pos = pos - pos_min;
			MinimapPixel &out_pixel =
				data->minimap_scan[inmap_pos.X + inmap_pos.Z * size];

			out_pixel.air_count += in_pixel.air_count;
			if (in_pixel.n.param0 != CONTENT_AIR) {
				out_pixel.n = in_pixel.n;
				out_pixel.height = inmap_pos.Y + in_pixel.height;
			}
		}
	}
}

////
//// Mapper
////

Minimap::Minimap(Client *client)
{
	this->client    = client;
	this->driver    = RenderingEngine::get_video_driver();
	this->m_tsrc    = client->getTextureSource();
	this->m_shdrsrc = client->getShaderSource();
	this->m_ndef    = client->getNodeDefManager();

	m_angle = 0.f;
	m_current_mode_index = 0;

	// Initialize static settings
	m_surface_mode_scan_height =
		g_settings->getBool("minimap_double_scan_height") ? 256 : 128;

	// Initialize minimap modes
	addMode(MINIMAP_TYPE_OFF);
	addMode(MINIMAP_TYPE_SURFACE, 256);
	addMode(MINIMAP_TYPE_SURFACE, 128);
	addMode(MINIMAP_TYPE_SURFACE, 64);
	addMode(MINIMAP_TYPE_RADAR,   512);
	addMode(MINIMAP_TYPE_RADAR,   256);
	addMode(MINIMAP_TYPE_RADAR,   128);

	// Initialize minimap data
	data = std::make_unique<MinimapData>();
	data->map_invalidated = true;

	data->minimap_shape_round = g_settings->getBool("minimap_shape_round");

	setModeIndex(0);

	// Create mesh buffer for minimap
	m_meshbuffer = createMinimapMeshBuffer();

	// Initialize and start thread
	m_minimap_update_thread = std::make_unique<MinimapUpdateThread>();
	m_minimap_update_thread->data = data.get();
	m_minimap_update_thread->start();
}

Minimap::~Minimap()
{
	m_minimap_update_thread->stop();
	m_minimap_update_thread->wait();

	m_meshbuffer.reset();

	if (data->minimap_mask_round)
		data->minimap_mask_round->drop();
	if (data->minimap_mask_square)
		data->minimap_mask_square->drop();

	driver->removeTexture(data->texture);
	driver->removeTexture(data->heightmap_texture);

	m_markers.clear();

	data.reset();
	m_minimap_update_thread.reset();
}

void Minimap::addBlock(v3s16 pos, MinimapMapblock *data)
{
	m_minimap_update_thread->enqueueBlock(pos, data);
}

void Minimap::toggleMinimapShape()
{
	MutexAutoLock lock(m_mutex);

	data->minimap_shape_round = !data->minimap_shape_round;
	g_settings->setBool("minimap_shape_round", data->minimap_shape_round);
	m_minimap_update_thread->deferUpdate();
}

void Minimap::setMinimapShape(MinimapShape shape)
{
	MutexAutoLock lock(m_mutex);

	if (shape == MINIMAP_SHAPE_SQUARE)
		data->minimap_shape_round = false;
	else if (shape == MINIMAP_SHAPE_ROUND)
		data->minimap_shape_round = true;

	g_settings->setBool("minimap_shape_round", data->minimap_shape_round);
	m_minimap_update_thread->deferUpdate();
}

MinimapShape Minimap::getMinimapShape()
{
	if (data->minimap_shape_round) {
		return MINIMAP_SHAPE_ROUND;
	}

	return MINIMAP_SHAPE_SQUARE;
}

void Minimap::setModeIndex(size_t index)
{
	MutexAutoLock lock(m_mutex);

	if (index < m_modes.size()) {
		data->mode = m_modes[index];
		m_current_mode_index = index;
	} else {
		data->mode = {MINIMAP_TYPE_OFF, gettext("Minimap hidden"), 0, 0, "", 0};
		m_current_mode_index = 0;
	}

	data->map_invalidated = true;

	if (m_minimap_update_thread)
		m_minimap_update_thread->deferUpdate();
}

void Minimap::addMode(MinimapModeDef mode)
{
	// Check validity
	if (mode.type == MINIMAP_TYPE_TEXTURE) {
		if (mode.texture.empty())
			return;
		if (mode.scale < 1)
			mode.scale = 1;
	}

	int zoom = -1;

	// Build a default standard label
	if (mode.label.empty()) {
		switch (mode.type) {
			case MINIMAP_TYPE_OFF:
				mode.label = gettext("Minimap hidden");
				break;
			case MINIMAP_TYPE_SURFACE:
				mode.label = gettext("Minimap in surface mode, Zoom x%d");
				if (mode.map_size > 0)
					zoom = 256 / mode.map_size;
				break;
			case MINIMAP_TYPE_RADAR:
				mode.label = gettext("Minimap in radar mode, Zoom x%d");
				if (mode.map_size > 0)
					zoom = 512 / mode.map_size;
				break;
			case MINIMAP_TYPE_TEXTURE:
				mode.label = gettext("Minimap in texture mode");
				break;
			default:
				break;
		}
	}
	// else: Custom labels need mod-provided client-side translation

	if (zoom >= 0) {
		char label_buf[1024];
		porting::mt_snprintf(label_buf, sizeof(label_buf),
			mode.label.c_str(), zoom);
		mode.label = label_buf;
	}

	m_modes.push_back(mode);
}

void Minimap::addMode(MinimapType type, u16 size, const std::string &label,
		const std::string &texture, u16 scale)
{
	MinimapModeDef mode;
	mode.type = type;
	mode.label = label;
	mode.map_size = size;
	mode.texture = texture;
	mode.scale = scale;
	switch (type) {
		case MINIMAP_TYPE_SURFACE:
			mode.scan_height = m_surface_mode_scan_height;
			break;
		case MINIMAP_TYPE_RADAR:
			mode.scan_height = 32;
			break;
		default:
			mode.scan_height = 0;
	}
	addMode(mode);
}

void Minimap::nextMode()
{
	if (m_modes.empty())
		return;
	m_current_mode_index++;
	if (m_current_mode_index >= m_modes.size())
		m_current_mode_index = 0;

	setModeIndex(m_current_mode_index);
}

void Minimap::setPos(v3s16 pos)
{
	bool do_update = false;

	{
		MutexAutoLock lock(m_mutex);

		if (pos != data->old_pos) {
			data->old_pos = data->pos;
			data->pos = pos;
			do_update = true;
		}
	}

	if (do_update)
		m_minimap_update_thread->deferUpdate();
}

void Minimap::setAngle(f32 angle)
{
	m_angle = angle;
}

void Minimap::blitMinimapPixelsToImageRadar(video::IImage *map_image)
{
	video::SColor c(240, 0, 0, 0);
	for (s16 x = 0; x < data->mode.map_size; x++)
	for (s16 z = 0; z < data->mode.map_size; z++) {
		MinimapPixel *mmpixel = &data->minimap_scan[x + z * data->mode.map_size];

		if (mmpixel->air_count > 0)
			c.setGreen(core::clamp(core::round32(32 + mmpixel->air_count * 8), 0, 255));
		else
			c.setGreen(0);

		map_image->setPixel(x, data->mode.map_size - z - 1, c);
	}
}

void Minimap::blitMinimapPixelsToImageSurface(
	video::IImage *map_image, video::IImage *heightmap_image)
{
	assert(map_image && heightmap_image);
	video::SColor tilecolor;
	for (s16 x = 0; x < data->mode.map_size; x++)
	for (s16 z = 0; z < data->mode.map_size; z++) {
		MinimapPixel *mmpixel = &data->minimap_scan[x + z * data->mode.map_size];

		const ContentFeatures &f = m_ndef->get(mmpixel->n);
		const auto &tile = f.tiledef[0], &overlay = f.tiledef_overlay[0];

		// Figure out the color of the top of the node
		// Note that this is very simplified. The overlay and normal tile can be
		// colored independently and the overlay might only cover half or less.
		if (!overlay.name.empty() && overlay.has_color) {
			tilecolor = overlay.color;
		} else if (overlay.name.empty() && tile.has_color) {
			tilecolor = tile.color;
		} else {
			mmpixel->n.getColor(f, &tilecolor);
		}
		// Multiply with pre-generated "color of texture"
		tilecolor.setRed(tilecolor.getRed() * f.minimap_color.getRed() / 255);
		tilecolor.setGreen(tilecolor.getGreen() * f.minimap_color.getGreen() / 255);
		tilecolor.setBlue(tilecolor.getBlue() * f.minimap_color.getBlue() / 255);
		tilecolor.setAlpha(240);

		map_image->setPixel(x, data->mode.map_size - z - 1, tilecolor);

		u32 h = mmpixel->height;
		heightmap_image->setPixel(x,data->mode.map_size - z - 1,
			video::SColor(255, h, h, h));
	}
}

video::IImage *Minimap::getMinimapMask()
{
	if (data->minimap_shape_round) {
		if (!data->minimap_mask_round) {
			// Get round minimap textures
			data->minimap_mask_round = driver->createImage(
				m_tsrc->getTexture("minimap_mask_round.png"),
				core::position2d<s32>(0, 0),
				core::dimension2d<u32>(MINIMAP_MAX_SX, MINIMAP_MAX_SY));
		}
		return data->minimap_mask_round;
	}

	if (!data->minimap_mask_square) {
		// Get square minimap textures
		data->minimap_mask_square = driver->createImage(
			m_tsrc->getTexture("minimap_mask_square.png"),
			core::position2d<s32>(0, 0),
			core::dimension2d<u32>(MINIMAP_MAX_SX, MINIMAP_MAX_SY));
	}
	return data->minimap_mask_square;
}

video::ITexture *Minimap::getMinimapTexture()
{
	// update minimap textures when new scan is ready
	if (data->map_invalidated && data->mode.type != MINIMAP_TYPE_TEXTURE)
		return data->texture;

	// create minimap and heightmap images in memory
	core::dimension2d<u32> dim(data->mode.map_size, data->mode.map_size);
	video::IImage *map_image       = driver->createImage(video::ECF_A8R8G8B8, dim);
	video::IImage *heightmap_image = driver->createImage(video::ECF_A8R8G8B8, dim);
	video::IImage *minimap_image   = driver->createImage(video::ECF_A8R8G8B8,
		core::dimension2d<u32>(MINIMAP_MAX_SX, MINIMAP_MAX_SY));

	// Blit MinimapPixels to images
	switch(data->mode.type) {
	case MINIMAP_TYPE_OFF:
		break;
	case MINIMAP_TYPE_SURFACE:
		blitMinimapPixelsToImageSurface(map_image, heightmap_image);
		break;
	case MINIMAP_TYPE_RADAR:
		blitMinimapPixelsToImageRadar(map_image);
		break;
	case MINIMAP_TYPE_TEXTURE:
		// FIXME: this is a pointless roundtrip through the gpu
		video::ITexture* texture = m_tsrc->getTexture(data->mode.texture);
		video::IImage* image = driver->createImageFromData(
			texture->getColorFormat(), texture->getSize(),
			texture->lock(video::ETLM_READ_ONLY), true, false);

		auto dim = image->getDimension();

		map_image->fill(video::SColor(255, 0, 0, 0));
		image->copyTo(map_image,
			core::vector2d<int> {
				((data->mode.map_size - (static_cast<int>(dim.Width))) >> 1)
					- data->pos.X / data->mode.scale,
				((data->mode.map_size - (static_cast<int>(dim.Height))) >> 1)
					+ data->pos.Z / data->mode.scale
			});

		image->drop();
		texture->unlock();
	}

	map_image->copyToScaling(minimap_image);
	map_image->drop();

	video::IImage *minimap_mask = getMinimapMask();

	for (s16 y = 0; y < MINIMAP_MAX_SY; y++)
	for (s16 x = 0; x < MINIMAP_MAX_SX; x++) {
		const video::SColor &mask_col = minimap_mask->getPixel(x, y);
		if (!mask_col.getAlpha())
			minimap_image->setPixel(x, y, video::SColor(0,0,0,0));
	}

	if (data->texture)
		driver->removeTexture(data->texture);
	if (data->heightmap_texture)
		driver->removeTexture(data->heightmap_texture);

	data->texture = driver->addTexture("minimap__", minimap_image);
	data->heightmap_texture =
		driver->addTexture("minimap_heightmap__", heightmap_image);
	minimap_image->drop();
	heightmap_image->drop();

	data->map_invalidated = true;

	return data->texture;
}

v3f Minimap::getYawVec()
{
	if (data->minimap_shape_round) {
		return v3f(
			std::cos(m_angle * core::DEGTORAD),
			std::sin(m_angle * core::DEGTORAD),
			1.0);
	}

	return v3f(1.0, 0.0, 1.0);
}

irr_ptr<scene::SMeshBuffer> Minimap::createMinimapMeshBuffer()
{
	auto buf = make_irr<scene::SMeshBuffer>();
	auto &vertices = buf->Vertices->Data;
	auto &indices = buf->Indices->Data;
	vertices.resize(4);
	indices.resize(6);
	static const video::SColor c(255, 255, 255, 255);

	vertices[0] = video::S3DVertex(-1, -1, 0, 0, 0, 1, c, 0, 1);
	vertices[1] = video::S3DVertex(-1,  1, 0, 0, 0, 1, c, 0, 0);
	vertices[2] = video::S3DVertex( 1,  1, 0, 0, 0, 1, c, 1, 0);
	vertices[3] = video::S3DVertex( 1, -1, 0, 0, 0, 1, c, 1, 1);

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 2;
	indices[4] = 3;
	indices[5] = 0;

	buf->setHardwareMappingHint(scene::EHM_STATIC);
	return buf;
}

void Minimap::drawMinimap(core::rect<s32> rect)
{
	if (data->mode.type == MINIMAP_TYPE_OFF)
		return;

	// Get textures
	video::ITexture *minimap_texture = getMinimapTexture();
	if (!minimap_texture)
		return;
	if (!data->textures_initialised) {
		data->minimap_overlay_round = m_tsrc->getTexture("minimap_overlay_round.png");
		data->minimap_overlay_square = m_tsrc->getTexture("minimap_overlay_square.png");
		data->player_marker = m_tsrc->getTexture("player_marker.png");
		data->object_marker_red = m_tsrc->getTexture("object_marker_red.png");
		data->textures_initialised = true;
	}

	updateActiveMarkers();

	core::rect<s32> oldViewPort = driver->getViewPort();
	core::matrix4 oldProjMat = driver->getTransform(video::ETS_PROJECTION);
	core::matrix4 oldViewMat = driver->getTransform(video::ETS_VIEW);

	driver->setViewPort(rect);
	driver->setTransform(video::ETS_PROJECTION, core::matrix4());
	driver->setTransform(video::ETS_VIEW, core::matrix4());

	core::matrix4 matrix;
	matrix.makeIdentity();

	video::SMaterial &material = m_meshbuffer->getMaterial();
	material.forEachTexture([] (auto &tex) {
		tex.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
		tex.MagFilter = video::ETMAGF_LINEAR;
	});
	material.TextureLayers[0].Texture = minimap_texture;
	material.TextureLayers[1].Texture = data->heightmap_texture;

	if (data->mode.type == MINIMAP_TYPE_SURFACE) {
		auto sid = m_shdrsrc->getShaderRaw("minimap_shader", true);
		material.MaterialType = m_shdrsrc->getShaderInfo(sid).material;
	} else {
		material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	}

	if (data->minimap_shape_round)
		matrix.setRotationDegrees(core::vector3df(0, 0, 360 - m_angle));

	// Draw minimap
	driver->setTransform(video::ETS_WORLD, matrix);
	driver->setMaterial(material);
	driver->drawMeshBuffer(m_meshbuffer.get());

	// Draw overlay
	video::ITexture *minimap_overlay = data->minimap_shape_round ?
		data->minimap_overlay_round : data->minimap_overlay_square;
	material.TextureLayers[0].Texture = minimap_overlay;
	material.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	driver->setMaterial(material);
	driver->drawMeshBuffer(m_meshbuffer.get());

	// Draw player marker on minimap
	if (data->minimap_shape_round) {
		matrix.setRotationDegrees(core::vector3df(0, 0, 0));
	} else {
		matrix.setRotationDegrees(core::vector3df(0, 0, m_angle));
	}

	material.TextureLayers[0].Texture = data->player_marker;
	driver->setTransform(video::ETS_WORLD, matrix);
	driver->setMaterial(material);
	driver->drawMeshBuffer(m_meshbuffer.get());

	// Reset transformations
	driver->setTransform(video::ETS_VIEW, oldViewMat);
	driver->setTransform(video::ETS_PROJECTION, oldProjMat);
	driver->setViewPort(oldViewPort);

	// Draw player markers
	v2s32 s_pos(rect.UpperLeftCorner.X, rect.UpperLeftCorner.Y);
	core::dimension2di imgsize(data->object_marker_red->getOriginalSize());
	core::rect<s32> img_rect(0, 0, imgsize.Width, imgsize.Height);
	static const video::SColor col(255, 255, 255, 255);
	static const video::SColor c[4] = {col, col, col, col};
	f32 sin_angle = std::sin(m_angle * core::DEGTORAD);
	f32 cos_angle = std::cos(m_angle * core::DEGTORAD);
	s32 marker_size2 =  0.025 * (float)rect.getWidth();;
	for (auto i = m_active_markers.begin();
			i != m_active_markers.end(); ++i) {
		v2f posf = *i;
		if (data->minimap_shape_round) {
			f32 t1 = posf.X * cos_angle - posf.Y * sin_angle;
			f32 t2 = posf.X * sin_angle + posf.Y * cos_angle;
			posf.X = t1;
			posf.Y = t2;
		}
		posf.X = (posf.X + 0.5) * (float)rect.getWidth();
		posf.Y = (posf.Y + 0.5) * (float)rect.getHeight();
		core::rect<s32> dest_rect(
			s_pos.X + posf.X - marker_size2,
			s_pos.Y + posf.Y - marker_size2,
			s_pos.X + posf.X + marker_size2,
			s_pos.Y + posf.Y + marker_size2);
		driver->draw2DImage(data->object_marker_red, dest_rect,
			img_rect, &dest_rect, &c[0], true);
	}
}

MinimapMarker *Minimap::addMarker(scene::ISceneNode *parent_node)
{
	auto m = std::make_unique<MinimapMarker>(parent_node);
	auto ret = m.get();
	m_markers.push_back(std::move(m));
	return ret;
}

void Minimap::removeMarker(MinimapMarker **m)
{
	m_markers.remove_if([ptr = *m](const auto &up) { return up.get() == ptr; });
	*m = nullptr;
}

void Minimap::updateActiveMarkers()
{
	video::IImage *minimap_mask = getMinimapMask();

	m_active_markers.clear();
	v3f cam_offset = intToFloat(client->getCamera()->getOffset(), BS);
	v3s16 pos_offset = data->pos - v3s16(data->mode.map_size / 2,
			data->mode.scan_height / 2,
			data->mode.map_size / 2);

	for (auto &&marker : m_markers) {
		v3s16 pos = floatToInt(marker->parent_node->getAbsolutePosition() +
			cam_offset, BS) - pos_offset;
		if (pos.X < 0 || pos.X > data->mode.map_size ||
				pos.Y < 0 || pos.Y > data->mode.scan_height ||
				pos.Z < 0 || pos.Z > data->mode.map_size) {
			continue;
		}
		pos.X = ((float)pos.X / data->mode.map_size) * MINIMAP_MAX_SX;
		pos.Z = ((float)pos.Z / data->mode.map_size) * MINIMAP_MAX_SY;
		const video::SColor &mask_col = minimap_mask->getPixel(pos.X, pos.Z);
		if (!mask_col.getAlpha()) {
			continue;
		}

		m_active_markers.emplace_back(((float)pos.X / (float)MINIMAP_MAX_SX) - 0.5f,
			(1.0f - (float)pos.Z / (float)MINIMAP_MAX_SY) - 0.5f);
	}
}

////
//// MinimapMapblock
////

void MinimapMapblock::getMinimapNodes(VoxelManipulator *vmanip, const NodeDefManager *nodedef, const v3s16 &pos)
{
	for (s16 x = 0; x < MAP_BLOCKSIZE; x++)
	for (s16 z = 0; z < MAP_BLOCKSIZE; z++) {
		s16 air_count = 0;
		bool surface_found = false;
		MinimapPixel *mmpixel = &data[z * MAP_BLOCKSIZE + x];

		for (s16 y = MAP_BLOCKSIZE -1; y >= 0; y--) {
			v3s16 p(x, y, z);
			MapNode n = vmanip->getNodeNoEx(pos + p);
			const ContentFeatures &f = nodedef->get(n);
			if (!surface_found && f.drawtype != NDT_AIRLIKE) {
				mmpixel->height = y;
				mmpixel->n = n;
				surface_found = true;
			} else if (f.drawtype == NDT_AIRLIKE) {
				air_count++;
			}
		}

		if (!surface_found)
			mmpixel->n = MapNode(CONTENT_AIR);

		mmpixel->air_count = air_count;
	}
}
