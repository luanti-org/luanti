-- Test for `notify_open[]` in the player inventory formspec

local states = {}

local function make_formspec(state)
	return "size[8,6]notify_open[true]" ..
		"label[0,0;" .. core.formspec_escape("Inventory opened " ..
			state.open_count .. " time(s) with notify_open[]") .. "]" ..
		"label[0,0.5;" .. core.formspec_escape("Game time when opened: " ..
			string.format("%.1f", core.get_gametime())) .. "]" ..
		"list[current_player;main;0,1.75;8,4;]"
end

core.register_on_formspec_open(function(player, formname)
	local state = states[player:get_player_name()]
	if formname ~= "" or not state then
		return
	end
	state.open_count = state.open_count + 1
	player:set_inventory_formspec(make_formspec(state))
end)

core.register_on_leaveplayer(function(player)
	states[player:get_player_name()] = nil
end)

core.register_chatcommand("test_notify_open", {
	params = "",
	description = "Test: Toggle an inventory formspec that is generated on open",
	func = function(name)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "No player."
		end
		local state = states[name]
		if state then
			player:set_inventory_formspec(state.old_formspec)
			states[name] = nil
			return true, "notify_open[] inventory disabled."
		end
		state = {open_count = 0, old_formspec = player:get_inventory_formspec()}
		states[name] = state
		player:set_inventory_formspec(make_formspec(state))
		return true, "notify_open[] inventory enabled. Open your inventory."
	end,
})
