// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2015 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "irr_ptr.h"
#include "rect.h"
#include "CMeshBuffer.h"

#include "../hud.h"
#include "mapnode.h"
#include "util/thread.h"
#include <map>
#include <string>
#include <vector>

namespace video {
	class IVideoDriver;
	class IImage;
	class ITexture;
}

namespace scene {
	class ISceneNode;
}

class Client;
class NodeDefManager;
class ITextureSource;
class IShaderSource;
class VoxelManipulator;

#define MINIMAP_MAX_SX 512
#define MINIMAP_MAX_SY 512

enum MinimapShape {
	MINIMAP_SHAPE_SQUARE,
	MINIMAP_SHAPE_ROUND,
};

struct MinimapModeDef {
	MinimapType type;
	std::string label;
	u16 scan_height;
	u16 map_size;
	std::string texture;
	u16 scale;
};

struct MinimapMarker {
	MinimapMarker(scene::ISceneNode *parent_node):
		parent_node(parent_node)
	{
	}
	scene::ISceneNode *parent_node;
};
struct MinimapPixel {
	//! The topmost node that the minimap displays.
	MapNode n;
	u16 height;
	u16 air_count;
};

struct MinimapMapblock {
	void getMinimapNodes(VoxelManipulator *vmanip, const NodeDefManager *nodedef, const v3s16 &pos);

	MinimapPixel data[MAP_BLOCKSIZE * MAP_BLOCKSIZE];
};

struct MinimapData {
	MinimapModeDef mode;
	v3s16 pos;
	v3s16 old_pos;
	MinimapPixel minimap_scan[MINIMAP_MAX_SX * MINIMAP_MAX_SY];
	bool map_invalidated;
	bool minimap_shape_round;
	video::IImage *minimap_mask_round = nullptr;
	video::IImage *minimap_mask_square = nullptr;
	video::ITexture *texture = nullptr;
	video::ITexture *heightmap_texture = nullptr;
	bool textures_initialised = false; // True if the following textures are not nullptrs.
	video::ITexture *minimap_overlay_round = nullptr;
	video::ITexture *minimap_overlay_square = nullptr;
	video::ITexture *player_marker = nullptr;
	video::ITexture *object_marker_red = nullptr;
};

struct QueuedMinimapUpdate {
	v3s16 pos;
	MinimapMapblock *data = nullptr;
};

class MinimapUpdateThread : public UpdateThread {
public:
	MinimapUpdateThread() : UpdateThread("Minimap") {}
	virtual ~MinimapUpdateThread();

	void getMap(v3s16 pos, s16 size, s16 height);
	void enqueueBlock(v3s16 pos, MinimapMapblock *data);
	bool pushBlockUpdate(v3s16 pos, MinimapMapblock *data);
	bool popBlockUpdate(QueuedMinimapUpdate *update);

	MinimapData *data = nullptr;

protected:
	virtual void doUpdate();

private:
	std::mutex m_queue_mutex;
	std::deque<QueuedMinimapUpdate> m_update_queue;
	std::map<v3s16, MinimapMapblock *> m_blocks_cache;
};

class Minimap {
public:
	Minimap(Client *client);
	~Minimap();

	void addBlock(v3s16 pos, MinimapMapblock *data);

	v3f getYawVec();

	void setPos(v3s16 pos);
	v3s16 getPos() const { return data->pos; }
	void setAngle(f32 angle);
	f32 getAngle() const { return m_angle; }
	void toggleMinimapShape();
	void setMinimapShape(MinimapShape shape);
	MinimapShape getMinimapShape();

	void clearModes() { m_modes.clear(); };
	void addMode(MinimapModeDef mode);
	void addMode(MinimapType type, u16 size = 0, const std::string &label = "",
			const std::string &texture = "", u16 scale = 1);

	void setModeIndex(size_t index);
	size_t getModeIndex() const { return m_current_mode_index; };
	size_t getMaxModeIndex() const { return m_modes.size() - 1; };
	void nextMode();

	MinimapModeDef getModeDef() const { return data->mode; }

	video::IImage *getMinimapMask();
	video::ITexture *getMinimapTexture();

	void blitMinimapPixelsToImageRadar(video::IImage *map_image);
	void blitMinimapPixelsToImageSurface(video::IImage *map_image,
		video::IImage *heightmap_image);

	irr_ptr<scene::SMeshBuffer> createMinimapMeshBuffer();

	MinimapMarker* addMarker(scene::ISceneNode *parent_node);
	void removeMarker(MinimapMarker **marker);

	void updateActiveMarkers();
	void drawMinimap(core::rect<s32> rect);

	video::IVideoDriver *driver;
	Client* client;
	std::unique_ptr<MinimapData> data;

private:
	ITextureSource *m_tsrc;
	IShaderSource *m_shdrsrc;
	const NodeDefManager *m_ndef;
	std::unique_ptr<MinimapUpdateThread> m_minimap_update_thread;
	irr_ptr<scene::SMeshBuffer> m_meshbuffer;
	std::vector<MinimapModeDef> m_modes;
	size_t m_current_mode_index;
	u16 m_surface_mode_scan_height;
	f32 m_angle;
	std::mutex m_mutex;
	std::list<std::unique_ptr<MinimapMarker>> m_markers;
	std::list<v2f> m_active_markers;
};
