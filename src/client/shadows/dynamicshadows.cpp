/*
Minetest
Copyright (C) 2021 Liso <anlismon@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <cmath>

#include "client/shadows/dynamicshadows.h"
#include "client/client.h"
#include "client/clientenvironment.h"
#include "client/clientmap.h"
#include "client/camera.h"

using m4f = core::matrix4;

static v3f quantizeDirection(v3f direction, float step)
{

	float yaw = std::atan2(direction.Z, direction.X);
	float pitch = std::asin(direction.Y); // assume look is normalized

	yaw = std::floor(yaw / step) * step;
	pitch = std::floor(pitch / step) * step;

	return v3f(std::cos(yaw)*std::cos(pitch), std::sin(pitch), std::sin(yaw)*std::cos(pitch));
}

void DirectionalLight::createSplitMatrices(const Camera *cam)
{
	const float DISTANCE_STEP = BS * 2.0; // 2 meters
	v3f newCenter;
	v3f look = cam->getDirection();
	look = quantizeDirection(look, M_PI / 12.0); // 15 degrees

	// camera view tangents
	float tanFovY = tanf(cam->getFovY() * 0.5f);
	float tanFovX = tanf(cam->getFovX() * 0.5f);

	// adjusted frustum boundaries
	float sfNear = future_frustum.zNear;
	float sfFar = adjustDist(future_frustum.zFar, cam->getFovY());

	// adjusted camera positions
	v3f cam_pos_world = cam->getPosition();
	cam_pos_world = v3f(
			floor(cam_pos_world.X / DISTANCE_STEP) * DISTANCE_STEP,
			floor(cam_pos_world.Y / DISTANCE_STEP) * DISTANCE_STEP,
			floor(cam_pos_world.Z / DISTANCE_STEP) * DISTANCE_STEP);
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
	v3f eye_displacement = quantizeDirection(direction, M_PI / 2880 /*15 seconds*/) * length;

	// we must compute the viewmat with the position - the camera offset
	// but the future_frustum position must be the actual world position
	v3f eye = center_scene - eye_displacement;
	future_frustum.player = cam_pos_scene;
	future_frustum.position = center_world - eye_displacement;
	future_frustum.length = length;
	future_frustum.radius = radius;
	future_frustum.ViewMat.buildCameraLookAtMatrixLH(eye, center_scene, v3f(0.0f, 1.0f, 0.0f));
	future_frustum.ProjOrthMat.buildProjectionMatrixOrthoLH(radius, radius, 
			0.0f, length, false);
	future_frustum.camera_offset = cam->getOffset();
}

DirectionalLight::DirectionalLight(const u32 shadowMapResolution,
		const v3f &position, video::SColorf lightColor,
		f32 farValue) :
		diffuseColor(lightColor),
		farPlane(farValue), mapRes(shadowMapResolution), pos(position)
{}

void DirectionalLight::update_frustum(const Camera *cam, Client *client, bool force)
{
	if (dirty && !force)
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

	// when camera offset changes, adjust the current frustum view matrix to avoid flicker
	v3s16 cam_offset = cam->getOffset();
	if (cam_offset != shadow_frustum.camera_offset) {
		v3f rotated_offset;
		shadow_frustum.ViewMat.rotateVect(rotated_offset, intToFloat(cam_offset - shadow_frustum.camera_offset, BS));
		shadow_frustum.ViewMat.setTranslation(shadow_frustum.ViewMat.getTranslation() + rotated_offset);
		shadow_frustum.player += intToFloat(shadow_frustum.camera_offset - cam->getOffset(), BS);
		shadow_frustum.camera_offset = cam_offset;
	}
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
