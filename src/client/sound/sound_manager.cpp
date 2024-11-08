// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 DS
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2011 Sebastian 'Bahamada' Rühl
// Copyright (C) 2011 Cyriaque 'Cisoun' Skrapits <cysoun@gmail.com>
// Copyright (C) 2011 Giuseppe Bilotta <giuseppe.bilotta@gmail.com>

#include "sound_manager.h"

#include "sound_singleton.h"
#include "util/numeric.h" // myrand()
#include "util/tracy_wrapper.h"
#include "filesys.h"
#include "porting.h"

#include <limits>

namespace sound {

void OpenALSoundManager::stepStreams(f32 dtime)
{
	// spread work across steps
	const size_t num_issued_sounds = std::min(
			m_sounds_streaming_current_bigstep.size(),
			(size_t)std::ceil(m_sounds_streaming_current_bigstep.size()
				* dtime / m_stream_timer)
		);

	for (size_t i = 0; i < num_issued_sounds; ++i) {
		auto wptr = std::move(m_sounds_streaming_current_bigstep.back());
		m_sounds_streaming_current_bigstep.pop_back();

		std::shared_ptr<PlayingSound> snd = wptr.lock();
		if (!snd)
			continue;

		if (!snd->stepStream())
			continue;

		// sound still lives and needs more stream-stepping => add to next bigstep
		m_sounds_streaming_next_bigstep.push_back(std::move(wptr));
	}

	m_stream_timer -= dtime;
	if (m_stream_timer <= 0.0f) {
		m_stream_timer = STREAM_BIGSTEP_TIME;
		using std::swap;
		swap(m_sounds_streaming_current_bigstep, m_sounds_streaming_next_bigstep);
	}
}

void OpenALSoundManager::doFades(f32 dtime)
{
	for (size_t i = 0; i < m_sounds_fading.size();) {
		std::shared_ptr<PlayingSound> snd = m_sounds_fading[i].lock();
		if (snd) {
			if (snd->doFade(dtime)) {
				// needs more fading later, keep in m_sounds_fading
				++i;
				continue;
			}
		}

		// sound no longer needs to be faded
		m_sounds_fading[i] = std::move(m_sounds_fading.back());
		m_sounds_fading.pop_back();
		// continue with same i
	}
}

std::shared_ptr<ISoundDataOpen> OpenALSoundManager::openSingleSound(const std::string &sound_name)
{
	// if already open, nothing to do
	auto it = m_sound_datas_open.find(sound_name);
	if (it != m_sound_datas_open.end())
		return it->second;

	// find unopened data
	auto it_unopen = m_sound_datas_unopen.find(sound_name);
	if (it_unopen == m_sound_datas_unopen.end())
		return nullptr;
	std::unique_ptr<ISoundDataUnopen> unopn_snd = std::move(it_unopen->second);
	m_sound_datas_unopen.erase(it_unopen);

	// open
	std::shared_ptr<ISoundDataOpen> opn_snd = std::move(*unopn_snd).open(sound_name);
	if (!opn_snd)
		return nullptr;
	m_sound_datas_open.emplace(sound_name, opn_snd);
	return opn_snd;
}

std::string OpenALSoundManager::getLoadedSoundNameFromGroup(const std::string &group_name)
{
	std::string chosen_sound_name = "";

	auto it_groups = m_sound_groups.find(group_name);
	if (it_groups == m_sound_groups.end())
		return "";

	std::vector<std::string> &group_sounds = it_groups->second;
	while (!group_sounds.empty()) {
		// choose one by random
		int j = myrand() % group_sounds.size();
		chosen_sound_name = group_sounds[j];

		// find chosen one
		std::shared_ptr<ISoundDataOpen> snd = openSingleSound(chosen_sound_name);
		if (snd)
			return chosen_sound_name;

		// it doesn't exist
		// remove it from the group and try again
		group_sounds[j] = std::move(group_sounds.back());
		group_sounds.pop_back();
	}

	return "";
}

std::string OpenALSoundManager::getOrLoadLoadedSoundNameFromGroup(const std::string &group_name)
{
	std::string sound_name = getLoadedSoundNameFromGroup(group_name);
	if (!sound_name.empty())
		return sound_name;

	// load
	std::vector<std::string> paths = m_fallback_path_provider
			->getLocalFallbackPathsForSoundname(group_name);
	for (const std::string &path : paths) {
		if (loadSoundFile(path, path))
			addSoundToGroup(path, group_name);
	}
	return getLoadedSoundNameFromGroup(group_name);
}

std::shared_ptr<PlayingSound> OpenALSoundManager::createPlayingSound(
		const std::string &sound_name, bool loop, f32 volume, f32 pitch,
		f32 start_time, const std::optional<std::pair<v3f, v3f>> &pos_vel_opt)
{
	infostream << "OpenALSoundManager: Creating playing sound \"" << sound_name
			<< "\"" << std::endl;
	warn_if_al_error("before createPlayingSound");

	std::shared_ptr<ISoundDataOpen> lsnd = openSingleSound(sound_name);
	if (!lsnd) {
		// does not happen because of the call to getLoadedSoundNameFromGroup
		errorstream << "OpenALSoundManager::createPlayingSound: Sound \""
				<< sound_name << "\" disappeared." << std::endl;
		return nullptr;
	}

	if (lsnd->m_decode_info.is_stereo && pos_vel_opt.has_value()
			&& m_warned_positional_stereo_sounds.find(sound_name)
					== m_warned_positional_stereo_sounds.end()) {
		warningstream << "OpenALSoundManager::createPlayingSound: "
				<< "Creating positional stereo sound \"" << sound_name << "\"."
				<< std::endl;
		m_warned_positional_stereo_sounds.insert(sound_name);
	}

	ALuint source_id;
	alGenSources(1, &source_id);
	if (warn_if_al_error("createPlayingSound (alGenSources)") != AL_NO_ERROR) {
		// happens ie. if there are too many sources (out of memory)
		return nullptr;
	}

	auto sound = std::make_shared<PlayingSound>(source_id, std::move(lsnd), loop,
			volume, pitch, start_time, pos_vel_opt, m_exts);

	sound->play();

	warn_if_al_error("createPlayingSound");
	return sound;
}

void OpenALSoundManager::playSoundGeneric(sound_handle_t id, const std::string &group_name,
		bool loop, f32 volume, f32 fade, f32 pitch, bool use_local_fallback,
		f32 start_time, const std::optional<std::pair<v3f, v3f>> &pos_vel_opt)
{
	assert(id != 0);

	if (group_name.empty()) {
		reportRemovedSound(id);
		return;
	}

	// choose random sound name from group name
	std::string sound_name = use_local_fallback ?
			getOrLoadLoadedSoundNameFromGroup(group_name) :
			getLoadedSoundNameFromGroup(group_name);
	if (sound_name.empty()) {
		infostream << "OpenALSoundManager: \"" << group_name << "\" not found."
				<< std::endl;
		reportRemovedSound(id);
		return;
	}

	volume = std::max(0.0f, volume);
	f32 target_fade_volume = volume;
	if (fade > 0.0f)
		volume = 0.0f;

	if (!(pitch > 0.0f)) {
		warningstream << "OpenALSoundManager::playSoundGeneric: Illegal pitch value: "
				<< start_time << std::endl;
		pitch = 1.0f;
	}

	if (!std::isfinite(start_time)) {
		warningstream << "OpenALSoundManager::playSoundGeneric: Illegal start_time value: "
				<< start_time << std::endl;
		start_time = 0.0f;
	}

	// play it
	std::shared_ptr<PlayingSound> sound = createPlayingSound(sound_name, loop,
			volume, pitch, start_time, pos_vel_opt);
	if (!sound) {
		reportRemovedSound(id);
		return;
	}

	// add to streaming sounds if streaming
	if (sound->isStreaming())
		m_sounds_streaming_next_bigstep.push_back(sound);

	m_sounds_playing.emplace(id, std::move(sound));

	if (fade > 0.0f)
		fadeSound(id, fade, target_fade_volume);
}

int OpenALSoundManager::removeDeadSounds()
{
	int num_deleted_sounds = 0;

	for (auto it = m_sounds_playing.begin(); it != m_sounds_playing.end();) {
		sound_handle_t id = it->first;
		PlayingSound &sound = *it->second;
		// If dead, remove it
		if (sound.isDead()) {
			it = m_sounds_playing.erase(it);
			reportRemovedSound(id);
			++num_deleted_sounds;
		} else {
			++it;
		}
	}

	return num_deleted_sounds;
}

OpenALSoundManager::OpenALSoundManager(SoundManagerSingleton *smg,
		std::unique_ptr<SoundFallbackPathProvider> fallback_path_provider) :
	Thread("OpenALSoundManager"),
	m_fallback_path_provider(std::move(fallback_path_provider)),
	m_device(smg->m_device.get()),
	m_context(smg->m_context.get()),
	m_exts(m_device)
{
	SANITY_CHECK(!!m_fallback_path_provider);

	infostream << "Audio: Initialized: OpenAL " << std::endl;
}

OpenALSoundManager::~OpenALSoundManager()
{
	infostream << "Audio: Deinitializing..." << std::endl;
}

/* Interface */

void OpenALSoundManager::step(f32 dtime)
{
	m_time_until_dead_removal -= dtime;
	if (m_time_until_dead_removal <= 0.0f) {
		if (!m_sounds_playing.empty()) {
			verbosestream << "OpenALSoundManager::step(): "
					<< m_sounds_playing.size() << " playing sounds, "
					<< m_sound_datas_unopen.size() << " unopen sounds, "
					<< m_sound_datas_open.size() << " open sounds and "
					<< m_sound_groups.size() << " sound groups loaded."
					<< std::endl;
		}

		int num_deleted_sounds = removeDeadSounds();

		if (num_deleted_sounds != 0)
			verbosestream << "OpenALSoundManager::step(): Deleted "
					<< num_deleted_sounds << " dead playing sounds." << std::endl;

		m_time_until_dead_removal = REMOVE_DEAD_SOUNDS_INTERVAL;
	}

	doFades(dtime);
	stepStreams(dtime);
}

void OpenALSoundManager::pauseAll()
{
	for (auto &snd_p : m_sounds_playing) {
		PlayingSound &snd = *snd_p.second;
		snd.pause();
	}
	m_is_paused = true;
}

void OpenALSoundManager::resumeAll()
{
	for (auto &snd_p : m_sounds_playing) {
		PlayingSound &snd = *snd_p.second;
		snd.resume();
	}
	m_is_paused = false;
}

void OpenALSoundManager::updateListener(const v3f &pos_, const v3f &vel_,
		const v3f &at_, const v3f &up_)
{
	v3f pos = swap_handedness(pos_);
	v3f vel = swap_handedness(vel_);
	v3f at = swap_handedness(at_);
	v3f up = swap_handedness(up_);
	ALfloat orientation[6] = {at.X, at.Y, at.Z, up.X, up.Y, up.Z};

	alListener3f(AL_POSITION, pos.X, pos.Y, pos.Z);
	alListener3f(AL_VELOCITY, vel.X, vel.Y, vel.Z);
	alListenerfv(AL_ORIENTATION, orientation);
	warn_if_al_error("updateListener");
}

void OpenALSoundManager::setListenerGain(f32 gain)
{
#if defined(__APPLE__)
	/* macOS OpenAL implementation ignore setting AL_GAIN to zero
	 * so we use smallest possible value
	 */
	if (gain == 0.0f)
		gain = std::numeric_limits<f32>::min();
#endif
	alListenerf(AL_GAIN, gain);
}

bool OpenALSoundManager::loadSoundFile(const std::string &name, const std::string &filepath)
{
	// do not add twice
	if (m_sound_datas_open.count(name) != 0 || m_sound_datas_unopen.count(name) != 0)
		return false;

	// coarse check
	if (!fs::IsFile(filepath))
		return false;

	loadSoundFileNoCheck(name, filepath);
	return true;
}

bool OpenALSoundManager::loadSoundData(const std::string &name, std::string &&filedata)
{
	// do not add twice
	if (m_sound_datas_open.count(name) != 0 || m_sound_datas_unopen.count(name) != 0)
		return false;

	loadSoundDataNoCheck(name, std::move(filedata));
	return true;
}

void OpenALSoundManager::loadSoundFileNoCheck(const std::string &name, const std::string &filepath)
{
	// remember for lazy loading
	m_sound_datas_unopen.emplace(name, std::make_unique<SoundDataUnopenFile>(filepath));
}

void OpenALSoundManager::loadSoundDataNoCheck(const std::string &name, std::string &&filedata)
{
	// remember for lazy loading
	m_sound_datas_unopen.emplace(name, std::make_unique<SoundDataUnopenBuffer>(std::move(filedata)));
}

void OpenALSoundManager::addSoundToGroup(const std::string &sound_name, const std::string &group_name)
{
	auto it_groups = m_sound_groups.find(group_name);
	if (it_groups != m_sound_groups.end())
		it_groups->second.push_back(sound_name);
	else
		m_sound_groups.emplace(group_name, std::vector<std::string>{sound_name});
}

void OpenALSoundManager::playSound(sound_handle_t id, const SoundSpec &spec)
{
	return playSoundGeneric(id, spec.name, spec.loop, spec.gain, spec.fade, spec.pitch,
			spec.use_local_fallback, spec.start_time, std::nullopt);
}

void OpenALSoundManager::playSoundAt(sound_handle_t id, const SoundSpec &spec,
		const v3f &pos_, const v3f &vel_)
{
	std::optional<std::pair<v3f, v3f>> pos_vel_opt({
			swap_handedness(pos_),
			swap_handedness(vel_)
		});

	return playSoundGeneric(id, spec.name, spec.loop, spec.gain, spec.fade, spec.pitch,
			spec.use_local_fallback, spec.start_time, pos_vel_opt);
}

void OpenALSoundManager::stopSound(sound_handle_t sound)
{
	m_sounds_playing.erase(sound);
	reportRemovedSound(sound);
}

void OpenALSoundManager::fadeSound(sound_handle_t soundid, f32 step, f32 target_gain)
{
	// Ignore the command if step isn't valid.
	if (step == 0.0f)
		return;
	auto sound_it = m_sounds_playing.find(soundid);
	if (sound_it == m_sounds_playing.end())
		return; // No sound to fade
	PlayingSound &sound = *sound_it->second;
	if (sound.fade(step, target_gain))
		m_sounds_fading.emplace_back(sound_it->second);
}

void OpenALSoundManager::updateSoundPosVel(sound_handle_t id, const v3f &pos_,
		const v3f &vel_)
{
	v3f pos = swap_handedness(pos_);
	v3f vel = swap_handedness(vel_);

	auto i = m_sounds_playing.find(id);
	if (i == m_sounds_playing.end())
		return;
	i->second->updatePosVel(pos, vel);
}

/* Thread stuff */

void *OpenALSoundManager::run()
{
	using namespace sound_manager_messages_to_mgr;

	struct MsgVisitor {
		enum class Result { Ok, Empty, StopRequested };

		OpenALSoundManager &mgr;

		Result operator()(std::monostate &&) {
			return Result::Empty; }

		Result operator()(PauseAll &&) {
			mgr.pauseAll(); return Result::Ok; }
		Result operator()(ResumeAll &&) {
			mgr.resumeAll(); return Result::Ok; }

		Result operator()(UpdateListener &&msg) {
			mgr.updateListener(msg.pos_, msg.vel_, msg.at_, msg.up_); return Result::Ok; }
		Result operator()(SetListenerGain &&msg) {
			mgr.setListenerGain(msg.gain); return Result::Ok; }

		Result operator()(LoadSoundFile &&msg) {
			mgr.loadSoundFileNoCheck(msg.name, msg.filepath); return Result::Ok; }
		Result operator()(LoadSoundData &&msg) {
			mgr.loadSoundDataNoCheck(msg.name, std::move(msg.filedata)); return Result::Ok; }
		Result operator()(AddSoundToGroup &&msg) {
			mgr.addSoundToGroup(msg.sound_name, msg.group_name); return Result::Ok; }

		Result operator()(PlaySound &&msg) {
			mgr.playSound(msg.id, msg.spec); return Result::Ok; }
		Result operator()(PlaySoundAt &&msg) {
			mgr.playSoundAt(msg.id, msg.spec, msg.pos_, msg.vel_); return Result::Ok; }
		Result operator()(StopSound &&msg) {
			mgr.stopSound(msg.sound); return Result::Ok; }
		Result operator()(FadeSound &&msg) {
			mgr.fadeSound(msg.soundid, msg.step, msg.target_gain); return Result::Ok; }
		Result operator()(UpdateSoundPosVel &&msg) {
			mgr.updateSoundPosVel(msg.sound, msg.pos_, msg.vel_); return Result::Ok; }

		Result operator()(PleaseStop &&msg) {
			return Result::StopRequested; }
	};

	u64 t_step_start = porting::getTimeMs();
	while (true) {
		auto framemarker = FrameMarker("OpenALSoundManager::run()-frame").started();

		auto get_time_since_last_step = [&] {
			return (f32)(porting::getTimeMs() - t_step_start);
		};
		auto get_remaining_timeout = [&] {
			return (s32)((1.0e3f * SOUNDTHREAD_DTIME) - get_time_since_last_step());
		};

		bool stop_requested = false;

		while (true) {
			SoundManagerMsgToMgr msg =
					m_queue_to_mgr.pop_frontNoEx(std::max(get_remaining_timeout(), 0));

			MsgVisitor::Result res = std::visit(MsgVisitor{*this}, std::move(msg));

			if (res == MsgVisitor::Result::Empty && get_remaining_timeout() <= 0) {
				break; // finished sleeping
			} else if (res == MsgVisitor::Result::StopRequested) {
				stop_requested = true;
				break;
			}
		}
		if (stop_requested)
			break;

		f32 dtime = get_time_since_last_step() * 1.0e-3f;
		t_step_start = porting::getTimeMs();
		step(dtime);
	}

	send(sound_manager_messages_to_proxy::Stopped{});

	return nullptr;
}

} // namespace sound
