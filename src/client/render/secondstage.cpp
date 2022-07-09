/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>
Copyright (C) 2020 appgurueu, Lars Mueller <appgurulars@gmx.de>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "secondstage.h"
#include "client/client.h"
#include "client/shader.h"
#include "client/tile.h"

PostProcessingStep::PostProcessingStep(video::IVideoDriver *_driver, video::E_MATERIAL_TYPE shader, const std::vector<u8> &_texture_map) :
	driver(_driver),
	texture_map(_texture_map)
{
	createMaterial(shader);
}

void PostProcessingStep::createMaterial(video::E_MATERIAL_TYPE shader)
{
	material.UseMipMaps = false;
	material.ZBuffer = true;
	material.ZWriteEnable = video::EZW_ON;
	material.MaterialType = shader;
	for (u32 k = 0; k < MYMIN(video::MATERIAL_MAX_TEXTURES, texture_map.size()); ++k) {
		material.TextureLayer[k].AnisotropicFilter = false;
		material.TextureLayer[k].BilinearFilter = false;
		material.TextureLayer[k].TrilinearFilter = false;
		material.TextureLayer[k].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
		material.TextureLayer[k].TextureWrapV = video::ETC_CLAMP_TO_EDGE;
	}
}

void PostProcessingStep::setRenderSource(RenderSource *_source)
{
	source = _source;
}

void PostProcessingStep::setRenderTarget(RenderTarget *_target)
{
	target = _target;
}

void PostProcessingStep::reset()
{
}

void PostProcessingStep::run()
{
	if (target)
		target->activate();

	for (u32 i = 0; i < MYMIN(texture_map.size(), video::MATERIAL_MAX_TEXTURES); i++)
		material.TextureLayer[i].Texture = source->getTexture(texture_map[i]);

	// driver->setTransform(video::ETS_VIEW, core::matrix4::EM4CONST_IDENTITY);
	// driver->setTransform(video::ETS_PROJECTION, core::matrix4::EM4CONST_IDENTITY);
	// driver->setTransform(video::ETS_WORLD, core::matrix4::EM4CONST_IDENTITY);
	static const video::SColor color = video::SColor(0, 0, 0, 255);
	static const video::S3DVertex vertices[4] = {
			video::S3DVertex(1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					color, 1.0, 0.0),
			video::S3DVertex(-1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					color, 0.0, 0.0),
			video::S3DVertex(-1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					color, 0.0, 1.0),
			video::S3DVertex(1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					color, 1.0, 1.0),
	};
	static const u16 indices[6] = {0, 1, 2, 2, 3, 0};
	driver->setMaterial(material);
	driver->drawVertexPrimitiveList(&vertices, 4, &indices, 2);
}


RenderingCoreSecondStage::RenderingCoreSecondStage(
		IrrlichtDevice *_device, Client *_client, Hud *_hud) :
		RenderingCoreStereo(_device, _client, _hud),
		buffer(_device->getVideoDriver())
{
}

void RenderingCoreSecondStage::createPipeline()
{
	// init post-processing buffer
	buffer.setTexture(0, screensize.X, screensize.Y, "3d_render", video::ECF_A8R8G8B8);
	buffer.setTexture(1, screensize.X, screensize.Y, "3d_normalmap", video::ECF_A8R8G8B8);
	buffer.setDepthTexture(2, screensize.X, screensize.Y, "3d_depthmap", video::ECF_D32);
	buffer.setClearColor(&skycolor);

	// link to 3D step
	step3D->setRenderTarget(&buffer);

	// 3d stage
	pipeline.addStep(pipeline.own(new TrampolineStep<RenderingCoreSecondStage>(this, &RenderingCoreSecondStage::resetBuffer)));
	pipeline.addStep(step3D);

	// post-processing stage
	{
		// set up shader
		IWritableShaderSource *s = client->getShaderSource();
		s->global_additional_headers = "#define SECONDSTAGE 1\n";
		s->rebuildShaders(); // TODO remove this if possible, use shader const setters?
		u32 shader_index = s->getShader("3d_secondstage", TILE_MATERIAL_BASIC, NDT_NORMAL);
		video::E_MATERIAL_TYPE shader = s->getShaderInfo(shader_index).material;

		PostProcessingStep *effect = new PostProcessingStep(driver, shader, std::vector<u8> {0, 1, 0, 2});
		effect->setRenderSource(&buffer);
		effect->setRenderTarget(screen);
		pipeline.addStep(pipeline.own(effect));
	}

	// HUD and overlays
	pipeline.addStep(stepPostFx);
	pipeline.addStep(stepHUD);
}

void RenderingCoreSecondStage::resetBuffer()
{
	buffer.RenderTarget::reset();
}
