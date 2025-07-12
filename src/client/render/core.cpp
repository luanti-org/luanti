// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "core.h"
#include "plain.h"
#include "client/shadows/dynamicshadowsrender.h"
#include "settings.h"

RenderingCore::RenderingCore(IrrlichtDevice *_device, Client *_client, Hud *_hud,
		std::unique_ptr<ShadowRenderer> _shadow_renderer,
		std::unique_ptr<RenderPipeline> _pipeline,
		v2f _virtual_size_scale)
	: device(_device), client(_client), hud(_hud), shadow_renderer(std::move(_shadow_renderer)),
	pipeline(std::move(_pipeline)), virtual_size_scale(_virtual_size_scale)
{
}

RenderingCore::~RenderingCore() = default;

void RenderingCore::draw(video::SColor _skycolor, bool _show_hud,
		bool _draw_wield_tool, bool _draw_crosshair)
{
	v2u32 screensize = device->getVideoDriver()->getScreenSize();
	virtual_size = v2u32(screensize.X * virtual_size_scale.X, screensize.Y * virtual_size_scale.Y);

	PipelineContext context(device, client, hud, shadow_renderer.get(), _skycolor, screensize);
	context.draw_crosshair = _draw_crosshair;
	context.draw_wield_tool = _draw_wield_tool;
	context.show_hud = _show_hud;

	pipeline->reset(context);
	pipeline->run(context);
}

v2u32 RenderingCore::getVirtualSize() const
{
	return virtual_size;
}
