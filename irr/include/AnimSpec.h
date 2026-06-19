// Copyright (C) 2026 Lars Müller
// This file is part of IrrlichtMt.
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include "irrTypes.h"
#include <unordered_map>
#include <variant>
#include <string>

namespace scene {

using TrackId = std::variant<u16, std::string>;

/// Specification for a single animation track
struct TrackAnimSpec {
	f32 min_frame = 0.0f;
	f32 max_frame = 0.0f;
	f32 fps = 1.0f; ///< can be negative for reversed playback
	bool loop = true;
	f32 blend_duration = 0.0f; ///< timespan in seconds over which to lerp to the new animation

	s32 priority = 0; ///< higher priority tracks override lower priority ones

	// Animation progress
	f32 cur_frame = 0.0f; ///< must be in [min_frame, max_frame]
	f32 blend_progress = 0.0f; ///< how many seconds of blending have passed so far

	void advance(f32 dtime_s);
};

/// Specification for multiple animation tracks
struct AnimSpec {
	std::unordered_map<u16, TrackAnimSpec> tracks;

	void advance(f32 dtime_s);
};

} // end namespace scene
