// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

#include "clientopcodes.h"
#include "client/client.h"

const static ToClientCommandHandler null_command_handler =
	{"TOCLIENT_NULL", TOCLIENT_STATE_ALL, &Client::handleCommand_Null};

const ToClientCommandHandler toClientCommandTable[TOCLIENT_NUM_MSG_TYPES] =
{
	null_command_handler, // 0x00 (never use this)
	null_command_handler, // 0x01
	{ "TOCLIENT_HELLO",                   TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_Hello }, // 0x02
	{ "TOCLIENT_AUTH_ACCEPT",             TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_AuthAccept }, // 0x03
	{ "TOCLIENT_ACCEPT_SUDO_MODE",        TOCLIENT_STATE_CONNECTED, &Client::handleCommand_AcceptSudoMode}, // 0x04
	{ "TOCLIENT_DENY_SUDO_MODE",          TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DenySudoMode}, // 0x05
	null_command_handler, // 0x06
	null_command_handler, // 0x07
	null_command_handler, // 0x08
	null_command_handler, // 0x09
	{ "TOCLIENT_ACCESS_DENIED",           TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_AccessDenied }, // 0x0A
	null_command_handler, // 0x0B
	null_command_handler, // 0x0C
	null_command_handler, // 0x0D
	null_command_handler, // 0x0E
	null_command_handler, // 0x0F
	null_command_handler, // 0x10
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	{ "TOCLIENT_BLOCKDATA",                TOCLIENT_STATE_CONNECTED, &Client::handleCommand_BlockData }, // 0x20
	{ "TOCLIENT_ADDNODE",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_AddNode }, // 0x21
	{ "TOCLIENT_REMOVENODE",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_RemoveNode }, // 0x22
	null_command_handler,
	null_command_handler,
	null_command_handler,
	null_command_handler,
	{ "TOCLIENT_INVENTORY",                TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Inventory }, // 0x27
	null_command_handler,
	{ "TOCLIENT_TIME_OF_DAY",              TOCLIENT_STATE_CONNECTED, &Client::handleCommand_TimeOfDay }, // 0x29
	{ "TOCLIENT_CSM_RESTRICTION_FLAGS",    TOCLIENT_STATE_CONNECTED, &Client::handleCommand_CSMRestrictionFlags }, // 0x2A
	{ "TOCLIENT_PLAYER_SPEED",             TOCLIENT_STATE_CONNECTED, &Client::handleCommand_PlayerSpeed }, // 0x2B
	{ "TOCLIENT_MEDIA_PUSH",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_MediaPush }, // 0x2C
	null_command_handler,
	null_command_handler,
	{ "TOCLIENT_CHAT_MESSAGE",             TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ChatMessage }, // 0x2F
	null_command_handler, // 0x30
	{ "TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ActiveObjectRemoveAdd }, // 0x31
	{ "TOCLIENT_ACTIVE_OBJECT_MESSAGES",   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ActiveObjectMessages }, // 0x32
	{ "TOCLIENT_HP",                       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HP }, // 0x33
	{ "TOCLIENT_MOVE_PLAYER",              TOCLIENT_STATE_CONNECTED, &Client::handleCommand_MovePlayer }, // 0x34
	{ "TOCLIENT_ACCESS_DENIED_LEGACY",     TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_AccessDenied }, // 0x35
	{ "TOCLIENT_FOV",                      TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Fov }, // 0x36
	{ "TOCLIENT_DEATHSCREEN_LEGACY",       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DeathScreenLegacy }, // 0x37
	{ "TOCLIENT_MEDIA",                    TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Media }, // 0x38
	null_command_handler,
	{ "TOCLIENT_NODEDEF",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NodeDef }, // 0x3a
	null_command_handler,
	{ "TOCLIENT_ANNOUNCE_MEDIA",           TOCLIENT_STATE_CONNECTED, &Client::handleCommand_AnnounceMedia }, // 0x3c
	{ "TOCLIENT_ITEMDEF",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ItemDef }, // 0x3d
	null_command_handler,
	{ "TOCLIENT_PLAY_SOUND",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_PlaySound }, // 0x3f
	{ "TOCLIENT_STOP_SOUND",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_StopSound }, // 0x40
	{ "TOCLIENT_PRIVILEGES",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Privileges }, // 0x41
	{ "TOCLIENT_INVENTORY_FORMSPEC",       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_InventoryFormSpec }, // 0x42
	{ "TOCLIENT_DETACHED_INVENTORY",       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DetachedInventory }, // 0x43
	{ "TOCLIENT_SHOW_FORMSPEC",            TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ShowFormSpec }, // 0x44
	{ "TOCLIENT_MOVEMENT",                 TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Movement }, // 0x45
	{ "TOCLIENT_SPAWN_PARTICLE",           TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticle }, // 0x46
	{ "TOCLIENT_ADD_PARTICLESPAWNER",      TOCLIENT_STATE_CONNECTED, &Client::handleCommand_AddParticleSpawner }, // 0x47
	{ "TOCLIENT_CAMERA",                   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Camera }, // 0x48
	{ "TOCLIENT_HUDADD",                   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudAdd }, // 0x49
	{ "TOCLIENT_HUDRM",                    TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudRemove }, // 0x4a
	{ "TOCLIENT_HUDCHANGE",                TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudChange }, // 0x4b
	{ "TOCLIENT_HUD_SET_FLAGS",            TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetFlags }, // 0x4c
	{ "TOCLIENT_HUD_SET_PARAM",            TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetParam }, // 0x4d
	{ "TOCLIENT_BREATH",                   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_Breath }, // 0x4e
	{ "TOCLIENT_SET_SKY",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetSky }, // 0x4f
	{ "TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_OverrideDayNightRatio }, // 0x50
	{ "TOCLIENT_LOCAL_PLAYER_ANIMATIONS",  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_LocalPlayerAnimations }, // 0x51
	{ "TOCLIENT_EYE_OFFSET",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_EyeOffset }, // 0x52
	{ "TOCLIENT_DELETE_PARTICLESPAWNER",   TOCLIENT_STATE_CONNECTED, &Client::handleCommand_DeleteParticleSpawner }, // 0x53
	{ "TOCLIENT_CLOUD_PARAMS",             TOCLIENT_STATE_CONNECTED, &Client::handleCommand_CloudParams }, // 0x54
	{ "TOCLIENT_FADE_SOUND",               TOCLIENT_STATE_CONNECTED, &Client::handleCommand_FadeSound }, // 0x55
	{ "TOCLIENT_UPDATE_PLAYER_LIST",       TOCLIENT_STATE_CONNECTED, &Client::handleCommand_UpdatePlayerList }, // 0x56
	{ "TOCLIENT_MODCHANNEL_MSG",           TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ModChannelMsg }, // 0x57
	{ "TOCLIENT_MODCHANNEL_SIGNAL",        TOCLIENT_STATE_CONNECTED, &Client::handleCommand_ModChannelSignal }, // 0x58
	{ "TOCLIENT_NODEMETA_CHANGED",         TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NodemetaChanged }, // 0x59
	{ "TOCLIENT_SET_SUN",                  TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetSun }, // 0x5a
	{ "TOCLIENT_SET_MOON",                 TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetMoon }, // 0x5b
	{ "TOCLIENT_SET_STARS",                TOCLIENT_STATE_CONNECTED, &Client::handleCommand_HudSetStars }, // 0x5c
	{ "TOCLIENT_MOVE_PLAYER_REL",          TOCLIENT_STATE_CONNECTED, &Client::handleCommand_MovePlayerRel }, // 0x5d,
	null_command_handler,
	null_command_handler,
	{ "TOCLIENT_SRP_BYTES_S_B",            TOCLIENT_STATE_NOT_CONNECTED, &Client::handleCommand_SrpBytesSandB }, // 0x60
	{ "TOCLIENT_FORMSPEC_PREPEND",         TOCLIENT_STATE_CONNECTED, &Client::handleCommand_FormspecPrepend }, // 0x61,
	{ "TOCLIENT_MINIMAP_MODES",            TOCLIENT_STATE_CONNECTED, &Client::handleCommand_MinimapModes }, // 0x62,
	{ "TOCLIENT_SET_LIGHTING",             TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SetLighting }, // 0x63,
	{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticleBatch }, // 0x64,
};

const static ServerCommandFactory null_command_factory = { nullptr, 0, false };

/*
	Channels used for Client -> Server communication
	2: Notifications back to the server (e.g. GOTBLOCKS)
	1: Init and Authentication
	0: everything else

	Packet order is only guaranteed inside a channel, so packets that operate on
	the same objects are *required* to be in the same channel.
*/

const ServerCommandFactory serverCommandFactoryTable[TOSERVER_NUM_MSG_TYPES] =
{
	null_command_factory, // 0x00
	null_command_factory, // 0x01
	{ "TOSERVER_INIT",               1, false }, // 0x02
	null_command_factory, // 0x03
	null_command_factory, // 0x04
	null_command_factory, // 0x05
	null_command_factory, // 0x06
	null_command_factory, // 0x07
	null_command_factory, // 0x08
	null_command_factory, // 0x09
	null_command_factory, // 0x0a
	null_command_factory, // 0x0b
	null_command_factory, // 0x0c
	null_command_factory, // 0x0d
	null_command_factory, // 0x0e
	null_command_factory, // 0x0f
	null_command_factory, // 0x10
	{ "TOSERVER_INIT2",              1, true }, // 0x11
	null_command_factory, // 0x12
	null_command_factory, // 0x13
	null_command_factory, // 0x14
	null_command_factory, // 0x15
	null_command_factory, // 0x16
	{ "TOSERVER_MODCHANNEL_JOIN",    0, true }, // 0x17
	{ "TOSERVER_MODCHANNEL_LEAVE",   0, true }, // 0x18
	{ "TOSERVER_MODCHANNEL_MSG",     0, true }, // 0x19
	null_command_factory, // 0x1a
	null_command_factory, // 0x1b
	null_command_factory, // 0x1c
	null_command_factory, // 0x1d
	null_command_factory, // 0x1e
	null_command_factory, // 0x1f
	null_command_factory, // 0x20
	null_command_factory, // 0x21
	null_command_factory, // 0x22
	{ "TOSERVER_PLAYERPOS",          0, false }, // 0x23
	{ "TOSERVER_GOTBLOCKS",          2, true }, // 0x24
	{ "TOSERVER_DELETEDBLOCKS",      2, true }, // 0x25
	null_command_factory, // 0x26
	null_command_factory, // 0x27
	null_command_factory, // 0x28
	null_command_factory, // 0x29
	null_command_factory, // 0x2a
	null_command_factory, // 0x2b
	null_command_factory, // 0x2c
	null_command_factory, // 0x2d
	null_command_factory, // 0x2e
	null_command_factory, // 0x2f
	null_command_factory, // 0x30
	{ "TOSERVER_INVENTORY_ACTION",   0, true }, // 0x31
	{ "TOSERVER_CHAT_MESSAGE",       0, true }, // 0x32
	null_command_factory, // 0x33
	null_command_factory, // 0x34
	{ "TOSERVER_DAMAGE",             0, true }, // 0x35
	null_command_factory, // 0x36
	{ "TOSERVER_PLAYERITEM",         0, true }, // 0x37
	{ "TOSERVER_RESPAWN_LEGACY",     0, true }, // 0x38
	{ "TOSERVER_INTERACT",           0, true }, // 0x39
	{ "TOSERVER_REMOVED_SOUNDS",     2, true }, // 0x3a
	{ "TOSERVER_NODEMETA_FIELDS",    0, true }, // 0x3b
	{ "TOSERVER_INVENTORY_FIELDS",   0, true }, // 0x3c
	null_command_factory, // 0x3d
	null_command_factory, // 0x3e
	null_command_factory, // 0x3f
	{ "TOSERVER_REQUEST_MEDIA",      1, true }, // 0x40
	{ "TOSERVER_HAVE_MEDIA",         2, true }, // 0x41
	null_command_factory, // 0x42
	{ "TOSERVER_CLIENT_READY",       1, true }, // 0x43
	null_command_factory, // 0x44
	null_command_factory, // 0x45
	null_command_factory, // 0x46
	null_command_factory, // 0x47
	null_command_factory, // 0x48
	null_command_factory, // 0x49
	null_command_factory, // 0x4a
	null_command_factory, // 0x4b
	null_command_factory, // 0x4c
	null_command_factory, // 0x4d
	null_command_factory, // 0x4e
	null_command_factory, // 0x4f
	{ "TOSERVER_FIRST_SRP",          1, true }, // 0x50
	{ "TOSERVER_SRP_BYTES_A",        1, true }, // 0x51
	{ "TOSERVER_SRP_BYTES_M",        1, true }, // 0x52
	{ "TOSERVER_UPDATE_CLIENT_INFO", 2, true }, // 0x53
};
