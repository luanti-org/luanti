-- Luanti
-- Copyright (C) 2023-24 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later


-- Screenshot
local screenshot_dir = core.get_cache_path() .. DIR_DELIM .. "cdb"
assert(core.create_dir(screenshot_dir))
local screenshot_downloading = {}
local screenshot_downloaded = {}


local function get_filename(path)
	local parts = path:split("/")
	return parts[#parts]
end


local function get_file_extension(path)
	local parts = path:split(".")
	return parts[#parts]
end


function get_screenshot(package, screenshot_url, level)
	if not screenshot_url then
		return defaulttexturedir .. "no_screenshot.png"
	end

	-- Luanti only supports png and jpg
	local ext = get_file_extension(screenshot_url)
	if ext ~= "png" and ext ~= "jpg" then
		screenshot_url = screenshot_url:sub(0, -#ext - 1) .. "png"
	end

	-- Set thumbnail level
	screenshot_url = screenshot_url:gsub("/thumbnails/[0-9]+/", "/thumbnails/" .. level .. "/")
	screenshot_url = screenshot_url:gsub("/uploads/", "/thumbnails/" .. level .. "/")

	if screenshot_downloading[screenshot_url] then
		return defaulttexturedir .. "loading_screenshot.png"
	end

	local filepath = screenshot_dir .. DIR_DELIM ..
			("%s-%s-%s-l%d-%s"):format(package.type, package.author, package.name,
				level, get_filename(screenshot_url))

	-- Return if already downloaded
	local file = io.open(filepath, "r")
	if file then
		file:close()
		return filepath
	end

	-- Show error if we've failed to download before
	if screenshot_downloaded[screenshot_url] then
		return defaulttexturedir .. "error_screenshot.png"
	end

	-- Download

	local function download_screenshot(params)
		return core.download_file(params.url, params.dest)
	end
	local function callback(success)
		screenshot_downloading[screenshot_url] = nil
		screenshot_downloaded[screenshot_url] = true
		if not success then
			core.log("warning", "Screenshot download failed for some reason")
		end
		ui.update()
	end
	if core.handle_async(download_screenshot,
			{ dest = filepath, url = screenshot_url }, callback) then
		screenshot_downloading[screenshot_url] = true
	else
		core.log("error", "ERROR: async event failed")
		return defaulttexturedir .. "error_screenshot.png"
	end

	return defaulttexturedir .. "loading_screenshot.png"
end
