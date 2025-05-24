-- minetest.lua
-- Packet dissector for the UDP-based Luanti protocol
-- Copy this to $HOME/.wireshark/plugins/

-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2011 celeron55, Perttu Ahola <celeron55@gmail.com>


-- Wireshark documentation:
-- https://www.wireshark.org/docs/wsdg_html_chunked/lua_module_Proto.html
-- https://www.wireshark.org/docs/wsdg_html_chunked/lua_module_Tree.html
-- https://www.wireshark.org/docs/wsdg_html_chunked/lua_module_Tvb.html


-- Table of Contents:
--   Part 1: Utility functions
--   Part 2: Client command dissectors (TOSERVER_*)
--   Part 3: Server command dissectors (TOCLIENT_*)
--   Part 4: Wrapper protocol subdissectors
--   Part 5: Wrapper protocol main dissector
--   Part 6: Utility functions part 2


-----------------------
-- Part 1            --
-- Utility functions --
-----------------------

-- Creates two ProtoFields to hold a length and variable-length text content
-- lentype must be either "uint16" or "uint32"
function minetest_field_helper(lentype, name, abbr)
	local f_textlen = ProtoField[lentype](name .. "len", abbr .. " (length)", base.DEC)
	local f_text = ProtoField.string(name, abbr)
	return f_textlen, f_text
end

-- global reference to 'minetest.peer' field (set later)
local minetest_peer_field


--------------------------------------------
-- Part 2                                 --
-- Client command dissectors (TOSERVER_*) --
--------------------------------------------

minetest_client_commands = {}
minetest_client_obsolete = {}

-- TOSERVER_INIT

do
	local abbr = "minetest.client.init_"

	local f_ser_fmt = ProtoField.uint8(abbr.."ser_version",
		"Maximum serialization format version", base.DEC)
	local f_comp_modes = ProtoField.uint16(abbr.."compression",
		"Supported compression modes", base.DEC, { [0] = "No compression" })
	local f_proto_min = ProtoField.uint16(abbr.."proto_min", "Minimum protocol version", base.DEC)
	local f_proto_max = ProtoField.uint16(abbr.."_proto_max", "Maximum protocol version", base.DEC)
	local f_player_namelen, f_player_name =
		minetest_field_helper("uint16", abbr.."player_name", "Player Name")

	minetest_client_commands[0x02] = {
		"INIT",                            -- Command name
		11,                                -- Minimum message length including code
		{ f_ser_fmt,                       -- List of fields [optional]
		  f_comp_modes,
		  f_proto_min,
		  f_proto_max,
		  f_player_namelen,
		  f_player_name },
		function(buffer, pinfo, tree, t)   -- Dissector function [optional]
			t:add(f_ser_fmt, buffer(2,1))
			t:add(f_comp_modes, buffer(3,2))
			t:add(f_proto_min, buffer(5,2))
			t:add(f_proto_max, buffer(7,2))
			minetest_decode_helper_ascii(buffer, t, "uint16", 9, f_player_namelen, f_player_name)
		end
	}
end

-- TOSERVER_INIT_LEGACY (obsolete)

minetest_client_commands[0x10] = { "INIT_LEGACY", 2 }
minetest_client_obsolete[0x10] = true

-- TOSERVER_INIT2

do
	local f_langlen, f_lang =
		minetest_field_helper("uint16", "minetest.client.init2_language", "Language Code")

	minetest_client_commands[0x11] = {
		"INIT2",
		2,
		{ f_langlen,
		  f_lang },
		function(buffer, pinfo, tree, t)
			minetest_decode_helper_ascii(buffer, t, "uint16", 2, f_langlen, f_lang)
		end
	}
end

-- TOSERVER_MODCHANNEL_JOIN

minetest_client_commands[0x17] = { "MODCHANNEL_JOIN", 2 }

-- TOSERVER_MODCHANNEL_LEAVE

minetest_client_commands[0x18] = { "MODCHANNEL_LEAVE", 2 }

-- TOSERVER_MODCHANNEL_MSG

minetest_client_commands[0x19] = { "MODCHANNEL_MSG", 2 }

-- TOSERVER_GETBLOCK (obsolete)

minetest_client_commands[0x20] = { "GETBLOCK", 2 }
minetest_client_obsolete[0x20] = true

-- TOSERVER_ADDNODE (obsolete)

minetest_client_commands[0x21] = { "ADDNODE", 2 }
minetest_client_obsolete[0x21] = true

-- TOSERVER_REMOVENODE (obsolete)

minetest_client_commands[0x22] = { "REMOVENODE", 2 }
minetest_client_obsolete[0x22] = true

-- TOSERVER_PLAYERPOS

do
	local abbr = "minetest.client.playerpos_"

	local f_x = ProtoField.int32(abbr.."x", "Position X", base.DEC)
	local f_y = ProtoField.int32(abbr.."y", "Position Y", base.DEC)
	local f_z = ProtoField.int32(abbr.."z", "Position Z", base.DEC)
	local f_speed_x = ProtoField.int32(abbr.."speed_x", "Speed X", base.DEC)
	local f_speed_y = ProtoField.int32(abbr.."speed_y", "Speed Y", base.DEC)
	local f_speed_z = ProtoField.int32(abbr.."speed_z", "Speed Z", base.DEC)
	local f_pitch = ProtoField.int32(abbr.."pitch", "Pitch", base.DEC)
	local f_yaw = ProtoField.int32(abbr.."yaw", "Yaw", base.DEC)
	local f_key_pressed = ProtoField.bytes(abbr.."key_pressed", "Pressed keys")
	local f_fov = ProtoField.uint8(abbr.."fov", "FOV", base.DEC)
	local f_wanted_range = ProtoField.uint8(abbr.."wanted_range", "Requested view range", base.DEC)

	minetest_client_commands[0x23] = {
		"PLAYERPOS", 34,
		{ f_x, f_y, f_z, f_speed_x, f_speed_y, f_speed_z, f_pitch, f_yaw,
		  f_key_pressed, f_fov, f_wanted_range },
		function(buffer, pinfo, tree, t)
			t:add(f_x, buffer(2,4))
			t:add(f_y, buffer(6,4))
			t:add(f_z, buffer(10,4))
			t:add(f_speed_x, buffer(14,4))
			t:add(f_speed_y, buffer(18,4))
			t:add(f_speed_z, buffer(22,4))
			t:add(f_pitch, buffer(26,4))
			t:add(f_yaw, buffer(30,4))
			t:add(f_key_pressed, buffer(34,4))
			t:add(f_fov, buffer(38,1))
			t:add(f_wanted_range, buffer(39,1))
		end
	}
end

-- TOSERVER_GOTBLOCKS

