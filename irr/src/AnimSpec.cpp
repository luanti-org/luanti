#include "AnimSpec.h"
#include <cmath>

namespace scene {

void TrackAnimSpec::advance(f32 dtime_s) {
	cur_frame += fps * dtime_s;
	blend_progress = std::min(blend_progress + dtime_s, blend);
	if (fps < 0) {
		cur_frame = loop
			? (max_frame - framemod(max_frame - cur_frame))
			: std::max(cur_frame, min_frame);
	} else {
		cur_frame = loop
			? (min_frame + framemod(cur_frame - min_frame))
			: std::min(cur_frame, max_frame);
	}
}

f32 TrackAnimSpec::framemod(f32 frame) const {
	const f32 frame_duration = max_frame - min_frame;
	return frame_duration == 0.0f ? 0.0f : std::fmod(frame, frame_duration);
}

void AnimSpec::advance(f32 dtime_s) {
	for (auto &[_, track] : tracks) {
		track.advance(dtime_s);
	}
}

} // end namespace scene
