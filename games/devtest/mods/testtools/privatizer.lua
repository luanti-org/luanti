core.register_tool("testtools:privatizer", {
	description = "Node Meta Privatizer".."\n"..
		"Punch: Marks 'infotext' and 'formspec' meta fields of chest as private",
	inventory_image = "testtools_privatizer.png",
	groups = { testtool = 1, disable_repair = 1 },
	on_use = function(itemstack, user, pointed_thing)
		if pointed_thing.type == "node" then
			local node = core.get_node(pointed_thing.under)
			if core.get_item_group(node.name, "meta_is_privatizable") == 1 then
				local p = pointed_thing.under
				core.log("action", "[testtools] Privatizer used at "..core.pos_to_string(p))
				core.get_meta(p):mark_as_private({"infotext", "formspec"})
				if user and user:is_player() then
					core.chat_send_player(user:get_player_name(), "Node metadata (infotext, formspec) set private!")
				end
				return
			elseif node.name == "chest_of_everything:chest" then
				if user and user:is_player() then
					core.chat_send_player(user:get_player_name(), "Privatizer can't be used on the Chest of Everything. Use it on a normal chest.")
				end
				return
			end
		end
		if user and user:is_player() then
			core.chat_send_player(user:get_player_name(), "Privatizer can only be used on chest!")
		end
	end,
})