do
	local f_count = ProtoField.uint8("minetest.client.gotblocks_count", "Count", base.DEC)
	local f_block = ProtoField.bytes("minetest.client.gotblocks_block", "Block", base.NONE)
	local f_x = ProtoField.int16("minetest.client.gotblocks_x", "Block position X", base.DEC)
	local f_y = ProtoField.int16("minetest.client.gotblocks_y", "Block position Y", base.DEC)
	local f_z = ProtoField.int16("minetest.client.gotblocks_z", "Block position Z", base.DEC)

	minetest_client_commands[0x24] = {
		"GOTBLOCKS", 3,
		{ f_count, f_block, f_x, f_y, f_z },
		function(buffer, pinfo, tree, t)
			t:add(f_count, buffer(2,1))
			local count = buffer(2,1):uint()
			if minetest_check_length(buffer, 3 + 6*count, t) then
				pinfo.cols.info:append(" * " .. count)
				local index
				for index = 0, count - 1 do
					local pos = 3 + 6*index
					local t2 = t:add(f_block, buffer(pos, 6))
					t2:set_text("Block, X: " .. buffer(pos, 2):int()
						.. ", Y: " .. buffer(pos + 2, 2):int()
						.. ", Z: " .. buffer(pos + 4, 2):int())
					t2:add(f_x, buffer(pos, 2))
					t2:add(f_y, buffer(pos + 2, 2))
					t2:add(f_z, buffer(pos + 4, 2))
				end
			end
		end
	}
end

-- TOSERVER_DELETEDBLOCKS

do
	local f_count = ProtoField.uint8("minetest.client.deletedblocks_count", "Count", base.DEC)
	local f_block = ProtoField.bytes("minetest.client.deletedblocks_block", "Block", base.NONE)
	local f_x = ProtoField.int16("minetest.client.deletedblocks_x", "Block position X", base.DEC)
	local f_y = ProtoField.int16("minetest.client.deletedblocks_y", "Block position Y", base.DEC)
	local f_z = ProtoField.int16("minetest.client.deletedblocks_z", "Block position Z", base.DEC)

	minetest_client_commands[0x25] = {
		"DELETEDBLOCKS", 3,
		{ f_count, f_block, f_x, f_y, f_z },
		function(buffer, pinfo, tree, t)
			t:add(f_count, buffer(2,1))
			local count = buffer(2,1):uint()
			if minetest_check_length(buffer, 3 + 6*count, t) then
				pinfo.cols.info:append(" * " .. count)
				local index
				for index = 0, count - 1 do
					local pos = 3 + 6*index
					local t2 = t:add(f_block, buffer(pos, 6))
					t2:set_text("Block, X: " .. buffer(pos, 2):int()
						.. ", Y: " .. buffer(pos + 2, 2):int()
						.. ", Z: " .. buffer(pos + 4, 2):int())
					t2:add(f_x, buffer(pos, 2))
					t2:add(f_y, buffer(pos + 2, 2))
					t2:add(f_z, buffer(pos + 4, 2))
				end
			end
		end
	}
end

-- TOSERVER_ADDNODE_FROM_INVENTORY (obsolete)

minetest_client_commands[0x26] = { "ADDNODE_FROM_INVENTORY", 2 }
minetest_client_obsolete[0x26] = true

-- TOSERVER_CLICK_OBJECT (obsolete)

minetest_client_commands[0x27] = { "CLICK_OBJECT", 2 }
minetest_client_obsolete[0x27] = true

-- TOSERVER_GROUND_ACTION (obsolete)

minetest_client_commands[0x28] = { "GROUND_ACTION", 2 }
minetest_client_obsolete[0x28] = true

-- TOSERVER_RELEASE (obsolete)

minetest_client_commands[0x29] = { "RELEASE", 2 }
minetest_client_obsolete[0x29] = true

-- TOSERVER_SIGNTEXT (obsolete)

minetest_client_commands[0x30] = { "SIGNTEXT", 2 }
minetest_client_obsolete[0x30] = true

-- TOSERVER_INVENTORY_ACTION

do
	local f_action = ProtoField.string("minetest.client.inventory_action", "Action")

	minetest_client_commands[0x31] = {
		"INVENTORY_ACTION", 2,
		{ f_action },
		function(buffer, pinfo, tree, t)
			t:add(f_action, buffer(2, buffer:len() - 2))
		end
	}
end

-- TOSERVER_CHAT_MESSAGE

do
	local f_length = ProtoField.uint16("minetest.client.chat_message_length", "Length", base.DEC)
	local f_message = ProtoField.string("minetest.client.chat_message", "Message")

	minetest_client_commands[0x32] = {
		"CHAT_MESSAGE", 4,
		{ f_length, f_message },
		function(buffer, pinfo, tree, t)
			t:add(f_length, buffer(2,2))
			local textlen = buffer(2,2):uint()
			if minetest_check_length(buffer, 4 + textlen*2, t) then
				t:add(f_message, buffer(4, textlen*2), buffer(4, textlen*2):ustring())
			end
		end
	}
end

-- TOSERVER_SIGNNODETEXT (obsolete)

minetest_client_commands[0x33] = { "SIGNNODETEXT", 2 }
minetest_client_obsolete[0x33] = true


-- TOSERVER_CLICK_ACTIVEOBJECT (obsolete)

minetest_client_commands[0x34] = { "CLICK_ACTIVEOBJECT", 2 }
minetest_client_obsolete[0x34] = true

-- TOSERVER_DAMAGE

do
	local f_amount = ProtoField.uint8("minetest.client.damage_amount", "Amount", base.DEC)

	minetest_client_commands[0x35] = {
		"DAMAGE", 3,
		{ f_amount },
		function(buffer, pinfo, tree, t)
			t:add(f_amount, buffer(2,1))
		end
	}
end

-- TOSERVER_PASSWORD (obsolete)

minetest_client_commands[0x36] = { "PASSWORD", 2 }
minetest_client_obsolete[0x36] = true

-- TOSERVER_PLAYERITEM

do
	local f_item = ProtoField.uint16("minetest.client.playeritem_item", "Wielded item")

	minetest_client_commands[0x37] = {
		"PLAYERITEM", 4,
		{ f_item },
		function(buffer, pinfo, tree, t)
			t:add(f_item, buffer(2,2))
		end
	}
end

-- TOSERVER_RESPAWN_LEGACY

minetest_client_commands[0x38] = { "RESPAWN_LEGACY", 2 }

-- TOSERVER_INTERACT

