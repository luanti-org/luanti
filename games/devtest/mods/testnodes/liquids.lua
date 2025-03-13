-- Add liquids for ranges and viscosity levels 0-8

for d=0, 8 do
	local tt_normal
	if d == 0 then
		tt_normal = "\n".."Swimmable, renewable liquid"
	else
		tt_normal = "\n".."Swimmable, spreading, renewable liquid"
	end
	core.register_node("testnodes:rliquid_"..d, {
		description = "Test Liquid Source, Range "..d..
			tt_normal .. "\n" .. "(falling & floating node)",
		drawtype = "liquid",
		tiles = {"testnodes_liquidsource_r"..d..".png"},
		special_tiles = {
			{name = "testnodes_liquidsource_r"..d..".png", backface_culling = false},
			{name = "testnodes_liquidsource_r"..d..".png", backface_culling = true},
		},
		use_texture_alpha = "blend",
		paramtype = "light",
		walkable = false,
		buildable_to = true,
		is_ground_content = false,
		liquidtype = "source",
		liquid_alternative_flowing = "testnodes:rliquid_flowing_"..d,
		liquid_alternative_source = "testnodes:rliquid_"..d,
		liquid_range = d,
		-- Also use these nodes to test falling, floating liquid source nodes
		groups = {float = 1, falling_node = 1},
	})

	core.register_node("testnodes:rliquid_flowing_"..d, {
		description = "Flowing Test Liquid, Range "..d..
			tt_normal,
		drawtype = "flowingliquid",
		tiles = {"testnodes_liquidflowing_r"..d..".png"},
		special_tiles = {
			{name = "testnodes_liquidflowing_r"..d..".png", backface_culling = false},
			{name = "testnodes_liquidflowing_r"..d..".png", backface_culling = false},
		},
		use_texture_alpha = "blend",
		paramtype = "light",
		paramtype2 = "flowingliquid",
		walkable = false,
		buildable_to = true,
		is_ground_content = false,
		liquidtype = "flowing",
		liquid_alternative_flowing = "testnodes:rliquid_flowing_"..d,
		liquid_alternative_source = "testnodes:rliquid_"..d,
		liquid_range = d,
	})

	if d <= 7 then

	local tt_reduced = ""
	if d > 0 then
		tt_reduced = "\n".."Reduced swimming/spreading speed"
	end

	local mod = "^[colorize:#000000:127"
	core.register_node("testnodes:vliquid_"..d, {
		description = "Test Liquid Source, Viscosity/Resistance "..d.."\n"..
			"Swimmable, spreading, renewable liquid"..
			tt_reduced,
		drawtype = "liquid",
		tiles = {"testnodes_liquidsource_r"..d..".png"..mod},
		special_tiles = {
			{name = "testnodes_liquidsource_r"..d..".png"..mod, backface_culling = false},
			{name = "testnodes_liquidsource_r"..d..".png"..mod, backface_culling = true},
		},
		use_texture_alpha = "blend",
		paramtype = "light",
		walkable = false,
		buildable_to = true,
		is_ground_content = false,
		liquidtype = "source",
		liquid_alternative_flowing = "testnodes:vliquid_flowing_"..d,
		liquid_alternative_source = "testnodes:vliquid_"..d,
		liquid_viscosity = d,
	})

	core.register_node("testnodes:vliquid_flowing_"..d, {
		description = "Flowing Test Liquid, Viscosity/Resistance "..d.."\n"..
			"Swimmable, spreading, renewable liquid"..
			tt_reduced,
		drawtype = "flowingliquid",
		tiles = {"testnodes_liquidflowing_r"..d..".png"..mod},
		special_tiles = {
			{name = "testnodes_liquidflowing_r"..d..".png"..mod, backface_culling = false},
			{name = "testnodes_liquidflowing_r"..d..".png"..mod, backface_culling = false},
		},
		use_texture_alpha = "blend",
		paramtype = "light",
		paramtype2 = "flowingliquid",
		walkable = false,
		buildable_to = true,
		is_ground_content = false,
		liquidtype = "flowing",
		liquid_alternative_flowing = "testnodes:vliquid_flowing_"..d,
		liquid_alternative_source = "testnodes:vliquid_"..d,
		liquid_viscosity = d,
	})

	mod = "^[colorize:#000000:192"
	local v = 4
	core.register_node("testnodes:vrliquid_"..d, {
		description = "Test Liquid Source, Viscosity "..v..", Resistance "..d.."\n"..
			"Swimmable, spreading, renewable liquid"..
			tt_reduced,
		drawtype = "liquid",
		tiles = {"testnodes_liquidsource_r"..d..".png"..mod},
		special_tiles = {
			{name = "testnodes_liquidsource_r"..d..".png"..mod, backface_culling = false},
			{name = "testnodes_liquidsource_r"..d..".png"..mod, backface_culling = true},
		},
		use_texture_alpha = "blend",
		paramtype = "light",
		walkable = false,
		pointable = false,
		diggable = false,
		buildable_to = true,
		is_ground_content = false,
		liquidtype = "source",
		liquid_alternative_flowing = "testnodes:vrliquid_flowing_"..d,
		liquid_alternative_source = "testnodes:vrliquid_"..d,
		liquid_viscosity = v,
		move_resistance = d,
	})

	core.register_node("testnodes:vrliquid_flowing_"..d, {
		description = "Flowing Test Liquid, Viscosity "..v..", Resistance "..d.."\n"..
			"Swimmable, spreading, renewable liquid"..
			tt_reduced,
		drawtype = "flowingliquid",
		tiles = {"testnodes_liquidflowing_r"..d..".png"..mod},
		special_tiles = {
			{name = "testnodes_liquidflowing_r"..d..".png"..mod, backface_culling = false},
			{name = "testnodes_liquidflowing_r"..d..".png"..mod, backface_culling = false},
		},
		use_texture_alpha = "blend",
		paramtype = "light",
		paramtype2 = "flowingliquid",
		walkable = false,
		pointable = false,
		diggable = false,
		buildable_to = true,
		is_ground_content = false,
		liquidtype = "flowing",
		liquid_alternative_flowing = "testnodes:vrliquid_flowing_"..d,
		liquid_alternative_source = "testnodes:vrliquid_"..d,
		liquid_viscosity = v,
		move_resistance = d,
	})

	end

end
