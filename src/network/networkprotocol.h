// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"

extern const u16 LATEST_PROTOCOL_VERSION;

// Server's supported network protocol range
constexpr u16 SERVER_PROTOCOL_VERSION_MIN = 37;

// Client's supported network protocol range
constexpr u16 CLIENT_PROTOCOL_VERSION_MIN = 37;

extern const u16 FORMSPEC_API_VERSION;

// (applies to all media files, not just textures)
#define TEXTURENAME_ALLOWED_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-"

// Due to our network protocol the maximum window size determines the maximum
// media size we can safely allow. See the comment and check in Connection::Send().
// This is a bit lower to include safety margin.
#define MEDIAFILE_MAX_SIZE (16700000U)

typedef u16 session_t;

enum ToClientCommand : u16
{
	TOCLIENT_HELLO = 0x02,
	/*
		Sent after TOSERVER_INIT.

		u8 deployed serialization version
		u16 unused (network compression, never implemeneted)
		u16 deployed protocol version
		u32 supported auth methods
		std::string unused (used to be username)
	*/

	TOCLIENT_AUTH_ACCEPT = 0x03,
	/*
		Message from server to accept auth.

		v3f unused
		u64 map seed
		f1000 recommended send interval
		u32 : supported auth methods for sudo mode
		      (where the user can change their password)
	*/

	TOCLIENT_ACCEPT_SUDO_MODE = 0x04,
	/*
		Sent to client to show it is in sudo mode now.
	*/

	TOCLIENT_DENY_SUDO_MODE = 0x05,
	/*
		Signals client that sudo mode auth failed.
	*/

	TOCLIENT_ACCESS_DENIED = 0x0A,
	/*
		u8 reason
		std::string custom reason (if needed, otherwise "")
		u8 (bool) reconnect
	*/

	TOCLIENT_BLOCKDATA = 0x20,
	/*
		v3s16 position
		serialized MapBlock
	*/

	TOCLIENT_ADDNODE = 0x21,
	/*
		v3s16 position
		serialized mapnode
		u8 keep_metadata
	*/

	TOCLIENT_REMOVENODE = 0x22,
	/*
		v3s16 position
	*/

	TOCLIENT_INVENTORY = 0x27,
	/*
		serialized inventory
	*/

	TOCLIENT_TIME_OF_DAY = 0x29,
	/*
		u16 time (0-23999)
		f1000 time_speed
	*/

	TOCLIENT_CSM_RESTRICTION_FLAGS = 0x2A,
	/*
		u32 CSMRestrictionFlags byteflag
	 */

	TOCLIENT_PLAYER_SPEED = 0x2B,
	/*
		v3f added_vel
	 */

	TOCLIENT_MEDIA_PUSH = 0x2C,
	/*
		std::string raw_hash
		std::string filename
		u32 callback_token
		bool should_be_cached
	*/

	TOCLIENT_CHAT_MESSAGE = 0x2F,
	/*
		u8 version
		u8 message_type
		u16 sendername length
		wstring sendername
		u16 length
		wstring message
	*/

	TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD = 0x31,
	/*
		u16 count of removed objects
		for all removed objects {
			u16 id
		}
		u16 count of added objects
		for all added objects {
			u16 id
			u8 type
			u32 initialization data length
			string initialization data
		}
	*/

	TOCLIENT_ACTIVE_OBJECT_MESSAGES = 0x32,
	/*
		for all objects
		{
			u16 id
			u16 message length
			string message
		}
	*/

	TOCLIENT_HP = 0x33,
	/*
		u8 hp
	*/

	TOCLIENT_MOVE_PLAYER = 0x34,
	/*
		v3f1000 player position
		f1000 player pitch
		f1000 player yaw
	*/

	TOCLIENT_ACCESS_DENIED_LEGACY = 0x35,
	/*
		u16 reason_length
		wstring reason
	*/

	TOCLIENT_FOV = 0x36,
	/*
		Sends an FOV override/multiplier to client.

		f32 fov
		bool is_multiplier
		f32 transition_time
	*/

