// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 DS

#pragma once

#include "../sound.h"
#include "../../sound.h"
#include <variant>

namespace sound {

namespace sound_manager_messages_to_mgr {
	struct PauseAll {};
	struct ResumeAll {};

	struct UpdateListener { v3f pos_; v3f vel_; v3f at_; v3f up_; };
	struct SetListenerGain { f32 gain; };

	struct LoadSoundFile { std::string name; std::string filepath; };
	struct LoadSoundData { std::string name; std::string filedata; };
	struct AddSoundToGroup { std::string sound_name; std::string group_name; };

	struct PlaySound { sound_handle_t id; SoundSpec spec; };
	struct PlaySoundAt { sound_handle_t id; SoundSpec spec; v3f pos_; v3f vel_; };
	struct StopSound { sound_handle_t sound; };
	struct FadeSound { sound_handle_t soundid; f32 step; f32 target_gain; };
	struct UpdateSoundPosVel { sound_handle_t sound; v3f pos_; v3f vel_; };

	struct PleaseStop {};
}

using SoundManagerMsgToMgr = std::variant<
		std::monostate,

		sound_manager_messages_to_mgr::PauseAll,
		sound_manager_messages_to_mgr::ResumeAll,

		sound_manager_messages_to_mgr::UpdateListener,
		sound_manager_messages_to_mgr::SetListenerGain,

		sound_manager_messages_to_mgr::LoadSoundFile,
		sound_manager_messages_to_mgr::LoadSoundData,
		sound_manager_messages_to_mgr::AddSoundToGroup,

		sound_manager_messages_to_mgr::PlaySound,
		sound_manager_messages_to_mgr::PlaySoundAt,
		sound_manager_messages_to_mgr::StopSound,
		sound_manager_messages_to_mgr::FadeSound,
		sound_manager_messages_to_mgr::UpdateSoundPosVel,

		sound_manager_messages_to_mgr::PleaseStop
	>;

namespace sound_manager_messages_to_proxy {
	struct ReportRemovedSound { sound_handle_t id; };

	struct Stopped {};
}

using SoundManagerMsgToProxy = std::variant<
		std::monostate,

		sound_manager_messages_to_proxy::ReportRemovedSound,

		sound_manager_messages_to_proxy::Stopped
	>;

} // namespace sound
