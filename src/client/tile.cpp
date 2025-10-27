// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "tile.h"
#include <cassert>

video::ITexture *AnimationInfo::getTexture(float animation_time) const
{
	if (getFrameCount() == 0)
		return nullptr;

	// Figure out current frame
	u16 frame = (u32)(animation_time * 1000.0f / std::max<u16>(1, m_frame_length_ms))
			% m_frame_count;

	assert(frame < m_frames->size());
	return (*m_frames)[frame].texture;
}

void AnimationInfo::updateTexture(video::SMaterial &material, float animation_time)
{
	video::ITexture *texture = getTexture(animation_time);
	if (texture) {
		material.setTexture(0, texture);
	}
}

void TileLayer::applyMaterialOptions(video::SMaterial &material, int layer) const
{
	material.setTexture(0, texture);

	material.BackfaceCulling = (material_flags & MATERIAL_FLAG_BACKFACE_CULLING) != 0;
	if (!(material_flags & MATERIAL_FLAG_TILEABLE_HORIZONTAL)) {
		material.TextureLayers[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayers[1].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
	}
	if (!(material_flags & MATERIAL_FLAG_TILEABLE_VERTICAL)) {
		material.TextureLayers[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayers[1].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	}

	/*
	 * The second layer is for overlays, but uses the same vertex positions
	 * as the first, which easily leads to Z-fighting.
	 * To fix this we offset the polygons of the *first layer* away from the camera.
	 * This only affects the depth buffer and leads to no visual gaps in geometry.
	 *
	 * However, doing so intrudes the "Z space" of the overlay of the next node
	 * so that leads to inconsistent Z-sorting again. :(
	 * HACK: For lack of a better approach we restrict this to cases where
	 * an overlay is actually present.
	 */
	if (need_polygon_offset) {
		material.PolygonOffsetSlopeScale = 1;
		material.PolygonOffsetDepthBias = 1;
	}
}
