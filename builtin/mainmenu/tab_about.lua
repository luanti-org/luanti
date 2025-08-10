-- Luanti
-- Copyright (C) 2013 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later


local function prepare_credits(dest, source)
	local string = table.concat(source, "\n") .. "\n"

	string = core.hypertext_escape(string)
	string = string:gsub("%[.-%]", "<gray>%1</gray>")

	table.insert(dest, string)
end

local function get_credits()
	local f = assert(io.open(core.get_mainmenu_path() .. "/credits.json"))
	local json = core.parse_json(f:read("*all"))
	f:close()
	return json
end

local function get_renderer_info()
	local ret = {}

	-- OpenGL version, stripped to just the important part
	local s1 = core.get_active_renderer()
	if s1:sub(1, 7) == "OpenGL " then
		s1 = s1:sub(8)
	end
	local m = s1:match("^[%d.]+")
	if not m then
		m = s1:match("^ES [%d.]+")
	end
	ret[#ret+1] = m or s1
	-- video driver
	ret[#ret+1] = core.get_active_driver():lower()
	-- irrlicht device
	ret[#ret+1] = core.get_active_irrlicht_device():upper()

	return table.concat(ret, " / ")
end

return {
	name = "about",
	caption = fgettext("About"),

	cbf_formspec = function(tabview, name, tabdata)
		local logofile = defaulttexturedir .. "logo.png"
		local version = core.get_version()

		local hypertext = {
			"<tag name=heading color=#ff0>",
			"<tag name=gray color=#aaa>",
		}

		local credits = get_credits()

		table.insert_all(hypertext, {
			"<heading>", fgettext_ne("Core Developers"), "</heading>\n",
		})
		prepare_credits(hypertext, credits.core_developers)
		table.insert_all(hypertext, {
			"\n",
			"<heading>", fgettext_ne("Core Team"), "</heading>\n",
		})
		prepare_credits(hypertext, credits.core_team)
		table.insert_all(hypertext, {
			"\n",
			"<heading>", fgettext_ne("Active Contributors"), "</heading>\n",
		})
		prepare_credits(hypertext, credits.contributors)
		table.insert_all(hypertext, {
			"\n",
			"<heading>", fgettext_ne("Previous Core Developers"), "</heading>\n",
		})
		prepare_credits(hypertext, credits.previous_core_developers)
		table.insert_all(hypertext, {
			"\n",
			"<heading>", fgettext_ne("Previous Contributors"), "</heading>\n",
		})
		prepare_credits(hypertext, credits.previous_contributors)

		hypertext = table.concat(hypertext):sub(1, -2)

		local fs = "image[1.5,0.6;2.5,2.5;" .. core.formspec_escape(logofile) .. "]" ..
			"style[label_button;border=false]" ..
			"button[0.1,3.4;5.3,0.5;label_button;" ..
			core.formspec_escape(version.project .. " " .. version.string) .. "]" ..
			"button_url[1.5,4.1;2.5,0.8;homepage;luanti.org;https://www.luanti.org/]" ..
			"hypertext[5.5,0.25;9.75,6.6;credits;" .. core.formspec_escape(hypertext) .. "]"

		local active_renderer_info = fgettext("Active renderer:") .. "\n" ..
			core.formspec_escape(get_renderer_info())
		fs = fs .. "style[label_button2;border=false]" ..
			"button[0.1,6;5.3,1;label_button2;" .. active_renderer_info .. "]"..
			"tooltip[label_button2;" .. active_renderer_info .. "]"

		if PLATFORM == "Android" then
			fs = fs .. "button[0.5,5.1;4.5,0.8;share_debug;" .. fgettext("Share debug log") .. "]"
		else
			fs = fs .. "tooltip[userdata;" ..
					fgettext("Opens the directory that contains user-provided worlds, games, mods,\n" ..
							"and texture packs in a file manager / explorer.") .. "]"
			fs = fs .. "button[0.5,5.1;4.5,0.8;userdata;" .. fgettext("Open User Data Directory") .. "]"
		end

		return fs
	end,

	cbf_button_handler = function(this, fields, name, tabdata)
		if fields.share_debug then
			local path = core.get_user_path() .. DIR_DELIM .. "debug.txt"
			core.share_file(path)
		end

		if fields.userdata then
			core.open_dir(core.get_user_path())
		end
	end,

	on_change = function(type)
		if type == "ENTER" then
			mm_game_theme.set_engine()
		end
	end,
}
