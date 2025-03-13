local give_if_not_gotten_already = function(inv, list, item)
	if not inv:contains_item(list, item) then
		inv:add_item(list, item)
	end
end

local give_initial_stuff = function(player)
	local inv = player:get_inventory()
	give_if_not_gotten_already(inv, "main", "basetools:pick_mese")
	give_if_not_gotten_already(inv, "main", "basetools:axe_steel")
	give_if_not_gotten_already(inv, "main", "basetools:shovel_steel")
	give_if_not_gotten_already(inv, "main", "bucket:bucket")
	give_if_not_gotten_already(inv, "main", "testnodes:light14")
	give_if_not_gotten_already(inv, "main", "chest_of_everything:bag")
	core.log("action", "[give_initial_stuff] Giving initial stuff to "..player:get_player_name())
end

core.register_on_newplayer(function(player)
	if core.settings:get_bool("give_initial_stuff", true) then
		give_initial_stuff(player)
	end
end)

core.register_chatcommand("stuff", {
	params = "",
	privs = { give = true },
	description = "Give yourself initial items",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player or not player:is_player() then
			return false, "No player."
		end
		give_initial_stuff(player)
		return true
	end,
})