	TOCLIENT_DEATHSCREEN_LEGACY = 0x37,
	/*
		u8 bool unused
		v3f1000 unused
	*/

	TOCLIENT_MEDIA = 0x38,
	/*
		u16 total number of bunches
		u16 index of this bunch
		u32 number of files in this bunch
		for each file {
			u16 length of name
			string name
			u32 length of data
			data (zstd-compressed)
		}
	*/

	TOCLIENT_NODEDEF = 0x3a,
	/*
		u32 length of buffer
		serialized NodeDefManager (zstd-compressed)
	*/

	TOCLIENT_ANNOUNCE_MEDIA = 0x3c,
	/*
		u32 length of compressed name array
		string16array names (zstd-compressed)
		for each file {
			char[20] sha1_digest
		}
		u16 length of remote media server url
		string url
	*/

	TOCLIENT_ITEMDEF = 0x3d,
	/*
		u32 length of buffer
		serialized ItemDefManager (zstd-compressed)
	*/

	TOCLIENT_PLAY_SOUND = 0x3f,
	/*
		s32 server_id
		u16 len
		u8[len] sound name
		f32 gain
		u8 type (SoundLocation: 0=local, 1=positional, 2=object)
		v3f pos_nodes (in BS-space)
		u16 object_id
		u8 loop (bool)
		f32 fade
		f32 pitch
		u8 ephemeral (bool)
		f32 start_time (in seconds)
	*/

	TOCLIENT_STOP_SOUND = 0x40,
	/*
		s32 sound_id
	*/

	TOCLIENT_PRIVILEGES = 0x41,
	/*
		u16 number of privileges
		for each privilege
			u16 len
			u8[len] privilege
	*/

	TOCLIENT_INVENTORY_FORMSPEC = 0x42,
	/*
		u32 len
		u8[len] formspec
	*/

	TOCLIENT_DETACHED_INVENTORY = 0x43,
	/*
		[0] u16 command
		u16 len
		u8[len] name
		[2] serialized inventory
	*/

	TOCLIENT_SHOW_FORMSPEC = 0x44,
	/*
		[0] u16 command
		u32 len
		u8[len] formspec
		u16 len
		u8[len] formname
	*/

	TOCLIENT_MOVEMENT = 0x45,
	/*
		f1000 movement_acceleration_default
		f1000 movement_acceleration_air
		f1000 movement_acceleration_fast
		f1000 movement_speed_walk
		f1000 movement_speed_crouch
		f1000 movement_speed_fast
		f1000 movement_speed_climb
		f1000 movement_speed_jump
		f1000 movement_liquid_fluidity
		f1000 movement_liquid_fluidity_smooth
		f1000 movement_liquid_sink
		f1000 movement_gravity
	*/

	TOCLIENT_SPAWN_PARTICLE = 0x46,
	/*
		ParticleParameters params:

		using range<T> = RangedParameter<T> {
			T min, max
			f32 bias
		}
		using tween<T> = TweenedParameter<T> {
			u8 style
			u16 reps
			f32 beginning
			T start, end
		}

		v3f pos
		v3f velocity
		v3f acceleration
		f32 expirationtime
		f32 size
		u8 bool collisiondetection

		u32 len
		u8[len] texture

		u8 bool vertical
		u8 bool collision_removal

		TileAnimation animation

		u8 glow
		u8 bool object_collision

		u16 node_param0
		u8 node_param2
		u8 node_tile

		v3f drag
		range<v3f> jitter
		range<f32> bounce

		texture {
			u8 flags (ParticleTextureFlags)
			-- bit 0: animated
			-- next bits: blend mode (BlendMode)
			tween<f32> alpha
			tween<v2f> scale
		}
	*/

