// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "modchannels.h"
#include <algorithm>
#include <cassert>
#include "util/basic_macros.h"

bool ModChannel::registerConsumer(session_t peer_id)
{

	// ignore if peer_id already joined
	if (CONTAINS(m_client_consumers, peer_id))
		return false;

	m_client_consumers.push_back(peer_id);
	return true;
}

bool ModChannel::removeConsumer(session_t peer_id)
{
	bool found = false;
	auto peer_removal_fct = [peer_id, &found](u16 p) {
		if (p == peer_id)
			found = true;

		return p == peer_id;
	};

	m_client_consumers.erase(
			std::remove_if(m_client_consumers.begin(),
					m_client_consumers.end(), peer_removal_fct),
			m_client_consumers.end());

	return found;
}

bool ModChannel::canWrite() const
{
	return m_state == MODCHANNEL_STATE_READ_WRITE;
}

void ModChannel::setState(ModChannelState state)
{
	assert(state != MODCHANNEL_STATE_INIT);

	m_state = state;
}

bool ModChannelMgr::channelRegistered(const std::string &channel) const
{
	return m_registered_channels.find(channel) != m_registered_channels.end();
}

ModChannel *ModChannelMgr::getModChannel(const std::string &channel)
{
	if (!channelRegistered(channel))
		return nullptr;

	return m_registered_channels[channel].get();
}

bool ModChannelMgr::canWriteOnChannel(const std::string &channel) const
{
	const auto channel_it = m_registered_channels.find(channel);
	if (channel_it == m_registered_channels.end()) {
		return false;
	}

	return channel_it->second->canWrite();
}

void ModChannelMgr::registerChannel(const std::string &channel)
{
	m_registered_channels[channel] = std::make_unique<ModChannel>(channel);
}

bool ModChannelMgr::setChannelState(const std::string &channel, ModChannelState state)
{
	if (!channelRegistered(channel))
		return false;

	auto channel_it = m_registered_channels.find(channel);
	channel_it->second->setState(state);

	return true;
}

bool ModChannelMgr::removeChannel(const std::string &channel)
{
	if (!channelRegistered(channel))
		return false;

	m_registered_channels.erase(channel);
	return true;
}

bool ModChannelMgr::joinChannel(const std::string &channel, session_t peer_id)
{
	if (!channelRegistered(channel))
		registerChannel(channel);

	return m_registered_channels[channel]->registerConsumer(peer_id);
}

bool ModChannelMgr::leaveChannel(const std::string &channel, session_t peer_id)
{
	if (!channelRegistered(channel))
		return false;

	// Remove consumer from channel
	bool consumerRemoved = m_registered_channels[channel]->removeConsumer(peer_id);

	// If channel is empty, remove it
	if (m_registered_channels[channel]->getChannelPeers().empty()) {
		removeChannel(channel);
	}
	return consumerRemoved;
}

void ModChannelMgr::leaveAllChannels(session_t peer_id)
{
	for (auto &channel_it : m_registered_channels)
		channel_it.second->removeConsumer(peer_id);
}

static std::vector<u16> empty_channel_list;
const std::vector<u16> &ModChannelMgr::getChannelPeers(const std::string &channel) const
{
	const auto &channel_it = m_registered_channels.find(channel);
	if (channel_it == m_registered_channels.end())
		return empty_channel_list;

	return channel_it->second->getChannelPeers();
}
