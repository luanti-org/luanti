// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "networkprotocol.h"


/*
	PROTOCOL VERSION < 37:
		Until (and including) version 0.4.17.1
	PROTOCOL VERSION 37:
		Redo detached inventory sending
		Add TOCLIENT_NODEMETA_CHANGED
		New network float format
		ContentFeatures version 13
		Add full Euler rotations instead of just yaw
		Add TOCLIENT_PLAYER_SPEED
		[bump for 5.0.0]
	PROTOCOL VERSION 38:
		Incremental inventory sending mode
		Unknown inventory serialization fields no longer throw an error
		Mod-specific formspec version
		Player FOV override API
		"ephemeral" added to TOCLIENT_PLAY_SOUND
	PROTOCOL VERSION 39:
		Updated set_sky packet
		Adds new sun, moon and stars packets
		Minimap modes
	PROTOCOL VERSION 40:
		TOCLIENT_MEDIA_PUSH changed, TOSERVER_HAVE_MEDIA added
	PROTOCOL VERSION 41:
		Added new particlespawner parameters
		[scheduled bump for 5.6.0]
	PROTOCOL VERSION 42:
		TOSERVER_UPDATE_CLIENT_INFO added
		new fields for TOCLIENT_SET_LIGHTING and TOCLIENT_SET_SKY
		Send forgotten TweenedParameter properties
		[scheduled bump for 5.7.0]
	PROTOCOL VERSION 43:
		"start_time" added to TOCLIENT_PLAY_SOUND
		place_param2 type change u8 -> optional<u8>
		[scheduled bump for 5.8.0]
	PROTOCOL VERSION 44:
		AO_CMD_SET_BONE_POSITION extended
		Add TOCLIENT_MOVE_PLAYER_REL
		Move default minimap from client-side C++ to server-side builtin Lua
		[scheduled bump for 5.9.0]
	PROTOCOL VERSION 45:
		Minimap HUD element supports negative size values as percentages
		[bump for 5.9.1]
	PROTOCOL VERSION 46:
		Move default hotbar from client-side C++ to server-side builtin Lua
		Add shadow tint to Lighting packets
		Add shadow color to CloudParam packets
		Move death screen to server and make it a regular formspec
			The server no longer triggers the hardcoded client-side death
			formspec, but the client still supports it for compatibility with
			old servers.
		Rename TOCLIENT_DEATHSCREEN to TOCLIENT_DEATHSCREEN_LEGACY
		Rename TOSERVER_RESPAWN to TOSERVER_RESPAWN_LEGACY
		Support float animation frame numbers in TOCLIENT_LOCAL_PLAYER_ANIMATIONS
		[scheduled bump for 5.10.0]
	PROTOCOL VERSION 47
		Add particle blend mode "clip"
		[scheduled bump for 5.11.0]
	PROTOCOL VERSION 48
		Add compression to some existing packets
		[scheduled bump for 5.12.0]
	PROTOCOL VERSION 49
		Support of showing a player inventory using 'core.show_formspec'
		[scheduled bump for 5.13.0]
	PROTOCOL VERSION 50
		Support for TOCLIENT_SPAWN_PARTICLE_BATCH
		[scheduled bump for 5.14.0]
*/

// Note: Also update core.protocol_versions in builtin when bumping
const u16 LATEST_PROTOCOL_VERSION = 50;

// See also formspec [Version History] in doc/lua_api.md
const u16 FORMSPEC_API_VERSION = 10;
