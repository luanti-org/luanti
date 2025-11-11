// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#pragma once
#include "stereo.h"

class DrawImageStep : public RenderStep
{
public:
	DrawImageStep(u8 texture_index, v2f offset);

	void setRenderSource(RenderSource *_source) override;
	void setRenderTarget(RenderTarget *_target) override;

	void reset(PipelineContext &context) override {}
	void run(PipelineContext &context) override;
private:
	u8 texture_index;
	v2f offset;
	RenderSource *source;
	RenderTarget *target;
};

void populateSideBySidePipeline(RenderPipeline *pipeline, Client *client, bool horizontal, bool flipped, v2f &virtual_size_scale);
