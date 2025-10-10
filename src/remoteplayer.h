// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2016 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2014-2016 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "player.h"
#include "skyparams.h"
#include "lighting.h"
#include "network/networkprotocol.h" // session_t

class PlayerSAO;

enum RemotePlayerChatResult
{
	RPLAYER_CHATRESULT_OK,
	RPLAYER_CHATRESULT_FLOODING,
	RPLAYER_CHATRESULT_KICK,
};

/*
	Player on the server
*/
class RemotePlayer : public Player
{
	friend class PlayerDatabaseFiles;

public:
	RemotePlayer(const std::string &name, IItemDefManager *idef);
	virtual ~RemotePlayer();

	PlayerSAO *getPlayerSAO() { return m_sao; }
	void setPlayerSAO(PlayerSAO *sao) { m_sao = sao; }

	RemotePlayerChatResult canSendChatMessage();

	void setHotbarItemcount(s32 hotbar_itemcount)
	{
		hud_hotbar_itemcount = hotbar_itemcount;
	}

	s32 getHotbarItemcount() const { return hud_hotbar_itemcount; }

	void overrideDayNightRatio(bool do_override, float ratio)
	{
		m_day_night_ratio_do_override = do_override;
		m_day_night_ratio = ratio;
	}

	void getDayNightRatio(bool *do_override, float *ratio)
	{
		*do_override = m_day_night_ratio_do_override;
		*ratio = m_day_night_ratio;
	}

	void setHotbarImage(const std::string &name) { hud_hotbar_image = name; }

	const std::string &getHotbarImage() const { return hud_hotbar_image; }

	void setHotbarSelectedImage(const std::string &name)
	{
		hud_hotbar_selected_image = name;
	}

	const std::string &getHotbarSelectedImage() const
	{
		return hud_hotbar_selected_image;
	}

	void setSky(const SkyboxParams &skybox_params)
	{
		m_skybox_params = skybox_params;
	}

	const SkyboxParams &getSkyParams() const { return m_skybox_params; }

	void setSun(const SunParams &sun_params) { m_sun_params = sun_params; }

	const SunParams &getSunParams() const { return m_sun_params; }

	void setMoon(const MoonParams &moon_params) { m_moon_params = moon_params; }

	const MoonParams &getMoonParams() const { return m_moon_params; }

	void setStars(const StarParams &star_params) { m_star_params = star_params; }

	const StarParams &getStarParams() const { return m_star_params; }

	void setCloudParams(const CloudParams &cloud_params)
	{
		m_cloud_params = cloud_params;
	}

	const CloudParams &getCloudParams() const { return m_cloud_params; }

	bool checkModified() const { return m_dirty || inventory.checkModified(); }

	inline void setModified(const bool x) { m_dirty = x; }

	void setLocalAnimations(v2f frames[4], float frame_speed)
	{
		for (int i = 0; i < 4; i++)
			local_animations[i] = frames[i];
		local_animation_speed = frame_speed;
	}

	void getLocalAnimations(v2f *frames, float *frame_speed)
	{
		for (int i = 0; i < 4; i++)
			frames[i] = local_animations[i];
		*frame_speed = local_animation_speed;
	}

	void setLighting(const Lighting &lighting) { m_lighting = lighting; }

	const Lighting& getLighting() const { return m_lighting; }

	void setDirty(bool dirty) { m_dirty = true; }

	u16 protocol_version = 0;
	u16 formspec_version = 0;

	bool inventory_formspec_overridden = false;

	/// returns PEER_ID_INEXISTENT when PlayerSAO is either
	/// not yet present or about to be removed (e.g. leave).
	session_t getPeerId() const { return m_peer_id; }

	void setPeerId(session_t peer_id) { m_peer_id = peer_id; }

	void onSuccessfulSave();

private:
	PlayerSAO *m_sao = nullptr;
	bool m_dirty = false;

	static bool m_setting_cache_loaded;
	static float m_setting_chat_message_limit_per_10sec;
	static u16 m_setting_chat_message_limit_trigger_kick;

	u32 m_last_chat_message_sent = std::time(0);
	float m_chat_message_allowance = 5.0f;
	u16 m_message_rate_overhead = 0;

	bool m_day_night_ratio_do_override = false;
	float m_day_night_ratio;
	std::string hud_hotbar_image = "";
	std::string hud_hotbar_selected_image = "";

	CloudParams m_cloud_params;

	SkyboxParams m_skybox_params;
	SunParams m_sun_params;
	MoonParams m_moon_params;
	StarParams m_star_params;

	Lighting m_lighting;

	session_t m_peer_id = PEER_ID_INEXISTENT;
};
