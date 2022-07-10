/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

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

#include "anaglyph.h"
#include "client/camera.h"


/// SetColorMaskStep step

SetColorMaskStep::SetColorMaskStep(int _color_mask)
	: color_mask(_color_mask)
{}

void SetColorMaskStep::run(PipelineContext *context)
{
	video::SOverrideMaterial &mat = context->device->getVideoDriver()->getOverrideMaterial();
	mat.reset();
	mat.Material.ColorMask = color_mask;
	mat.EnableFlags = video::EMF_COLOR_MASK;
	mat.EnablePasses = scene::ESNRP_SKY_BOX | scene::ESNRP_SOLID |
			   scene::ESNRP_TRANSPARENT | scene::ESNRP_TRANSPARENT_EFFECT |
			   scene::ESNRP_SHADOW;
}

/// ClearDepthBufferTarget

ClearDepthBufferTarget::ClearDepthBufferTarget(video::IVideoDriver *_driver, RenderTarget *_target) :
	driver(_driver), target(_target)
{}

void ClearDepthBufferTarget::activate()
{
	target->activate();
	driver->clearBuffers(video::ECBF_DEPTH);
}

void RenderingCoreAnaglyph::createPipeline()
{
	// clear depth buffer every time 3D is rendered
	step3D->setRenderTarget(pipeline.own(new ClearDepthBufferTarget(driver, screen)));

	// left eye
	pipeline.addStep(pipeline.own(new OffsetCameraStep(false)));
	pipeline.addStep(pipeline.own(new SetColorMaskStep(video::ECP_RED)));
	pipeline.addStep(step3D);

	// right eye
	pipeline.addStep(pipeline.own(new OffsetCameraStep(true)));
	pipeline.addStep(pipeline.own(new SetColorMaskStep(video::ECP_GREEN | video::ECP_BLUE)));
	pipeline.addStep(step3D);

	// reset
	pipeline.addStep(pipeline.own(new OffsetCameraStep(0.0f)));
	pipeline.addStep(pipeline.own(new SetColorMaskStep(video::ECP_ALL)));
	
	pipeline.addStep(stepPostFx);
	pipeline.addStep(stepHUD);
}