local ALLOW_PUT_MAX = 1
local ALLOW_TAKE_MAX = 4

local function print_to_everything(msg)
	core.log("action", "[chest] " .. msg)
	core.chat_send_all(msg)
end

-- Create a detached inventory
local inv = core.create_detached_inventory("detached_inventory", {
	allow_move = function(inv, from_list, from_index, to_list, to_index, count, player)
		print_to_everything("Detached inventory: "..player:get_player_name().." triggered allow_move")
		return count -- Allow all
	end,
	allow_put = function(inv, listname, index, stack, player)
		print_to_everything("Detached inventory: "..player:get_player_name().." triggered allow_put for "..stack:to_string().." (max. allowed: "..ALLOW_PUT_MAX..")")
		return ALLOW_PUT_MAX -- Allow to put a limited amount of items
	end,
	allow_take = function(inv, listname, index, stack, player)
		print_to_everything("Detached inventory: "..player:get_player_name().." triggered allow_take for "..stack:to_string().." (max. allowed: "..ALLOW_TAKE_MAX..")")
		return ALLOW_TAKE_MAX -- Allow to take a limited amount of items
	end,
	on_move = function(inv, from_list, from_index, to_list, to_index, count, player)
		print_to_everything("Detached inventory: " .. player:get_player_name().." moved item(s)")
	end,
	on_put = function(inv, listname, index, stack, player)
		print_to_everything("Detached inventory: " .. player:get_player_name().." put "..stack:to_string())
	end,
	on_take = function(inv, listname, index, stack, player)
		print_to_everything("Detached inventory: " .. player:get_player_name().." took "..stack:to_string())
	end,
})
inv:set_size("main", 8*3)


-- Add a special chest to grant access to this inventory
core.register_node("chest:detached_chest", {
	description = "Detached Chest" .. "\n" ..
		"Grants access to a shared detached inventory" .."\n" ..
		"Max. item put count: "..ALLOW_PUT_MAX .."\n"..
		"Max. item take count: "..ALLOW_TAKE_MAX,
	tiles = {"chest_detached_chest.png^[sheet:2x2:0,0", "chest_detached_chest.png^[sheet:2x2:0,0",
		"chest_detached_chest.png^[sheet:2x2:1,0", "chest_detached_chest.png^[sheet:2x2:1,0",
		"chest_detached_chest.png^[sheet:2x2:1,0", "chest_detached_chest.png^[sheet:2x2:0,1"},
	paramtype2 = "4dir",
	groups = {dig_immediate=2,choppy=3,meta_is_privatizable=1},
	is_ground_content = false,
	on_construct = function(pos)
		local meta = core.get_meta(pos)
		meta:set_string("formspec",
			"size[8,9]"..
			"list[detached:detached_inventory;main;0,0;8,3;0]"..
			"list[current_player;main;0,5;8,4;]"..
			"listring[]")
		meta:set_string("infotext", "Detached Chest")
	end,
})

