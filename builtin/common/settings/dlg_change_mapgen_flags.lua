-- Luanti
-- Copyright (C) 2015 PilzAdam
-- SPDX-License-Identifier: LGPL-2.1-or-later


local checkboxes = {}

local function flags_to_table(flags)
	return flags:gsub("%s+", ""):split(",", true) -- Remove all spaces and split
end

local function get_current_np_group(setting)
	local value = core.settings:get_np_group(setting.name)
	if value == nil then
		return setting.values
	end
	local p = "%g"
	return {
		p:format(value.offset),
		p:format(value.scale),
		p:format(value.spread.x),
		p:format(value.spread.y),
		p:format(value.spread.z),
		p:format(value.seed),
		p:format(value.octaves),
		p:format(value.persistence),
		p:format(value.lacunarity),
		value.flags
	}
end


local function get_formspec(dialogdata)
	local setting = dialogdata.setting

	-- Final formspec will be created at the end of this function
	-- Default values below, may be changed depending on setting type
	local width = 10
	local height = 2
	local description_height = 1.5

	local t = get_current_np_group(setting)
	local dimension = 3
	if setting.type == "noise_params_2d" then
		dimension = 2
	end

	local fields = {}
	local function add_field(x, name, label, value, tooltips)
		fields[#fields + 1] = ("field[%f,%f;3.3,1;%s;%s;%s]"):format(
			x, height, name, label, core.formspec_escape(value or "")
		)
		if tooltips then
			fields[#fields + 1] = ("tooltip[%s;%s]"):format(name, table.concat(tooltips, "\n"))
		end
	end
	-- First row
	height = height + 0.3
	add_field(0.3, "te_offset", fgettext("Offset"), t[1], {fgettext("Offsets the final noise by this value.")})
	add_field(3.6, "te_scale",  fgettext("Scale"),  t[2], {
			fgettext("The multiplier for the noise before the offset is added.")})
	add_field(6.9, "te_seed",   fgettext("Seed"),   t[6], {
			fgettext("The random seed for the noise. The same seed results in the same noise.")})
	height = height + 1.1

	-- Second row
	add_field(0.3, "te_spreadx", fgettext("$1 spread", "X"), t[3], {
			fgettext("Scales the noise in the $1 axis by this value.", "X"),
			fgettext("This is also the scale of the largest structures in the $1 direction of the noise.", "X")})
	if dimension == 3 then
		add_field(3.6, "te_spready", fgettext("$1 spread", "Y"), t[4], {
			fgettext("Scales the noise in the $1 axis by this value."),
			fgettext("This is also the scale of the largest structures in the $1 direction of the noise.", "Y")})
	else
		fields[#fields + 1] = "label[4," .. height - 0.2 .. ";" ..
				fgettext("2D Noise") .. "]"
	end
	add_field(6.9, "te_spreadz", fgettext("$1 spread", "Z"), t[5], {
			fgettext("Scales the noise in the $1 axis by this value.", "Z"),
			fgettext("This is also the scale of the largest structures in the $1 direction of the noise.", "Z")})
	height = height + 1.1

	-- Third row
	add_field(0.3, "te_octaves", fgettext("Octaves"),     t[7], {
			fgettext("Controls how many octaves (layers) the noise will have."),
			fgettext("Each octave is a simple noise generator. Each one also adds more detail to the noise."),
			fgettext("Lower values will result in smoother noise and higher values will result in rougher noise."),
			fgettext("Finally, all of the octaves will be combined to generate the noise."),
			fgettext("Having lots of octaves is not recommended because it increases terrain generation time.")})
	add_field(3.6, "te_persist", fgettext("Persistence"), t[8], {
			fgettext("Amplifies every octave by (amplitude of the previous octave * Persistence)."),
			fgettext("The first octave is amplified by 1."),
			fgettext("Lower values make the terrain simple and higher values make the terrain rough.")})
	add_field(6.9, "te_lacun",   fgettext("Lacunarity"),  t[9], {
			fgettext("Scales every octave by (scale of the previous octave / Lacunarity)."),
			fgettext("The first octave is scaled by 1."),
			fgettext("Lower values make the terrain smooth and higher values make the terrain have fine detail.")})
	height = height + 1.1


	local enabled_flags = flags_to_table(t[10])
	local flags = {}
	for _, name in ipairs(enabled_flags) do
		-- Index by name, to avoid iterating over all enabled_flags for every possible flag.
		flags[name] = true
	end
	for _, name in ipairs(setting.flags) do
		local checkbox_name = "cb_" .. name
		local is_enabled = flags[name] == true -- to get false if nil
		checkboxes[checkbox_name] = is_enabled
	end

	local formspec = table.concat(fields)
			.. "checkbox[0.5," .. height - 0.6 .. ";cb_defaults;"
			--[[~ "defaults" is a noise parameter flag.
			It describes the default processing options
			for noise settings in the settings menu. ]]
			.. fgettext("defaults") .. ";" -- defaults
			.. tostring(flags["defaults"] == true) .. "]" -- to get false if nil
			.. "tooltip[cb_defaults;" .. fgettext("Overrides the 'eased' flag based on the noise type.")
			.. "\n" .. fgettext("2D noise: flag on.\n3D noise: flag off.") .. "]"
			.. "checkbox[5," .. height - 0.6 .. ";cb_eased;"
			--[[~ "eased" is a noise parameter flag.
			It is used to make the map smoother and
			can be enabled in noise settings in
			the settings menu. ]]
			.. fgettext("eased") .. ";" -- eased
			.. tostring(flags["eased"] == true) .. "]"
			.. "tooltip[cb_eased;"
			 .. fgettext("Maps noise gradient values onto a quintic S-curve before performing interpolation.")
			.. "\n" .. fgettext("This results in smooth noise instead of gridlike noise.") .. "\n"
			.. fgettext("Making 3D noise eased is not recommended because it significantly increases the load.") .. "]"
			.. "checkbox[5," .. height - 0.15 .. ";cb_absvalue;"
			--[[~ "absvalue" is a noise parameter flag.
			It is short for "absolute value".
			It can be enabled in noise settings in
			the settings menu. ]]
			.. fgettext("absvalue") .. ";" -- absvalue
			.. tostring(flags["absvalue"] == true) .. "]"
			.. "tooltip[cb_absvalue;"
			.. fgettext("Takes the absolute value of each octave while adding them together.")
			.. "\n" .. fgettext("This results in spiky noise.") .. "]"

	height = height + 1

	-- Box good, textarea bad. Calculate textarea size from box.
	local function create_textfield(size, label, text, bg_color)
		local textarea = {
			x = size.x + 0.3,
			y = size.y,
			w = size.w + 0.25,
			h = size.h * 1.16 + 0.12
		}
		return ("box[%f,%f;%f,%f;%s]textarea[%f,%f;%f,%f;;%s;%s]"):format(
			size.x, size.y, size.w, size.h, bg_color or "#000",
			textarea.x, textarea.y, textarea.w, textarea.h,
			core.formspec_escape(label), core.formspec_escape(text)
		)

	end

	-- When there's an error: Shrink description textarea and add error below
	if dialogdata.error_message then
		local error_box = {
			x = 0,
			y = description_height - 0.4,
			w = width - 0.25,
			h = 0.5
		}
		formspec = formspec ..
			create_textfield(error_box, "", dialogdata.error_message, "#600")
		description_height = description_height - 0.75
	end

	-- Get description field
	local description_box = {
		x = 0,
		y = 0.2,
		w = width - 0.25,
		h = description_height
	}

	local setting_name = setting.name
	if setting.readable_name then
		setting_name = fgettext_ne(setting.readable_name) ..
			" (" .. setting.name .. ")"
	end

	local comment_text
	if setting.comment == "" then
		comment_text = fgettext_ne("(No description of setting given)")
	else
		comment_text = fgettext_ne(setting.comment)
	end

	return (
		"size[" .. width .. "," .. height + 0.25 .. ",true]" ..
		create_textfield(description_box, setting_name, comment_text) ..
		formspec ..
		"button[" .. width / 2 - 2.5 .. "," .. height - 0.4 .. ";2.5,1;btn_done;" ..
			fgettext("Save") .. "]" ..
		"button[" .. width / 2 .. "," .. height - 0.4 .. ";2.5,1;btn_cancel;" ..
			fgettext("Cancel") .. "]"
	)
end


local function buttonhandler(this, fields)
	local setting = this.data.setting
	if fields["btn_done"] or fields["key_enter"] then
		local np_flags = {}
		for _, name in ipairs(setting.flags) do
			if checkboxes["cb_" .. name] then
				table.insert(np_flags, name)
			end
		end

		checkboxes = {}

		if setting.type == "noise_params_2d" then
			fields["te_spready"] = fields["te_spreadz"]
		end
		local new_value = {
			offset = fields["te_offset"],
			scale = fields["te_scale"],
			spread = {
				x = fields["te_spreadx"],
				y = fields["te_spready"],
				z = fields["te_spreadz"]
			},
			seed = fields["te_seed"],
			octaves = fields["te_octaves"],
			persistence = fields["te_persist"],
			lacunarity = fields["te_lacun"],
			flags = table.concat(np_flags, ", ")
		}
		core.settings:set_np_group(setting.name, new_value)

		core.settings:write()
		this:delete()
		return true
	end

	if fields["btn_cancel"] then
		this:delete()
		return true
	end

	for name, value in pairs(fields) do
		if name:sub(1, 3) == "cb_" then
			checkboxes[name] = core.is_yes(value)
			return false -- Don't update the formspec!
		end
	end

	return false
end


function create_change_mapgen_flags_dlg(setting)
	assert(type(setting) == "table")

	local retval = dialog_create("dlg_change_mapgen_flags",
					get_formspec,
					buttonhandler,
					nil)

	retval.data.setting = setting
	return retval
end
