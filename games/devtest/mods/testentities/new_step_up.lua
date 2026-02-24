-- This command tests the new_step_up setting.
local stair_switch = false
core.register_chatcommand("newstepup", {
	func = function(name)
		local player = core.get_player_by_name(name)
		if not player then return end
		stair_switch = not stair_switch
		player:set_properties({ new_step_up = stair_switch })
		core.chat_send_player(name, "new_step_up is now: " .. tostring(stair_switch))
	end
})

core.register_chatcommand("sniff", {
	func = function(name)
		local player = core.get_player_by_name(name)
		if not player then return end
		local pos = player:get_pos()
		pos.y = pos.y + 0.5
		core.add_entity(pos, "testentities:stair_sniffer")
	end
})

core.register_entity("testentities:stair_sniffer", {
	initial_properties = {
		physical = true,
		collide_with_objects = false,
		visual = "cube",
		textures = { "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png", "testentities_cube1.png" },
	},
	on_activate = function(self, staticdata, dtime_s)
		self.object:set_acceleration(vector.new(0, -10, 0))
		self.object:set_properties({
			-- This setting.
			new_step_up = true,
			stepheight = 0.5,
		})
	end,
	on_step = function(self, dtime, moveresult)
		local touching_ground = moveresult.touching_ground
		local x_vel = self.object:get_velocity().x
		if x_vel < 1 then
			self.object:add_velocity(vector.new(0.05, 0, 0))
		end
		if touching_ground and x_vel == 0 then
			self.object:add_velocity(vector.new(0, 5, 0))
		end
	end,
})
