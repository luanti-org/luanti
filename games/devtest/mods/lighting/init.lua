
local modpath = core.get_modpath(core.get_current_modname())


local function dumpByRecipe(data, recipe)
	local result = "{\n"
	local section_count = 0
	for _,section in ipairs(recipe) do
		section_count = section_count + 1

		local parameters = section.entries or {}
		local state = data[section.n] or {}

		result = result.."  "..section.n.." = {\n"

		local count = 0
		for _,v in ipairs(parameters) do
			count = count + 1
			result = result.."    "..v.n.." = "..(math.floor(state[v.n] * 1000)/1000)
			if count < #parameters then
				result = result..","
			end
			result = result.."\n"
		end

		result = result.."  }"

		if section_count < #recipe then
			result = result..","
		end
		result = result.."\n"
	end
	result = result .."}"
	return result
end

local function buildGUI(player, data, recipe, gui_name)
	local form = {
		"formspec_version[2]",
		"size[15,30]",
		"position[0.99,0.15]",
		"anchor[1,0]",
		"padding[0.05,0.1]",
		"no_prepend[]"
	};

	local line = 1
	for _,section in ipairs(recipe) do
		local parameters = section.entries or {}
		local state = data[section.n] or {}

		table.insert(form, "label[1,"..line..";"..section.d.."]")
		line  = line + 1

		for _,v in ipairs(parameters) do
			table.insert(form, "label[2,"..line..";"..v.d.."]")
			table.insert(form, "scrollbaroptions[min=0;max=1000;smallstep=10;largestep=100;thumbsize=10]")
			local value = state[v.n]
			if v.type == "log2" then
				value = math.log(value or 1) / math.log(2)
			end
			local sb_scale = math.floor(1000 * (math.max(v.min, value or 0) - v.min) / (v.max - v.min))
			table.insert(form, "scrollbar[2,"..(line+0.7)..";12,1;horizontal;"..section.n.."."..v.n..";"..sb_scale.."]")
			line = line + 2.7
		end

		line = line + 1
	end

	core.show_formspec(player:get_player_name(), gui_name, table.concat(form))
	local debug_value = dumpByRecipe(data, recipe)
	local debug_ui = player:hud_add({type="text", position={x=0.1, y=0.3}, scale={x=1,y=1}, alignment = {x=1, y=1}, text=debug_value, number=0xFFFFFF})
	player:get_meta():set_int(gui_name.."_hud", debug_ui)
end

local function receiveFields(player, fields, data, recipe, gui_name)
	local hud_id = player:get_meta():get_int(gui_name.."_hud")

	if fields.quit then
		player:hud_remove(hud_id)
		player:get_meta():set_int(gui_name.."_hud", -1)
		return
	end

	for _,section in ipairs(recipe) do
		local parameters = section.entries or {}

		local state = (data[section.n] or {})
		data[section.n] = state

		for _,v in ipairs(parameters) do

			if fields[section.n.."."..v.n] then
				local event = core.explode_scrollbar_event(fields[section.n.."."..v.n])
				if event.type == "CHG" then
					local value = v.min + (v.max - v.min) * (event.value / 1000);
					if v.type == "log2" then
						value = math.pow(2, value);
					end
					state[v.n] = value;
				end
			end
		end
	end

	local debug_value = dumpByRecipe(data, recipe)
	player:hud_change(hud_id, "text", debug_value)
end

local lighting_recipe = {
	{n = "shadows", d = "Shadows",
		entries = {
			{ n = "intensity", d = "Shadow Intensity", min = 0, max = 1 }
		}
	},
	{
		n = "exposure", d = "Exposure",
		entries = {
			{n = "luminance_min", d = "Minimum Luminance", min = -10, max = 10},
			{n = "luminance_max", d = "Maximum Luminance", min = -10, max = 10},
			{n = "exposure_correction", d = "Exposure Correction", min = -10, max = 10},
			{n = "speed_dark_bright", d = "Bright light adaptation speed", min = -10, max = 10, type="log2"},
			{n = "speed_bright_dark", d = "Dark scene adaptation speed", min = -10, max = 10, type="log2"},
			{n = "center_weight_power", d = "Power factor for center-weighting", min = 0.1, max = 10},
		}
	},
	{
		n = "bloom", d = "Bloom",
		entries = {
			{n = "intensity", d = "Intensity", min = 0, max = 1},
			{n = "strength_factor", d = "Strength Factor", min = 0.1, max = 10},
			{n = "radius", d = "Radius", min = 0.1, max = 8},
		},
	},
	{
		n = "volumetric_light", d = "Volumetric Lighting",
		entries = {
			{n = "strength", d = "Strength", min = 0, max = 1},
		},
	},
}

core.register_chatcommand("set_lighting", {
	params = "",
	description = "Tune lighting parameters",
	func = function(player_name, param)
		local player = core.get_player_by_name(player_name)
		if not player then return end

		local lighting = player:get_lighting()

		buildGUI(player, lighting, lighting_recipe, "lighting")
	end
})

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname ~= "lighting" then return end

	if not player then return end

	local lighting = player:get_lighting()

	receiveFields(player, fields, lighting, lighting_recipe, "lighting")

	player:set_lighting(lighting)
end)

local sky_light_recipe = {
	{n = "color_offset", d = "Color offset",
		entries = {
			{n = "r", d = "Red color offset", min = -1, max = 2},
			{n = "g", d = "Green color offset", min = -1, max = 2},
			{n = "b", d = "Blue color offset", min = -1, max = 2},
		}
	},
	{n = "color_ratio_coef", d = "Color day-night ratio coefficient",
		entries = {
			{n = "r", d = "Red color day-night ratio coefficient", min = -1e-3, max = 2e-3},
			{n = "g", d = "Green color day-night ratio coefficient", min = -1e-3, max = 2e-3},
			{n = "b", d = "Blue color day-night ratio coefficient", min = -1e-3, max = 2e-3},
		}
	}
}


core.register_chatcommand("set_sky_light", {
	params = "",
	description = "Tune lighting sky_light parameters",
	func = function(player_name, param)
		local player = core.get_player_by_name(player_name)
		if not player then return end

		local lighting = player:get_lighting()
		local sky_light = lighting.sky_light

		buildGUI(player, sky_light, sky_light_recipe, "sky_light")
	end
})

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname ~= "sky_light" then return end

	if not player then return end

	local lighting = player:get_lighting()
	local sky_light = lighting.sky_light

	receiveFields(player, fields, sky_light, sky_light_recipe, "sky_light")

	player:set_lighting(lighting)
end)