do
	local abbr = "minetest.client.interact_"
	local vs_action = {
		[0] = "Start digging",
		[1] = "Stop digging",
		[2] = "Digging completed",
		[3] = "Place block or item",
		[4] = "Use item",
		[5] = "Activate held item",
	}
	local vs_pointed_type = {
		[0] = "Nothing",
		[1] = "Node",
		[2] = "Object",
	}

	local f_action = ProtoField.uint8(abbr.."action", "Action", base.DEC, vs_action)
	local f_item = ProtoField.uint16(abbr.."item", "Item Index", base.DEC)
	local f_plen = ProtoField.uint32(abbr.."plen", "Length of pointed thing", base.DEC)
	local f_pointed_version = ProtoField.uint8(abbr.."pointed_version",
		"Pointed Thing Version", base.DEC)
	local f_pointed_type = ProtoField.uint8(abbr.."pointed_version",
		"Pointed Thing Type", base.DEC, vs_pointed_type)
	local f_pointed_under_x = ProtoField.int16(abbr.."pointed_under_x",
		"Node position (under surface) X")
	local f_pointed_under_y = ProtoField.int16(abbr.."pointed_under_y",
		"Node position (under surface) Y")
	local f_pointed_under_z = ProtoField.int16(abbr.."pointed_under_z",
		"Node position (under surface) Z")
	local f_pointed_above_x = ProtoField.int16(abbr.."pointed_above_x",
		"Node position (above surface) X")
	local f_pointed_above_y = ProtoField.int16(abbr.."pointed_above_y",
		"Node position (above surface) Y")
	local f_pointed_above_z = ProtoField.int16(abbr.."pointed_above_z",
		"Node position (above surface) Z")
	local f_pointed_object_id = ProtoField.int16(abbr.."pointed_object_id",
		"Object ID")
	-- mising: additional playerpos data just like in TOSERVER_PLAYERPOS

	minetest_client_commands[0x39] = {
		"INTERACT", 11,
		{ f_action,
		  f_item,
		  f_plen,
		  f_pointed_version,
		  f_pointed_type,
		  f_pointed_under_x,
		  f_pointed_under_y,
		  f_pointed_under_z,
		  f_pointed_above_x,
		  f_pointed_above_y,
		  f_pointed_above_z,
		  f_pointed_object_id },
		function(buffer, pinfo, tree, t)
			t:add(f_action, buffer(2,1))
			t:add(f_item, buffer(3,2))
			t:add(f_plen, buffer(5,4))
			local plen = buffer(5,4):uint()
			if minetest_check_length(buffer, 9 + plen, t) then
				t:add(f_pointed_version, buffer(9,1))
				t:add(f_pointed_type, buffer(10,1))
				local ptype = buffer(10,1):uint()
				if ptype == 1 then -- Node
					t:add(f_pointed_under_x, buffer(11,2))
					t:add(f_pointed_under_y, buffer(13,2))
					t:add(f_pointed_under_z, buffer(15,2))
					t:add(f_pointed_above_x, buffer(17,2))
					t:add(f_pointed_above_y, buffer(19,2))
					t:add(f_pointed_above_z, buffer(21,2))
				elseif ptype == 2 then -- Object
					t:add(f_pointed_object_id, buffer(11,2))
				end
			end
		end
	}
end

-- ...

minetest_client_commands[0x3a] = { "REMOVED_SOUNDS", 2 }
minetest_client_commands[0x3b] = { "NODEMETA_FIELDS", 2 }
minetest_client_commands[0x3c] = { "INVENTORY_FIELDS", 2 }
minetest_client_commands[0x40] = { "REQUEST_MEDIA", 2 }
minetest_client_commands[0x41] = { "RECEIVED_MEDIA", 2 }

-- TOSERVER_BREATH (obsolete)

minetest_client_commands[0x42] = { "BREATH", 2 }
minetest_client_obsolete[0x42] = true

-- TOSERVER_CLIENT_READY

do
	local abbr = "minetest.client.client_ready_"
	local f_major = ProtoField.uint8(abbr.."major","Version Major")
	local f_minor = ProtoField.uint8(abbr.."minor","Version Minor")
	local f_patch = ProtoField.uint8(abbr.."patch","Version Patch")
	local f_reserved = ProtoField.uint8(abbr.."reserved","Reserved")
	local f_versionlen, f_version =
		minetest_field_helper("uint16", abbr.."version", "Full Version String")
	local f_formspec_ver = ProtoField.uint16(abbr.."formspec_version",
		"Formspec API version")

	minetest_client_commands[0x43] = {
		"CLIENT_READY",
		8,
		{ f_major, f_minor, f_patch, f_reserved, f_versionlen,
		  f_version, f_formspec_ver },
		function(buffer, pinfo, tree, t)
			t:add(f_major, buffer(2,1))
			t:add(f_minor, buffer(3,1))
			t:add(f_patch, buffer(4,1))
			t:add(f_reserved, buffer(5,1))
			local off = minetest_decode_helper_ascii(buffer, t, "uint16", 6,
				f_versionlen, f_version)
			if off and minetest_check_length(buffer, off + 2, t) then
				t:add(f_formspec_ver, buffer(off,2))
			end
		end
	}
end

-- ...

minetest_client_commands[0x50] = { "FIRST_SRP", 2 }
minetest_client_commands[0x51] = { "SRP_BYTES_A", 2 }
minetest_client_commands[0x52] = { "SRP_BYTES_M", 2 }
minetest_client_commands[0x53] = { "UPDATE_CLIENT_INFO", 2 }

--------------------------------------------
-- Part 3                                 --
-- Server command dissectors (TOCLIENT_*) --
--------------------------------------------

minetest_server_commands = {}
minetest_server_obsolete = {}

-- TOCLIENT_HELLO

do
	local abbr = "minetest.server.hello_"

	local f_ser_fmt = ProtoField.uint8(abbr.."ser_version",
		"Deployed serialization format version", base.DEC)
	local f_comp_mode = ProtoField.uint16(abbr.."compression",
		"Deployed compression mode", base.DEC, { [0] = "No compression" })
	local f_proto = ProtoField.uint16(abbr.."proto",
		"Deployed protocol version", base.DEC)
	local f_auth_methods = ProtoField.bytes(abbr.."auth_modes",
		"Supported authentication modes")
	local f_legacy_namelen, f_legacy_name = minetest_field_helper("uint16",
		abbr.."legacy_name", "Legacy player name for hashing")

	minetest_server_commands[0x02] = {
		"HELLO",
		13,
		{ f_ser_fmt, f_comp_mode, f_proto, f_auth_methods,
		  f_legacy_namelen, f_legacy_name },
		function(buffer, pinfo, tree, t)
			t:add(f_ser_fmt, buffer(2,1))
			t:add(f_comp_mode, buffer(3,2))
			t:add(f_proto, buffer(5,2))
			t:add(f_auth_methods, buffer(7,4))
			minetest_decode_helper_ascii(buffer, t, "uint16", 11, f_legacy_namelen, f_legacy_name)
		end
	}
end

-- TOCLIENT_AUTH_ACCEPT

