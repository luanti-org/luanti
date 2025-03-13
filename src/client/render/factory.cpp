// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "factory.h"
#include "log.h"
#include "plain.h"
#include "anaglyph.h"
#include "interlaced.h"
#include "sidebyside.h"
#include "secondstage.h"
#include "client/shadows/dynamicshadowsrender.h"

struct CreatePipelineResult
{
	v2f virtual_size_scale;
	ShadowRenderer *shadow_renderer { nullptr };
	RenderPipeline *pipeline { nullptr };
};

void createPipeline(const std::string &stereo_mode, IrrlichtDevice *device, Client *client, Hud *hud, CreatePipelineResult &result);

RenderingCore *createRenderingCore(const std::string &stereo_mode, IrrlichtDevice *device,
		Client *client, Hud *hud)
{
	CreatePipelineResult created_pipeline;
	createPipeline(stereo_mode, device, client, hud, created_pipeline);
	return new RenderingCore(device, client, hud,
			created_pipeline.shadow_renderer, created_pipeline.pipeline, created_pipeline.virtual_size_scale);
}

void createPipeline(const std::string &stereo_mode, IrrlichtDevice *device, Client *client, Hud *hud, CreatePipelineResult &result)
{
	result.shadow_renderer = createShadowRenderer(device, client);
	result.virtual_size_scale = v2f(1.0f);
	result.pipeline = new RenderPipeline();

	if (result.shadow_renderer)
		result.pipeline->addStep<RenderShadowMapStep>();

	if (stereo_mode == "none") {
		populatePlainPipeline(result.pipeline, client);
		return;
	}
	if (stereo_mode == "anaglyph") {
		populateAnaglyphPipeline(result.pipeline, client);
		return;
	}
	if (stereo_mode == "interlaced") {
		populateInterlacedPipeline(result.pipeline, client);
		return;
	}
	if (stereo_mode == "sidebyside") {
		populateSideBySidePipeline(result.pipeline, client, false, false, result.virtual_size_scale);
		return;
	}
	if (stereo_mode == "topbottom") {
		populateSideBySidePipeline(result.pipeline, client, true, false, result.virtual_size_scale);
		return;
	}
	if (stereo_mode == "crossview") {
		populateSideBySidePipeline(result.pipeline, client, false, true, result.virtual_size_scale);
		return;
	}

	// fallback to plain renderer
	errorstream << "Invalid rendering mode: " << stereo_mode << std::endl;
	populatePlainPipeline(result.pipeline, client);
}