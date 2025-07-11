// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 Liso <anlismon@gmail.com>

#include <cmath>

#include "client/shadows/dynamicshadows.h"
#include "client/client.h"
#include "client/clientenvironment.h"
#include "client/clientmap.h"
#include "client/camera.h"
#include <IVideoDriver.h>

using m4f = core::matrix4;

void DirectionalLight::createSplitMatrices(const Camera *cam)
{
	static const float COS_15_DEG = 0.965926f;
	v3f look = cam->getDirection().normalize();

	// if current look direction is < 15 degrees away from the captured
	// look direction then stick to the captured value, otherwise recapture.
	if (look.dotProduct(last_look) >= COS_15_DEG)
		look = last_look;
	else
		last_look = look;

	// camera view tangents
	float tanFovY = tanf(cam->getFovY() * 0.5f);
	float tanFovX = tanf(cam->getFovX() * 0.5f);

	// adjusted frustum boundaries
	float sfNear = future_frustum.zNear;
	float sfFar = adjustDist(future_frustum.zFar, cam->getFovY());
	assert(sfFar - sfNear > 0);

	// adjusted camera positions
	v3f cam_pos_world = cam->getPosition();

	// if world position is less than 1 node away from the captured
	// world position then stick to the captured value, otherwise recapture.
	if (cam_pos_world.getDistanceFromSQ(last_cam_pos_world) < BS * BS)
		cam_pos_world = last_cam_pos_world;
	else
		last_cam_pos_world = cam_pos_world;

	v3f cam_pos_scene = v3f(cam_pos_world.X - cam->getOffset().X * BS,
			cam_pos_world.Y - cam->getOffset().Y * BS,
			cam_pos_world.Z - cam->getOffset().Z * BS);
	cam_pos_scene += look * sfNear;
	cam_pos_world += look * sfNear;

	// center point of light frustum
	v3f center_scene = cam_pos_scene + look * 0.35 * (sfFar - sfNear);
	v3f center_world = cam_pos_world + look * 0.35 * (sfFar - sfNear);

	// Create a vector to the frustum far corner
	const v3f &viewUp = cam->getCameraNode()->getUpVector();
	v3f viewRight = look.crossProduct(viewUp);

	v3f farCorner = (look + viewRight * tanFovX + viewUp * tanFovY).normalize();
	// Compute the frustumBoundingSphere radius
	v3f boundVec = (cam_pos_scene + farCorner * sfFar) - center_scene;
	float radius = boundVec.getLength();
	float length = radius * 3.0f;
	v3f eye_displacement = direction * length;

	// we must compute the viewmat with the position - the camera offset
	// but the future_frustum position must be the actual world position
	v3f eye = center_scene - eye_displacement;
	v3f up(0.0f, 1.0f, 0.0f);
	// eye_displacement and up shall not be collinear
	// At noon, light points straight down, the "up" vector can be anything
	// that is not along the up axis
	if (core::equals(eye_displacement.crossProduct(up).getLengthSQ(), 0.f))
		up = v3f(1.0f, 0.0f, 0.0f);
	future_frustum.player = cam_pos_scene;
	future_frustum.position = center_world - eye_displacement;
	future_frustum.length = length;
	future_frustum.radius = radius;
	future_frustum.ViewMat.buildCameraLookAtMatrixLH(eye, center_scene, up);
	future_frustum.ProjOrthMat.buildProjectionMatrixOrthoLH(radius, radius,
			0.0f, length, false);
}

DirectionalLight::DirectionalLight(const u32 shadowMapResolution,
		const v3f &position, video::SColorf lightColor,
		f32 farValue) :
		diffuseColor(lightColor),
		farPlane(farValue), mapRes(shadowMapResolution), pos(position)
{}

void DirectionalLight::updateCameraOffset(const Camera *cam)
{
	if (future_frustum.zFar == 0.0f) // not initialized
		return;
	createSplitMatrices(cam);
	should_update_map_shadow = true;
	dirty = true;
}

void DirectionalLight::updateFrustum(const Camera *cam, Client *client)
{
	if (dirty)
		return;

	float zNear = cam->getCameraNode()->getNearValue();
	float zFar = getMaxFarValue();
	if (!client->getEnv().getClientMap().getControl().range_all)
		zFar = MYMIN(zFar, client->getEnv().getClientMap().getControl().wanted_range * BS);

	///////////////////////////////////
	// update splits near and fars
	future_frustum.zNear = zNear;
	future_frustum.zFar = zFar;

	// update shadow frustum
	createSplitMatrices(cam);
	// get the draw list for shadows
	client->getEnv().getClientMap().updateDrawListShadow(
			getPosition(), getDirection(), future_frustum.radius, future_frustum.length);
	should_update_map_shadow = true;
	dirty = true;
}

void DirectionalLight::commitFrustum()
{
	if (!dirty)
		return;

	shadow_frustum = future_frustum;
	dirty = false;
}

void DirectionalLight::setDirection(v3f dir)
{
	direction = -dir;
	direction.normalize();
}

v3f DirectionalLight::getPosition() const
{
	return shadow_frustum.position;
}

v3f DirectionalLight::getPlayerPos() const
{
	return shadow_frustum.player;
}

v3f DirectionalLight::getFuturePlayerPos() const
{
	return future_frustum.player;
}

const m4f &DirectionalLight::getViewMatrix() const
{
	return shadow_frustum.ViewMat;
}

const m4f &DirectionalLight::getProjectionMatrix() const
{
	return shadow_frustum.ProjOrthMat;
}

const m4f &DirectionalLight::getFutureViewMatrix() const
{
	return future_frustum.ViewMat;
}

const m4f &DirectionalLight::getFutureProjectionMatrix() const
{
	return future_frustum.ProjOrthMat;
}

m4f DirectionalLight::getViewProjMatrix()
{
	return shadow_frustum.ProjOrthMat * shadow_frustum.ViewMat;
}
