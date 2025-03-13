// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#pragma once
#include "stereo.h"

class InitInterlacedMaskStep : public TrivialRenderStep
{
public:
	InitInterlacedMaskStep(TextureBuffer *buffer, u8 index);
	void run(PipelineContext &context);
private:
	TextureBuffer *buffer;
	video::ITexture *last_mask { nullptr };
	u8 index;
};

void populateInterlacedPipeline(RenderPipeline *pipeline, Client *client);
