// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CFPSCounter.h"
#include "irrMath.h"

namespace video
{

CFPSCounter::CFPSCounter() :
		FPS(0), StartTime(0), FramesCounted(0)
{
}

//! to be called every frame
void CFPSCounter::registerFrame(u32 now)
{
	++FramesCounted;

	const u32 milliseconds = now - StartTime;
	if (milliseconds >= 1500) {
		const f32 invMilli = core::reciprocal((f32)milliseconds);

		FPS = core::ceil32((1000 * FramesCounted) * invMilli);

		FramesCounted = 0;
		StartTime = now;
	}
}

} // end namespace video
