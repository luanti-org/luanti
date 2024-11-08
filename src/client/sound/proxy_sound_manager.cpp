// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 DS

#include "proxy_sound_manager.h"

#include "filesys.h"

namespace sound {

ProxySoundManager::MsgResult ProxySoundManager::handleMsg(SoundManagerMsgToProxy &&msg)
{
	using namespace sound_manager_messages_to_proxy;

	return std::visit([&](auto &&msg) {
			using T = std::decay_t<decltype(msg)>;

			if constexpr (std::is_same_v<T, std::monostate>)
				return MsgResult::Empty;
			else if constexpr (std::is_same_v<T, ReportRemovedSound>)
				reportRemovedSound(msg.id);
			else if constexpr (std::is_same_v<T, Stopped>)
				return MsgResult::Stopped;

			return MsgResult::Ok;
		},
		std::move(msg));
}

ProxySoundManager::~ProxySoundManager()
{
	if (m_sound_manager.isRunning()) {
		send(sound_manager_messages_to_mgr::PleaseStop{});

		// recv until it stopped
		auto recv = [&]	{
			return m_sound_manager.m_queue_to_proxy.pop_frontNoEx();
		};

		while (true) {
			if (handleMsg(recv()) == MsgResult::Stopped)
				break;
		}

		// join
		m_sound_manager.stop();
		SANITY_CHECK(m_sound_manager.wait());
	}
}

void ProxySoundManager::step(f32 dtime)
{
	auto recv = [&]	{
		return m_sound_manager.m_queue_to_proxy.pop_frontNoEx(0);
	};

	while (true) {
		MsgResult res = handleMsg(recv());
		if (res == MsgResult::Empty)
			break;
		else if (res == MsgResult::Stopped)
			throw std::runtime_error("OpenALSoundManager stopped unexpectedly");
	}
}

void ProxySoundManager::pauseAll()
{
	send(sound_manager_messages_to_mgr::PauseAll{});
}

void ProxySoundManager::resumeAll()
{
	send(sound_manager_messages_to_mgr::ResumeAll{});
}

void ProxySoundManager::updateListener(const v3f &pos_, const v3f &vel_,
		const v3f &at_, const v3f &up_)
{
	send(sound_manager_messages_to_mgr::UpdateListener{pos_, vel_, at_, up_});
}

void ProxySoundManager::setListenerGain(f32 gain)
{
	send(sound_manager_messages_to_mgr::SetListenerGain{gain});
}

bool ProxySoundManager::loadSoundFile(const std::string &name,
		const std::string &filepath)
{
	// do not add twice
	if (m_known_sound_names.count(name) != 0)
		return false;

	// coarse check
	if (!fs::IsFile(filepath))
		return false;

	send(sound_manager_messages_to_mgr::LoadSoundFile{name, filepath});

	m_known_sound_names.insert(name);
	return true;
}

bool ProxySoundManager::loadSoundData(const std::string &name, std::string &&filedata)
{
	// do not add twice
	if (m_known_sound_names.count(name) != 0)
		return false;

	send(sound_manager_messages_to_mgr::LoadSoundData{name, std::move(filedata)});

	m_known_sound_names.insert(name);
	return true;
}

void ProxySoundManager::addSoundToGroup(const std::string &sound_name,
		const std::string &group_name)
{
	send(sound_manager_messages_to_mgr::AddSoundToGroup{sound_name, group_name});
}

void ProxySoundManager::playSound(sound_handle_t id, const SoundSpec &spec)
{
	if (id == 0)
		id = allocateId(1);
	send(sound_manager_messages_to_mgr::PlaySound{id, spec});
}

void ProxySoundManager::playSoundAt(sound_handle_t id, const SoundSpec &spec, const v3f &pos_,
		const v3f &vel_)
{
	if (id == 0)
		id = allocateId(1);
	send(sound_manager_messages_to_mgr::PlaySoundAt{id, spec, pos_, vel_});
}

void ProxySoundManager::stopSound(sound_handle_t sound)
{
	send(sound_manager_messages_to_mgr::StopSound{sound});
}

void ProxySoundManager::fadeSound(sound_handle_t soundid, f32 step, f32 target_gain)
{
	send(sound_manager_messages_to_mgr::FadeSound{soundid, step, target_gain});
}

void ProxySoundManager::updateSoundPosVel(sound_handle_t sound, const v3f &pos_, const v3f &vel_)
{
	send(sound_manager_messages_to_mgr::UpdateSoundPosVel{sound, pos_, vel_});
}

} // namespace sound
