// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2026 The Luanti Contributors

#pragma once

#include <cstdint>
#include <string>

namespace porting {

enum class EmscriptenPersistenceReason : std::uint8_t {
	Periodic,
	Settings,
	Disconnect,
	Shutdown,
};

#ifdef __EMSCRIPTEN__

// PROXY_TO_PTHREAD must place the synchronous launcher/game stack on an
// application worker. Fail early if the browser build contract regresses.
void emscripten_validate_main_loop();
void emscripten_service_browser_frame();
// Send human-readable engine loading state to the browser launcher. Phase is a
// stable machine-readable value such as "engine", "loading", or "playing".
void emscripten_report_status(const std::string &message, int percent = -1,
		const char *phase = nullptr);

// Browser video overlay (theater Tier B). The page owns an HTML <video>; the
// engine only asks for a clip by name. `clip` is a bare file name, never a
// path or a URL: the launcher resolves it inside its own media/ directory, so
// a mod cannot point the overlay at a third-party origin. Returns false if the
// name is not acceptable.
bool emscripten_show_video_overlay(const std::string &clip,
		const std::string &title, bool loop, bool muted);
void emscripten_hide_video_overlay();

// Theater Tier C spike (GH issue #29): the browser decodes a clip and hands
// back RGBA frames, which get uploaded onto a named engine texture instead of
// an HTML overlay. Deliberately capped to a single small quad while this is a
// feasibility spike, not a production path.
constexpr int VIDEO_TEXTURE_SPIKE_MAX_WIDTH = 320;
constexpr int VIDEO_TEXTURE_SPIKE_MAX_HEIGHT = 180;
constexpr int VIDEO_TEXTURE_SPIKE_MAX_FPS = 24;

// Starts decoding `clip` in the browser and republishing frames onto the
// engine texture named `texture_name` (any tile/node using that texture name
// picks it up, same as any other named texture). `width`/`height`/`fps` are
// clamped to the spike limits above. Returns false if the clip name is
// unusable or no browser-side decoder is present.
bool emscripten_start_video_texture(const std::string &clip,
		const std::string &texture_name, int width, int height, int fps);
void emscripten_stop_video_texture();

// Called once per client frame on the application worker -- the only thread
// allowed to touch ITextureSource. Returns true and fills the out params when
// a new decoded frame is waiting; `*out_pixels` points at BGRA8 data (see
// SColor.h's note on ECF_A8R8G8B8 memory order) valid until the next call.
bool emscripten_poll_video_texture_frame(std::string *out_texture_name,
		const std::uint8_t **out_pixels, int *out_width, int *out_height);

// Save paths on any engine thread only record committed work. The application
// worker calls emscripten_service_persistence() while pumping frames and the
// service forwards eligible generations to the browser main thread.
void emscripten_mark_persistence_dirty(EmscriptenPersistenceReason reason,
		bool urgent = false);
void emscripten_service_persistence();

// Runtime state used by the graceful-shutdown driver. A failed sync remains
// pending because the JavaScript service retries it in the background.
bool emscripten_persistence_pending();
bool emscripten_persistence_error();
void emscripten_wait_for_persistence();

// Frame statistics for Module.luantiPerf (client/web/perf.js). Called once per
// game loop iteration on the application worker; the frame time is the wall
// time since the previous call. The record goes into a ring in shared memory
// that the page reads directly, so this never waits on the browser main thread.
// draw_ms covers the whole scene draw including present, swap_ms the present
// (endScene/SwapWindow) alone.
void emscripten_record_frame(float draw_ms, float swap_ms,
		std::uint32_t drawcalls, std::uint32_t primitives,
		std::uint32_t mesh_queue);

#else

inline void emscripten_validate_main_loop()
{
}

inline void emscripten_service_browser_frame()
{
}

inline void emscripten_report_status(const std::string &, int = -1,
		const char * = nullptr)
{
}

inline bool emscripten_show_video_overlay(const std::string &,
		const std::string &, bool, bool)
{
	return false;
}

inline void emscripten_hide_video_overlay()
{
}

constexpr int VIDEO_TEXTURE_SPIKE_MAX_WIDTH = 320;
constexpr int VIDEO_TEXTURE_SPIKE_MAX_HEIGHT = 180;
constexpr int VIDEO_TEXTURE_SPIKE_MAX_FPS = 24;

inline bool emscripten_start_video_texture(const std::string &,
		const std::string &, int, int, int)
{
	return false;
}

inline void emscripten_stop_video_texture()
{
}

inline bool emscripten_poll_video_texture_frame(std::string *,
		const std::uint8_t **, int *, int *)
{
	return false;
}

inline void emscripten_mark_persistence_dirty(
		EmscriptenPersistenceReason, bool = false)
{
}

inline void emscripten_service_persistence()
{
}

inline bool emscripten_persistence_pending()
{
	return false;
}

inline bool emscripten_persistence_error()
{
	return false;
}

inline void emscripten_wait_for_persistence()
{
}

inline void emscripten_record_frame(float, float, std::uint32_t, std::uint32_t,
		std::uint32_t)
{
}

#endif

} // namespace porting
