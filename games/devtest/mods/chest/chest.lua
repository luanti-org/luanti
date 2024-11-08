local function print_to_everything(msg)
	core.log("action", "[chest] " .. msg)
	core.chat_send_all(msg)
end

core.register_node("chest:chest", {
	description = "Chest" .. "\n" ..
		"32 inventory slots",
	tiles ={"chest_chest.png^[sheet:2x2:0,0", "chest_chest.png^[sheet:2x2:0,0",
		"chest_chest.png^[sheet:2x2:1,0", "chest_chest.png^[sheet:2x2:1,0",
		"chest_chest.png^[sheet:2x2:1,0", "chest_chest.png^[sheet:2x2:0,1"},
	paramtype2 = "4dir",
	groups = {dig_immediate=2,choppy=3,meta_is_privatizable=1},
	is_ground_content = false,
	on_construct = function(pos)
		local meta = core.get_meta(pos)
		meta:set_string("formspec",
				"size[8,9]"..
				"list[current_name;main;0,0;8,4;]"..
				"list[current_player;main;0,5;8,4;]" ..
				"listring[]")
		meta:set_string("infotext", "Chest")
		local inv = meta:get_inventory()
		inv:set_size("main", 8*4)
	end,
	can_dig = function(pos,player)
		local meta = core.get_meta(pos);
		local inv = meta:get_inventory()
		return inv:is_empty("main")
	end,
	allow_metadata_inventory_put = function(pos, listname, index, stack, player)
		print_to_everything("Chest: ".. player:get_player_name() .. " triggered 'allow put' (10) event for " .. stack:to_string())
		return 10
	end,
	allow_metadata_inventory_take = function(pos, listname, index, stack, player)
		print_to_everything("Chest: ".. player:get_player_name() .. " triggered 'allow take' (20) event for " .. stack:to_string())
		return 20
	end,
	allow_metadata_inventory_move = function(pos, from_list, from_index, to_list, to_index, count, player)
		print_to_everything("Chest: ".. player:get_player_name() .. " triggered 'allow move' (30) event")
		return 30
	end,
	on_metadata_inventory_put = function(pos, listname, index, stack, player)
		print_to_everything("Chest: ".. player:get_player_name() .. " put " .. stack:to_string())
	end,
	on_metadata_inventory_take = function(pos, listname, index, stack, player)
		print_to_everything("Chest: ".. player:get_player_name() .. " took " .. stack:to_string())
	end,
	on_metadata_inventory_move = function(pos, from_list, from_index, to_list, to_index, count, player)
		print_to_everything("Chest: ".. player:get_player_name() .. " moved " .. count)
	end,
})