	TOCLIENT_ADD_PARTICLESPAWNER = 0x47,
	/*
		using range<T> = RangedParameter<T> {
			T min, max
			f32 bias
		}
		using tween<T> = TweenedParameter<T> {
			u8 style
			u16 reps
			f32 beginning
			T start, end
		}

		u16 amount
		f32 spawntime
		if PROTOCOL_VERSION >= 42 {
			tween<range<T>> pos, vel, acc, exptime, size
		} else {
			v3f minpos
			v3f maxpos
			v3f minvel
			v3f maxvel
			v3f minacc
			v3f maxacc
			f32 minexptime
			f32 maxexptime
			f32 minsize
			f32 maxsize
		}
		u8 bool collisiondetection

		u32 len
		u8[len] texture

		u32 spawner_id
		u8 bool vertical
		u8 bool collision_removal
		u32 attached_id

		TileAnimation animation

		u8 glow
		u8 bool object_collision

		u16 node_param0
		u8 node_param2
		u8 node_tile

		if PROTOCOL_VERSION < 42 {
			f32 pos_start_bias
			f32 vel_start_bias
			f32 acc_start_bias
			f32 exptime_start_bias
			f32 size_start_bias

			range<v3f> pos_end
			-- i.e v3f pos_end_min
			--     v3f pos_end_max
			--     f32 pos_end_bias
			range<v3f> vel_end
			range<v3f> acc_end
			range<f32> exptime_end
			range<f32> size_end
		}

		texture {
			u8 flags (ParticleTextureFlags)
			-- bit 0: animated
			-- next bits: blend mode (BlendMode)
			tween<f32> alpha
			tween<v2f> scale

			if (flags.animated)
				TileAnimation animation
		}

		tween<range<v3f>> drag
		-- i.e. v3f drag_start_min
		--      v3f drag_start_max
		--      f32 drag_start_bias
		--      v3f drag_end_min
		--      v3f drag_end_max
		--      f32 drag_end_bias
		tween<range<v3f>> jitter
		tween<range<f32>> bounce

		u8 attraction_kind
			none  = 0
			point = 1
			line  = 2
			plane = 3

		if attraction_kind > none {
			tween<range<f32>> attract_strength
			tween<v3f>        attractor_origin
			u16               attractor_origin_attachment_object_id
			u8                spawner_flags
			    bit 1: attractor_kill (particles dies on contact)
			if attraction_mode > point {
				tween<v3f> attractor_direction
				u16        attractor_direction_attachment_object_id
			}
		}

		tween<range<v3f>> radius

		u16 texpool_size
		texpool_size.times {
			u8 flags (ParticleTextureFlags)
			-- bit 0: animated
			-- next bits: blend mode (BlendMode)
			tween<f32> alpha
			tween<v2f> scale

			u32 len
			u8[len] texture

			if (flags.animated)
				TileAnimation animation
		}

	*/

	TOCLIENT_CAMERA = 0x48,
	/*
		u8 allowed_camera_mode
	*/

	TOCLIENT_HUDADD = 0x49,
	/*
		u32 id
		u8 type
		v2f1000 pos
		u16 len
		u8[len] name
		v2f1000 scale
		u16 len2
		u8[len2] text
		u32 number
		u32 item
		u32 dir
		v2f1000 align
		v2f1000 offset
		v3f1000 world_pos
		v2s32 size
		s16 z_index
		u16 len3
		u8[len3] text2
	*/

	TOCLIENT_HUDRM = 0x4a,
	/*
		u32 id
	*/

	TOCLIENT_HUDCHANGE = 0x4b,
	/*
		u32 id
		u8 stat
		[v2f1000 data |
		 u32 len
		 u8[len] data |
		 u32 data]
	*/

	TOCLIENT_HUD_SET_FLAGS = 0x4c,
	/*
		u32 flags
		u32 mask
	*/

	TOCLIENT_HUD_SET_PARAM = 0x4d,
	/*
		u16 param
		u16 len
		u8[len] value
	*/

	TOCLIENT_BREATH = 0x4e,
	/*
		u16 breath
	*/

