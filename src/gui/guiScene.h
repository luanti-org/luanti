// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2020 Jean-Patrick Guerrero <jeanpatrick.guerrero@gmail.com>

#pragma once

#include "ICameraSceneNode.h"
#include "StyleSpec.h"
#include <IAnimatedMeshSceneNode.h>
#include <IGUIElement.h>
#include <IGUIEnvironment.h>


class GUIScene : public gui::IGUIElement
{
public:
	GUIScene(gui::IGUIEnvironment *env, scene::ISceneManager *smgr,
		 gui::IGUIElement *parent, core::recti rect, s32 id = -1);

	~GUIScene();

	scene::IAnimatedMeshSceneNode *setMesh(scene::IAnimatedMesh *mesh = nullptr);
	void setTexture(u32 idx, video::ITexture *texture);
	void setBackgroundColor(const video::SColor &color) noexcept { m_bgcolor = color; };
	void setFrameLoop(f32 begin, f32 end);
	void setAnimationSpeed(f32 speed);
	void enableMouseControl(bool enable) noexcept { m_mouse_ctrl = enable; };
	void setRotation(v2f rot) noexcept { m_custom_rot = rot; };
	void enableContinuousRotation(bool enable) noexcept { m_inf_rot = enable; };
	void setStyles(const std::array<StyleSpec, StyleSpec::NUM_STATES> &styles);

	virtual void draw();
	virtual bool OnEvent(const SEvent &event);

private:
	void calcOptimalDistance();
	void updateTargetPos();
	void updateCamera(scene::ISceneNode *target);
	void setCameraRotation(v3f rot);
	/// @return true indicates that the rotation was corrected
	bool correctBounds(v3f &rot);
	void cameraLoop();

	void updateCameraPos() { m_cam_pos = m_cam->getPosition(); };
	v3f getCameraRotation() const { return (m_cam_pos - m_target_pos).getHorizontalAngle(); };
	void rotateCamera(const v3f &delta) { setCameraRotation(getCameraRotation() + delta); };

	scene::ISceneManager *m_smgr;
	video::IVideoDriver *m_driver;
	scene::ICameraSceneNode *m_cam;
	scene::ISceneNode *m_target = nullptr;
	scene::IAnimatedMeshSceneNode *m_mesh = nullptr;

	f32 m_cam_distance = 50.f;

	u64 m_last_time = 0;

	v3f m_cam_pos;
	v3f m_target_pos;
	v3f m_last_target_pos;
	// Cursor positions
	v2f m_curr_pos;
	v2f m_last_pos;
	// Initial rotation
	v2f m_custom_rot;

	bool m_mouse_ctrl = true;
	bool m_update_cam = false;
	bool m_inf_rot    = false;
	bool m_initial_rotation = true;

	video::SColor m_bgcolor = 0;
};
