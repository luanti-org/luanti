// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "ISceneCollisionManager.h"
#include "ISceneManager.h"
#include "IVideoDriver.h"

namespace scene
{

class CSceneCollisionManager : public ISceneCollisionManager
{
public:
	//! constructor
	CSceneCollisionManager(ISceneManager *smanager, video::IVideoDriver *driver);

	//! destructor
	virtual ~CSceneCollisionManager();

	//! Returns a 3d ray which would go through the 2d screen coordinates.
	virtual core::line3d<f32> getRayFromScreenCoordinates(
			const core::position2d<s32> &pos, const ICameraSceneNode *camera = 0) override;

private:
	ISceneManager *SceneManager;
	video::IVideoDriver *Driver;
};

} // end namespace scene