	TOCLIENT_SET_SKY = 0x4f,
	/*
		Protocol 38:
		u8[4] base_color (ARGB)
		u8 len
		u8[len] type
		u16 count
		foreach count:
			u8 len
			u8[len] param
		u8 clouds (boolean)

		Protocol 39:
		u8[4] bgcolor (ARGB)
		std::string type
		int texture_count
		std::string[6] param
		bool clouds
		bool bgcolor_fog
		u8[4] day_sky (ARGB)
		u8[4] day_horizon (ARGB)
		u8[4] dawn_sky (ARGB)
		u8[4] dawn_horizon (ARGB)
		u8[4] night_sky (ARGB)
		u8[4] night_horizon (ARGB)
		u8[4] indoors (ARGB)
		u8[4] fog_sun_tint (ARGB)
		u8[4] fog_moon_tint (ARGB)
		std::string fog_tint_type
		float body_orbit_tilt
	*/

	TOCLIENT_OVERRIDE_DAY_NIGHT_RATIO = 0x50,
	/*
		u8 do_override (boolean)
		u16 day-night ratio 0...65535
	*/

	TOCLIENT_LOCAL_PLAYER_ANIMATIONS = 0x51,
	/*
		v2s32 stand/idle
		v2s32 walk
		v2s32 dig
		v2s32 walk+dig
		f1000 frame_speed
	*/

	TOCLIENT_EYE_OFFSET = 0x52,
	/*
		v3f1000 first
		v3f1000 third
		v3f1000 third_front
	*/

	TOCLIENT_DELETE_PARTICLESPAWNER = 0x53,
	/*
		u32 id
	*/

	TOCLIENT_CLOUD_PARAMS = 0x54,
	/*
		f1000 density
		u8[4] color_diffuse (ARGB)
		u8[4] color_ambient (ARGB)
		f1000 height
		f1000 thickness
		v2f1000 speed
	*/

	TOCLIENT_FADE_SOUND = 0x55,
	/*
		s32 sound_id
		float step
		float gain
	*/
	TOCLIENT_UPDATE_PLAYER_LIST = 0x56,
	/*
	 	u8 type
	 	u16 number of players
		for each player
			u16 len
			u8[len] player name
	*/

	TOCLIENT_MODCHANNEL_MSG = 0x57,
	/*
		u16 channel name length
	 	std::string channel name
	 	u16 channel name sender
	 	std::string channel name
	 	u16 message length
	 	std::string message
	*/

	TOCLIENT_MODCHANNEL_SIGNAL = 0x58,
	/*
		u8 signal id
	 	u16 channel name length
	 	std::string channel name
	*/

	TOCLIENT_NODEMETA_CHANGED = 0x59,
	/*
		serialized and compressed node metadata
	*/

	TOCLIENT_SET_SUN = 0x5a,
	/*
		bool visible
		std::string texture
		std::string tonemap
		std::string sunrise
		f32 scale
	*/

	TOCLIENT_SET_MOON = 0x5b,
	/*
		bool visible
		std::string texture
		std::string tonemap
		f32 scale
	*/

	TOCLIENT_SET_STARS = 0x5c,
	/*
		bool visible
		u32 count
		u8[4] starcolor (ARGB)
		f32 scale
		f32 day_opacity
	*/

	TOCLIENT_MOVE_PLAYER_REL = 0x5d,
	/*
		v3f added_pos
	*/

	TOCLIENT_SRP_BYTES_S_B = 0x60,
	/*
		Belonging to AUTH_MECHANISM_SRP.

		std::string bytes_s
		std::string bytes_B
	*/

	TOCLIENT_FORMSPEC_PREPEND = 0x61,
	/*
		u16 len
		u8[len] formspec
	*/

	TOCLIENT_MINIMAP_MODES = 0x62,
	/*
		u16 count // modes
		u16 mode  // wanted current mode index after change
		for each mode
			u16 type
			std::string label
			u16 size
			std::string extra
	*/

	TOCLIENT_SET_LIGHTING = 0x63,
	/*
		f32 shadow_intensity
		f32 saturation
		exposure parameters
			f32 luminance_min
			f32 luminance_max
			f32 exposure_correction
			f32 speed_dark_bright
			f32 speed_bright_dark
			f32 center_weight_power
	*/

	TOCLIENT_SPAWN_PARTICLE_BATCH = 0x64,
	/*
		std::string data, zstd-compressed, for each particle:
			u32 len
			u8[len] serialized ParticleParameters
	*/

	TOCLIENT_NUM_MSG_TYPES = 0x65,
};

