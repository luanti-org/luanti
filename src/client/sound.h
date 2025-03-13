// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irr_v3d.h"
#include "config.h"
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if !IS_CLIENT_BUILD
#error Do not include in server builds
#endif

struct SoundSpec;

class SoundFallbackPathProvider
{
public:
	virtual ~SoundFallbackPathProvider() = default;
	std::vector<std::string> getLocalFallbackPathsForSoundname(const std::string &name);
protected:
	virtual void addThePaths(const std::string &name, std::vector<std::string> &paths);
	// adds <common>.ogg, <common>.1.ogg, ..., <common>.9.ogg to paths
	void addAllAlternatives(const std::string &common, std::vector<std::string> &paths);
private:
	std::unordered_set<std::string> m_done_names;
};


/**
 * IDs for playing sounds.
 * 0 is for sounds that are never modified after creation.
 * Negative numbers are invalid.
 * Positive numbers are allocated via allocateId and are manually reference-counted.
 */
using sound_handle_t = int;

constexpr sound_handle_t SOUND_HANDLE_T_MAX = std::numeric_limits<sound_handle_t>::max();

class ISoundManager
{
private:
	std::unordered_map<sound_handle_t, u32> m_occupied_ids;
	sound_handle_t m_next_id = 1;
	std::vector<sound_handle_t> m_removed_sounds;

protected:
	void reportRemovedSound(sound_handle_t id);

public:
	virtual ~ISoundManager() = default;

	/**
	 * Removes finished sounds, steps streamed sounds, and does similar tasks.
	 * Should not be called while paused.
	 * @param dtime In seconds.
	 */
	virtual void step(f32 dtime) = 0;
	/**
	 * Pause all sound playback.
	 */
	virtual void pauseAll() = 0;
	/**
	 * Resume sound playback after pause.
	 */
	virtual void resumeAll() = 0;

	/**
	 * @param pos In node-space.
	 * @param vel In node-space.
	 * @param at Vector in node-space pointing forwards.
	 * @param up Vector in node-space pointing upwards, orthogonal to `at`.
	 */
	virtual void updateListener(const v3f &pos, const v3f &vel, const v3f &at,
			const v3f &up) = 0;
	virtual void setListenerGain(f32 gain) = 0;

	/**
	 * Adds a sound to load from a file (only OggVorbis).
	 * @param name The name of the sound. Must be unique, otherwise call fails.
	 * @param filepath The path for
	 * @return true on success, false on failure (ie. sound was already added or
	 *         file does not exist).
	 */
	virtual bool loadSoundFile(const std::string &name, const std::string &filepath) = 0;
	/**
	 * Same as `loadSoundFile`, but reads the OggVorbis file from memory.
	 */
	virtual bool loadSoundData(const std::string &name, std::string &&filedata) = 0;
	/**
	 * Adds sound with name sound_name to group `group_name`. Creates the group
	 * if non-existent.
	 * @param sound_name The name of the sound, as used in `loadSoundData`.
	 * @param group_name The name of the sound group.
	 */
	virtual void addSoundToGroup(const std::string &sound_name,
			const std::string &group_name) = 0;

	/**
	 * Plays a random sound from a sound group (position-less).
	 * @param id Id for new sound. Move semantics apply if id > 0.
	 */
	virtual void playSound(sound_handle_t id, const SoundSpec &spec) = 0;
	/**
	 * Same as `playSound`, but at a position.
	 * @param pos In node-space.
	 * @param vel In node-space.
	 */
	virtual void playSoundAt(sound_handle_t id, const SoundSpec &spec, const v3f &pos,
			const v3f &vel) = 0;
	/**
	 * Request the sound to be stopped.
	 * The id should be freed afterwards.
	 */
	virtual void stopSound(sound_handle_t sound) = 0;
	virtual void fadeSound(sound_handle_t sound, f32 step, f32 target_gain) = 0;
	/**
	 * Update position and velocity of positional sound.
	 * @param pos In node-space.
	 * @param vel In node-space.
	 */
	virtual void updateSoundPosVel(sound_handle_t sound, const v3f &pos,
			const v3f &vel) = 0;

	/**
	 * Get and reset the list of sounds that were stopped.
	 */
	std::vector<sound_handle_t> pollRemovedSounds()
	{
		std::vector<sound_handle_t> ret;
		std::swap(m_removed_sounds, ret);
		return ret;
	}

	/**
	 * Returns a positive id.
	 * The id will be returned again until freeId is called.
	 * @param num_owners Owner-counter for id. Set this to 2, if you want to play
	 *                   a sound and store the id also otherwhere.
	 */
	sound_handle_t allocateId(u32 num_owners);

	/**
	 * Free an id allocated via allocateId.
	 * @param num_owners How much the owner-counter should be decreased. Id can
	 *                   be reused when counter reaches 0.
	 */
	void freeId(sound_handle_t id, u32 num_owners = 1);
};

class DummySoundManager final : public ISoundManager
{
public:
	void step(f32 dtime) override {}
	void pauseAll() override {}
	void resumeAll() override {}

	void updateListener(const v3f &pos, const v3f &vel, const v3f &at, const v3f &up) override {}
	void setListenerGain(f32 gain) override {}

	bool loadSoundFile(const std::string &name, const std::string &filepath) override { return true; }
	bool loadSoundData(const std::string &name, std::string &&filedata) override { return true; }
	void addSoundToGroup(const std::string &sound_name, const std::string &group_name) override {};

	void playSound(sound_handle_t id, const SoundSpec &spec) override { reportRemovedSound(id); }
	void playSoundAt(sound_handle_t id, const SoundSpec &spec, const v3f &pos,
			const v3f &vel) override { reportRemovedSound(id); }
	void stopSound(sound_handle_t sound) override {}
	void fadeSound(sound_handle_t sound, f32 step, f32 target_gain) override {}
	void updateSoundPosVel(sound_handle_t sound, const v3f &pos, const v3f &vel) override {}
};

/**
 * A helper function to control sound volume based on some values: sound volume
 * settings, mute sound setting, and window activity.
 */
void sound_volume_control(ISoundManager *sound_mgr, bool is_window_active);
