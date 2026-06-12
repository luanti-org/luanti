// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "network/networkprotocol.h"

class NetworkPacket;
// Forward-declare Server here. The Server::* member pointer used to live in
// this header; it has since moved to the packet dispatcher, so no include is
// required (#14324).

enum ToServerConnectionState {
	TOSERVER_STATE_NOT_CONNECTED,
	TOSERVER_STATE_STARTUP,
	TOSERVER_STATE_INGAME,
	TOSERVER_STATE_ALL,
};
struct ToServerCommandHandler
{
	const char *name;
	ToServerConnectionState state;
};

struct ClientCommandFactory
{
	const char* name;
	u8 channel;
	bool reliable;
};

extern const ToServerCommandHandler toServerCommandTable[TOSERVER_NUM_MSG_TYPES];

extern const ClientCommandFactory clientCommandFactoryTable[TOCLIENT_NUM_MSG_TYPES];
