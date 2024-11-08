local lighting_sections = {
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

local function dump_lighting(lighting)
	local result = "{\n"
	local section_count = 0
	for _,section in ipairs(lighting_sections) do
		section_count = section_count + 1

		local parameters = section.entries or {}
		local state = lighting[section.n] or {}

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

		if section_count < #lighting_sections then
			result = result..","
		end
		result = result.."\n"
	end
	result = result .."}"
	return result
end

core.register_chatcommand("set_lighting", {
	params = "",
	description = "Tune lighting parameters",
	func = function(player_name, param)
		local player = core.get_player_by_name(player_name);
		if not player then return end

		local lighting = player:get_lighting()
		local exposure = lighting.exposure or {}

		local content = {}
		local line = 1
		for _,section in ipairs(lighting_sections) do
			local parameters = section.entries or {}
			local state = lighting[section.n] or {}

			table.insert(content, "label[1,"..line..";"..section.d.."]")
			line  = line + 1

			for _,v in ipairs(parameters) do
				table.insert(content, "label[2,"..line..";"..v.d.."]")
				table.insert(content, "scrollbaroptions[min=0;max=1000;smallstep=10;largestep=100;thumbsize=10]")
				local value = state[v.n]
				if v.type == "log2" then
					value = math.log(value or 1) / math.log(2)
				end
				local sb_scale = math.floor(1000 * (math.max(v.min, value or 0) - v.min) / (v.max - v.min))
				table.insert(content, "scrollbar[2,"..(line+0.7)..";12,1;horizontal;"..section.n.."."..v.n..";"..sb_scale.."]")
				line = line + 2.7
			end

			line = line + 1
		end

		local form = {
			"formspec_version[2]",
			"size[15,", line, "]",
			"position[0.99,0.15]",
			"anchor[1,0]",
			"padding[0.05,0.1]",
			"no_prepend[]",
		}
		table.insert_all(form, content)
	
		core.show_formspec(player_name, "lighting", table.concat(form))
		local debug_value = dump_lighting(lighting)
		local debug_ui = player:hud_add({type="text", position={x=0.1, y=0.3}, scale={x=1,y=1}, alignment = {x=1, y=1}, text=debug_value, number=0xFFFFFF})
		player:get_meta():set_int("lighting_hud", debug_ui)
	end
})

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname ~= "lighting" then return end

	if not player then return end

	local hud_id = player:get_meta():get_int("lighting_hud")

	if fields.quit then
		player:hud_remove(hud_id)
		player:get_meta():set_int("lighting_hud", -1)
		return
	end

	local lighting = player:get_lighting()
	for _,section in ipairs(lighting_sections) do
		local parameters = section.entries or {}

		local state = (lighting[section.n] or {})
		lighting[section.n] = state

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

	local debug_value = dump_lighting(lighting)
	player:hud_change(hud_id, "text", debug_value)

	player:set_lighting(lighting)
end)