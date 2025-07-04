-- Luanti
-- Copyright (C) 2014 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

local function table_to_flags(ftable)
	-- Convert e.g. { jungles = true, caves = false } to "jungles,nocaves"
	local str = {}
	for flag, is_set in pairs(ftable) do
		str[#str + 1] = is_set and flag or ("no" .. flag)
	end
	return table.concat(str, ",")
end

-- Same as check_flag but returns a string
local function strflag(flags, flag)
	return (flags[flag] == true) and "true" or "false"
end

local cb_caverns = { "caverns", fgettext("Caverns"),
	fgettext("Very large caverns deep in the underground") }

local flag_checkboxes = {
	v5 = {
		cb_caverns,
	},
	v7 = {
		cb_caverns,
		{ "ridges", fgettext("Rivers"), fgettext("Sea level rivers") },
		{ "mountains", fgettext("Mountains") },
		{ "floatlands", fgettext("Floatlands (experimental)"),
		fgettext("Floating landmasses in the sky") },
	},
	carpathian = {
		cb_caverns,
		{ "rivers", fgettext("Rivers"), fgettext("Sea level rivers") },
	},
	valleys = {
		{ "altitude_chill", fgettext("Altitude chill"),
		fgettext("Reduces heat with altitude") },
		{ "altitude_dry", fgettext("Altitude dry"),
		fgettext("Reduces humidity with altitude") },
		{ "humid_rivers", fgettext("Humid rivers"),
		fgettext("Increases humidity around rivers") },
		{ "vary_river_depth", fgettext("Vary river depth"),
		fgettext("Low humidity and high heat causes shallow or dry rivers") },
	},
	flat = {
		cb_caverns,
		{ "hills", fgettext("Hills") },
		{ "lakes", fgettext("Lakes") },
	},
	fractal = {
		{ "terrain", fgettext("Additional terrain"),
		fgettext("Generate non-fractal terrain: Oceans and underground") },
	},
	v6 = {
		{ "trees", fgettext("Trees and jungle grass") },
		{ "flat", fgettext("Flat terrain") },
		{ "mudflow", fgettext("Mud flow"), fgettext("Terrain surface erosion") },
		{ "temples", fgettext("Desert temples"),
		fgettext("Different dungeon variant generated in desert biomes (only if dungeons enabled)") },
		-- Biome settings are in mgv6_biomes below
	},
}

local mgv6_biomes = {
	{
		fgettext("Temperate, Desert, Jungle, Tundra, Taiga"),
		{jungles = true, snowbiomes = true}
	},
	{
		fgettext("Temperate, Desert, Jungle"),
		{jungles = true, snowbiomes = false}
	},
	{
		fgettext("Temperate, Desert"),
		{jungles = false, snowbiomes = false}
	},
}

local function create_world_formspec(dialogdata)

	local current_mg = dialogdata.mg
	local mapgens = core.get_mapgen_names()

	local flags = dialogdata.flags

	local game = pkgmgr.find_by_gameid(core.settings:get("menu_last_game"))
	if game == nil then
		-- should never happen but just pick the first game
		game = pkgmgr.games[1]
		core.settings:set("menu_last_game", game.id)
	end

	local disallowed_mapgen_settings = {}
	if game ~= nil then
		local gameconfig = Settings(game.path.."/game.conf")

		local allowed_mapgens = (gameconfig:get("allowed_mapgens") or ""):split()
		for key, value in pairs(allowed_mapgens) do
			allowed_mapgens[key] = value:trim()
		end

		local disallowed_mapgens = (gameconfig:get("disallowed_mapgens") or ""):split()
		for key, value in pairs(disallowed_mapgens) do
			disallowed_mapgens[key] = value:trim()
		end

		if #allowed_mapgens > 0 then
			for i = #mapgens, 1, -1 do
				if table.indexof(allowed_mapgens, mapgens[i]) == -1 then
					table.remove(mapgens, i)
				end
			end
		end

		if #disallowed_mapgens > 0 then
			for i = #mapgens, 1, -1 do
				if table.indexof(disallowed_mapgens, mapgens[i]) > 0 then
					table.remove(mapgens, i)
				end
			end
		end

		local ds = (gameconfig:get("disallowed_mapgen_settings") or ""):split()
		for _, value in pairs(ds) do
			disallowed_mapgen_settings[value:trim()] = true
		end
	end

	local mglist = ""
	local selindex
	do -- build the list of mapgens
		local i = 1
		local first_mg
		for k, v in pairs(mapgens) do
			if not first_mg then
				first_mg = v
			end
			if current_mg == v then
				selindex = i
			end
			i = i + 1
			mglist = mglist .. core.formspec_escape(v) .. ","
		end
		if not selindex then
			selindex = 1
			current_mg = first_mg
		end
		mglist = mglist:sub(1, -2)
	end

	-- The logic of the flag element IDs is as follows:
	-- "flag_main_foo-bar-baz" controls dialogdata.flags["main"]["foo_bar_baz"]
	-- see the buttonhandler for the implementation of this

	local mg_main_flags = function(mapgen, y)
		if mapgen == "singlenode" then
			return "", y
		end
		if disallowed_mapgen_settings["mg_flags"] then
			return "", y
		end

		local form = "checkbox[0," .. y .. ";flag_main_caves;" ..
			fgettext("Caves") .. ";"..strflag(flags.main, "caves").."]"
		y = y + 0.5

		form = form .. "checkbox[0,"..y..";flag_main_dungeons;" ..
			fgettext("Dungeons") .. ";"..strflag(flags.main, "dungeons").."]"
		y = y + 0.5

		local d_name = fgettext("Decorations")
		local d_tt
		if mapgen == "v6" then
			d_tt = fgettext("Structures appearing on the terrain (no effect on trees and jungle grass created by v6)")
		else
			d_tt = fgettext("Structures appearing on the terrain, typically trees and plants")
		end
		form = form .. "checkbox[0,"..y..";flag_main_decorations;" ..
			d_name .. ";" ..
			strflag(flags.main, "decorations").."]" ..
			"tooltip[flag_mg_decorations;" ..
			d_tt ..
			"]"
		y = y + 0.5

		form = form .. "tooltip[flag_main_caves;" ..
		fgettext("Network of tunnels and caves")
		.. "]"
		return form, y
	end

	local mg_specific_flags = function(mapgen, y)
		if not flag_checkboxes[mapgen] then
			return "", y
		end
		if disallowed_mapgen_settings["mg"..mapgen.."_spflags"] then
			return "", y
		end
		local form = ""
		for _, tab in pairs(flag_checkboxes[mapgen]) do
			local id = "flag_"..mapgen.."_"..tab[1]:gsub("_", "-")
			form = form .. ("checkbox[0,%f;%s;%s;%s]"):
				format(y, id, tab[2], strflag(flags[mapgen], tab[1]))

			if tab[3] then
				form = form .. "tooltip["..id..";"..tab[3].."]"
			end
			y = y + 0.5
		end

		if mapgen ~= "v6" then
			-- No special treatment
			return form, y
		end
		-- Special treatment for v6 (add biome widgets)

		-- Biome type (jungles, snowbiomes)
		local biometype
		if flags.v6.snowbiomes == true then
			biometype = 1
		elseif flags.v6.jungles == true  then
			biometype = 2
		else
			biometype = 3
		end
		y = y + 0.3

		form = form .. "label[0,"..(y+0.1)..";" .. fgettext("Biomes") .. "]"
		y = y + 0.6

		form = form .. "dropdown[0,"..y..";6.3;mgv6_biomes;"
		for b=1, #mgv6_biomes do
			form = form .. mgv6_biomes[b][1]
			if b < #mgv6_biomes then
				form = form .. ","
			end
		end
		form = form .. ";" .. biometype.. "]"

		-- biomeblend
		y = y + 0.55
		form = form .. "checkbox[0,"..y..";flag_v6_biomeblend;" ..
			fgettext("Biome blending") .. ";"..strflag(flags.v6, "biomeblend").."]" ..
			"tooltip[flag_v6_biomeblend;" ..
			fgettext("Smooth transition between biomes") .. "]"

		return form, y
	end

	local y_start = 0.0
	local y = y_start
	local str_flags, str_spflags
	local label_flags, label_spflags = "", ""
	y = y + 0.3
	str_flags, y = mg_main_flags(current_mg, y)
	if str_flags ~= "" then
		label_flags = "label[0,"..y_start..";" .. fgettext("Mapgen flags") .. "]"
		y_start = y + 0.4
	else
		y_start = 0.0
	end
	y = y_start + 0.3
	str_spflags = mg_specific_flags(current_mg, y)
	if str_spflags ~= "" then
		label_spflags = "label[0,"..y_start..";" .. fgettext("Mapgen-specific flags") .. "]"
	end

	local retval =
		"size[12.25,7.4,true]" ..

		-- Left side
		"container[0,0]"..
		"field[0.3,0.6;6,0.5;te_world_name;" ..
		fgettext("World name") ..
		";" .. core.formspec_escape(dialogdata.worldname) .. "]" ..
		"set_focus[te_world_name;false]"

	if not disallowed_mapgen_settings["seed"] then

		retval = retval .. "field[0.3,1.7;6,0.5;te_seed;" ..
				fgettext("Seed") ..
				";".. core.formspec_escape(dialogdata.seed) .. "]"

	end

	retval = retval ..
		"label[0,2;" .. fgettext("Mapgen") .. "]"..
		"dropdown[0,2.5;6.3;dd_mapgen;" .. mglist .. ";" .. selindex .. "]"

	-- Warning when making a devtest world
	if game.id == "devtest" then
		retval = retval ..
			"container[0,3.5]" ..
			"box[0,0;5.8,1.7;#ff8800]" ..
			"textarea[0.4,0.1;6,1.8;;;"..
			fgettext("Development Test is meant for developers.") .. "]" ..
			"button[1,1;4,0.5;world_create_open_cdb;" .. fgettext("Install another game") .. "]" ..
			"container_end[]"
	end

	retval = retval ..
		"container_end[]" ..

		-- Right side
		"container[6.2,0]"..
		label_flags .. str_flags ..
		label_spflags .. str_spflags ..
		"container_end[]"..

		-- Menu buttons
		"container[0,6.9]"..
		"button[3.25,0;3,0.5;world_create_confirm;" .. fgettext("Create") .. "]" ..
		"button[6.25,0;3,0.5;world_create_cancel;" .. fgettext("Cancel") .. "]" ..
		"container_end[]"

	return retval

end

local function create_world_buttonhandler(this, fields)

	if fields["world_create_open_cdb"] then
		local dlg = create_contentdb_dlg("game")
		dlg:set_parent(this.parent)
		this:delete()
		this.parent:hide()
		dlg:show()
		return true
	end

	if fields["world_create_confirm"] or
		fields["key_enter"] then

		if fields["key_enter"] then
			-- HACK: This timestamp prevents double-triggering when pressing Enter on an input box
			-- and releasing it on a button[] or textlist[] due to instant formspec updates.
			this.parent.dlg_create_world_closed_at = core.get_us_time()
		end

		local worldname = fields["te_world_name"]
		local game, _ = pkgmgr.find_by_gameid(core.settings:get("menu_last_game"))

		local message
		if game == nil then
			message = fgettext_ne("No game selected")
		end

		if message == nil then
			-- For unnamed worlds use the generated name 'world<number>',
			-- where the number increments: it is set to 1 larger than the largest
			-- generated name number found.
			if worldname == "" then
				local worldnum_max = 0
				for _, world in ipairs(menudata.worldlist:get_list()) do
					if world.name:match("^world%d+$") then
						local worldnum = tonumber(world.name:sub(6))
						worldnum_max = math.max(worldnum_max, worldnum)
					end
				end
				worldname = "world" .. worldnum_max + 1
			end

			if menudata.worldlist:uid_exists_raw(worldname) then
				message = fgettext_ne("A world named \"$1\" already exists", worldname)
			end
		end

		if message == nil then
			this.data.seed = fields["te_seed"] or ""
			this.data.mg = fields["dd_mapgen"]

			-- actual names as used by engine
			local settings = {
				fixed_map_seed = this.data.seed,
				mg_name = this.data.mg,
				mg_flags = table_to_flags(this.data.flags.main),
				mgv5_spflags = table_to_flags(this.data.flags.v5),
				mgv6_spflags = table_to_flags(this.data.flags.v6),
				mgv7_spflags = table_to_flags(this.data.flags.v7),
				mgfractal_spflags = table_to_flags(this.data.flags.fractal),
				mgcarpathian_spflags = table_to_flags(this.data.flags.carpathian),
				mgvalleys_spflags = table_to_flags(this.data.flags.valleys),
				mgflat_spflags = table_to_flags(this.data.flags.flat),
			}
			message = core.create_world(worldname, game.id, settings)
		end

		if message == nil then
			core.settings:set("menu_last_game", game.id)
			menudata.worldlist:set_filtercriteria(game.id)
			menudata.worldlist:refresh()
			core.settings:set("mainmenu_last_selected_world",
					menudata.worldlist:raw_index_by_uid(worldname))
		end

		gamedata.errormessage = message
		this:delete()
		return true
	end

	this.data.worldname = fields["te_world_name"]
	this.data.seed = fields["te_seed"] or ""

	if fields["games"] then
		local gameindex = core.get_textlist_index("games")
		core.settings:set("menu_last_game", pkgmgr.games[gameindex].id)
		return true
	end

	for k,v in pairs(fields) do
		local split = string.split(k, "_", nil, 3)
		if split and split[1] == "flag" then
			-- We replaced the underscore of flag names with a dash.
			local flag = string.gsub(split[3], "-", "_")
			local ftable = this.data.flags[split[2]]
			assert(ftable)
			ftable[flag] = v == "true"
			return true
		end
	end

	if fields["world_create_cancel"] then
		this:delete()
		return true
	end

	if fields["mgv6_biomes"] then
		local entry = core.formspec_escape(fields["mgv6_biomes"])
		for b=1, #mgv6_biomes do
			if entry == mgv6_biomes[b][1] then
				local ftable = this.data.flags.v6
				ftable.jungles = mgv6_biomes[b][2].jungles
				ftable.snowbiomes = mgv6_biomes[b][2].snowbiomes
				return true
			end
		end
	end

	if fields["dd_mapgen"] then
		this.data.mg = fields["dd_mapgen"]
		return true
	end

	return false
end


function create_create_world_dlg()
	local retval = dialog_create("sp_create_world",
					create_world_formspec,
					create_world_buttonhandler,
					nil)
	retval.data = {
		worldname = "",
		-- settings the world is created with:
		seed = core.settings:get("fixed_map_seed") or "",
		mg = core.settings:get("mg_name"),
		flags = {
			main = core.settings:get_flags("mg_flags"),
			v5 = core.settings:get_flags("mgv5_spflags"),
			v6 = core.settings:get_flags("mgv6_spflags"),
			v7 = core.settings:get_flags("mgv7_spflags"),
			fractal = core.settings:get_flags("mgfractal_spflags"),
			carpathian = core.settings:get_flags("mgcarpathian_spflags"),
			valleys = core.settings:get_flags("mgvalleys_spflags"),
			flat = core.settings:get_flags("mgflat_spflags"),
		}
	}

	return retval
end
