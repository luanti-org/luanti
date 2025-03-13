// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "tile.h"

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
	 * To fix this we can offset the polygons in the direction of the camera.
	 * This only affects the depth buffer and leads to no visual gaps in geometry.
	 */
	if (layer == 1) {
		material.PolygonOffsetSlopeScale = -1;
		material.PolygonOffsetDepthBias = -1;
	}
}
