// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "anaglyph.h"
#include "client/camera.h"
#include <IrrlichtDevice.h>
#include <ISceneManager.h>

/// SetColorMaskStep step

SetColorMaskStep::SetColorMaskStep(int _color_mask)
	: color_mask(_color_mask)
{}

void SetColorMaskStep::run(PipelineContext &context)
{
	video::SOverrideMaterial &mat = context.device->getVideoDriver()->getOverrideMaterial();
	mat.reset();
	mat.Material.ColorMask = static_cast<video::E_COLOR_PLANE>(color_mask);
	mat.EnableProps = video::EMP_COLOR_MASK;
	mat.EnablePasses = scene::ESNRP_SKY_BOX | scene::ESNRP_SOLID |
			   scene::ESNRP_TRANSPARENT | scene::ESNRP_TRANSPARENT_EFFECT;
}

/// ClearDepthBufferTarget

ClearDepthBufferTarget::ClearDepthBufferTarget(RenderTarget *_target) :
	target(_target)
{}

void ClearDepthBufferTarget::activate(PipelineContext &context)
{
	target->activate(context);
	context.device->getVideoDriver()->clearBuffers(video::ECBF_DEPTH);
}

ConfigureOverrideMaterialTarget::ConfigureOverrideMaterialTarget(RenderTarget *_upstream, bool _enable) :
	upstream(_upstream), enable(_enable)
{
}

void ConfigureOverrideMaterialTarget::activate(PipelineContext &context)
{
	upstream->activate(context);
	context.device->getVideoDriver()->getOverrideMaterial().Enabled = enable;
}


void populateAnaglyphPipeline(RenderPipeline *pipeline, Client *client)
{
	// clear depth buffer every time 3D is rendered
	auto step3D = pipeline->own(create3DStage(client, v2f(1.0)));
	auto screen = pipeline->createOwned<ScreenTarget>();
	auto clear_depth = pipeline->createOwned<ClearDepthBufferTarget>(screen);
	auto enable_override_material = pipeline->createOwned<ConfigureOverrideMaterialTarget>(clear_depth, true);
	step3D->setRenderTarget(enable_override_material);

	// left eye
	pipeline->addStep<OffsetCameraStep>(false);
	pipeline->addStep<SetColorMaskStep>(video::ECP_RED);
	pipeline->addStep(step3D);

	// right eye
	pipeline->addStep<OffsetCameraStep>(true);
	pipeline->addStep<SetColorMaskStep>(video::ECP_GREEN | video::ECP_BLUE);
	pipeline->addStep(step3D);

	// reset
	pipeline->addStep<OffsetCameraStep>(0.0f);
	pipeline->addStep<SetColorMaskStep>(video::ECP_ALL);

	pipeline->addStep<DrawWield>();
	pipeline->addStep<MapPostFxStep>();
	pipeline->addStep<DrawHUD>();
}
