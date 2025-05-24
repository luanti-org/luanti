// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "sidebyside.h"
#include "client/client.h"
#include "client/hud.h"
#include "client/camera.h"
#include <ISceneManager.h>

DrawImageStep::DrawImageStep(u8 texture_index, v2f _offset) :
	texture_index(texture_index), offset(_offset)
{}

void DrawImageStep::setRenderSource(RenderSource *_source)
{
	source = _source;
}
void DrawImageStep::setRenderTarget(RenderTarget *_target)
{
	target = _target;
}

void DrawImageStep::run(PipelineContext &context)
{
	if (target)
		target->activate(context);

	auto texture = source->getTexture(texture_index);
	core::dimension2du output_size = context.device->getVideoDriver()->getScreenSize();
	v2s32 pos(offset.X * output_size.Width, offset.Y * output_size.Height);
	context.device->getVideoDriver()->draw2DImage(texture, pos);
}

void populateSideBySidePipeline(RenderPipeline *pipeline, Client *client, bool horizontal, bool flipped, v2f &virtual_size_scale)
{
	static const u8 TEXTURE_LEFT = 0;
	static const u8 TEXTURE_RIGHT = 1;
	static const u8 TEXTURE_DEPTH = 2;

	auto driver = client->getSceneManager()->getVideoDriver();
	video::ECOLOR_FORMAT color_format = selectColorFormat(driver);
	video::ECOLOR_FORMAT depth_format = selectDepthFormat(driver);

	v2f offset;
	if (horizontal) {
		virtual_size_scale = v2f(1.0f, 0.5f);
		offset = v2f(0.0f, 0.5f);
	}
	else {
		virtual_size_scale = v2f(0.5f, 1.0f);
		offset = v2f(0.5f, 0.0f);
	}

	TextureBuffer *buffer = pipeline->createOwned<TextureBuffer>();
	buffer->setTexture(TEXTURE_LEFT, virtual_size_scale, "3d_render_left", color_format);
	buffer->setTexture(TEXTURE_RIGHT, virtual_size_scale, "3d_render_right", color_format);
	buffer->setTexture(TEXTURE_DEPTH, virtual_size_scale, "3d_depthmap_sidebyside", depth_format);

	auto step3D = pipeline->own(create3DStage(client, virtual_size_scale));

	// eyes
	for (bool right : { false, true }) {
		pipeline->addStep<OffsetCameraStep>(flipped ? !right : right);
		auto output = pipeline->createOwned<TextureBufferOutput>(
				buffer, std::vector<u8> {right ? TEXTURE_RIGHT : TEXTURE_LEFT}, TEXTURE_DEPTH);
		pipeline->addStep<SetRenderTargetStep>(step3D, output);
		pipeline->addStep(step3D);
		pipeline->addStep<DrawWield>();
		pipeline->addStep<MapPostFxStep>();
		pipeline->addStep<DrawHUD>();
	}

	pipeline->addStep<OffsetCameraStep>(0.0f);

	auto screen = pipeline->createOwned<ScreenTarget>();

	for (bool right : { false, true }) {
		auto step = pipeline->addStep<DrawImageStep>(
				right ? TEXTURE_RIGHT : TEXTURE_LEFT,
				right ? offset : v2f());
		step->setRenderSource(buffer);
		step->setRenderTarget(screen);
	}
}
