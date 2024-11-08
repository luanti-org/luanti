--Luanti
--Copyright (C) 2015 PilzAdam
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

settingtypes = {}

-- A Setting type is a table with the following keys:
--
-- name: Identifier
-- readable_name: Readable title
-- type: Category
--
-- 	name = mod.name,
-- 	readable_name = mod.title,
-- 	level = 1,
-- 	type = "category", "int", "string", ""
-- }


local FILENAME = "settingtypes.txt"

local CHAR_CLASSES = {
	SPACE = "[%s]",
	VARIABLE = "[%w_%-%.]",
	INTEGER = "[+-]?[%d]",
	FLOAT = "[+-]?[%d%.]",
	FLAGS = "[%w_%-%.,]",
}

local function flags_to_table(flags)
	return flags:gsub("%s+", ""):split(",", true) -- Remove all spaces and split
end

-- returns error message, or nil
local function parse_setting_line(settings, line, read_all, base_level, allow_secure)

	-- strip carriage returns (CR, /r)
	line = line:gsub("\r", "")

	-- comment
	local comment_match = line:match("^#" .. CHAR_CLASSES.SPACE .. "*(.*)$")
	if comment_match then
		settings.current_comment[#settings.current_comment + 1] = comment_match
		return
	end

	-- clear current_comment so only comments directly above a setting are bound to it
	-- but keep a local reference to it for variables in the current line
	local current_comment = settings.current_comment
	settings.current_comment = {}

	-- empty lines
	if line:match("^" .. CHAR_CLASSES.SPACE .. "*$") then
		return
	end

	-- category
	local stars, category = line:match("^%[([%*]*)([^%]]+)%]$")
	if category then
		local category_level = stars:len() + base_level

		if settings.current_hide_level then
			if settings.current_hide_level < category_level then
				-- Skip this category, it's inside a hidden category.
				return
			else
				-- The start of this category marks the end of a hidden category.
				settings.current_hide_level = nil
			end
		end

		if not read_all and category:sub(1, 5) == "Hide:" then
			-- This category is hidden.
			settings.current_hide_level = category_level
			return
		end

		table.insert(settings, {
			name = category,
			level = category_level,
			type = "category",
		})
		return
	end

	if settings.current_hide_level then
		-- Ignore this line, we're inside a hidden category.
		return
	end

	-- settings
	local first_part, name, readable_name, setting_type = line:match("^"
			-- this first capture group matches the whole first part,
			--  so we can later strip it from the rest of the line
			.. "("
				.. "([" .. CHAR_CLASSES.VARIABLE .. "+)" -- variable name
				.. CHAR_CLASSES.SPACE .. "*"
				.. "%(([^%)]*)%)"  -- readable name
				.. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.VARIABLE .. "+)" -- type
				.. CHAR_CLASSES.SPACE .. "*"
			.. ")")

	if not first_part then
		return "Invalid line"
	end

	if name:match("secure%.[.]*") and not allow_secure then
		return "Tried to add \"secure.\" setting"
	end

	local requires = {}
	local last_line = #current_comment > 0 and current_comment[#current_comment]:trim()
	if last_line and last_line:lower():sub(1, 9) == "requires:" then
		local parts = last_line:sub(10):split(",")
		current_comment[#current_comment] = nil

		for _, part in ipairs(parts) do
			part = part:trim()

			local value = true
			if part:sub(1, 1) == "!" then
				value = false
				part = part:sub(2):trim()
			end

			requires[part] = value
		end
	end

	if readable_name == "" then
		readable_name = nil
	end
	local remaining_line = line:sub(first_part:len() + 1)

	local comment = table.concat(current_comment, "\n"):trim()

	if setting_type == "int" then
		local default, min, max = remaining_line:match("^"
				-- first int is required, the last 2 are optional
				.. "(" .. CHAR_CLASSES.INTEGER .. "+)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.INTEGER .. "*)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.INTEGER .. "*)"
				.. "$")

		if not default or not tonumber(default) then
			return "Invalid integer setting"
		end

		min = tonumber(min)
		max = tonumber(max)
		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "int",
			default = default,
			min = min,
			max = max,
			requires = requires,
			comment = comment,
		})
		return
	end

	if setting_type == "string"
			or setting_type == "key" or setting_type == "v3f" then
		local default = remaining_line:match("^(.*)$")

		if not default then
			return "Invalid string setting"
		end
		if setting_type == "key" and not read_all then
			-- ignore key type if read_all is false
			return
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = setting_type,
			default = default,
			requires = requires,
			comment = comment,
		})
		return
	end

	if setting_type == "noise_params_2d"
			or setting_type == "noise_params_3d" then
		local default = remaining_line:match("^(.*)$")

		if not default then
			return "Invalid string setting"
		end

		local values = {}
		local ti = 1
		local index = 1
		for match in default:gmatch("[+-]?[%d.-e]+") do -- All numeric characters
			index = default:find("[+-]?[%d.-e]+", index) + match:len()
			table.insert(values, match)
			ti = ti + 1
			if ti > 9 then
				break
			end
		end
		index = default:find("[^, ]", index)
		local flags = ""
		if index then
			flags = default:sub(index)
		end
		table.insert(values, flags)

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = setting_type,
			default = default,
			default_table = {
				offset = values[1],
				scale = values[2],
				spread = {
					x = values[3],
					y = values[4],
					z = values[5]
				},
				seed = values[6],
				octaves = values[7],
				persistence = values[8],
				lacunarity = values[9],
				flags = values[10]
			},
			values = values,
			requires = requires,
			comment = comment,
			noise_params = true,
			flags = flags_to_table("defaults,eased,absvalue")
		})
		return
	end

	if setting_type == "bool" then
		if remaining_line ~= "false" and remaining_line ~= "true" then
			return "Invalid boolean setting"
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "bool",
			default = remaining_line,
			requires = requires,
			comment = comment,
		})
		return
	end

	if setting_type == "float" then
		local default, min, max = remaining_line:match("^"
				-- first float is required, the last 2 are optional
				.. "(" .. CHAR_CLASSES.FLOAT .. "+)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLOAT .. "*)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLOAT .. "*)"
				.."$")

		if not default or not tonumber(default) then
			return "Invalid float setting"
		end

		min = tonumber(min)
		max = tonumber(max)
		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "float",
			default = default,
			min = min,
			max = max,
			requires = requires,
			comment = comment,
		})
		return
	end

	if setting_type == "enum" then
		local default, values = remaining_line:match("^"
				-- first value (default) may be empty (i.e. is optional)
				.. "(" .. CHAR_CLASSES.VARIABLE .. "*)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLAGS .. "+)"
				.. "$")

		if not default or values == "" then
			return "Invalid enum setting"
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "enum",
			default = default,
			values = values:split(",", true),
			requires = requires,
			comment = comment,
		})
		return
	end

	if setting_type == "path" or setting_type == "filepath" then
		local default = remaining_line:match("^(.*)$")

		if not default then
			return "Invalid path setting"
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = setting_type,
			default = default,
			requires = requires,
			comment = comment,
		})
		return
	end

	if setting_type == "flags" then
		local default, possible = remaining_line:match("^"
				-- first value (default) may be empty (i.e. is optional)
				-- this is implemented by making the last value optional, and
				-- swapping them around if it turns out empty.
				.. "(" .. CHAR_CLASSES.FLAGS .. "+)" .. CHAR_CLASSES.SPACE .. "*"
				.. "(" .. CHAR_CLASSES.FLAGS .. "*)"
				.. "$")

		if not default or not possible then
			return "Invalid flags setting"
		end

		if possible == "" then
			possible = default
			default = ""
		end

		table.insert(settings, {
			name = name,
			readable_name = readable_name,
			type = "flags",
			default = default,
			possible = flags_to_table(possible),
			requires = requires,
			comment = comment,
		})
		return
	end

	return "Invalid setting type \"" .. setting_type .. "\""