enum ToServerCommand : u16
{
	TOSERVER_INIT = 0x02,
	/*
		Sent first after connected.

		u8 serialization version (=SER_FMT_VER_HIGHEST_READ)
		u16 unused (supported network compression modes, never implemeneted)
		u16 minimum supported network protocol version
		u16 maximum supported network protocol version
		std::string player name
	*/

	TOSERVER_INIT2 = 0x11,
	/*
		Sent as an ACK for TOCLIENT_AUTH_ACCEPT.
		After this, the server can send data.
	*/

	TOSERVER_MODCHANNEL_JOIN = 0x17,
	/*
		u16 channel name length
	 	std::string channel name
	 */

	TOSERVER_MODCHANNEL_LEAVE = 0x18,
	/*
		u16 channel name length
	 	std::string channel name
	 */

	TOSERVER_MODCHANNEL_MSG = 0x19,
	/*
		u16 channel name length
	 	std::string channel name
	 	u16 message length
	 	std::string message
	 */

	TOSERVER_PLAYERPOS = 0x23,
	/*
		v3s32 position*100
		v3s32 speed*100
		s32 pitch*100
		s32 yaw*100
		u32 keyPressed
		u8 fov*80
		u8 ceil(wanted_range / MAP_BLOCKSIZE)
		u8 camera_inverted (bool)
		f32 movement_speed
		f32 movement_direction
	*/

	TOSERVER_GOTBLOCKS = 0x24,
	/*
		[0] u16 command
		[2] u8 count
		[3] v3s16 pos_0
		[3+6] v3s16 pos_1
		...
	*/

	TOSERVER_DELETEDBLOCKS = 0x25,
	/*
		[0] u16 command
		[2] u8 count
		[3] v3s16 pos_0
		[3+6] v3s16 pos_1
		...
	*/

	TOSERVER_INVENTORY_ACTION = 0x31,
	/*
		See InventoryAction in inventorymanager.h
	*/

	TOSERVER_CHAT_MESSAGE = 0x32,
	/*
		u16 length
		wstring message
	*/

	TOSERVER_DAMAGE = 0x35,
	/*
		u8 amount
	*/

	TOSERVER_PLAYERITEM = 0x37,
	/*
		Sent to change selected item.

		[0] u16 TOSERVER_PLAYERITEM
		[2] u16 item
	*/

	TOSERVER_RESPAWN_LEGACY = 0x38,

	TOSERVER_INTERACT = 0x39,
	/*
		[0] u16 command
		[2] u8 action
		[3] u16 item
		[5] u32 length of the next item
		[9] serialized PointedThing
		actions:
		0: start digging (from undersurface) or use
		1: stop digging (all parameters ignored)
		2: digging completed
		3: place block or item (to abovesurface)
		4: use item
	*/

	TOSERVER_REMOVED_SOUNDS = 0x3a,
	/*
		u16 len
		s32[len] sound_id
	*/

	TOSERVER_NODEMETA_FIELDS = 0x3b,
	/*
		v3s16 p
		u16 len
		u8[len] form name (reserved for future use)
		u16 number of fields
		for each field:
			u16 len
			u8[len] field name
			u32 len
			u8[len] field value
	*/

	TOSERVER_INVENTORY_FIELDS = 0x3c,
	/*
		u16 len
		u8[len] form name (reserved for future use)
		u16 number of fields
		for each field:
			u16 len
			u8[len] field name
			u32 len
			u8[len] field value
	*/

	TOSERVER_REQUEST_MEDIA = 0x40,
	/*
		u16 number of files requested
		for each file {
			u16 length of name
			string name
		}
	*/

	TOSERVER_HAVE_MEDIA = 0x41,
	/*
		u8 number of callback tokens
		for each:
			u32 token
	*/

	TOSERVER_CLIENT_READY = 0x43,
	/*
		u8 major
		u8 minor
		u8 patch
		u8 reserved
		u16 len
		u8[len] full_version_string
	*/

