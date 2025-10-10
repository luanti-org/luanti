// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "inventory.h"
#include "util/numeric.h"
#include "client/localplayer.h"
#include <ICameraSceneNode.h>
#include <ISceneNode.h>
#include <plane3d.h>
#include <array>
#include <list>
#include <optional>

class LocalPlayer;
struct MapDrawControl;
class Client;
class RenderingEngine;
class WieldMeshSceneNode;

struct Nametag
{
	scene::ISceneNode *parent_node = nullptr;
	std::string text;
	video::SColor textcolor;
	std::optional<video::SColor> bgcolor;
	std::optional<u32> textsize;
	v3f pos; // offset from parent node
	bool scale_z;

	video::SColor getBgColor(bool use_fallback) const
	{
		if (bgcolor.has_value())
			return bgcolor.value();
		else if (!use_fallback)
			return video::SColor(0, 0, 0, 0);
		else if (textcolor.getBrightness() > 186)
			// Dark background for light text
			return video::SColor(50, 50, 50, 50);
		else
			// Light background for dark text
			return video::SColor(50, 255, 255, 255);
	}
};

/*
	Client camera class, manages the player and camera scene nodes, the viewing distance
	and performs view bobbing etc. It also displays the wielded tool in front of the
	first-person camera.
*/
class Camera
{
public:
	Camera(MapDrawControl &draw_control, Client *client, RenderingEngine *rendering_engine);
	~Camera();

	static void settingChangedCallback(const std::string &name, void *data);
	void readSettings();

	// Get camera scene node.
	// It has the eye transformation, pitch and view bobbing applied.
	inline scene::ICameraSceneNode* getCameraNode() const
	{
		return m_cameranode;
	}

	// Get the camera position (in absolute scene coordinates).
	// This has view bobbing applied.
	inline v3f getPosition() const
	{
		return m_camera_position;
	}

	// Returns the absolute position of the head SceneNode in the world
	inline v3f getHeadPosition() const
	{
		return m_headnode->getAbsolutePosition();
	}

	// Get the camera direction (in absolute camera coordinates).
	// This has view bobbing applied.
	inline v3f getDirection() const
	{
		return m_camera_direction;
	}

	// Get the camera offset
	inline v3s16 getOffset() const
	{
		return m_camera_offset;
	}

	// Horizontal field of view
	inline f32 getFovX() const
	{
		return m_fov_x;
	}

	// Vertical field of view
	inline f32 getFovY() const
	{
		return m_fov_y;
	}

	// Get maximum of getFovX() and getFovY()
	inline f32 getFovMax() const
	{
		return MYMAX(m_fov_x, m_fov_y);
	}

	// Returns a lambda that when called with an object's position and bounding-sphere
	// radius (both in BS space) returns true if, and only if the object should be
	// frustum-culled.
	auto getFrustumCuller() const
	{
		return [planes = getFrustumCullPlanes(),
				camera_offset = intToFloat(m_camera_offset, BS)
				](v3f position, f32 radius) {
			v3f pos_camspace = position - camera_offset;
			for (auto &plane : planes) {
				if (plane.getDistanceTo(pos_camspace) > radius)
					return true;
			}
			return false;
		};
	}

	// Notify about new server-sent FOV and initialize smooth FOV transition
	void notifyFovChange();

	// Step the camera: updates the viewing range and view bobbing.
	void step(f32 dtime);

	// Update the camera from the local player's position.
	void update(LocalPlayer* player, f32 frametime, f32 tool_reload_ratio);

	// Adjust the camera offset when needed
	void updateOffset();

	// Update render distance
	void updateViewingRange();

	// Start digging animation
	// Pass 0 for left click, 1 for right click
	void setDigging(s32 button);

	// Replace the wielded item mesh
	void wield(const ItemStack &item);

	// Draw the wielded tool.
	// This has to happen *after* the main scene is drawn.
	// Warning: This clears the Z buffer.
	void drawWieldedTool(core::matrix4* translation=NULL);

	// Toggle the current camera mode
	void toggleCameraMode()
	{
		if (m_camera_mode == CAMERA_MODE_FIRST)
			m_camera_mode = CAMERA_MODE_THIRD;
		else if (m_camera_mode == CAMERA_MODE_THIRD)
			m_camera_mode = CAMERA_MODE_THIRD_FRONT;
		else
			m_camera_mode = CAMERA_MODE_FIRST;
	}

	// Set the current camera mode
	inline void setCameraMode(CameraMode mode)
	{
		m_camera_mode = mode;
	}

	//read the current camera mode
	inline CameraMode getCameraMode() const
	{
		return m_camera_mode;
	}

	Nametag *addNametag(const Nametag &params);

	void removeNametag(Nametag *nametag);

	void drawNametags();

	inline void addArmInertia(f32 player_yaw);

private:
	// Use getFrustumCuller().
	// This helper just exists to decrease the header's number of includes.
	std::array<core::plane3d<f32>, 4> getFrustumCullPlanes() const;

	// Nodes
	scene::ISceneNode *m_playernode = nullptr;
	scene::ISceneNode *m_headnode = nullptr;
	scene::ICameraSceneNode *m_cameranode = nullptr;

	scene::ISceneManager *m_wieldmgr = nullptr;
	WieldMeshSceneNode *m_wieldnode = nullptr;

	// draw control
	MapDrawControl& m_draw_control;

	Client *m_client;

	// Default Client FOV (as defined by the "fov" setting)
	f32 m_cache_fov;

	// Absolute camera position
	v3f m_camera_position;
	// Absolute camera direction
	v3f m_camera_direction;
	// Camera offset
	v3s16 m_camera_offset;

	bool m_stepheight_smooth_active = false;

	// Server-sent FOV variables
	bool m_server_sent_fov = false;
	f32 m_curr_fov_degrees, m_target_fov_degrees;

	// FOV transition variables
	bool m_fov_transition_active = false;
	f32 m_fov_diff, m_transition_time;

	v2f m_wieldmesh_offset = v2f(55.0f, -35.0f);
	v2f m_arm_dir;
	v2f m_cam_vel;
	v2f m_cam_vel_old;
	v2f m_last_cam_pos;

	// Field of view and aspect ratio stuff
	f32 m_aspect = 1.0f;
	f32 m_fov_x = 1.0f;
	f32 m_fov_y = 1.0f;

	// View bobbing animation frame (0 <= m_view_bobbing_anim < 1)
	f32 m_view_bobbing_anim = 0.0f;
	// If 0, view bobbing is off (e.g. player is standing).
	// If 1, view bobbing is on (player is walking).
	// If 2, view bobbing is getting switched off.
	s32 m_view_bobbing_state = 0;
	// Speed of view bobbing animation
	f32 m_view_bobbing_speed = 0.0f;

	// Digging animation frame (0 <= m_digging_anim < 1)
	f32 m_digging_anim = 0.0f;
	// If -1, no digging animation
	// If 0, left-click digging animation
	// If 1, right-click digging animation
	s32 m_digging_button = -1;

	// Animation when changing wielded item
	f32 m_wield_change_timer = 0.125f;
	ItemStack m_wield_item_next;

	CameraMode m_camera_mode = CAMERA_MODE_FIRST;

	f32 m_cache_view_bobbing_amount;
	bool m_arm_inertia;

	std::list<Nametag *> m_nametags;
	bool m_show_nametag_backgrounds;

	// Last known light color of the player
	video::SColor m_player_light_color;
};
