// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"

namespace video
{

class CFPSCounter
{
public:
	CFPSCounter();

	//! returns current fps
	s32 getFPS() const { return FPS; }

	//! to be called every frame
	void registerFrame(u32 now);

private:
	s32 FPS;
	u32 StartTime;
	u32 FramesCounted;
};

} // end namespace video
