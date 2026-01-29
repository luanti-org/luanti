// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"
#include "config.h"
#include <csignal>
#include <string>

#if !IS_CLIENT_BUILD
#error Do not include in server builds
#endif

class InputHandler;
class ChatBackend;
class RenderingEngine;
struct GameStartData;

struct Jitter {
	f32 dtime_avg = 1.0f,   //< FPS = 1 / avg
		dtime_sum = 0.0f;
	int dtime_samples = 0;
	// Jitter indicators
	f32 max = 0.0f,          //< maximum jitter
		counter = 0.0f,
		max_fraction = 0.0f; //< is =(max/avg)
};

struct RunStats {
	u64 drawtime; // (us)

	Jitter dtime_jitter;
};

struct CameraOrientation {
	f32 camera_yaw;    // "right/left"
	f32 camera_pitch;  // "up/down"
};

#define GAME_FALLBACK_TIMEOUT 1.8f
#define GAME_CONNECTION_TIMEOUT 10.0f

void the_game(volatile std::sig_atomic_t *kill,
		InputHandler *input,
		RenderingEngine *rendering_engine,
		const GameStartData &start_data,
		std::string &error_message,
		ChatBackend &chat_backend,
		bool *reconnect_requested);
