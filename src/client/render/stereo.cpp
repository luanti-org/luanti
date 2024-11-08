// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "stereo.h"
#include "client/client.h"
#include "client/camera.h"
#include "constants.h"
#include "settings.h"

OffsetCameraStep::OffsetCameraStep(float eye_offset)
{
	move.setTranslation(core::vector3df(eye_offset, 0.0f, 0.0f));
}


OffsetCameraStep::OffsetCameraStep(bool right_eye)
{
	float eye_offset = BS * g_settings->getFloat("3d_paralax_strength", -0.087f, 0.087f) * (right_eye ? 1 : -1);
	move.setTranslation(core::vector3df(eye_offset, 0.0f, 0.0f));
}

void OffsetCameraStep::reset(PipelineContext &context)
{
	base_transform = context.client->getCamera()->getCameraNode()->getRelativeTransformation();
}

void OffsetCameraStep::run(PipelineContext &context)
{
	context.client->getCamera()->getCameraNode()->setPosition((base_transform * move).getTranslation());
}
