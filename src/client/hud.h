// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
// Copyright (C) 2017 red-001 <red-001@outlook.ie>

#pragma once

#include <vector>
#include <IGUIFont.h>
#include <SMaterial.h>
#include <CMeshBuffer.h>
#include "irr_ptr.h"
#include "irr_aabb3d.h"
#include "../hud.h"

class Client;
class ITextureSource;
class Inventory;
class InventoryList;
class LocalPlayer;
struct ItemStack;

namespace scene
{
	class IMesh;
}

namespace video
{
	class ITexture;
	class IVideoDriver;
}

class Hud
{
public:
	enum BlockBoundsMode
	{
		BLOCK_BOUNDS_OFF,
		BLOCK_BOUNDS_CURRENT,
		BLOCK_BOUNDS_NEAR,
	} m_block_bounds_mode = BLOCK_BOUNDS_OFF;

	video::SColor crosshair_argb;
	video::SColor selectionbox_argb;

	bool use_crosshair_image = false;
	bool use_object_crosshair_image = false;
	std::string hotbar_image = "";
	bool use_hotbar_image = false;
	std::string hotbar_selected_image = "";
	bool use_hotbar_selected_image = false;

	bool pointing_at_object = false;

	Hud(Client *client, LocalPlayer *player,
			Inventory *inventory);
	void readScalingSetting();
	~Hud();

	enum BlockBoundsMode toggleBlockBounds();
	void disableBlockBounds();
	void drawBlockBounds();

	void drawHotbar(const v2s32 &pos, const v2f &offset, u16 direction, const v2f &align);
	void resizeHotbar();
	void drawCrosshair();
	void drawSelectionMesh();
	void updateSelectionMesh(const v3s16 &camera_offset);

	std::vector<aabb3f> *getSelectionBoxes() { return &m_selection_boxes; }

	void setSelectionPos(const v3f &pos, const v3s16 &camera_offset);

	v3f getSelectionPos() const { return m_selection_pos; }

	void setSelectionRotationRadians(v3f rotation)
	{
		m_selection_rotation_radians = rotation;
	}

	v3f getSelectionRotationRadians() const
	{
		return m_selection_rotation_radians;
	}

	void setSelectionMeshColor(const video::SColor &color)
	{
		m_selection_mesh_color = color;
	}

	void setSelectedFaceNormal(const v3f &face_normal)
	{
		m_selected_face_normal = face_normal;
	}

	bool hasElementOfType(HudElementType type);

	void drawLuaElements(const v3s16 &camera_offset);

private:
	bool calculateScreenPos(const v3s16 &camera_offset, HudElement *e, v2s32 *pos);
	void drawStatbar(v2s32 pos, u16 corner, u16 drawdir,
			const std::string &texture, const std::string& bgtexture,
			s32 count, s32 maxcount, v2s32 offset, v2s32 size = v2s32());

	void drawItems(v2s32 screen_pos, v2s32 screen_offset, s32 itemcount, v2f alignment,
			s32 inv_offset, InventoryList *mainlist, u16 selectitem,
			u16 direction, bool is_hotbar);

	void drawItem(const ItemStack &item, const core::rect<s32> &rect, bool selected);

	void drawCompassTranslate(HudElement *e, video::ITexture *texture,
			const core::rect<s32> &rect, int way);

	void drawCompassRotate(HudElement *e, video::ITexture *texture,
			const core::rect<s32> &rect, int way);

	Client *client = nullptr;
	video::IVideoDriver *driver = nullptr;
	LocalPlayer *player = nullptr;
	Inventory *inventory = nullptr;
	ITextureSource *tsrc = nullptr;

	float m_hud_scaling; // cached minetest setting
	float m_scale_factor;
	v3s16 m_camera_offset;
	v2u32 m_screensize;
	v2s32 m_displaycenter;
	s32 m_hotbar_imagesize; // Takes hud_scaling into account, updated by resizeHotbar()
	s32 m_padding; // Takes hud_scaling into account, updated by resizeHotbar()
	video::SColor hbar_colors[4];

	std::vector<aabb3f> m_selection_boxes;
	std::vector<aabb3f> m_halo_boxes;
	v3f m_selection_pos;
	v3f m_selection_pos_with_offset;
	v3f m_selection_rotation_radians;

	scene::IMesh *m_selection_mesh = nullptr;
	video::SColor m_selection_mesh_color;
	v3f m_selected_face_normal;

	video::SMaterial m_selection_material;
	video::SMaterial m_block_bounds_material;

	irr_ptr<scene::SMeshBuffer> m_rotation_mesh_buffer;

	enum
	{
		HIGHLIGHT_BOX,
		HIGHLIGHT_HALO,
		HIGHLIGHT_NONE
	} m_mode;
};
