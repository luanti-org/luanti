// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2024-2026 The Luanti Contributors

#ifdef __EMSCRIPTEN__

#include "porting_emscripten.h"

#include "porting.h"
#include "log.h"
#include "util/numeric.h"

#include <string>

#include <atomic>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <emscripten.h>
#include <emscripten/threading.h>

namespace {

constexpr std::uint64_t ORDINARY_SYNC_INTERVAL_MS = 10'000;

std::atomic<std::uint64_t> dirty_generation{0};
std::atomic<std::uint64_t> submitted_generation{0};
std::atomic<std::uint64_t> saved_generation{0};
std::atomic<porting::EmscriptenPersistenceReason> pending_reason{
	porting::EmscriptenPersistenceReason::Periodic};
std::atomic<bool> pending_urgent{false};
std::atomic<bool> sync_error{false};
std::uint64_t last_ordinary_start_ms = 0;

const char *reason_name(porting::EmscriptenPersistenceReason reason)
{
	switch (reason) {
	case porting::EmscriptenPersistenceReason::Settings:
		return "settings";
	case porting::EmscriptenPersistenceReason::Disconnect:
		return "disconnect";
	case porting::EmscriptenPersistenceReason::Shutdown:
		return "shutdown";
	case porting::EmscriptenPersistenceReason::Periodic:
	default:
		return "periodic";
	}
}

// A clip name has to be a bare file name that the launcher can append to its
// own media/ directory. Rejecting everything else here means a mod cannot aim
// the page's <video> element at another origin, and cannot escape the media
// directory with "../". The browser's own COOP/COEP rules would already block
// a cross-origin fetch, but a mod should not be able to make the attempt.
bool is_safe_clip_name(const std::string &clip)
{
	if (clip.empty() || clip.size() > 128)
		return false;
	if (clip.front() == '.' || clip.front() == '-')
		return false;
	for (char c : clip) {
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
		if (!ok)
			return false;
	}
	return clip.find("..") == std::string::npos;
}

// Theater Tier C spike state. The browser main thread decodes frames and
// writes them straight into these buffers (shared Wasm memory, see
// emscripten_start_video_texture()); the application worker is the only
// reader, via emscripten_poll_video_texture_frame(). Double-buffered so a
// frame currently being decoded never overwrites the one still being read.
constexpr std::size_t VIDEO_FRAME_BUFFER_BYTES =
	static_cast<std::size_t>(porting::VIDEO_TEXTURE_SPIKE_MAX_WIDTH) *
	porting::VIDEO_TEXTURE_SPIKE_MAX_HEIGHT * 4;

alignas(4) std::uint8_t video_frame_buffer[2][VIDEO_FRAME_BUFFER_BYTES];
std::atomic<std::uint64_t> video_frame_generation{0};
std::uint64_t video_frame_consumed = 0; // application worker only
std::atomic<int> video_frame_ready_index{-1};
std::atomic<int> video_frame_width{0};
std::atomic<int> video_frame_height{0};
// Written only by emscripten_start_video_texture()/emscripten_stop_video_texture(),
// both of which run on the application worker; read on that same thread.
std::string video_texture_name;

// Frame statistics ring (Module.luantiPerf). Layout is read by
// client/web/perf.js; bump PERF_RING_VERSION when it changes. The application
// worker is the only writer: it fills frames[written % capacity] and then
// publishes it by storing written + 1 with release order. Readers on the
// browser main thread load `written` with Atomics.load and copy what they
// need; at one record per frame the ring holds a little over two minutes at
// 60 fps, far longer than any reader needs between reads.
constexpr std::uint32_t PERF_RING_VERSION = 1;
constexpr std::uint32_t PERF_RING_CAPACITY = 8192;

struct PerfFrame {
	float frame_ms;
	float draw_ms;
	float swap_ms;
	std::uint32_t drawcalls;
	std::uint32_t primitives;
	std::uint32_t mesh_queue;
};
static_assert(sizeof(PerfFrame) == 24, "perf.js reads 6 words per frame");

struct PerfRing {
	std::uint32_t version;
	std::uint32_t capacity;
	std::uint32_t words_per_frame;
	std::atomic<std::uint32_t> written;
	PerfFrame frames[PERF_RING_CAPACITY];
};
static_assert(sizeof(std::atomic<std::uint32_t>) == 4, "lock-free 32-bit atomic");

alignas(16) PerfRing perf_ring = {PERF_RING_VERSION, PERF_RING_CAPACITY,
	sizeof(PerfFrame) / 4, {0}, {}};
double perf_last_frame_ms = 0.0; // application worker only

} // namespace

