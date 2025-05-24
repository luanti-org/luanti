// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#pragma once

#include "irrlichttypes_bloated.h"
#include <matrix4.h>
#include "util/basic_macros.h"
#include "constants.h"

class Camera;
class Client;

struct shadowFrustum
{
	f32 zNear{0.0f};
	f32 zFar{0.0f};
	f32 length{0.0f};
	f32 radius{0.0f};
	core::matrix4 ProjOrthMat;
	core::matrix4 ViewMat;
	v3f position;
	v3f player;
};

class DirectionalLight
{
public:
	DirectionalLight(const u32 shadowMapResolution,
			const v3f &position,
			video::SColorf lightColor = video::SColor(0xffffffff),
			f32 farValue = 100.0f);
	~DirectionalLight() = default;

	void updateCameraOffset(const Camera *cam);

	void updateFrustum(const Camera *cam, Client *client);

	// when set direction is updated to negative normalized(direction)
	void setDirection(v3f dir);
	v3f getDirection() const{
		return direction;
	};
	v3f getPosition() const;
	v3f getPlayerPos() const;
	v3f getFuturePlayerPos() const;

	/// Gets the light's matrices.
	const core::matrix4 &getViewMatrix() const;
	const core::matrix4 &getProjectionMatrix() const;
	const core::matrix4 &getFutureViewMatrix() const;
	const core::matrix4 &getFutureProjectionMatrix() const;
	core::matrix4 getViewProjMatrix();

	/// Gets the light's maximum far value, i.e. the shadow boundary
	f32 getMaxFarValue() const
	{
		return farPlane * BS;
	}

	/// Gets the current far value of the light
	f32 getFarValue() const
	{
		return shadow_frustum.zFar;
	}


	/// Gets the light's color.
	const video::SColorf &getLightColor() const
	{
		return diffuseColor;
	}

	/// Sets the light's color.
	void setLightColor(const video::SColorf &lightColor)
	{
		diffuseColor = lightColor;
	}

	/// Gets the shadow map resolution for this light.
	u32 getMapResolution() const
	{
		return mapRes;
	}

	/// If true, shadow map needs to be invalidated due to frustum change
	bool should_update_map_shadow{true};

	void commitFrustum();

private:
	void createSplitMatrices(const Camera *cam);

	video::SColorf diffuseColor;

	f32 farPlane;
	u32 mapRes;

	v3f pos;
	v3f direction{0};

	v3f last_cam_pos_world{0,0,0};
	v3f last_look{0,1,0};

	shadowFrustum shadow_frustum;
	shadowFrustum future_frustum;
	bool dirty{false};
};
