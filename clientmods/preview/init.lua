local modname = assert(core.get_current_modname())
local modstorage = core.get_mod_storage()
local mod_channel

dofile(core.get_modpath(modname) .. "example.lua")

core.register_on_shutdown(function()
	print("[PREVIEW] shutdown client")
end)
local id = nil

do
	local server_info = core.get_server_info()
	print("Server version: " .. server_info.protocol_version)
	print("Server ip: " .. server_info.ip)
	print("Server address: " .. server_info.address)
	print("Server port: " .. server_info.port)

	print("CSM restrictions: " .. dump(core.get_csm_restrictions()))

	local l1, l2 = core.get_language()
	print("Configured language: " .. l1 .. " / " .. l2)
end

mod_channel = core.mod_channel_join("experimental_preview")

core.after(4, function()
	if mod_channel:is_writeable() then
		mod_channel:send_all("preview talk to experimental")
	end
end)

core.after(1, function()
	print("armor: " .. dump(core.localplayer:get_armor_groups()))
	id = core.localplayer:hud_add({
			type = "text",
			name = "example",
			number = 0xff0000,
			position = {x=0, y=1},
			offset = {x=8, y=-8},
			text = "You are using the preview mod",
			scale = {x=200, y=60},
			alignment = {x=1, y=-1},
	})
end)

core.register_on_modchannel_message(function(channel, sender, message)
	print("[PREVIEW][modchannels] Received message `" .. message .. "` on channel `"
			.. channel .. "` from sender `" .. sender .. "`")
	core.after(1, function()
		mod_channel:send_all("CSM preview received " .. message)
	end)
end)

core.register_on_modchannel_signal(function(channel, signal)
	print("[PREVIEW][modchannels] Received signal id `" .. signal .. "` on channel `"
			.. channel)
end)

core.register_on_inventory_open(function(inventory)
	print("INVENTORY OPEN")
	print(dump(inventory))
	return false
end)

core.register_on_placenode(function(pointed_thing, node)
	print("The local player place a node!")
	print("pointed_thing :" .. dump(pointed_thing))
	print("node placed :" .. dump(node))
	return false
end)

core.register_on_item_use(function(itemstack, pointed_thing)
	print("The local player used an item!")
	print("pointed_thing :" .. dump(pointed_thing))
	print("item = " .. itemstack:get_name())

	if not itemstack:is_empty() then
		return false
	end

	local pos = core.camera:get_pos()
	local pos2 = vector.add(pos, vector.multiply(core.camera:get_look_dir(), 100))

	local rc = core.raycast(pos, pos2)
	local i = rc:next()
	print("[PREVIEW] raycast next: " .. dump(i))
	if i then
		print("[PREVIEW] line of sight: " .. (core.line_of_sight(pos, i.above) and "yes" or "no"))

		local n1 = core.find_nodes_in_area(pos, i.under, {"default:stone"})
		local n2 = core.find_nodes_in_area_under_air(pos, i.under, {"default:stone"})
		print(("[PREVIEW] found %s nodes, %s nodes under air"):format(
				n1 and #n1 or "?", n2 and #n2 or "?"))
	end

	return false
end)

-- This is an example function to ensure it's working properly, should be removed before merge
core.register_on_receiving_chat_message(function(message)
	print("[PREVIEW] Received message " .. message)
	return false
end)

-- This is an example function to ensure it's working properly, should be removed before merge
core.register_on_sending_chat_message(function(message)
	print("[PREVIEW] Sending message " .. message)
	return false
end)

core.register_on_chatcommand(function(command, params)
	print("[PREVIEW] caught command '"..command.."'. Parameters: '"..params.."'")
end)

-- This is an example function to ensure it's working properly, should be removed before merge
core.register_on_hp_modification(function(hp)
	print("[PREVIEW] HP modified " .. hp)
end)

-- This is an example function to ensure it's working properly, should be removed before merge
core.register_on_damage_taken(function(hp)
	print("[PREVIEW] Damage taken " .. hp)
end)

-- This is an example function to ensure it's working properly, should be removed before merge
core.register_chatcommand("dump", {
	func = function(param)
		return true, dump(_G)
	end,
})

local function preview_minimap()
	local minimap = core.ui.minimap
	if not minimap then
		print("[PREVIEW] Minimap is disabled. Skipping.")
		return
	end
	minimap:set_mode(4)
	minimap:show()
	minimap:set_pos({x=5, y=50, z=5})
	minimap:set_shape(math.random(0, 1))

	print("[PREVIEW] Minimap: mode => " .. dump(minimap:get_mode()) ..
			" position => " .. dump(minimap:get_pos()) ..
			" angle => " .. dump(minimap:get_angle()))
end

core.after(2, function()
	print("[PREVIEW] loaded " .. modname .. " mod")
	modstorage:set_string("current_mod", modname)
	assert(modstorage:get_string("current_mod") == modname)
	preview_minimap()
end)

core.after(5, function()
	if core.ui.minimap then
		core.ui.minimap:show()
	end

	print("[PREVIEW] Time of day " .. core.get_timeofday())

	print("[PREVIEW] Node level: " .. core.get_node_level({x=0, y=20, z=0}) ..
		" max level " .. core.get_node_max_level({x=0, y=20, z=0}))

	print("[PREVIEW] Find node near: " .. dump(core.find_node_near({x=0, y=20, z=0}, 10,
		{"group:tree", "default:dirt", "default:stone"})))

	print("[PREVIEW] Settings: preview_csm_test_setting = " ..
		tostring(core.settings:get_bool("preview_csm_test_setting", false)))
end)

core.register_on_dignode(function(pos, node)
	print("The local player dug a node!")
	print("pos:" .. dump(pos))
	print("node:" .. dump(node))
	return false
end)

core.register_on_punchnode(function(pos, node)
	print("The local player punched a node!")
	local itemstack = core.localplayer:get_wielded_item()
	print(dump(itemstack:to_table()))
	print("pos:" .. dump(pos))
	print("node:" .. dump(node))
	local meta = core.get_meta(pos)
	print("punched meta: " .. (meta and dump(meta:to_table()) or "(missing)"))
	return false
end)

core.register_chatcommand("privs", {
	func = function(param)
		return true, core.privs_to_string(minetest.get_privilege_list())
	end,
})

core.register_chatcommand("text", {
	func = function(param)
		return core.localplayer:hud_change(id, "text", param)
	end,
})


core.register_on_mods_loaded(function()
	core.log("Yeah preview mod is loaded with other CSM mods.")
end)


-- Test commands

local id_list = {}
core.register_chatcommand("test_hud", {
	func = function(param)
		if param == "list" then
			print(dump(core.localplayer:hud_get_all()))
			return true, "See stdout"
		elseif param == "add" then
			local n = math.random(5) + 1
			for i = 1, n do
				id_list[#id_list + 1] = core.localplayer:hud_add({
					type = "text",
					position = {x=math.random(), y=math.random()},
					name = "lab:" .. math.random(999),
					text = math.random(999999)
				})
			end
			return true, "Added " .. n .. " elements"
		elseif param == "clear" then
			local n = #id_list
			for _, id in ipairs(id_list) do
				assert(core.localplayer:hud_remove(id))
			end
			id_list = {}
			return true, "Removed " .. n .. " elements"
		end

		return false, "Unknown command?"
	end
})