do
	local abbr = "minetest.server.auth_accept_"

	local f_player_x = ProtoField.float(abbr.."player_x", "Player position X")
	local f_player_y = ProtoField.float(abbr.."player_y", "Player position Y")
	local f_player_z = ProtoField.float(abbr.."player_z", "Player position Z")
	local f_map_seed = ProtoField.uint64(abbr.."map_seed", "Map seed")
	local f_send_interval = ProtoField.float(abbr.."send_interval",
		"Recommended send interval")
	local f_sudo_auth_methods = ProtoField.bytes(abbr.."sudo_auth_methods",
		"Supported auth methods for sudo mode")

	minetest_server_commands[0x03] = {
		"AUTH_ACCEPT",
		30,
		{ f_player_x, f_player_y, f_player_z, f_map_seed,
		  f_send_interval, f_sudo_auth_methods },
		function(buffer, pinfo, tree, t)
			t:add(f_player_x, buffer(2,4))
			t:add(f_player_y, buffer(6,4))
			t:add(f_player_z, buffer(10,4))
			t:add(f_map_seed, buffer(14,8))
			t:add(f_send_interval, buffer(22,4))
			t:add(f_sudo_auth_methods, buffer(26,4))
		end
	}
end

-- ...

minetest_server_commands[0x04] = {"ACCEPT_SUDO_MODE", 2}
minetest_server_commands[0x05] = {"DENY_SUDO_MODE", 2}
minetest_server_commands[0x0A] = {"ACCESS_DENIED", 2}

-- TOCLIENT_INIT (obsolete)

minetest_server_commands[0x10] = { "INIT", 2 }
minetest_server_obsolete[0x10] = true

-- TOCLIENT_BLOCKDATA

do
	local f_x = ProtoField.int16("minetest.server.blockdata_x", "Block position X", base.DEC)
	local f_y = ProtoField.int16("minetest.server.blockdata_y", "Block position Y", base.DEC)
	local f_z = ProtoField.int16("minetest.server.blockdata_z", "Block position Z", base.DEC)
	local f_data = ProtoField.bytes("minetest.server.blockdata_block", "Serialized MapBlock")

	minetest_server_commands[0x20] = {
		"BLOCKDATA", 8,
		{ f_x, f_y, f_z, f_data },
		function(buffer, pinfo, tree, t)
			t:add(f_x, buffer(2,2))
			t:add(f_y, buffer(4,2))
			t:add(f_z, buffer(6,2))
			t:add(f_data, buffer(8, buffer:len() - 8))
		end
	}
end

-- TOCLIENT_ADDNODE

do
	local f_x = ProtoField.int16("minetest.server.addnode_x", "Position X", base.DEC)
	local f_y = ProtoField.int16("minetest.server.addnode_y", "Position Y", base.DEC)
	local f_z = ProtoField.int16("minetest.server.addnode_z", "Position Z", base.DEC)
	local f_data = ProtoField.bytes("minetest.server.addnode_node", "Serialized MapNode")

	minetest_server_commands[0x21] = {
		"ADDNODE", 8,
		{ f_x, f_y, f_z, f_data },
		function(buffer, pinfo, tree, t)
			t:add(f_x, buffer(2,2))
			t:add(f_y, buffer(4,2))
			t:add(f_z, buffer(6,2))
			t:add(f_data, buffer(8, buffer:len() - 8))
		end
	}
end

-- TOCLIENT_REMOVENODE

do
	local f_x = ProtoField.int16("minetest.server.removenode_x", "Position X", base.DEC)
	local f_y = ProtoField.int16("minetest.server.removenode_y", "Position Y", base.DEC)
	local f_z = ProtoField.int16("minetest.server.removenode_z", "Position Z", base.DEC)

	minetest_server_commands[0x22] = {
		"REMOVENODE", 8,
		{ f_x, f_y, f_z },
		function(buffer, pinfo, tree, t)
			t:add(f_x, buffer(2,2))
			t:add(f_y, buffer(4,2))
			t:add(f_z, buffer(6,2))
		end
	}
end

-- TOCLIENT_PLAYERPOS (obsolete)

minetest_server_commands[0x23] = { "PLAYERPOS", 2 }
minetest_server_obsolete[0x23] = true

-- TOCLIENT_PLAYERINFO (obsolete)

minetest_server_commands[0x24] = { "PLAYERINFO", 2 }
minetest_server_obsolete[0x24] = true

-- TOCLIENT_OPT_BLOCK_NOT_FOUND (obsolete)

minetest_server_commands[0x25] = { "OPT_BLOCK_NOT_FOUND", 2 }
minetest_server_obsolete[0x25] = true

-- TOCLIENT_SECTORMETA (obsolete)

minetest_server_commands[0x26] = { "SECTORMETA", 2 }
minetest_server_obsolete[0x26] = true

-- TOCLIENT_INVENTORY

do
	local f_inventory = ProtoField.string("minetest.server.inventory", "Inventory")

	minetest_server_commands[0x27] = {
		"INVENTORY", 2,
		{ f_inventory },
		function(buffer, pinfo, tree, t)
			t:add(f_inventory, buffer(2, buffer:len() - 2))
		end
	}
end

-- TOCLIENT_OBJECTDATA (obsolete)

minetest_server_commands[0x28] = { "OBJECTDATA", 2 }
minetest_server_obsolete[0x28] = true

-- TOCLIENT_TIME_OF_DAY

do
	local f_time = ProtoField.uint16("minetest.server.time_of_day", "Time", base.DEC)
	local f_time_speed = ProtoField.float("minetest.server.time_speed", "Time Speed", base.DEC)

	minetest_server_commands[0x29] = {
		"TIME_OF_DAY", 8,
		{ f_time, f_time_speed },
		function(buffer, pinfo, tree, t)
			t:add(f_time, buffer(2,2))
			t:add(f_time_speed, buffer(4,4))
		end
	}
end

-- ...

minetest_server_commands[0x2a] = { "CSM_RESTRICTION_FLAGS", 2 }
minetest_server_commands[0x2b] = { "PLAYER_SPEED", 2 }
minetest_server_commands[0x2c] = { "MEDIA_PUSH", 2 }

-- TOCLIENT_CHAT_MESSAGE

do
	local abbr = "minetest.server.chat_message_"
	local vs_type = {
		[0] = "Raw",
		[1] = "Normal",
		[2] = "Announce",
		[3] = "System",
	}

	local f_version = ProtoField.uint8(abbr.."version", "Version")
	local f_type = ProtoField.uint8(abbr.."type", "Message Type", base.DEC, vs_type)
	local f_senderlen, f_sender = minetest_field_helper("uint16", abbr.."sender",
		"Message sender")
	local f_messagelen, f_message = minetest_field_helper("uint16", abbr:sub(1,-2),
		"Message")

	minetest_server_commands[0x2f] = {
		"CHAT_MESSAGE", 8,
		{ f_version, f_type, f_senderlen, f_sender,
		  f_messagelen, f_message },
		function(buffer, pinfo, tree, t)
			t:add(f_version, buffer(2,1))
			t:add(f_type, buffer(3,1))
			local off = 4
			off = minetest_decode_helper_utf16(buffer, t, "uint16", off, f_senderlen, f_sender)
			if off then
				off = minetest_decode_helper_utf16(buffer, t, "uint16", off, f_messagelen, f_message)
			end
		end
	}
end

-- TOCLIENT_CHAT_MESSAGE_OLD (obsolete)

minetest_server_commands[0x30] = { "CHAT_MESSAGE_OLD", 2 }
minetest_server_obsolete[0x30] = true

