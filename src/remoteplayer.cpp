// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2016 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2014-2016 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "remoteplayer.h"
#include <json/json.h>
#include "filesys.h"
#include "gamedef.h"
#include "porting.h"  // strlcpy
#include "server.h"
#include "settings.h"
#include "convert_json.h"
#include "server/player_sao.h"

/*
	RemotePlayer
*/

// static config cache for remoteplayer
bool RemotePlayer::m_setting_cache_loaded = false;
float RemotePlayer::m_setting_chat_message_limit_per_10sec = 0.0f;
u16 RemotePlayer::m_setting_chat_message_limit_trigger_kick = 0;

RemotePlayer::RemotePlayer(const std::string &name, IItemDefManager *idef):
	Player(name, idef)
{
	if (!RemotePlayer::m_setting_cache_loaded) {
		RemotePlayer::m_setting_chat_message_limit_per_10sec =
			g_settings->getFloat("chat_message_limit_per_10sec");
		RemotePlayer::m_setting_chat_message_limit_trigger_kick =
			g_settings->getU16("chat_message_limit_trigger_kick");
		RemotePlayer::m_setting_cache_loaded = true;
	}

	movement_acceleration_default   = g_settings->getFloat("movement_acceleration_default")   * BS;
	movement_acceleration_air       = g_settings->getFloat("movement_acceleration_air")       * BS;
	movement_acceleration_fast      = g_settings->getFloat("movement_acceleration_fast")      * BS;
	movement_speed_walk             = g_settings->getFloat("movement_speed_walk")             * BS;
	movement_speed_crouch           = g_settings->getFloat("movement_speed_crouch")           * BS;
	movement_speed_fast             = g_settings->getFloat("movement_speed_fast")             * BS;
	movement_speed_climb            = g_settings->getFloat("movement_speed_climb")            * BS;
	movement_speed_jump             = g_settings->getFloat("movement_speed_jump")             * BS;
	movement_liquid_fluidity        = g_settings->getFloat("movement_liquid_fluidity")        * BS;
	movement_liquid_fluidity_smooth = g_settings->getFloat("movement_liquid_fluidity_smooth") * BS;
	movement_liquid_sink            = g_settings->getFloat("movement_liquid_sink")            * BS;
	movement_gravity                = g_settings->getFloat("movement_gravity")                * BS;

	// Skybox defaults:
	m_cloud_params  = SkyboxDefaults::getCloudDefaults();
	m_skybox_params = SkyboxDefaults::getSkyDefaults();
	m_sun_params    = SkyboxDefaults::getSunDefaults();
	m_moon_params   = SkyboxDefaults::getMoonDefaults();
	m_star_params   = SkyboxDefaults::getStarDefaults();
}

RemotePlayer::~RemotePlayer()
{
	if (m_sao)
		m_sao->setPlayer(nullptr);
}

RemotePlayerChatResult RemotePlayer::canSendChatMessage()
{
	// Rate limit messages
	u32 now = time(NULL);
	float time_passed = now - m_last_chat_message_sent;
	m_last_chat_message_sent = now;

	// If this feature is disabled
	if (m_setting_chat_message_limit_per_10sec <= 0.0) {
		return RPLAYER_CHATRESULT_OK;
	}

	m_chat_message_allowance += time_passed * (m_setting_chat_message_limit_per_10sec / 8.0f);
	if (m_chat_message_allowance > m_setting_chat_message_limit_per_10sec) {
		m_chat_message_allowance = m_setting_chat_message_limit_per_10sec;
	}

	if (m_chat_message_allowance < 1.0f) {
		infostream << "Player " << m_name
				<< " chat limited due to excessive message amount." << std::endl;

		// Kick player if flooding is too intensive
		m_message_rate_overhead++;
		if (m_message_rate_overhead > RemotePlayer::m_setting_chat_message_limit_trigger_kick) {
			return RPLAYER_CHATRESULT_KICK;
		}

		return RPLAYER_CHATRESULT_FLOODING;
	}

	// Reinit message overhead
	if (m_message_rate_overhead > 0) {
		m_message_rate_overhead = 0;
	}

	m_chat_message_allowance -= 1.0f;
	return RPLAYER_CHATRESULT_OK;
}

void RemotePlayer::onSuccessfulSave()
{
	setModified(false);
	if (m_sao)
		m_sao->getMeta().setModified(false);
}
