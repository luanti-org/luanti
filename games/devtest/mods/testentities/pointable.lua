-- Pointability test Entities

-- Register wrapper for compactness
local function register_pointable_testentity(name, pointable)
	local texture = "testnodes_"..name..".png"
	core.register_entity("testentities:"..name, {
		initial_properties = {
			visual = "cube",
			visual_size = {x = 0.6, y = 0.6, z = 0.6},
			textures = {
				texture, texture, texture, texture, texture, texture
			},
			pointable = pointable,
		},
		on_activate = function(self)
			self.object:set_armor_groups({[name.."_test"] = 1})
		end
	})
end

register_pointable_testentity("pointable", true)
register_pointable_testentity("not_pointable", false)
register_pointable_testentity("blocking_pointable", "blocking")
