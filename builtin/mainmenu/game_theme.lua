-- Luanti
-- Copyright (C) 2013 sapier
-- SPDX-License-Identifier: LGPL-2.1-or-later


mm_game_theme = {}

--------------------------------------------------------------------------------
function mm_game_theme.init()
	mm_game_theme.texturepack = core.settings:get("texture_path")

	mm_game_theme.gameid = nil

	mm_game_theme.music_handle = nil
end

--------------------------------------------------------------------------------
function mm_game_theme.set_engine(hide_decorations)
	mm_game_theme.gameid = nil
	mm_game_theme.stop_music()

	core.set_topleft_text("")

	local have_bg      = false
	local have_overlay = mm_game_theme.set_engine_single("overlay")

	if not have_overlay then
		have_bg = mm_game_theme.set_engine_single("background")
	end

	mm_game_theme.clear_single("header")
	mm_game_theme.clear_single("footer")
	core.set_clouds(false)

	if not hide_decorations then
		mm_game_theme.set_engine_single("header")
		mm_game_theme.set_engine_single("footer")
	end

	if not have_bg then
		if core.settings:get_bool("menu_clouds") then
			core.set_clouds(true)
		else
			mm_game_theme.set_dirt_bg()
		end
	end
end

--------------------------------------------------------------------------------
function mm_game_theme.set_game(gamedetails)
	assert(gamedetails ~= nil)

	if mm_game_theme.gameid == gamedetails.id then
		return
	end
	mm_game_theme.gameid = gamedetails.id
	mm_game_theme.set_music(gamedetails)

	core.set_topleft_text(gamedetails.name)

	local have_bg      = false
	local have_overlay = mm_game_theme.set_game_single("overlay", gamedetails)

	if not have_overlay then
		have_bg = mm_game_theme.set_game_single("background", gamedetails)
	end

	mm_game_theme.clear_single("header")
	mm_game_theme.clear_single("footer")
	core.set_clouds(false)

	mm_game_theme.set_game_single("header", gamedetails)
	mm_game_theme.set_game_single("footer", gamedetails)

	if not have_bg then
		if core.settings:get_bool("menu_clouds") then
			core.set_clouds(true)
		else
			mm_game_theme.set_dirt_bg()
		end
	end
end

--------------------------------------------------------------------------------
function mm_game_theme.clear_single(identifier)
	core.set_background(identifier,"")
end

--------------------------------------------------------------------------------
function mm_game_theme.set_engine_single(identifier)
	--try texture pack first
	if mm_game_theme.texturepack ~= nil then
		local path = mm_game_theme.texturepack .. DIR_DELIM .."menu_" ..
										identifier .. ".png"
		if core.set_background(identifier,path) then
			return true
		end
	end

	local path = defaulttexturedir .. DIR_DELIM .. "menu_" .. identifier .. ".png"
	if core.set_background(identifier, path) then
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function mm_game_theme.set_game_single(identifier, gamedetails)
	assert(gamedetails ~= nil)

	if mm_game_theme.texturepack ~= nil then
		local path = mm_game_theme.texturepack .. DIR_DELIM ..
			gamedetails.id .. "_menu_" .. identifier .. ".png"
		if core.set_background(identifier, path) then
			return true
		end
	end

	-- Find out how many randomized textures the game provides
	local n = 0
	local filename
	local menu_files = core.get_dir_list(gamedetails.path .. DIR_DELIM .. "menu", false)
	for i = 1, #menu_files do
		filename = identifier .. "." .. i .. ".png"
		if table.indexof(menu_files, filename) == -1 then
			n = i - 1
			break
		end
	end
	-- Select random texture, 0 means standard texture
	n = math.random(0, n)
	if n == 0 then
		filename = identifier .. ".png"
	else
		filename = identifier .. "." .. n .. ".png"
	end

	local path = gamedetails.path .. DIR_DELIM .. "menu" ..
		DIR_DELIM .. filename
	if core.set_background(identifier, path) then
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function mm_game_theme.set_dirt_bg()
	if mm_game_theme.texturepack ~= nil then
		local path = mm_game_theme.texturepack .. DIR_DELIM .."default_dirt.png"
		if core.set_background("background", path, true, 128) then
			return true
		end
	end

	-- Use universal fallback texture in textures/base/pack
	local minimalpath = defaulttexturedir .. "menu_bg.png"
	core.set_background("background", minimalpath, true, 128)
end

--------------------------------------------------------------------------------
function mm_game_theme.stop_music()
	if mm_game_theme.music_handle ~= nil then
		core.sound_stop(mm_game_theme.music_handle)
	end
end

--------------------------------------------------------------------------------
function mm_game_theme.set_music(gamedetails)
	mm_game_theme.stop_music()

	assert(gamedetails ~= nil)

	local music_path = gamedetails.path .. DIR_DELIM .. "menu" .. DIR_DELIM .. "theme"
	mm_game_theme.music_handle = core.sound_play(music_path, true)
end