extern "C" {

// Address of the frame statistics ring, for client/web/perf.js. The ring is a
// static in shared memory, so the address is the same on every thread.
EMSCRIPTEN_KEEPALIVE std::uintptr_t luanti_perf_ring()
{
	return reinterpret_cast<std::uintptr_t>(&perf_ring);
}

// Called by client/web/videotexture.js after it has written a decoded frame
// into video_frame_buffer[buffer_index]. Runs on the browser main thread's
// own module instance, touching only shared memory -- same pattern as the
// persistence completion callbacks below.
EMSCRIPTEN_KEEPALIVE void luanti_video_texture_frame(int buffer_index, int width, int height)
{
	if (buffer_index != 0 && buffer_index != 1)
		return;
	if (width <= 0 || height <= 0 ||
			width > porting::VIDEO_TEXTURE_SPIKE_MAX_WIDTH ||
			height > porting::VIDEO_TEXTURE_SPIKE_MAX_HEIGHT)
		return;

	video_frame_width.store(width, std::memory_order_relaxed);
	video_frame_height.store(height, std::memory_order_relaxed);
	video_frame_ready_index.store(buffer_index, std::memory_order_release);
	video_frame_generation.fetch_add(1, std::memory_order_release);
}

EMSCRIPTEN_KEEPALIVE void luanti_persistence_sync_completed(double generation)
{
	auto completed = static_cast<std::uint64_t>(generation);
	auto current = saved_generation.load(std::memory_order_relaxed);
	while (current < completed && !saved_generation.compare_exchange_weak(
			current, completed, std::memory_order_release,
			std::memory_order_relaxed)) {
	}
	sync_error.store(false, std::memory_order_release);
}

EMSCRIPTEN_KEEPALIVE void luanti_persistence_sync_failed(double generation)
{
	(void)generation;
	sync_error.store(true, std::memory_order_release);
}

} // extern "C"

