// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "client/client.h"
#include "networkprotocol.h"

class NetworkPacket;
// Note: don't forward-declare Client here (#14324)

enum ToClientConnectionState {
	TOCLIENT_STATE_NOT_CONNECTED,
	TOCLIENT_STATE_CONNECTED,
	TOCLIENT_STATE_ALL,
};

struct ToClientCommandHandler
{
	const char* name;
	ToClientConnectionState state;
	void (Client::*handler)(NetworkPacket* pkt);
};

struct ServerCommandFactory
{
	const char* name;
	u8 channel;
	bool reliable;
};

extern const ToClientCommandHandler toClientCommandTable[TOCLIENT_NUM_MSG_TYPES];

extern const ServerCommandFactory serverCommandFactoryTable[TOSERVER_NUM_MSG_TYPES];