-- TOCLIENT_ACTIVE_OBJECT_REMOVE_ADD

do
	local f_removed_count = ProtoField.uint16(
		"minetest.server.active_object_remove_add_removed_count",
		"Count of removed objects", base.DEC)
	local f_removed = ProtoField.bytes(
		"minetest.server.active_object_remove_add_removed",
		"Removed object")
	local f_removed_id = ProtoField.uint16(
		"minetest.server.active_object_remove_add_removed_id",
		"ID", base.DEC)

	local f_added_count = ProtoField.uint16(
		"minetest.server.active_object_remove_add_added_count",
		"Count of added objects", base.DEC)
	local f_added = ProtoField.bytes(
		"minetest.server.active_object_remove_add_added",
		"Added object")
	local f_added_id = ProtoField.uint16(
		"minetest.server.active_object_remove_add_added_id",
		"ID", base.DEC)
	local f_added_type = ProtoField.uint8(
		"minetest.server.active_object_remove_add_added_type",
		"Type", base.DEC)
	local f_added_init_length = ProtoField.uint32(
		"minetest.server.active_object_remove_add_added_init_length",
		"Initialization data length", base.DEC)
	local f_added_init_data = ProtoField.bytes(
		"minetest.server.active_object_remove_add_added_init_data",
		"Initialization data")

	minetest_server_commands[0x31] = {
		"ACTIVE_OBJECT_REMOVE_ADD", 6,
		{ f_removed_count, f_removed, f_removed_id,
		  f_added_count, f_added, f_added_id,
		  f_added_type, f_added_init_length, f_added_init_data },
		function(buffer, pinfo, tree, t)
			local t2, index, pos

			local removed_count_pos = 2
			local removed_count = buffer(removed_count_pos, 2):uint()
			t:add(f_removed_count, buffer(removed_count_pos, 2))

			local added_count_pos = removed_count_pos + 2 + 2 * removed_count
			if not minetest_check_length(buffer, added_count_pos + 2, t) then
				return
			end

			-- Loop through removed active objects
			for index = 0, removed_count - 1 do
				pos = removed_count_pos + 2 + 2 * index
				t2 = t:add(f_removed, buffer(pos, 2))
				t2:set_text("Removed object, ID = " ..  buffer(pos, 2):uint())
				t2:add(f_removed_id, buffer(pos, 2))
			end

			local added_count = buffer(added_count_pos, 2):uint()
			t:add(f_added_count, buffer(added_count_pos, 2))

			-- Loop through added active objects
			pos = added_count_pos + 2
			for index = 0, added_count - 1 do
				if not minetest_check_length(buffer, pos + 7, t) then
					return
				end

				local init_length = buffer(pos + 3, 4):uint()
				if not minetest_check_length(buffer, pos + 7 + init_length, t) then
					return
				end

				t2 = t:add(f_added, buffer(pos, 7 + init_length))
				t2:set_text("Added object, ID = " .. buffer(pos, 2):uint())
				t2:add(f_added_id, buffer(pos, 2))
				t2:add(f_added_type, buffer(pos + 2, 1))
				t2:add(f_added_init_length, buffer(pos + 3, 4))
				t2:add(f_added_init_data, buffer(pos + 7, init_length))

				pos = pos + 7 + init_length
			end

			pinfo.cols.info:append(" * " .. (removed_count + added_count))
		end
	}
end

-- TOCLIENT_ACTIVE_OBJECT_MESSAGES

do
	local f_object_count = ProtoField.uint16(
		"minetest.server.active_object_messages_object_count",
		"Count of objects", base.DEC)
	local f_object = ProtoField.bytes(
		"minetest.server.active_object_messages_object",
		"Object")
	local f_object_id = ProtoField.uint16(
		"minetest.server.active_object_messages_id",
		"ID", base.DEC)
	local f_message_length = ProtoField.uint16(
		"minetest.server.active_object_messages_message_length",
		"Message length", base.DEC)
	local f_message = ProtoField.bytes(
		"minetest.server.active_object_messages_message",
		"Message")

	minetest_server_commands[0x32] = {
		"ACTIVE_OBJECT_MESSAGES", 2,
		{ f_object_count, f_object, f_object_id, f_message_length, f_message },
		function(buffer, pinfo, tree, t)
			local t2, count, pos, message_length

			count = 0
			pos = 2
			while pos < buffer:len() do
				if not minetest_check_length(buffer, pos + 4, t) then
					return
				end
				message_length = buffer(pos + 2, 2):uint()
				if not minetest_check_length(buffer, pos + 4 + message_length, t) then
					return
				end
				count = count + 1
				pos = pos + 4 + message_length
			end

			pinfo.cols.info:append(" * " .. count)
			t:add(f_object_count, count):set_generated()

			pos = 2
			while pos < buffer:len() do
				message_length = buffer(pos + 2, 2):uint()

				t2 = t:add(f_object, buffer(pos, 4 + message_length))
				t2:set_text("Object, ID = " ..  buffer(pos, 2):uint())
				t2:add(f_object_id, buffer(pos, 2))
				t2:add(f_message_length, buffer(pos + 2, 2))
				t2:add(f_message, buffer(pos + 4, message_length))

				pos = pos + 4 + message_length
			end
		end
	}
end

-- TOCLIENT_HP

do
	local f_hp = ProtoField.uint16("minetest.server.hp", "Health points", base.DEC)

	minetest_server_commands[0x33] = {
		"HP", 4,
		{ f_hp },
		function(buffer, pinfo, tree, t)
			t:add(f_hp, buffer(2,2))
		end
	}
end

-- TOCLIENT_MOVE_PLAYER

do
	local abbr = "minetest.server.move_player_"

	local f_x = ProtoField.float(abbr.."x", "Position X")
	local f_y = ProtoField.float(abbr.."y", "Position Y")
	local f_z = ProtoField.float(abbr.."z", "Position Z")
	local f_pitch = ProtoField.float(abbr.."_pitch", "Pitch")
	local f_yaw = ProtoField.float(abbr.."yaw", "Yaw")

	minetest_server_commands[0x34] = {
		"MOVE_PLAYER", 22,
		{ f_x, f_y, f_z, f_pitch, f_yaw, f_garbage },
		function(buffer, pinfo, tree, t)
			t:add(f_x, buffer(2, 4))
			t:add(f_y, buffer(6, 4))
			t:add(f_z, buffer(10, 4))
			t:add(f_pitch, buffer(14, 4))
			t:add(f_yaw, buffer(18, 4))
		end
	}
end

-- TOCLIENT_ACCESS_DENIED_LEGACY

do
	local f_reasonlen, f_reason = minetest_field_helper("uint16",
		"minetest.server.access_denied_reason", "Reason")

	minetest_server_commands[0x35] = {
		"ACCESS_DENIED_LEGACY", 4,
		{ f_reasonlen, f_reason },
		function(buffer, pinfo, tree, t)
			local off = 2
			minetest_decode_helper_utf16(buffer, t, "uint16", off, f_reasonlen, f_reason)
		end
	}
