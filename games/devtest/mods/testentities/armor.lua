-- Armorball: Test entity for testing armor groups
-- Rightclick to change armor group

local phasearmor = {
	[0]={icy=100},
	[1]={fiery=100},
	[2]={icy=100, fiery=100},
	[3]={fleshy=-100},
	[4]={fleshy=1},
	[5]={fleshy=10},
	[6]={fleshy=50},
	[7]={fleshy=100},
	[8]={fleshy=200},
	[9]={fleshy=1000},
	[10]={fleshy=32767},
	[11]={immortal=1},
	[12]={punch_operable=1},
}
local max_phase = 12

core.register_entity("testentities:armorball", {
	initial_properties = {
		hp_max = 20,
		physical = false,
		collisionbox = {-0.4,-0.4,-0.4, 0.4,0.4,0.4},
		visual = "sprite",
		visual_size = {x=1, y=1},
		textures = {"testentities_armorball.png"},
		spritediv = {x=1, y=max_phase+1},
		initial_sprite_basepos = {x=0, y=0},
	},

	_phase = 7,

	on_activate = function(self, staticdata)
		core.log("action", "[testentities] armorball.on_activate")
		self.object:set_armor_groups(phasearmor[self._phase])
		self.object:set_sprite({x=0, y=self._phase})
	end,

	on_rightclick = function(self, clicker)
		-- Change armor group and sprite
		self._phase = self._phase + 1
		if self._phase >= max_phase + 1 then
			self._phase = 0
		end
		self.object:set_sprite({x=0, y=self._phase})
		self.object:set_armor_groups(phasearmor[self._phase])
	end,

	on_punch = function(self, puncher, time_from_last_punch, tool_capabilities, dir, damage)
		if not puncher then
			return
		end
		local name = puncher:get_player_name()
		if not name then
			return
		end
		core.chat_send_player(name, "time_from_last_punch="..string.format("%.3f", time_from_last_punch).."; damage="..tostring(damage))
	end,
})