	TOSERVER_FIRST_SRP = 0x50,
	/*
		Belonging to AUTH_MECHANISM_FIRST_SRP.

		std::string srp salt
		std::string srp verification key
		u8 is_empty (=1 if password is empty, 0 otherwise)
	*/

	TOSERVER_SRP_BYTES_A = 0x51,
	/*
		Belonging to AUTH_MECHANISM_SRP,
			depending on current_login_based_on.

		std::string bytes_A
		u8 current_login_based_on : on which version of the password's
		                            hash this login is based on (0 legacy hash,
		                            or 1 directly the password)
	*/

	TOSERVER_SRP_BYTES_M = 0x52,
	/*
		Belonging to AUTH_MECHANISM_SRP.

		std::string bytes_M
	*/

	TOSERVER_UPDATE_CLIENT_INFO = 0x53,
	/*
		v2s16 render_target_size
		f32 gui_scaling
		f32 hud_scaling
		v2f32 max_fs_info
	*/

	TOSERVER_NUM_MSG_TYPES = 0x54,
};

enum AuthMechanism
{
	// reserved
	AUTH_MECHANISM_NONE = 0,

	// SRP based on the legacy hash
	AUTH_MECHANISM_LEGACY_PASSWORD = 1 << 0,

	// SRP based on the srp verification key
	AUTH_MECHANISM_SRP = 1 << 1,

	// Establishes a srp verification key, for first login and password changing
	AUTH_MECHANISM_FIRST_SRP = 1 << 2,
};

enum AccessDeniedCode : u8 {
	SERVER_ACCESSDENIED_WRONG_PASSWORD,
	SERVER_ACCESSDENIED_UNEXPECTED_DATA,
	SERVER_ACCESSDENIED_SINGLEPLAYER,
	SERVER_ACCESSDENIED_WRONG_VERSION,
	SERVER_ACCESSDENIED_WRONG_CHARS_IN_NAME,
	SERVER_ACCESSDENIED_WRONG_NAME,
	SERVER_ACCESSDENIED_TOO_MANY_USERS,
	SERVER_ACCESSDENIED_EMPTY_PASSWORD,
	SERVER_ACCESSDENIED_ALREADY_CONNECTED,
	SERVER_ACCESSDENIED_SERVER_FAIL,
	SERVER_ACCESSDENIED_CUSTOM_STRING,
	SERVER_ACCESSDENIED_SHUTDOWN,
	SERVER_ACCESSDENIED_CRASH,
	SERVER_ACCESSDENIED_MAX,
};

enum PlayerListModifer : u8
{
	PLAYER_LIST_INIT,
	PLAYER_LIST_ADD,
	PLAYER_LIST_REMOVE,
};

enum CSMRestrictionFlags : u64 {
	CSM_RF_NONE = 0x00000000,
	// Until server-sent CSM and verifying of builtin are complete,
	// 'CSM_RF_LOAD_CLIENT_MODS' also disables loading 'builtin'.
	// When those are complete, this should return to only being a restriction on the
	// loading of client mods.
	CSM_RF_LOAD_CLIENT_MODS = 0x00000001, // Don't load client-provided mods or 'builtin'
	CSM_RF_CHAT_MESSAGES = 0x00000002,    // Disable chat message sending from CSM
	CSM_RF_READ_ITEMDEFS = 0x00000004,    // Disable itemdef lookups
	CSM_RF_READ_NODEDEFS = 0x00000008,    // Disable nodedef lookups
	CSM_RF_LOOKUP_NODES = 0x00000010,     // Limit node lookups
	CSM_RF_READ_PLAYERINFO = 0x00000020,  // Disable player info lookups
	CSM_RF_ALL = 0xFFFFFFFF,
};

enum InteractAction : u8
{
	INTERACT_START_DIGGING,     // 0: start digging (from undersurface) or use
	INTERACT_STOP_DIGGING,      // 1: stop digging (all parameters ignored)
	INTERACT_DIGGING_COMPLETED, // 2: digging completed
	INTERACT_PLACE,             // 3: place block or item (to abovesurface)
	INTERACT_USE,               // 4: use item
	INTERACT_ACTIVATE           // 5: rightclick air ("activate")
};