end

-- TOCLIENT_FOV

minetest_server_commands[0x36] = { "FOV", 2 }

-- TOCLIENT_DEATHSCREEN_LEGACY

minetest_server_commands[0x37] = { "DEATHSCREEN_LEGACY", 2 }

-- TOCLIENT_MEDIA

minetest_server_commands[0x38] = {"MEDIA", 2}

-- TOCLIENT_TOOLDEF (obsolete)

minetest_server_commands[0x39] = {"TOOLDEF", 2}
minetest_server_obsolete[0x39] = true

-- TOCLIENT_NODEDEF

minetest_server_commands[0x3a] = {"NODEDEF", 2}

-- TOCLIENT_CRAFTITEMDEF (obsolete)

minetest_server_commands[0x3b] = {"CRAFTITEMDEF", 2}
minetest_server_obsolete[0x3b] = true

-- ...

minetest_server_commands[0x3c] = {"ANNOUNCE_MEDIA", 2}
minetest_server_commands[0x3d] = {"ITEMDEF", 2}
minetest_server_commands[0x3f] = {"PLAY_SOUND", 2}
minetest_server_commands[0x40] = {"STOP_SOUND", 2}
minetest_server_commands[0x41] = {"PRIVILEGES", 2}
minetest_server_commands[0x42] = {"INVENTORY_FORMSPEC", 2}
minetest_server_commands[0x43] = {"DETACHED_INVENTORY", 2}
minetest_server_commands[0x44] = {"SHOW_FORMSPEC", 2}
minetest_server_commands[0x45] = {"MOVEMENT", 2}
minetest_server_commands[0x46] = {"SPAWN_PARTICLE", 2}
minetest_server_commands[0x47] = {"ADD_PARTICLESPAWNER", 2}
minetest_server_commands[0x48] = {"CAMERA", 2}
minetest_server_commands[0x49] = {"HUDADD", 2}
minetest_server_commands[0x4a] = {"HUDRM", 2}

-- TOCLIENT_HUDCHANGE

do
	local abbr = "minetest.server.hudchange_"
	local vs_stat = {
		[0] = "pos",
		[1] = "name",
		[2] = "scale",
		[3] = "text",
		[4] = "number",
		[5] = "item",
		[6] = "dir",
		[7] = "align",
		[8] = "offset",
		[9] = "world_pos",
		[10] = "size",
		[11] = "z_index",
		[12] = "text2",
		[13] = "style",
	}
	local uses_v2f = {
		[0] = true, [2] = true, [7] = true, [8] = true,
	}
	local uses_string = {
		[1] = true, [3] = true, [12] = true,
	}

	local f_id = ProtoField.uint32(abbr.."id", "HUD ID", base.DEC)
	local f_stat = ProtoField.uint8(abbr.."stat", "HUD Type", base.DEC, vs_stat)
	local f_x = ProtoField.float(abbr.."x", "Position X")
	local f_y = ProtoField.float(abbr.."y", "Position Y")
	local f_z = ProtoField.float(abbr.."z", "Position Z")
	local f_str = ProtoField.string(abbr.."string", "String data")
	local f_sx = ProtoField.int32(abbr.."sy", "Size X", base.DEC)
	local f_sy = ProtoField.int32(abbr.."sz", "Size Y", base.DEC)
	local f_int = ProtoField.uint32(abbr.."int", "Integer data", base.DEC)

	minetest_server_commands[0x4b] = {
		"HUDCHANGE", 7,
		{ f_id, f_stat, f_x, f_y, f_z, f_str, f_sx, f_sy, f_int, },
		function(buffer, pinfo, tree, t)
			t:add(f_id, buffer(2,4))
			t:add(f_stat, buffer(6,1))
			local stat = buffer(6,1):uint()
			local off = 7
			if uses_v2f[stat] then
				t:add(f_x, buffer(off,4))
				t:add(f_y, buffer(off+4,4))
			elseif uses_string[stat] then
				minetest_decode_helper_ascii(buffer, t, "uint16", off, nil, f_str)
			elseif stat == 9 then -- v3f
				t:add(f_x, buffer(off,4))
				t:add(f_y, buffer(off+4,4))
				t:add(f_z, buffer(off+8,4))
			elseif stat == 10 then -- v2s32
				t:add(f_sx, buffer(off,4))
				t:add(f_sy, buffer(off+8,4))
			else
				t:add(f_int, buffer(off,4))
			end
		end
	}
end

-- ...

minetest_server_commands[0x4c] = {"HUD_SET_FLAGS", 2}
minetest_server_commands[0x4d] = {"HUD_SET_PARAM", 2}
minetest_server_commands[0x4e] = {"BREATH", 2}
minetest_server_commands[0x4f] = {"SET_SKY", 2}
minetest_server_commands[0x50] = {"OVERRIDE_DAY_NIGHT_RATIO", 2}
minetest_server_commands[0x51] = {"LOCAL_PLAYER_ANIMATIONS", 2}
minetest_server_commands[0x52] = {"EYE_OFFSET", 2}
minetest_server_commands[0x53] = {"DELETE_PARTICLESPAWNER", 2}
minetest_server_commands[0x54] = {"CLOUD_PARAMS", 2}
minetest_server_commands[0x55] = {"FADE_SOUND", 2}

-- TOCLIENT_UPDATE_PLAYER_LIST

do
	local abbr = "minetest.server.update_player_list_"
	local vs_type = {
		[0] = "Init",
		[1] = "Add",
		[2] = "Remove",
	}

	local f_type = ProtoField.uint8(abbr.."type", "Type", base.DEC, vs_type)
	local f_count = ProtoField.uint16(abbr.."count", "Number of players", base.DEC)
	local f_name = ProtoField.string(abbr.."name", "Name")

	minetest_server_commands[0x56] = {
		"UPDATE_PLAYER_LIST",
		5,
		{ f_type, f_count, f_name },
		function(buffer, pinfo, tree, t)
			t:add(f_type, buffer(2,1))
			t:add(f_count, buffer(3,2))
			local count = buffer(3,2):uint()
			local off = 5
			for i = 1, count do
				if not minetest_check_length(buffer, off + 2, t) then
					return
				end
				off = minetest_decode_helper_ascii(buffer, t, "uint16", off, nil, f_name)
				if not off then
					return
				end
			end
		end
	}
end

-- ...

minetest_server_commands[0x57] = {"MODCHANNEL_MSG", 2}
minetest_server_commands[0x58] = {"MODCHANNEL_SIGNAL", 2}
minetest_server_commands[0x59] = {"NODEMETA_CHANGED", 2}
minetest_server_commands[0x5a] = {"SET_SUN", 2}
minetest_server_commands[0x5b] = {"SET_MOON", 2}
minetest_server_commands[0x5c] = {"SET_STARS", 2}
minetest_server_commands[0x5d] = {"MOVE_PLAYER_REL", 2}
minetest_server_commands[0x60] = {"SRP_BYTES_S_B", 2}
minetest_server_commands[0x61] = {"FORMSPEC_PREPEND", 2}
minetest_server_commands[0x62] = {"MINIMAP_MODES", 2}
minetest_server_commands[0x63] = {"SET_LIGHTING", 2}