namespace porting {

void emscripten_validate_main_loop()
{
	if (emscripten_is_main_browser_thread()) {
		emscripten_log(EM_LOG_ERROR,
				"Luanti: browser main-loop contract violated; "
				"build with -sPROXY_TO_PTHREAD=1");
		std::abort();
	}

	emscripten_log(EM_LOG_CONSOLE,
			"Luanti: synchronous engine loop running on an application worker");
	emscripten_log(EM_LOG_CONSOLE,
			"Luanti: clock=emscripten_get_now t_ms=%.3f getTimeMs=%llu",
			emscripten_get_now(),
			(unsigned long long)porting::getTimeMs());
}

void emscripten_service_browser_frame()
{
	emscripten_service_persistence();
}

void emscripten_report_status(const std::string &message, int percent,
		const char *phase)
{
	// Called from both the client and the server init path, which are not
	// guaranteed to be the same thread, so the de-duplication state is locked.
	static std::mutex lock;
	static std::string last_message;
	static std::string last_phase;
	static int last_percent = -2;
	const std::string phase_string = phase ? phase : "";
	std::lock_guard<std::mutex> guard(lock);
	if (message == last_message && phase_string == last_phase &&
			percent == last_percent)
		return;
	last_message = message;
	last_phase = phase_string;
	last_percent = percent;

	MAIN_THREAD_EM_ASM({
		var launcher = Module["luantiLauncher"];
		if (launcher && typeof launcher["reportEngineStatus"] === "function") {
			launcher["reportEngineStatus"](
				UTF8ToString($0), $1, $2 ? UTF8ToString($2) : null);
		}
	}, message.c_str(), percent,
			phase_string.empty() ? nullptr : phase_string.c_str());
}

void emscripten_mark_persistence_dirty(EmscriptenPersistenceReason reason,
		bool urgent)
{
	pending_reason.store(reason, std::memory_order_relaxed);
	if (urgent)
		pending_urgent.store(true, std::memory_order_release);
	dirty_generation.fetch_add(1, std::memory_order_release);
}

void emscripten_service_persistence()
{
	auto generation = dirty_generation.load(std::memory_order_acquire);
	if (generation <= submitted_generation.load(std::memory_order_acquire))
		return;

	const bool urgent = pending_urgent.load(std::memory_order_acquire);
	const auto now = porting::getTimeMs();
	if (!urgent && last_ordinary_start_ms != 0 &&
			now - last_ordinary_start_ms < ORDINARY_SYNC_INTERVAL_MS)
		return;

	// Exchange only after the request is eligible. An urgent worker request
	// racing after this exchange remains set for the next service point.
	const bool submitted_urgent = pending_urgent.exchange(false,
		std::memory_order_acq_rel);
	const auto reason = pending_reason.load(std::memory_order_relaxed);

	if (!submitted_urgent)
		last_ordinary_start_ms = now;
	submitted_generation.store(generation, std::memory_order_release);

	MAIN_THREAD_EM_ASM({
		var persistence = Module["luantiPersistence"];
		if (!persistence || !persistence["_requestGeneration"]) {
			console.error("Luanti persistence service is unavailable");
			var failed = Module["_luanti_persistence_sync_failed"];
			if (typeof failed === "function")
				failed($0);
		} else {
			persistence["_requestGeneration"]({
				"generation": $0,
				"reason": UTF8ToString($1),
				"urgent": !!$2
			});
		}
	}, static_cast<double>(generation), reason_name(reason), submitted_urgent);
}

bool emscripten_persistence_pending()
{
	return saved_generation.load(std::memory_order_acquire) <
		dirty_generation.load(std::memory_order_acquire);
}

bool emscripten_persistence_error()
{
	return sync_error.load(std::memory_order_acquire);
}

void emscripten_wait_for_persistence()
{
	while (emscripten_persistence_pending()) {
		emscripten_service_persistence();
		// main() runs on the application worker. Sleeping it leaves the browser
		// main thread free to complete syncfs callbacks and display errors.
		emscripten_thread_sleep(16);
	}
}

void emscripten_record_frame(float draw_ms, float swap_ms,
		std::uint32_t drawcalls, std::uint32_t primitives,
		std::uint32_t mesh_queue)
{
	const double now = emscripten_get_now();
	const double last = perf_last_frame_ms;
	perf_last_frame_ms = now;
	// The first call only starts the clock; a frame time needs two ends.
	if (last == 0.0)
		return;
	const std::uint32_t n = perf_ring.written.load(std::memory_order_relaxed);
	PerfFrame &f = perf_ring.frames[n % PERF_RING_CAPACITY];
	f.frame_ms = static_cast<float>(now - last);
	f.draw_ms = draw_ms;
	f.swap_ms = swap_ms;
	f.drawcalls = drawcalls;
	f.primitives = primitives;
	f.mesh_queue = mesh_queue;
	perf_ring.written.store(n + 1, std::memory_order_release);
}

bool emscripten_show_video_overlay(const std::string &clip,
		const std::string &title, bool loop, bool muted)
{
	if (!is_safe_clip_name(clip)) {
		errorstream << "Refusing to play web video clip with unusable name: "
			<< clip << std::endl;
		return false;
	}

	// The launcher may have no overlay (an embedder replaced the shell) or no
	// clip on disk. Either way Tier A keeps playing, so a false return is a
	// normal outcome rather than an error.
	return MAIN_THREAD_EM_ASM_INT({
		var theater = Module["luantiTheater"];
		if (!theater || typeof theater["show"] !== "function")
			return 0;
		return theater["show"]({
			"clip": UTF8ToString($0),
			"title": UTF8ToString($1),
			"loop": !!$2,
			"muted": !!$3
		}) ? 1 : 0;
	}, clip.c_str(), title.c_str(), loop, muted) != 0;
}

void emscripten_hide_video_overlay()
{
	MAIN_THREAD_EM_ASM({
		var theater = Module["luantiTheater"];
		if (theater && typeof theater["hide"] === "function")
			theater["hide"]();
	});
}

bool emscripten_start_video_texture(const std::string &clip,
		const std::string &texture_name, int width, int height, int fps)
{
	if (!is_safe_clip_name(clip)) {
		errorstream << "Refusing to start web video texture with unusable "
			"clip name: " << clip << std::endl;
		return false;
	}
	if (texture_name.empty() || texture_name.size() > 128) {
		errorstream << "Refusing to start web video texture with unusable "
			"texture name" << std::endl;
		return false;
	}

	width = rangelim(width, 1, VIDEO_TEXTURE_SPIKE_MAX_WIDTH);
	height = rangelim(height, 1, VIDEO_TEXTURE_SPIKE_MAX_HEIGHT);
	fps = rangelim(fps, 1, VIDEO_TEXTURE_SPIKE_MAX_FPS);

	video_texture_name = texture_name;
	video_frame_ready_index.store(-1, std::memory_order_relaxed);
	video_frame_generation.store(0, std::memory_order_relaxed);
	video_frame_consumed = 0;

	// Anything the launcher does not implement (no decoder shell, browser
	// refuses the codec) is a normal "stay on Tier A/B" outcome, not an error.
	return MAIN_THREAD_EM_ASM_INT({
		var texturer = Module["luantiVideoTexture"];
		if (!texturer || typeof texturer["start"] !== "function")
			return 0;
		return texturer["start"]({
			"clip": UTF8ToString($0),
			"width": $1,
			"height": $2,
			"fps": $3,
			"bufferPtr0": $4,
			"bufferPtr1": $5,
			"bufferSize": $6
		}) ? 1 : 0;
	}, clip.c_str(), width, height, fps,
			(int)(std::uintptr_t)video_frame_buffer[0],
			(int)(std::uintptr_t)video_frame_buffer[1],
			(int)VIDEO_FRAME_BUFFER_BYTES) != 0;
}

void emscripten_stop_video_texture()
{
	video_texture_name.clear();
	video_frame_ready_index.store(-1, std::memory_order_relaxed);

	MAIN_THREAD_EM_ASM({
		var texturer = Module["luantiVideoTexture"];
		if (texturer && typeof texturer["stop"] === "function")
			texturer["stop"]();
	});
}

bool emscripten_poll_video_texture_frame(std::string *out_texture_name,
		const std::uint8_t **out_pixels, int *out_width, int *out_height)
{
	if (video_texture_name.empty())
		return false;

	const auto generation = video_frame_generation.load(std::memory_order_acquire);
	if (generation == 0 || generation == video_frame_consumed)
		return false;

	const int index = video_frame_ready_index.load(std::memory_order_acquire);
	if (index != 0 && index != 1)
		return false;

	video_frame_consumed = generation;
	*out_texture_name = video_texture_name;
	*out_pixels = video_frame_buffer[index];
	*out_width = video_frame_width.load(std::memory_order_relaxed);
	*out_height = video_frame_height.load(std::memory_order_relaxed);
	return true;
}

} // namespace porting

#endif // __EMSCRIPTEN__
