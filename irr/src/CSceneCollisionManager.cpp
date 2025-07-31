// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CSceneCollisionManager.h"
#include "ICameraSceneNode.h"
#include "SViewFrustum.h"

#include "irrMath.h"

namespace scene
{

//! constructor
CSceneCollisionManager::CSceneCollisionManager(ISceneManager *smanager, video::IVideoDriver *driver) :
		SceneManager(smanager), Driver(driver)
{
	if (Driver)
		Driver->grab();
}

//! destructor
CSceneCollisionManager::~CSceneCollisionManager()
{
	if (Driver)
		Driver->drop();
}

//! Returns a 3d ray which would go through the 2d screen coordinates.
core::line3d<f32> CSceneCollisionManager::getRayFromScreenCoordinates(
		const core::position2d<s32> &pos, const ICameraSceneNode *camera)
{
	core::line3d<f32> ln(0, 0, 0, 0, 0, 0);

	if (!SceneManager)
		return ln;

	if (!camera)
		camera = SceneManager->getActiveCamera();

	if (!camera)
		return ln;

	const scene::SViewFrustum *f = camera->getViewFrustum();

	core::vector3df farLeftUp = f->getFarLeftUp();
	core::vector3df lefttoright = f->getFarRightUp() - farLeftUp;
	core::vector3df uptodown = f->getFarLeftDown() - farLeftUp;

	const core::rect<s32> &viewPort = Driver->getViewPort();
	core::dimension2d<u32> screenSize(viewPort.getWidth(), viewPort.getHeight());

	f32 dx = pos.X / (f32)screenSize.Width;
	f32 dy = pos.Y / (f32)screenSize.Height;

	if (camera->isOrthogonal())
		ln.start = f->cameraPosition + (lefttoright * (dx - 0.5f)) + (uptodown * (dy - 0.5f));
	else
		ln.start = f->cameraPosition;

	ln.end = farLeftUp + (lefttoright * dx) + (uptodown * dy);

	return ln;
}

} // end namespace scene