------------------------------------
-- Part 4                         --
-- Wrapper protocol subdissectors --
------------------------------------

-- minetest.control dissector

do
	local p_control = Proto("minetest.control", "Luanti Control")

	local vs_control_type = {
		[0] = "Ack",
		[1] = "Set Peer ID",
		[2] = "Ping",
		[3] = "Disconnect"
	}

	local f_control_type = ProtoField.uint8("minetest.control.type", "Control Type", base.DEC, vs_control_type)
	local f_control_ack = ProtoField.uint16("minetest.control.ack", "ACK sequence number", base.DEC)
	local f_control_peerid = ProtoField.uint8("minetest.control.peerid", "New peer ID", base.DEC)
	p_control.fields = { f_control_type, f_control_ack, f_control_peerid }

	local data_dissector = Dissector.get("data")

	function p_control.dissector(buffer, pinfo, tree)
		local t = tree:add(p_control, buffer(0,1))
		t:add(f_control_type, buffer(0,1))

		pinfo.cols.info = "Control message"

		local pos = 1
		if buffer(0,1):uint() == 0 then
			pos = 3
			t:set_len(3)
			t:add(f_control_ack, buffer(1,2))
			pinfo.cols.info = "Ack " .. buffer(1,2):uint()
		elseif buffer(0,1):uint() == 1 then
			pos = 3
			t:set_len(3)
			t:add(f_control_peerid, buffer(1,2))
			pinfo.cols.info = "Set peer ID " .. buffer(1,2):uint()
		elseif buffer(0,1):uint() == 2 then
			pinfo.cols.info = "Ping"
		elseif buffer(0,1):uint() == 3 then
			pinfo.cols.info = "Disconnect"
		end

		data_dissector:call(buffer(pos):tvb(), pinfo, tree)
	end
end

-- minetest.client dissector
-- minetest.server dissector

-- Defines the minetest.client or minetest.server Proto. These two protocols
-- are created by the same function because they are so similar.
-- Parameter: proto: the Proto object
-- Parameter: this_peer: "Client" or "Server"
-- Parameter: other_peer: "Server" or "Client"
-- Parameter: commands: table of command information, built above
-- Parameter: obsolete: table of obsolete commands, built above
function minetest_define_client_or_server_proto(is_client)
	-- Differences between minetest.client and minetest.server
	local proto_name, this_peer, other_peer, empty_message_info
	local commands, obsolete
	if is_client then
		proto_name = "minetest.client"
		this_peer = "Client"
		other_peer = "Server"
		empty_message_info = "Empty message / Connect"
		commands = minetest_client_commands  -- defined in Part 2
		obsolete = minetest_client_obsolete  -- defined in Part 2
	else
		proto_name = "minetest.server"
		this_peer = "Server"
		other_peer = "Client"
		empty_message_info = "Empty message"
		commands = minetest_server_commands  -- defined in Part 3
		obsolete = minetest_server_obsolete  -- defined in Part 3
	end

	-- Create the protocol object.
	local proto = Proto(proto_name, "Luanti " .. this_peer .. " to " .. other_peer)

	-- Create a table vs_command that maps command codes to command names.
	local vs_command = {}
	local code, command_info
	for code, command_info in pairs(commands) do
		local command_name = command_info[1]
		vs_command[code] = "TO" .. other_peer:upper() .. "_" .. command_name
	end

	-- Field definitions
	local f_command = ProtoField.uint16(proto_name .. ".command", "Command", base.HEX, vs_command)
	local f_empty = ProtoField.bool(proto_name .. ".empty", "Is empty", BASE_NONE)
	proto.fields = { f_command, f_empty }

	-- Add command-specific fields to the protocol
	for code, command_info in pairs(commands) do
		local command_fields = command_info[3]
		if command_fields ~= nil then
			for index, field in ipairs(command_fields) do
				assert(field ~= nil)
				table.insert(proto.fields, field)
			end
		end
	end

	-- minetest.client or minetest.server dissector function
	function proto.dissector(buffer, pinfo, tree)
		local t = tree:add(proto, buffer)

		-- If we're nested, don't reset
		if string.find(tostring(pinfo.cols.info), this_peer, 1, true) ~= 1 then
			pinfo.cols.info = this_peer
		end

		if buffer:len() == 0 then
			-- Empty message.
			t:add(f_empty, 1):set_generated()
			pinfo.cols.info:append(": " .. empty_message_info)

		elseif minetest_check_length(buffer, 2, t) then
			-- Get the command code.
			t:add(f_command, buffer(0,2))
			local code = buffer(0,2):uint()
			local command_info = commands[code]
			if command_info == nil then
				-- Error: Unknown command.
				pinfo.cols.info:append(": Unknown command")
				t:add_expert_info(PI_UNDECODED, PI_WARN, "Unknown " .. this_peer .. " to " .. other_peer .. " command")
			else
				-- Process a known command
				local command_name = command_info[1]
				local command_min_length = command_info[2]
				local command_fields = command_info[3]
				local command_dissector = command_info[4]
				pinfo.cols.info:append(": " .. command_name)
				if minetest_check_length(buffer, command_min_length, t) then
					if command_dissector ~= nil then
						command_dissector(buffer, pinfo, tree, t)
					end
				end
				if obsolete[code] then
					t:add_expert_info(PI_REQUEST_CODE, PI_WARN, "Obsolete command.")
				end
			end
		end
	end
end

minetest_define_client_or_server_proto(true)  -- minetest.client
minetest_define_client_or_server_proto(false) -- minetest.server

-- minetest.split dissector

do
	local p_split = Proto("minetest.split", "Luanti Split Message")

	local f_split_seq = ProtoField.uint16("minetest.split.seq", "Sequence number", base.DEC)
	local f_split_chunkcount = ProtoField.uint16("minetest.split.chunkcount", "Chunk count", base.DEC)
	local f_split_chunknum = ProtoField.uint16("minetest.split.chunknum", "Chunk number", base.DEC)
	local f_split_data = ProtoField.bytes("minetest.split.data", "Split message data")
	p_split.fields = { f_split_seq, f_split_chunkcount, f_split_chunknum, f_split_data }

	function p_split.dissector(buffer, pinfo, tree)
		local t = tree:add(p_split, buffer(0,6))
		t:add(f_split_seq, buffer(0,2))
		t:add(f_split_chunkcount, buffer(2,2))
		t:add(f_split_chunknum, buffer(4,2))
		t:add(f_split_data, buffer(6))
		pinfo.cols.info:append(" " .. buffer(0,2):uint() .. " chunk " .. buffer(4,2):uint() .. "/" .. buffer(2,2):uint())
		-- If it's the first chunk, read the peer id from the upper layer and
		-- pass the data that we have to the right dissector.
		-- this provides at least a partial decoding
		if buffer(4,2):uint() == 0 then
			local peer_id = minetest_peer_field()
			if peer_id.value and peer_id.value == 1 then
				Dissector.get("minetest.server"):call(buffer(6):tvb(), pinfo, tree)
			elseif peer_id.value then
				Dissector.get("minetest.client"):call(buffer(6):tvb(), pinfo, tree)
			end
		end
	end
