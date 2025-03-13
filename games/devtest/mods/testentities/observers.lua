local function player_names_excluding(exclude_player_name)
	local player_names = {}
	for _, player in ipairs(core.get_connected_players()) do
		player_names[player:get_player_name()] = true
	end
	player_names[exclude_player_name] = nil
	return player_names
end

core.register_entity("testentities:observable", {
	initial_properties = {
		visual = "sprite",
		textures = { "testentities_sprite.png" },
		static_save = false,
		infotext = "Punch to set observers to anyone but you"
	},
	on_activate = function(self)
		self.object:set_armor_groups({punch_operable = 1})
		assert(self.object:get_observers() == nil)
		-- Using a value of `false` in the table should error.
		assert(not pcall(self.object, self.object.set_observers, self.object, {test = false}))
	end,
	on_punch = function(self, puncher)
		local puncher_name = puncher:get_player_name()
		local observers = player_names_excluding(puncher_name)
		self.object:set_observers(observers)
		local got_observers = self.object:get_observers()
		for name in pairs(observers) do
			assert(got_observers[name])
		end
		for name in pairs(got_observers) do
			assert(observers[name])
		end
		self.object:set_properties({infotext = "Excluding " .. puncher_name})
		return true
	end
})
