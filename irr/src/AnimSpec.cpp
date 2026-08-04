// Copyright (C) 2026 Lars Müller
// This file is part of IrrlichtMt.
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "AnimSpec.h"
#include "irrMath.h"

#include <algorithm>

namespace scene {

void TrackAnimSpec::advance(f32 dtime_s)
{
	cur_frame += fps * dtime_s;
	blend_progress = std::min(blend_progress + dtime_s, blend_duration);
	if (loop) {
		const f32 duration = max_frame - min_frame;
		const f32 rem = duration == 0.0f ? 0.0f : core::frem(cur_frame - min_frame, duration);
		cur_frame = rem + min_frame;
	} else {
		cur_frame = std::clamp(cur_frame, min_frame, max_frame);
	}
}

void TrackAnimSpec::clamp(f32 model_max_frame)
{
	max_frame = std::min(max_frame, model_max_frame);
	min_frame = std::min(max_frame, min_frame);
	cur_frame = std::clamp(cur_frame, min_frame, max_frame);
}

void AnimSpec::advance(f32 dtime_s)
{
	for (auto &[_, track] : tracks) {
		track.advance(dtime_s);
	}
}

} // end namespace scene