end




-------------------------------------
-- Part 5                          --
-- Wrapper protocol main dissector --
-------------------------------------

do
	local p_minetest = Proto("minetest", "Luanti")

	local minetest_id = 0x4f457403
	local vs_id = {
		[minetest_id] = "Valid"
	}

	local vs_peer = {
		[0] = "Inexistent",
		[1] = "Server"
	}

	local vs_type = {
		[0] = "Control",
		[1] = "Original",
		[2] = "Split",
		[3] = "Reliable"
	}

	local f_id = ProtoField.uint32("minetest.id", "Protocol ID", base.HEX, vs_id)
	local f_peer = ProtoField.uint16("minetest.peer", "Peer", base.DEC, vs_peer)
	local f_channel = ProtoField.uint8("minetest.channel", "Channel", base.DEC)
	local f_type = ProtoField.uint8("minetest.type", "Type", base.DEC, vs_type)
	local f_seq = ProtoField.uint16("minetest.seq", "Sequence number", base.DEC)
	local f_subtype = ProtoField.uint8("minetest.subtype", "Subtype", base.DEC, vs_type)

	p_minetest.fields = { f_id, f_peer, f_channel, f_type, f_seq, f_subtype }

	local data_dissector = Dissector.get("data")
	local control_dissector = Dissector.get("minetest.control")
	local client_dissector = Dissector.get("minetest.client")
	local server_dissector = Dissector.get("minetest.server")
	local split_dissector = Dissector.get("minetest.split")

	function p_minetest.dissector(buffer, pinfo, tree)

		-- Defer if payload doesn't have Minetest's magic number
		if buffer(0,4):uint() ~= minetest_id then
			return false
		end

		-- Add Minetest tree item
		local t = tree:add(p_minetest, buffer(0,8))
		t:add(f_id, buffer(0,4))

		-- ID is valid, so replace packet's shown protocol
		pinfo.cols.protocol = "Luanti"
		pinfo.cols.info = "Luanti"

		-- Set the other header fields
		local peer_id = buffer(4,2):uint()
		t:add(f_peer, buffer(4,2))
		t:add(f_channel, buffer(6,1))
		t:add(f_type, buffer(7,1))
		t:set_text("Luanti, Peer: " .. peer_id .. ", Channel: " .. buffer(6,1):uint())

		local reliability_info
		local pos
		if buffer(7,1):uint() == 3 then
			-- Reliable message
			reliability_info = "Seq=" .. buffer(8,2):uint()
			t:set_len(11)
			t:add(f_seq, buffer(8,2))
			t:add(f_subtype, buffer(10,1))
			pos = 10
		else
			-- Unreliable message
			reliability_info = "Unrel"
			pos = 7
		end

		if buffer(pos,1):uint() == 0 then
			-- Control message, possibly reliable
			control_dissector:call(buffer(pos+1):tvb(), pinfo, tree)
		elseif buffer(pos,1):uint() == 1 then
			-- Original message, possibly reliable
			if peer_id == 1 then
				server_dissector:call(buffer(pos+1):tvb(), pinfo, tree)
			else
				client_dissector:call(buffer(pos+1):tvb(), pinfo, tree)
			end
		elseif buffer(pos,1):uint() == 2 then
			-- Split message, possibly reliable
			if peer_id == 1 then
				pinfo.cols.info = "Server: Split message"
			else
				pinfo.cols.info = "Client: Split message"
			end
			split_dissector:call(buffer(pos+1):tvb(), pinfo, tree)
		elseif buffer(pos,1):uint() == 3 then
			-- Doubly reliable message??
			t:add_expert_info(PI_MALFORMED, PI_ERROR, "Reliable message wrapped in reliable message")
		else
			data_dissector:call(buffer(pos+1):tvb(), pinfo, tree)
		end

		pinfo.cols.info:append(" (" .. reliability_info .. ")")
		return true

	end

	p_minetest:register_heuristic("udp", p_minetest.dissector)
end




------------------------------
-- Part 6                   --
-- Utility functions part 2 --
------------------------------

minetest_peer_field = Field.new("minetest.peer")

-- Checks if a (sub-)Tvb is long enough to be further dissected.
-- If it is long enough, sets the dissector tree item length to min_len
-- and returns true. If it is not long enough, adds expert info to the
-- dissector tree and returns false.
-- Parameter: tvb: the Tvb
-- Parameter: min_len: required minimum length
-- Parameter: t: dissector tree item
-- Returns: true if tvb:len() >= min_len, false otherwise
function minetest_check_length(tvb, min_len, t)
	if tvb:len() >= min_len then
		t:set_len(min_len)
		return true

	-- TODO: check if other parts of
	-- the dissector could benefit from reported_length_remaining
	elseif tvb:reported_length_remaining() >= min_len then
		t:add_expert_info(PI_UNDECODED, PI_INFO, "Only part of this packet was captured, unable to decode.")
		return false

	else
		t:add_expert_info(PI_MALFORMED, PI_ERROR, "Message is too short")
		return false
	end
end

-- Decodes a variable-length string as ASCII text
-- t_textlen, t_text should be the ProtoFields created by minetest_field_helper
--   alternatively t_text can be a ProtoField.string and t_textlen can be nil
-- lentype must be the type of the length field (as passed to minetest_field_helper)
-- returns nil if length check failed
function minetest_decode_helper_ascii(tvb, t, lentype, offset, f_textlen, f_text)
	local n = ({uint16 = 2, uint32 = 4})[lentype]
	assert(n)

	if f_textlen then
		t:add(f_textlen, tvb(offset, n))
	end
	local textlen = tvb(offset, n):uint()
	if minetest_check_length(tvb, offset + n + textlen, t) then
		t:add(f_text, tvb(offset + n, textlen))
		return offset + n + textlen
	end
end

-- Decodes a variable-length string as UTF-16 text
-- (see minetest_decode_helper_ascii)
function minetest_decode_helper_utf16(tvb, t, lentype, offset, f_textlen, f_text)
	local n = ({uint16 = 2, uint32 = 4})[lentype]
	assert(n)

	if f_textlen then
		t:add(f_textlen, tvb(offset, n))
	end
	local textlen = tvb(offset, n):uint() * 2
	if minetest_check_length(tvb, offset + n + textlen, t) then
		t:add(f_text, tvb(offset + n, textlen), tvb(offset + n, textlen):ustring())
		return offset + n + textlen
	end
end