end

local function parse_single_file(file, filepath, read_all, result, base_level, allow_secure)
	-- store this helper variable in the table so it's easier to pass to parse_setting_line()
	result.current_comment = {}
	result.current_hide_level = nil

	local line = file:read("*line")
	while line do
		local error_msg = parse_setting_line(result, line, read_all, base_level, allow_secure)
		if error_msg then
			core.log("error", error_msg .. " in " .. filepath .. " \"" .. line .. "\"")
		end
		line = file:read("*line")
	end

	result.current_comment = nil
	result.current_hide_level = nil
end


--- Returns table of setting types
--
-- @param read_all Whether to ignore certain setting types for GUI or not
-- @parse_mods Whether to parse settingtypes.txt in mods and games
function settingtypes.parse_config_file(read_all, parse_mods)
	local settings = {}

	do
		local builtin_path = core.get_builtin_path() .. FILENAME
		local file = io.open(builtin_path, "r")
		if not file then
			core.log("error", "Can't load " .. FILENAME)
			return settings
		end

		parse_single_file(file, builtin_path, read_all, settings, 0, true)

		file:close()
	end

	if parse_mods then
		-- Parse games
		local games_category_initialized = false
		for _, game in ipairs(pkgmgr.games) do
			local path = game.path .. DIR_DELIM .. FILENAME
			local file = io.open(path, "r")
			if file then
				if not games_category_initialized then
					fgettext_ne("Content: Games") -- not used, but needed for xgettext
					table.insert(settings, {
						name = "Content: Games",
						level = 0,
						type = "category",
					})
					games_category_initialized = true
				end

				table.insert(settings, {
					name = game.path,
					readable_name = game.title,
					level = 1,
					type = "category",
				})

				parse_single_file(file, path, read_all, settings, 2, false)

				file:close()
			end
		end

		-- Parse mods
		pkgmgr.load_all()
		local mods_category_initialized = false
		local mods = pkgmgr.global_mods:get_list()
		table.sort(mods, function(a, b) return a.name < b.name end)

		for _, mod in ipairs(mods) do
			local path = mod.path .. DIR_DELIM .. FILENAME
			local file = io.open(path, "r")
			if file then
				if not mods_category_initialized then
					fgettext_ne("Content: Mods") -- not used, but needed for xgettext
					table.insert(settings, {
						name = "Content: Mods",
						level = 0,
						type = "category",
					})
					mods_category_initialized = true
				end

				table.insert(settings, {
					name = mod.path,
					readable_name = mod.title or mod.name,
					level = 1,
					type = "category",
				})

				parse_single_file(file, path, read_all, settings, 2, false)

				file:close()
			end
		end

		-- Parse client mods
		local clientmods_category_initialized = false
		local clientmods = {}
		pkgmgr.get_mods(core.get_clientmodpath(), "clientmods", clientmods)
		for _, mod in ipairs(clientmods) do
			local path = mod.path .. DIR_DELIM .. FILENAME
			local file = io.open(path, "r")
			if file then
				if not clientmods_category_initialized then
					fgettext_ne("Client Mods") -- not used, but needed for xgettext
					table.insert(settings, {
						name = "Client Mods",
						level = 0,
						type = "category",
					})
					clientmods_category_initialized = true
				end

				table.insert(settings, {
					name = mod.path,
					readable_name = mod.title or mod.name,
					level = 1,
					type = "category",
				})

				parse_single_file(file, path, read_all, settings, 2, false)

				file:close()
			end
		end
	end

	return settings
end
