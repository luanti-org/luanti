// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "serveropcodes.h"
#include "server.h"

const static ToServerCommandHandler null_command_handler =
	{ "TOSERVER_NULL", TOSERVER_STATE_ALL, &Server::handleCommand_Null };

const ToServerCommandHandler toServerCommandTable[TOSERVER_NUM_MSG_TYPES] =
{
	null_command_handler, // 0x00 (never use this)
	null_command_handler, // 0x01
	{ "TOSERVER_INIT",                     TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_Init }, // 0x02
	null_command_handler, // 0x03
	null_command_handler, // 0x04
	null_command_handler, // 0x05
	null_command_handler, // 0x06
	null_command_handler, // 0x07
	null_command_handler, // 0x08
	null_command_handler, // 0x09
	null_command_handler, // 0x0a
	null_command_handler, // 0x0b
	null_command_handler, // 0x0c
	null_command_handler, // 0x0d
	null_command_handler, // 0x0e
	null_command_handler, // 0x0f
	null_command_handler, // 0x10
	{ "TOSERVER_INIT2",                    TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_Init2 }, // 0x11
	null_command_handler, // 0x12
	null_command_handler, // 0x13
	null_command_handler, // 0x14
	null_command_handler, // 0x15
	null_command_handler, // 0x16
	{ "TOSERVER_MODCHANNEL_JOIN",          TOSERVER_STATE_INGAME, &Server::handleCommand_ModChannelJoin }, // 0x17
	{ "TOSERVER_MODCHANNEL_LEAVE",         TOSERVER_STATE_INGAME, &Server::handleCommand_ModChannelLeave }, // 0x18
	{ "TOSERVER_MODCHANNEL_MSG",           TOSERVER_STATE_INGAME, &Server::handleCommand_ModChannelMsg }, // 0x19
	null_command_handler, // 0x1a
	null_command_handler, // 0x1b
	null_command_handler, // 0x1c
	null_command_handler, // 0x1d
	null_command_handler, // 0x1e
	null_command_handler, // 0x1f
	null_command_handler, // 0x20
	null_command_handler, // 0x21
	null_command_handler, // 0x22
	{ "TOSERVER_PLAYERPOS",                TOSERVER_STATE_INGAME, &Server::handleCommand_PlayerPos }, // 0x23
	{ "TOSERVER_GOTBLOCKS",                TOSERVER_STATE_STARTUP, &Server::handleCommand_GotBlocks }, // 0x24
	{ "TOSERVER_DELETEDBLOCKS",            TOSERVER_STATE_INGAME, &Server::handleCommand_DeletedBlocks }, // 0x25
	null_command_handler, // 0x26
	null_command_handler, // 0x27
	null_command_handler, // 0x28
	null_command_handler, // 0x29
	null_command_handler, // 0x2a
	null_command_handler, // 0x2b
	null_command_handler, // 0x2c
	null_command_handler, // 0x2d
	null_command_handler, // 0x2e
	null_command_handler, // 0x2f
	null_command_handler, // 0x30
	{ "TOSERVER_INVENTORY_ACTION",         TOSERVER_STATE_INGAME, &Server::handleCommand_InventoryAction }, // 0x31
	{ "TOSERVER_CHAT_MESSAGE",             TOSERVER_STATE_INGAME, &Server::handleCommand_ChatMessage }, // 0x32
	null_command_handler, // 0x33
	null_command_handler, // 0x34
	{ "TOSERVER_DAMAGE",                   TOSERVER_STATE_INGAME, &Server::handleCommand_Damage }, // 0x35
	null_command_handler, // 0x36
	{ "TOSERVER_PLAYERITEM",               TOSERVER_STATE_INGAME, &Server::handleCommand_PlayerItem }, // 0x37
	null_command_handler, // 0x38
	{ "TOSERVER_INTERACT",                 TOSERVER_STATE_INGAME, &Server::handleCommand_Interact }, // 0x39
	{ "TOSERVER_REMOVED_SOUNDS",           TOSERVER_STATE_INGAME, &Server::handleCommand_RemovedSounds }, // 0x3a
	{ "TOSERVER_NODEMETA_FIELDS",          TOSERVER_STATE_INGAME, &Server::handleCommand_NodeMetaFields }, // 0x3b
	{ "TOSERVER_INVENTORY_FIELDS",         TOSERVER_STATE_INGAME, &Server::handleCommand_InventoryFields }, // 0x3c
	null_command_handler, // 0x3d
	null_command_handler, // 0x3e
	null_command_handler, // 0x3f
	{ "TOSERVER_REQUEST_MEDIA",            TOSERVER_STATE_STARTUP, &Server::handleCommand_RequestMedia }, // 0x40
	{ "TOSERVER_HAVE_MEDIA",               TOSERVER_STATE_INGAME, &Server::handleCommand_HaveMedia }, // 0x41
	null_command_handler, // 0x42
	{ "TOSERVER_CLIENT_READY",             TOSERVER_STATE_STARTUP, &Server::handleCommand_ClientReady }, // 0x43
	null_command_handler, // 0x44
	null_command_handler, // 0x45
	null_command_handler, // 0x46
	null_command_handler, // 0x47
	null_command_handler, // 0x48
	null_command_handler, // 0x49
	null_command_handler, // 0x4a
	null_command_handler, // 0x4b
	null_command_handler, // 0x4c
	null_command_handler, // 0x4d
	null_command_handler, // 0x4e
	null_command_handler, // 0x4f
	{ "TOSERVER_FIRST_SRP",                TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_FirstSrp }, // 0x50
	{ "TOSERVER_SRP_BYTES_A",              TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_SrpBytesA }, // 0x51
	{ "TOSERVER_SRP_BYTES_M",              TOSERVER_STATE_NOT_CONNECTED, &Server::handleCommand_SrpBytesM }, // 0x52
	{ "TOSERVER_UPDATE_CLIENT_INFO",       TOSERVER_STATE_INGAME, &Server::handleCommand_UpdateClientInfo }, // 0x53
};

