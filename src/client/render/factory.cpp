// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "factory.h"

#include "log.h"
#include "plain.h"
#include "anaglyph.h"
#include "sidebyside.h"
#include "secondstage.h"
#include "client/shadows/dynamicshadowsrender.h"

#include <memory>

struct CreatePipelineResult
{
	v2f virtual_size_scale;
	std::unique_ptr<ShadowRenderer> shadow_renderer;
	std::unique_ptr<RenderPipeline> pipeline;
};

void createPipeline(const std::string &stereo_mode, IrrlichtDevice *device, Client *client, Hud *hud, CreatePipelineResult &result);

RenderingCore *createRenderingCore(const std::string &stereo_mode, IrrlichtDevice *device,
		Client *client, Hud *hud)
{
	CreatePipelineResult created_pipeline;
	createPipeline(stereo_mode, device, client, hud, created_pipeline);
	return new RenderingCore(device, client, hud,
			std::move(created_pipeline.shadow_renderer),
			std::move(created_pipeline.pipeline),
			created_pipeline.virtual_size_scale);
}

void createPipeline(const std::string &stereo_mode, IrrlichtDevice *device, Client *client, Hud *hud, CreatePipelineResult &result)
{
	result.shadow_renderer = createShadowRenderer(device, client);
	result.virtual_size_scale = v2f(1.0f);
	result.pipeline = std::make_unique<RenderPipeline>();

	if (result.shadow_renderer)
		result.pipeline->addStep<RenderShadowMapStep>();

	if (stereo_mode == "none") {
		populatePlainPipeline(result.pipeline.get(), client);
		return;
	}
	if (stereo_mode == "anaglyph") {
		populateAnaglyphPipeline(result.pipeline.get(), client);
		return;
	}
	if (stereo_mode == "sidebyside") {
		populateSideBySidePipeline(result.pipeline.get(), client, false, false, result.virtual_size_scale);
		return;
	}
	if (stereo_mode == "topbottom") {
		populateSideBySidePipeline(result.pipeline.get(), client, true, false, result.virtual_size_scale);
		return;
	}
	if (stereo_mode == "crossview") {
		populateSideBySidePipeline(result.pipeline.get(), client, false, true, result.virtual_size_scale);
		return;
	}

	// fallback to plain renderer
	errorstream << "Invalid rendering mode: " << stereo_mode << std::endl;
	populatePlainPipeline(result.pipeline.get(), client);
}
