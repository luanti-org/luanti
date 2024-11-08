// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include "network/networkprotocol.h"
#include "irrlichttypes.h"

enum ModChannelState : u8
{
	MODCHANNEL_STATE_INIT,
	MODCHANNEL_STATE_READ_WRITE,
	MODCHANNEL_STATE_READ_ONLY,
	MODCHANNEL_STATE_MAX,
};

class ModChannel
{
public:
	ModChannel(const std::string &name) : m_name(name) {}
	~ModChannel() = default;

	const std::string &getName() const { return m_name; }
	bool registerConsumer(session_t peer_id);
	bool removeConsumer(session_t peer_id);
	const std::vector<u16> &getChannelPeers() const { return m_client_consumers; }
	bool canWrite() const;
	void setState(ModChannelState state);

private:
	std::string m_name;
	ModChannelState m_state = MODCHANNEL_STATE_INIT;
	std::vector<u16> m_client_consumers;
};

enum ModChannelSignal : u8
{
	MODCHANNEL_SIGNAL_JOIN_OK,
	MODCHANNEL_SIGNAL_JOIN_FAILURE,
	MODCHANNEL_SIGNAL_LEAVE_OK,
	MODCHANNEL_SIGNAL_LEAVE_FAILURE,
	MODCHANNEL_SIGNAL_CHANNEL_NOT_REGISTERED,
	MODCHANNEL_SIGNAL_SET_STATE,
};

class ModChannelMgr
{
public:
	ModChannelMgr() = default;
	~ModChannelMgr() = default;

	void registerChannel(const std::string &channel);
	bool setChannelState(const std::string &channel, ModChannelState state);
	bool joinChannel(const std::string &channel, session_t peer_id);
	bool leaveChannel(const std::string &channel, session_t peer_id);
	bool channelRegistered(const std::string &channel) const;
	ModChannel *getModChannel(const std::string &channel);
	/**
	 * This function check if a local mod can write on the channel
	 *
	 * @param channel
	 * @return true if write is allowed
	 */
	bool canWriteOnChannel(const std::string &channel) const;
	void leaveAllChannels(session_t peer_id);
	const std::vector<u16> &getChannelPeers(const std::string &channel) const;

private:
	bool removeChannel(const std::string &channel);

	std::unordered_map<std::string, std::unique_ptr<ModChannel>>
			m_registered_channels;
};
