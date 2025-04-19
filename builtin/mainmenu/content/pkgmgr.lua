-- Luanti
-- Copyright (C) 2013 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later

--------------------------------------------------------------------------------
local function get_last_folder(text,count)
	local parts = text:split(DIR_DELIM)

	if count == nil then
		return parts[#parts]
	end

	local retval = ""
	for i=1,count,1 do
		retval = retval .. parts[#parts - (count-i)] .. DIR_DELIM
	end

	return retval
end

local function cleanup_path(temppath)

	local parts = temppath:split("-")
	temppath = ""
	for i=1,#parts,1 do
		if temppath ~= "" then
			temppath = temppath .. "_"
		end
		temppath = temppath .. parts[i]
	end

	parts = temppath:split(".")
	temppath = ""
	for i=1,#parts,1 do
		if temppath ~= "" then
			temppath = temppath .. "_"
		end
		temppath = temppath .. parts[i]
	end

	parts = temppath:split("'")
	temppath = ""
	for i=1,#parts,1 do
		if temppath ~= "" then
			temppath = temppath .. ""
		end
		temppath = temppath .. parts[i]
	end

	parts = temppath:split(" ")
	temppath = ""
	for i=1,#parts,1 do
		if temppath ~= "" then
			temppath = temppath
		end
		temppath = temppath .. parts[i]
	end

	return temppath
end

local function load_texture_packs(txtpath, retval)
	local list = core.get_dir_list(txtpath, true)
	local current_texture_path = core.settings:get("texture_path")

	for _, item in ipairs(list) do
		if item ~= "base" then
			local path = txtpath .. DIR_DELIM .. item .. DIR_DELIM
			local conf = Settings(path .. "texture_pack.conf")
			local enabled = path == current_texture_path

			local title = conf:get("title") or item

			-- list_* is only used if non-nil, else the regular versions are used.
			retval[#retval + 1] = {
				name = item,
				title = title,
				list_name = enabled and fgettext("$1 (Enabled)", item) or nil,
				list_title = enabled and fgettext("$1 (Enabled)", title) or nil,
				author = conf:get("author"),
				release = tonumber(conf:get("release")) or 0,
				type = "txp",
				path = path,
				enabled = enabled,
			}
		end
	end
end

--modmanager implementation
pkgmgr = {}

--- Scans a directory recursively for mods and adds them to `listing`
-- @param path         Absolute directory path to scan recursively
-- @param virtual_path Prettified unique path (e.g. "mods", "mods/mt_modpack")
-- @param listing      Input. Flat array to insert located mods and modpacks
-- @param modpack      Currently processing modpack or nil/"" if none (recursion)
function pkgmgr.get_mods(path, virtual_path, listing, modpack)
	local mods = core.get_dir_list(path, true)
	local added = {}
	for _, name in ipairs(mods) do
		if name:sub(1, 1) ~= "." then
			local mod_path = path .. DIR_DELIM .. name
			local mod_virtual_path = virtual_path .. "/" .. name
			local toadd = {
				dir_name = name,
				parent_dir = path,
			}
			listing[#listing + 1] = toadd
			added[#added + 1] = toadd

			-- Get config file
			local mod_conf
			local modpack_conf = io.open(mod_path .. DIR_DELIM .. "modpack.conf")
			if modpack_conf then
				toadd.is_modpack = true
				modpack_conf:close()

				mod_conf = Settings(mod_path .. DIR_DELIM .. "modpack.conf"):to_table()
				if mod_conf.name then
					name = mod_conf.name
					toadd.is_name_explicit = true
				end
			else
				mod_conf = Settings(mod_path .. DIR_DELIM .. "mod.conf"):to_table()
				if mod_conf.name then
					name = mod_conf.name
					toadd.is_name_explicit = true
				end
			end

			-- Read from config
			toadd.name = name
			toadd.title = mod_conf.title
			toadd.author = mod_conf.author
			toadd.release = tonumber(mod_conf.release) or 0
			toadd.path = mod_path
			toadd.virtual_path = mod_virtual_path
			toadd.type = "mod"

			-- Check modpack.txt
			-- Note: modpack.conf is already checked above
			local modpackfile = io.open(mod_path .. DIR_DELIM .. "modpack.txt")
			if modpackfile then
				modpackfile:close()
				toadd.is_modpack = true
			end

			-- Deal with modpack contents
			if modpack and modpack ~= "" then
				toadd.modpack = modpack
			elseif toadd.is_modpack then
				toadd.type = "modpack"
				toadd.is_modpack = true
				pkgmgr.get_mods(mod_path, mod_virtual_path, listing, name)
			end
		end
	end

	pkgmgr.update_translations(added)

	if not modpack then
		-- Sort all when the recursion is done
		table.sort(listing, function(a, b)
			return a.virtual_path:lower() < b.virtual_path:lower()
		end)
	end
end

--------------------------------------------------------------------------------
function pkgmgr.reload_texture_packs()
	local txtpath = core.get_texturepath()
	local txtpath_system = core.get_texturepath_share()
	local retval = {}

	load_texture_packs(txtpath, retval)

	-- on portable versions these two paths coincide. It avoids loading the path twice
	if txtpath ~= txtpath_system then
		load_texture_packs(txtpath_system, retval)
	end

	pkgmgr.update_translations(retval)

	table.sort(retval, function(a, b)
		return a.title:lower() < b.title:lower()
	end)

	pkgmgr.texture_packs = retval
end

--------------------------------------------------------------------------------
function pkgmgr.get_all()
	pkgmgr.load_all()

	local result = {}

	for _, mod in pairs(pkgmgr.global_mods:get_list()) do
		result[#result + 1] = mod
	end
	for _, game in pairs(pkgmgr.games) do
		result[#result + 1] = game
	end
	for _, txp in pairs(pkgmgr.texture_packs) do
		result[#result + 1] = txp
	end

	return result
end

--------------------------------------------------------------------------------
function pkgmgr.get_folder_type(path)
	local testfile = io.open(path .. DIR_DELIM .. "init.lua","r")
	if testfile ~= nil then
		testfile:close()
		return { type = "mod", path = path }
	end

	testfile = io.open(path .. DIR_DELIM .. "modpack.conf","r")
	if testfile ~= nil then
		testfile:close()
		return { type = "modpack", path = path }
	end

	testfile = io.open(path .. DIR_DELIM .. "modpack.txt","r")
	if testfile ~= nil then
		testfile:close()
		return { type = "modpack", path = path }
	end

	testfile = io.open(path .. DIR_DELIM .. "game.conf","r")
	if testfile ~= nil then
		testfile:close()
		return { type = "game", path = path }
	end

	testfile = io.open(path .. DIR_DELIM .. "texture_pack.conf","r")
	if testfile ~= nil then
		testfile:close()
		return { type = "txp", path = path }
	end

	return nil
end

-------------------------------------------------------------------------------
function pkgmgr.get_base_folder(temppath)
	if temppath == nil then
		return { type = "invalid", path = "" }
	end

	local ret = pkgmgr.get_folder_type(temppath)
	if ret then
		return ret
	end

	local subdirs = core.get_dir_list(temppath, true)
	if #subdirs == 1 then
		ret = pkgmgr.get_folder_type(temppath .. DIR_DELIM .. subdirs[1])
		if ret then
			return ret
		else
			return { type = "invalid", path = temppath .. DIR_DELIM .. subdirs[1] }
		end
	end

	return nil
end

--------------------------------------------------------------------------------
function pkgmgr.is_valid_modname(modpath)
	return modpath:match("[^a-z0-9_]") == nil
end

--------------------------------------------------------------------------------
--- @param render_list filterlist
--- @param use_technical_names boolean to show technical names instead of human-readable titles
--- @param with_icon table or nil, from virtual path to icon object
function pkgmgr.render_packagelist(render_list, use_technical_names, with_icon)
	if not render_list then
		if not pkgmgr.global_mods then
			pkgmgr.reload_global_mods()
		end
		render_list = pkgmgr.global_mods
	end

	local list = render_list:get_list()
	local retval = {}
	for i, v in ipairs(list) do
		local color = ""
		local icon = 0
		local icon_info = with_icon and with_icon[v.virtual_path or v.path]
		local function update_icon_info(val)
			if val and (not icon_info or (icon_info.type == "warning" and val.type == "error")) then
				icon_info = val
			end
		end

		if v.is_modpack then
			local rawlist = render_list:get_raw_list()
			color = mt_color_dark_green

			for j = 1, #rawlist do
				if rawlist[j].modpack == list[i].name then
					if with_icon then
						update_icon_info(with_icon[rawlist[j].virtual_path or rawlist[j].path])
					end

					if rawlist[j].enabled then
						icon = 1
					else
						-- Modpack not entirely enabled so showing as grey
						color = mt_color_grey
					end
				end
			end
		elseif v.is_game_content or v.type == "game" then
			icon = 1
			color = mt_color_blue

			local rawlist = render_list:get_raw_list()
			if v.type == "game" and with_icon then
				for j = 1, #rawlist do
					if rawlist[j].is_game_content then
						update_icon_info(with_icon[rawlist[j].virtual_path or rawlist[j].path])
					end
				end
			end
		elseif v.enabled or v.type == "txp" then
			icon = 1
			color = mt_color_green
		end

		if icon_info then
			if icon_info.type == "warning" then
				color = mt_color_orange
				icon = 2
			elseif icon_info.type == "error" then
				color = mt_color_red
				icon = 3
			elseif icon_info.type == "update" then
				icon = 4
			else
				error("Unknown icon type " .. icon_info.type)
			end
		end

		retval[#retval + 1] = color
		if v.modpack ~= nil or v.loc == "game" then
			retval[#retval + 1] = "1"
		else
			retval[#retval + 1] = "0"
		end

		if with_icon then
			retval[#retval + 1] = icon
		end

		if use_technical_names then
			retval[#retval + 1] = core.formspec_escape(v.list_name or v.name)
		else
			retval[#retval + 1] = core.formspec_escape(v.list_title or v.list_name or v.title or v.name)
		end
	end

	return table.concat(retval, ",")
end

--------------------------------------------------------------------------------
function pkgmgr.get_dependencies(path)
	if path == nil then
		return {}, {}
	end

	local info = core.get_content_info(path)
	return info.depends or {}, info.optional_depends or {}
end

----------- tests whether all of the mods in the modpack are enabled -----------
function pkgmgr.is_modpack_entirely_enabled(data, name)
	local rawlist = data.list:get_raw_list()
	for j = 1, #rawlist do
		if rawlist[j].modpack == name and not rawlist[j].enabled then
			return false
		end
	end
	return true
end

local function disable_all_by_name(list, name, except)
	for i=1, #list do
		if list[i].name == name and list[i] ~= except then
			list[i].enabled = false
		end
	end
end

---------- toggles or en/disables a mod or modpack and its dependencies --------
local function toggle_mod_or_modpack(list, toggled_mods, enabled_mods, toset, mod)
	if not mod.is_modpack then
		-- Toggle or en/disable the mod
		if toset == nil then
			toset = not mod.enabled
		end
		if mod.enabled ~= toset then
			toggled_mods[#toggled_mods+1] = mod.name
		end
		if toset then
			-- Mark this mod for recursive dependency traversal
			enabled_mods[mod.name] = true

			-- Disable other mods with the same name
			disable_all_by_name(list, mod.name, mod)
		end
		mod.enabled = toset
	else
		-- Toggle or en/disable every mod in the modpack,
		-- interleaved unsupported
		for i = 1, #list do
			if list[i].modpack == mod.name then
				toggle_mod_or_modpack(list, toggled_mods, enabled_mods, toset, list[i])
			end
		end
	end
end

function pkgmgr.enable_mod(this, toset)
	local list = this.data.list:get_list()
	local mod = list[this.data.selected_mod]

	-- Game mods can't be enabled or disabled
	if mod.is_game_content then
		return
	end

	local toggled_mods = {}
	local enabled_mods = {}
	toggle_mod_or_modpack(list, toggled_mods, enabled_mods, toset, mod)

	if next(enabled_mods) == nil then
		-- Mod(s) were disabled, so no dependencies need to be enabled
		table.sort(toggled_mods)
		core.log("info", "Following mods were disabled: " ..
			table.concat(toggled_mods, ", "))
		return
	end

	-- Enable mods' depends after activation

	-- Make a list of mod ids indexed by their names. Among mods with the
	-- same name, enabled mods take precedence, after which game mods take
	-- precedence, being last in the mod list.
	local mod_ids = {}
	for id, mod2 in pairs(list) do
		if mod2.type == "mod" and not mod2.is_modpack then
			local prev_id = mod_ids[mod2.name]
			if not prev_id or not list[prev_id].enabled then
				mod_ids[mod2.name] = id
			end
		end
	end

	-- to_enable is used as a DFS stack with sp as stack pointer
	local to_enable = {}
	local sp = 0
	for name in pairs(enabled_mods) do
		local depends = pkgmgr.get_dependencies(list[mod_ids[name]].path)
		for i = 1, #depends do
			local dependency_name = depends[i]
			if not enabled_mods[dependency_name] then
				sp = sp+1
				to_enable[sp] = dependency_name
			end
		end
	end

	-- If sp is 0, every dependency is already activated
	while sp > 0 do
		local name = to_enable[sp]
		sp = sp-1

		if not enabled_mods[name] then
			enabled_mods[name] = true
			local mod_to_enable = list[mod_ids[name]]
			if not mod_to_enable then
				core.log("warning", "Mod dependency \"" .. name ..
					"\" not found!")
			elseif not mod_to_enable.is_game_content then
				if not mod_to_enable.enabled then
					mod_to_enable.enabled = true
					toggled_mods[#toggled_mods+1] = mod_to_enable.name
				end
				-- Push the dependencies of the dependency onto the stack
				local depends = pkgmgr.get_dependencies(mod_to_enable.path)
				for i = 1, #depends do
					if not enabled_mods[depends[i]] then
						sp = sp+1
						to_enable[sp] = depends[i]
					end
				end
			end
		end
	end

	-- Log the list of enabled mods
	table.sort(toggled_mods)
	core.log("info", "Following mods were enabled: " ..
		table.concat(toggled_mods, ", "))
end

--------------------------------------------------------------------------------
function pkgmgr.get_worldconfig(worldpath)
	local filename = worldpath ..
				DIR_DELIM .. "world.mt"

	local worldfile = Settings(filename)

	local worldconfig = {}
	worldconfig.global_mods = {}
	worldconfig.game_mods = {}

	for key,value in pairs(worldfile:to_table()) do
		if key == "gameid" then
			worldconfig.id = value
		elseif key:sub(0, 9) == "load_mod_" then
			-- Compatibility: Check against "nil" which was erroneously used
			-- as value for fresh configured worlds
			worldconfig.global_mods[key] = value ~= "false" and value ~= "nil"
				and value
		else
			worldconfig[key] = value
		end
	end

	--read gamemods
	local gamespec = pkgmgr.find_by_gameid(worldconfig.id)
	pkgmgr.get_game_mods(gamespec, worldconfig.game_mods)

	return worldconfig
end

--------------------------------------------------------------------------------
-- Caller is responsible for reloading content types (see reload_by_type)
function pkgmgr.install_dir(expected_type, path, basename, targetpath)
	assert(type(expected_type) == "string")
	assert(type(path) == "string")
	assert(basename == nil or type(basename) == "string")
	assert(targetpath == nil or type(targetpath) == "string")

	local basefolder = pkgmgr.get_base_folder(path)

	if expected_type == "txp" then
		assert(basename)

		-- There's no good way to detect a texture pack, so let's just assume
		-- it's correct for now.
		if basefolder and basefolder.type ~= "invalid" and basefolder.type ~= "txp" then
			return nil, fgettext_ne("Unable to install a $1 as a texture pack", basefolder.type)
		end

		local from = basefolder and basefolder.path or path
		if not targetpath then
			targetpath = core.get_texturepath() .. DIR_DELIM .. basename
		end
		core.delete_dir(targetpath)
		if not core.copy_dir(from, targetpath, false) then
			return nil,
				fgettext_ne("Failed to install $1 to $2", basename, targetpath)
		end
		return targetpath, nil

	elseif not basefolder then
		return nil, fgettext_ne("Unable to find a valid mod, modpack, or game")
	end

	-- Check type
	if basefolder.type ~= expected_type and (basefolder.type ~= "modpack" or expected_type ~= "mod") then
		return nil, fgettext_ne("Unable to install a $1 as a $2", basefolder.type, expected_type)
	end

	-- Set targetpath if not predetermined
	if not targetpath then
		local content_path
		if basefolder.type == "modpack" or basefolder.type == "mod" then
			if not basename then
				basename = get_last_folder(cleanup_path(basefolder.path))
			end
			content_path = core.get_modpath()
		elseif basefolder.type == "game" then
			content_path = core.get_gamepath()
		else
			error("Unknown content type")
		end

		if basename and (basefolder.type ~= "mod" or pkgmgr.is_valid_modname(basename)) then
			targetpath = content_path .. DIR_DELIM .. basename
		else
			return nil,
				fgettext_ne("Install: Unable to find suitable folder name for $1", path)
		end
	end

	-- Copy it
	core.delete_dir(targetpath)
	if not core.copy_dir(basefolder.path, targetpath, false) then
		return nil,
			fgettext_ne("Failed to install $1 to $2", basename, targetpath)
	end

	return targetpath, nil
end

--------------------------------------------------------------------------------
function pkgmgr.preparemodlist(data)
	local retval = {}

	local global_mods = {}
	local game_mods = {}

	--read global mods
	local modpaths = core.get_modpaths()
	for key, modpath in pairs(modpaths) do
		pkgmgr.get_mods(modpath, key, global_mods)
	end

	for i=1,#global_mods,1 do
		global_mods[i].type = "mod"
		global_mods[i].loc = "global"
		global_mods[i].enabled = false
		retval[#retval + 1] = global_mods[i]
	end

	--read game mods
	local gamespec = pkgmgr.find_by_gameid(data.gameid)
	pkgmgr.get_game_mods(gamespec, game_mods)

	if #game_mods > 0 then
		-- Add title
		retval[#retval + 1] = {
			type = "game",
			is_game_content = true,
			name = fgettext("$1 mods", gamespec.title),
			path = gamespec.path
		}
	end

	for i=1,#game_mods,1 do
		game_mods[i].type = "mod"
		game_mods[i].loc = "game"
		game_mods[i].is_game_content = true
		retval[#retval + 1] = game_mods[i]
	end

	if data.worldpath == nil then
		return retval
	end

	--read world mod configuration
	local filename = data.worldpath ..
				DIR_DELIM .. "world.mt"

	local worldfile = Settings(filename)
	for key, value in pairs(worldfile:to_table()) do
		if key:sub(1, 9) == "load_mod_" then
			key = key:sub(10)
			local mod_found = false

			local fallback_found = false
			local fallback_mod = nil

			for i=1, #retval do
				if retval[i].name == key and
						not retval[i].is_modpack then
					if core.is_yes(value) or retval[i].virtual_path == value then
						retval[i].enabled = true
						mod_found = true
						break
					elseif fallback_found then
						-- Only allow fallback if only one mod matches
						fallback_mod = nil
					else
						fallback_found = true
						fallback_mod = retval[i]
					end
				end
			end

			if not mod_found then
				if fallback_mod and value:find("/") then
					fallback_mod.enabled = true
				else
					core.log("info", "Mod: " .. key .. " " .. dump(value) .. " but not found")
				end
			end
		end
	end

	return retval
end

function pkgmgr.compare_package(a, b)
	return a and b and a.name == b.name and a.path == b.path
end

--------------------------------------------------------------------------------
function pkgmgr.comparemod(elem1,elem2)
	if elem1 == nil or elem2 == nil then
		return false
	end
	if elem1.name ~= elem2.name then
		return false
	end
	if elem1.is_modpack ~= elem2.is_modpack then
		return false
	end
	if elem1.type ~= elem2.type then
		return false
	end
	if elem1.modpack ~= elem2.modpack then
		return false
	end

	if elem1.path ~= elem2.path then
		return false
	end

	return true
end

--------------------------------------------------------------------------------
function pkgmgr.reload_global_mods()
	local function is_equal(element,uid) --uid match
		if element.name == uid then
			return true
		end
	end
	pkgmgr.global_mods = filterlist.create(pkgmgr.preparemodlist,
			pkgmgr.comparemod, is_equal, nil, {})
	pkgmgr.global_mods:add_sort_mechanism("alphabetic", sort_mod_list)
	pkgmgr.global_mods:set_sortmode("alphabetic")
end

--------------------------------------------------------------------------------
function pkgmgr.find_by_gameid(gameid)
	for i, game in ipairs(pkgmgr.games) do
		if game.id == gameid then
			return game, i
		end
	end
	return nil, nil
end

--------------------------------------------------------------------------------
function pkgmgr.get_game_mods(gamespec, retval)
	if gamespec ~= nil and
		gamespec.gamemods_path ~= nil and
		gamespec.gamemods_path ~= "" then
		pkgmgr.get_mods(gamespec.gamemods_path, ("games/%s/mods"):format(gamespec.id), retval)
	end
end

--------------------------------------------------------------------------------
function pkgmgr.reload_games()
	pkgmgr.games = core.get_games()
	table.sort(pkgmgr.games, function(a, b)
		return a.title:lower() < b.title:lower()
	end)
	pkgmgr.update_translations(pkgmgr.games)
end

--------------------------------------------------------------------------------
function pkgmgr.reload_by_type(type)
	if type == "game" then
		pkgmgr.reload_games()
	elseif type == "txp" then
		pkgmgr.reload_texture_packs()
	elseif type == "mod" or type == "modpack" then
		pkgmgr.reload_global_mods()
	else
		error("Unknown package type: " .. type)
	end
end

--------------------------------------------------------------------------------
function pkgmgr.load_all()
	if not pkgmgr.global_mods then
		pkgmgr.reload_global_mods()
	end
	if not pkgmgr.games then
		pkgmgr.reload_games()
	end
	if not pkgmgr.texture_packs then
		pkgmgr.reload_texture_packs()
	end
end

--------------------------------------------------------------------------------
function pkgmgr.update_translations(list)
	for _, item in ipairs(list) do
		local info = core.get_content_info(item.path)
		assert(info.path)
		assert(info.textdomain)

		assert(not item.is_translated)
		item.is_translated = true

		if info.title and info.title ~= "" then
			item.title = core.get_content_translation(info.path, info.textdomain,
				core.translate(info.textdomain, info.title))
		end

		if info.description and info.description ~= "" then
			item.description = core.get_content_translation(info.path, info.textdomain,
				core.translate(info.textdomain, info.description))
		end
	end
end

--------------------------------------------------------------------------------
-- Returns the ContentDB ID for an installed piece of content.
function pkgmgr.get_contentdb_id(content)
	-- core.get_games() will return "" instead of nil if there is no "author" field.
	if content.author and content.author ~= "" and content.release > 0 then
		if content.type == "game" then
			return content.author:lower() .. "/" .. content.id
		end
		return content.author:lower() .. "/" .. content.name
	end

	-- Until version 5.8.0, Minetest Game was bundled with the engine.
	-- Unfortunately, the bundled MTG was not versioned (missing "release"
	-- field in game.conf).
	-- Therefore, we consider any installation of MTG that is not versioned,
	-- has not been cloned from Git, and is not system-wide to be updatable.
	if content.type == "game" and content.id == "minetest" and content.release == 0 and
			not core.is_dir(content.path .. "/.git") and core.may_modify_path(content.path) then
		return "minetest/minetest"
	end

	return nil
end

--------------------------------------------------------------------------------
-- read initial data
--------------------------------------------------------------------------------
pkgmgr.reload_games()