const static ClientCommandFactory null_command_factory = { nullptr, 0, false };

/*
	Channels used for Server -> Client communication
	2: Bulk data (mapblocks, media, ...)
	1: HUD packets
	0: everything else

	Packet order is only guaranteed inside a channel, so packets that operate on
	the same objects are *required* to be in the same channel.
*/

const ClientCommandFactory clientCommandFactoryTable[TOCLIENT_NUM_MSG_TYPES] =
{
	null_command_factory, // 0x00
	null_command_factory, // 0x01
	{ "TOCLIENT_HELLO",                    0, true }, // 0x02
	{ "TOCLIENT_AUTH_ACCEPT",              0, true }, // 0x03
	{ "TOCLIENT_ACCEPT_SUDO_MODE",         0, true }, // 0x04
	{ "TOCLIENT_DENY_SUDO_MODE",           0, true }, // 0x05
	null_command_factory, // 0x06
	null_command_factory, // 0x07
	null_command_factory, // 0x08
	null_command_factory, // 0x09
	{ "TOCLIENT_ACCESS_DENIED",            0, true }, // 0x0A
	null_command_factory, // 0x0B
	null_command_factory, // 0x0C
	null_command_factory, // 0x0D
	null_command_factory, // 0x0E
	null_command_factory, // 0x0F
	null_command_factory, // 0x10
	null_command_factory, // 0x11
	null_command_factory, // 0x12
	null_command_factory, // 0x13
	null_command_factory, // 0x14
	null_command_factory, // 0x15
	null_command_factory, // 0x16
	null_command_factory, // 0x17
	null_command_factory, // 0x18
	null_command_factory, // 0x19
	null_command_factory, // 0x1A
	null_command_factory, // 0x1B
	null_command_factory, // 0x1C
	null_command_factory, // 0x1D
	null_command_factory, // 0x1E
	null_command_factory, // 0x1F
	{ "TOCLIENT_BLOCKDATA",                2, true }, // 0x20
	{ "TOCLIENT_ADDNODE",                  0, true }, // 0x21
	{ "TOCLIENT_REMOVENODE",               0, true }, // 0x22
	null_command_factory, // 0x23
	null_command_factory, // 0x24
	null_command_factory, // 0x25
	null_command_factory, // 0x26
	{ "TOCLIENT_INVENTORY",                0, true }, // 0x27
	null_command_factory, // 0x28
	{ "TOCLIENT_TIME_OF_DAY",              0, true }, // 0x29
	{ "TOCLIENT_CSM_RESTRICTION_FLAGS",    0, true }, // 0x2A
	{ "TOCLIENT_PLAYER_SPEED",             0, true }, // 0x2B
	{ "TOCLIENT_MEDIA_PUSH",               0, true }, // 0x2C (sent over channel 1 too if legacy)
	null_command_factory, // 0x2D
	null_command_factory, // 0x2E
	{ "TOCLIENT_CHAT_MESSAGE",             0, true }, // 0x2F
	null_command_factory, // 0x30
	{ "TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD", 0, true }, // 0x31
	{ "TOCLIENT_ACTIVE_OBJECT_MESSAGES",   0, true }, // 0x32 (may be sent as unrel over channel 1 too)
	{ "TOCLIENT_HP",                       0, true }, // 0x33
	{ "TOCLIENT_MOVE_PLAYER",              0, true }, // 0x34
	null_command_factory, // 0x35
	{ "TOCLIENT_FOV",                      0, true }, // 0x36
	null_command_factory, // 0x37
	{ "TOCLIENT_MEDIA",                    2, true }, // 0x38
	null_command_factory, // 0x39
	{ "TOCLIENT_NODEDEF",                  0, true }, // 0x3A
	null_command_factory, // 0x3B
	{ "TOCLIENT_ANNOUNCE_MEDIA",           0, true }, // 0x3C
	{ "TOCLIENT_ITEMDEF",                  0, true }, // 0x3D
	null_command_factory, // 0x3E
	{ "TOCLIENT_PLAY_SOUND",               0, true }, // 0x3f (may be sent as unrel too)
	{ "TOCLIENT_STOP_SOUND",               0, true }, // 0x40
	{ "TOCLIENT_PRIVILEGES",               0, true }, // 0x41
	{ "TOCLIENT_INVENTORY_FORMSPEC",       0, true }, // 0x42
	// ^ `channel` MUST be the same as TOCLIENT_SHOW_FORMSPEC
	{ "TOCLIENT_DETACHED_INVENTORY",       0, true }, // 0x43
	{ "TOCLIENT_SHOW_FORMSPEC",            0, true }, // 0x44
	{ "TOCLIENT_MOVEMENT",                 0, true }, // 0x45
	{ "TOCLIENT_SPAWN_PARTICLE",           0, true }, // 0x46
	{ "TOCLIENT_ADD_PARTICLESPAWNER",      0, true }, // 0x47
	{ "TOCLIENT_CAMERA",                   0, true }, // 0x48
	{ "TOCLIENT_HUDADD",                   1, true }, // 0x49
	{ "TOCLIENT_HUDRM",                    1, true }, // 0x4a
	{ "TOCLIENT_HUDCHANGE",                1, true }, // 0x4b
	{ "TOCLIENT_HUD_SET_FLAGS",            1, true }, // 0x4c
	{ "TOCLIENT_HUD_SET_PARAM",            1, true }, // 0x4d
	{ "TOCLIENT_BREATH",                   0, true }, // 0x4e
	{ "TOCLIENT_SET_SKY",                  0, true }, // 0x4f
	{ "TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO", 0, true }, // 0x50
	{ "TOCLIENT_LOCAL_PLAYER_ANIMATIONS",  0, true }, // 0x51
	{ "TOCLIENT_EYE_OFFSET",               0, true }, // 0x52
	{ "TOCLIENT_DELETE_PARTICLESPAWNER",   0, true }, // 0x53
	{ "TOCLIENT_CLOUD_PARAMS",             0, true }, // 0x54
	{ "TOCLIENT_FADE_SOUND",               0, true }, // 0x55
	{ "TOCLIENT_UPDATE_PLAYER_LIST",       0, true }, // 0x56
	{ "TOCLIENT_MODCHANNEL_MSG",           0, true }, // 0x57
	{ "TOCLIENT_MODCHANNEL_SIGNAL",        0, true }, // 0x58
	{ "TOCLIENT_NODEMETA_CHANGED",         0, true }, // 0x59
	{ "TOCLIENT_SET_SUN",                  0, true }, // 0x5a
	{ "TOCLIENT_SET_MOON",                 0, true }, // 0x5b
	{ "TOCLIENT_SET_STARS",                0, true }, // 0x5c
	{ "TOCLIENT_MOVE_PLAYER_REL",          0, true }, // 0x5d
	null_command_factory, // 0x5e
	null_command_factory, // 0x5f
	{ "TOCLIENT_SRP_BYTES_S_B",            0, true }, // 0x60
	{ "TOCLIENT_FORMSPEC_PREPEND",         0, true }, // 0x61
	{ "TOCLIENT_MINIMAP_MODES",            0, true }, // 0x62
	{ "TOCLIENT_SET_LIGHTING",             0, true }, // 0x63
	{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     0, true }, // 0x64
};
