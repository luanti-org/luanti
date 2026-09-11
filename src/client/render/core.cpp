// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "core.h"

#include "pipeline.h"
#include "client/shadows/dynamicshadowsrender.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "hud_element.h"
#include "util/numeric.h"

RenderingCore::RenderingCore(IrrlichtDevice *_device, Client *_client, Hud *_hud,
		std::unique_ptr<ShadowRenderer> _shadow_renderer,
		std::unique_ptr<RenderPipeline> _pipeline,
		v2f _virtual_size_scale)
	: device(_device), client(_client), hud(_hud), shadow_renderer(std::move(_shadow_renderer)),
	pipeline(std::move(_pipeline)), virtual_size_scale(_virtual_size_scale)
{
}

RenderingCore::~RenderingCore() = default;

static f32 apply_easing(HudAnimEasing easing, f32 t)
{
	switch (easing) {
		case HUD_ANIM_EASING_EASE_IN:     return ease_in(t);
		case HUD_ANIM_EASING_EASE_OUT:    return ease_out(t);
		case HUD_ANIM_EASING_EASE_IN_OUT: return ease_in_out(t);
		default:                          return t;
	}
}

static f32 evaluate_property(HudAnimProperty &prop, f32 dtime)
{
	if (prop.finished || prop.keyframes.size() < 2)
		return prop.keyframes.empty() ? 0.0f : prop.keyframes[0].value;

	f32 cycle_duration = 0.0f;
	for (size_t i = 0; i < prop.keyframes.size() - 1; i++)
		cycle_duration += prop.keyframes[i].duration;

	bool loops = (prop.loop == -1 || prop.loop > 0);
	if (loops)
		cycle_duration += prop.keyframes.back().duration;

	if (cycle_duration <= 0.0f)
		return prop.keyframes[0].value;

	prop.elapsed += dtime;

	while (prop.elapsed >= cycle_duration) {
		if (!loops) {
			prop.finished = true;
			return prop.keyframes.back().value;
		}

		prop.elapsed -= cycle_duration;
		prop.loops_completed++;

		if (prop.loop > 0 && prop.loops_completed >= prop.loop) {
			prop.finished = true;
			return prop.keyframes.back().value;
		}
	}

	f32 t_acc = 0.0f;
	for (size_t i = 0; i < prop.keyframes.size(); i++) {
		f32 seg_dur = prop.keyframes[i].duration;
		if (seg_dur <= 0.0f)
			continue;

		if (prop.elapsed < t_acc + seg_dur) {
			f32 local_t = (prop.elapsed - t_acc) / seg_dur;
			local_t = apply_easing(prop.easing, local_t);

			size_t next = (i + 1) % prop.keyframes.size();
			f32 a = prop.keyframes[i].value;
			f32 b = prop.keyframes[next].value;
			return a + (b - a) * local_t;
		}
		t_acc += seg_dur;
	}

	return prop.keyframes.back().value;
}

static void apply_anim_value(HudElement *e, u8 stat, f32 value)
{
	switch (static_cast<HudAnimationStat>(stat)) {
		case HUD_ANIM_POS_X:    e->pos.X = value;    break;
		case HUD_ANIM_POS_Y:    e->pos.Y = value;    break;
		case HUD_ANIM_SCALE_X:  e->scale.X = value;  break;
		case HUD_ANIM_SCALE_Y:  e->scale.Y = value;  break;
		case HUD_ANIM_OFFSET_X: e->offset.X = value;  break;
		case HUD_ANIM_OFFSET_Y: e->offset.Y = value;  break;
		case HUD_ANIM_SIZE_X:   e->size.X = value;   break;
		case HUD_ANIM_SIZE_Y:   e->size.Y = value;   break;
		case HUD_ANIM_OPACITY:  e->opacity = rangelim(value, 0.0f, 1.0f); break;
		default: break;
	}
}

void RenderingCore::step(float dtime)
{
	LocalPlayer *player = client->getEnv().getLocalPlayer();
	if (!player)
		return;

	for (const auto &e : player->hud.getElements()) {
		if (!e || !e->animations)
			continue;

		bool all_finished = true;
		for (auto &[stat, prop] : e->animations->properties) {
			if (prop.finished)
				continue;

			f32 value = evaluate_property(prop, dtime);
			apply_anim_value(e.get(), stat, value);
			all_finished = false;
		}

		if (all_finished)
			e->animations.reset();
	}
}

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
