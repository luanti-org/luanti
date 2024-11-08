// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS

#pragma once

/*
 *
 * The coordinate space for sounds (sound-space):
 * ----------------------------------------------
 *
 * * The functions from ISoundManager (see sound.h) take spatial vectors in node-space.
 * * All other `v3f`s here are, if not told otherwise, in sound-space, which is
 *   defined as node-space mirrored along the x-axis.
 *   (This is needed because OpenAL uses a right-handed coordinate system.)
 * * Use `swap_handedness()` from `al_helpers.h` to convert between those two
 *   coordinate spaces.
 *
 *
 * How sounds are loaded:
 * ----------------------
 *
 * * Step 1:
 *   `loadSoundFile` or `loadSoundFile` is called. This adds an unopen sound with
 *   the given name to `m_sound_datas_unopen`.
 *   Unopen / lazy sounds (`ISoundDataUnopen`) are ogg-vorbis files that we did not yet
 *   start to decode. (Decoding an unopen sound does not fail under normal circumstances
 *   (because we check whether the file exists at least), if it does fail anyways,
 *   we should notify the user.)
 * * Step 2:
 *   `addSoundToGroup` is called, to add the name from step 1 to a group. If the
 *   group does not yet exist, a new one is created. A group can later be played.
 *   (The mapping is stored in `m_sound_groups`.)
 * * Step 3:
 *   `playSound` or `playSoundAt` is called.
 *   * Step 3.1:
 *     If the group with the name `spec.name` does not exist, and `spec.use_local_fallback`
 *     is true, a new group is created using the user's sound-pack.
 *   * Step 3.2:
 *     We choose one random sound name from the given group.
 *   * Step 3.3:
 *     We open the sound (see `openSingleSound`).
 *     If the sound is already open (in `m_sound_datas_open`), we take that one.
 *     Otherwise we open it by calling `ISoundDataUnopen::open`. We choose (by
 *     sound length), whether it's a single-buffer (`SoundDataOpenBuffer`) or
 *     streamed (`SoundDataOpenStream`) sound.
 *     Single-buffer sounds are always completely loaded. Streamed sounds can be
 *     partially loaded.
 *     The sound is erased from `m_sound_datas_unopen` and added to `m_sound_datas_open`.
 *     Open sounds are kept forever.
 *   * Step 3.4:
 *     We create the new `PlayingSound`. It has a `shared_ptr` to its open sound.
 *     If the open sound is streaming, the playing sound needs to be stepped using
 *     `PlayingSound::stepStream` for enqueuing buffers. For this purpose, the sound
 *     is added to `m_sounds_streaming` (as `weak_ptr`).
 *     If the sound is fading, it is added to `m_sounds_fading` for regular fade-stepping.
 *     The sound is also added to `m_sounds_playing`, so that one can access it
 *     via its sound handle.
 * * Step 4:
 *     Streaming sounds are updated. For details see [Streaming of sounds].
 * * Step 5:
 *     At deinitialization, we can just let the destructors do their work.
 *     Sound sources are deleted (and with this also stopped) by ~PlayingSound.
 *     Buffers can't be deleted while sound sources using them exist, because
 *     PlayingSound has a shared_ptr to its ISoundData.
 *
 *
 * Streaming of sounds:
 * --------------------
 *
 * In each "bigstep", all streamed sounds are stepStream()ed. This means a
 * sound can be stepped at any point in time in the bigstep's interval.
 *
 * In the worst case, a sound is stepped at the start of one bigstep and in the
 * end of the next bigstep. So between two stepStream()-calls lie at most
 * 2 * STREAM_BIGSTEP_TIME seconds.
 * We ensure that there are always enough untouched full buffers left such that
 * we do not run into an empty queue in this time period, see stepStream().
 *
 */

namespace sound {

// constants

// in seconds
constexpr f32 REMOVE_DEAD_SOUNDS_INTERVAL = 2.0f;
// maximum length in seconds that a sound can have without being streamed
constexpr f32 SOUND_DURATION_MAX_SINGLE = 3.0f;
// minimum time in seconds of a single buffer in a streamed sound
constexpr f32 MIN_STREAM_BUFFER_LENGTH = 1.0f;
// duration in seconds of one bigstep
constexpr f32 STREAM_BIGSTEP_TIME = 0.3f;
// step duration for the OpenALSoundManager thread, in seconds
constexpr f32 SOUNDTHREAD_DTIME = 0.016f;

static_assert(SOUND_DURATION_MAX_SINGLE >= MIN_STREAM_BUFFER_LENGTH * 2.0f,
		"There's no benefit in streaming if we can't queue more than 2 buffers.");

} // namespace sound
