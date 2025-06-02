#pragma once

#include "irrTypes.h"
#include <unordered_map>
#include <variant>
#include <string>

namespace scene {

using TrackId = std::variant<u16, std::string>;

//! Specification for a single animation track
struct TrackAnimSpec {
	f32 min_frame = 0.0f;
	f32 max_frame = 0.0f;
	f32 cur_frame = 0.0f;
	//! Can be negative for reversed playback
	f32 fps = 1.0f;
	bool loop = true;
	f32 blend = 0.0f;
	f32 blend_progress = 0.0f; // TODO refactor and move to different struct along with cur frame?

	f32 priority = 0.0f;

	void advance(f32 dtime_s);

private:

	f32 framemod(f32 frame) const;
};

//! Specification for multiple animation tracks
struct AnimSpec {
	std::unordered_map<u16, TrackAnimSpec> tracks;

	void advance(f32 dtime_s);
};

} // end namespace scene
