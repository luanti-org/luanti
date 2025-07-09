// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#pragma once
#include "stereo.h"
#include "pipeline.h"

/**
 * Set color mask when rendering the next steps
 */
class SetColorMaskStep : public TrivialRenderStep
{
public:
	SetColorMaskStep(int color_mask);

	void run(PipelineContext &context) override;
private:
	int color_mask;
};

/**
 * Resets depth buffer of the current render target
 *
 */
class ClearDepthBufferTarget : public RenderTarget
{
public:
	ClearDepthBufferTarget(RenderTarget *target);

	void reset(PipelineContext &context) override {}
	void activate(PipelineContext &context) override;
private:
	RenderTarget *target;
};


/**
 * Enables or disables override material when activated
 *
 */
class ConfigureOverrideMaterialTarget : public RenderTarget
{
public:
	ConfigureOverrideMaterialTarget(RenderTarget *upstream, bool enable);

	virtual void activate(PipelineContext &context) override;
private:
	RenderTarget *upstream;
	bool enable;
};

void populateAnaglyphPipeline(RenderPipeline *pipeline, Client *client);
