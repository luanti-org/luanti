-- This command tests the new_stair setting.
local stair_switch = false
core.register_chatcommand("newstair",{
	func = function(name)
		local player = core.get_player_by_name(name)
		if not player then return end
		stair_switch = not stair_switch
		player:set_physics_override({
			new_stair = stair_switch
		})
		core.chat_send_player(name, "new_stair is now: " .. tostring(stair_switch))
	end
})