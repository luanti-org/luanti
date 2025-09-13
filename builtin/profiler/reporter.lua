-- Luanti
-- Copyright (C) 2016 T4im
-- SPDX-License-Identifier: LGPL-2.1-or-later

local S = core.get_translator("__builtin")
-- Note: In this file, only messages are translated
-- but not the table itself, to keep it simple.

local DIR_DELIM, LINE_DELIM = DIR_DELIM, "\n"
local table, unpack, string, pairs, io, os = table, unpack, string, pairs, io, os
local rep, sprintf, tonumber = string.rep, string.format, tonumber
local core, settings = core, core.settings
local reporter = {}

---
-- Shorten a string. End on an ellipsis if shortened.
--
local function shorten(str, length)
	if str and str:len() > length then
		return "..." .. str:sub(-(length-3))
	end
	return str
end

local function filter_matches(filter, text)
	return not filter or string.match(text, filter)
end

local function format_number(number, fmt)
	number = tonumber(number)
	if not number then
		return "N/A"
	end
	return sprintf(fmt or "%d", number)
end

local Formatter = {
	new = function(self, object)
		object = object or {}
		object.out = {} -- output buffer
		self.__index = self
		return setmetatable(object, self)
	end,
	__tostring = function (self)
		return table.concat(self.out, LINE_DELIM)
	end,
	print = function(self, text, ...)
		if (...) then
			text = sprintf(text, ...)
		end

		if text then
			-- Avoid format unicode issues.
			text = text:gsub("Ms", "µs")
		end

		table.insert(self.out, text or LINE_DELIM)
	end,
	flush = function(self)
		table.insert(self.out, LINE_DELIM)
		local text = table.concat(self.out, LINE_DELIM)
		self.out = {}
		return text
	end
}

local widths = { 80, 9, 9, 9, 5, 5, 5 }
local txt_row_format = sprintf(" %%-%ds | %%%ds | %%%ds | %%%ds | %%%ds | %%%ds | %%%ds", unpack(widths))

local HR = {}
for i=1, #widths do
	HR[i]= rep("-", widths[i])
end
-- ' | ' should break less with github than '-+-', when people are pasting there
HR = sprintf("-%s-", table.concat(HR, " | "))

local TxtFormatter = Formatter:new {
	format_row = function(self, modname, instrument_name, statistics)
		local label
		if instrument_name then
			label = shorten(instrument_name, widths[1] - 5)
			label = sprintf(" - %s %s", label, rep(".", widths[1] - 5 - label:len()))
		else -- Print mod_stats
			label = shorten(modname, widths[1] - 2) .. ":"
		end

		self:print(txt_row_format, label,
			format_number(statistics.time_min),
			format_number(statistics.time_max),
			format_number(statistics:get_time_avg()),
			format_number(statistics.part_min, "%.1f"),
			format_number(statistics.part_max, "%.1f"),
			format_number(statistics:get_part_avg(), "%.1f")
		)
	end,
	format = function(self, filter)
		local profile = self.profile
		self:print(S("Values below show absolute/relative times spend per server step by the instrumented function."))
		self:print(S("A total of @1 sample(s) were taken.", profile.stats_total.samples))

		if filter then
			self:print(S("The output is limited to '@1'.", filter))
		end

		self:print()
		self:print(
			txt_row_format,
			"instrumentation", "min Ms", "max Ms", "avg Ms", "min %", "max %", "avg %"
		)
		self:print(HR)
		for modname,mod_stats in pairs(profile.stats) do
			if filter_matches(filter, modname) then
				self:format_row(modname, nil, mod_stats)

				if mod_stats.instruments ~= nil then
					for instrument_name, instrument_stats in pairs(mod_stats.instruments) do
						self:format_row(nil, instrument_name, instrument_stats)
					end
				end
			end
		end
		self:print(HR)
		if not filter then
			self:format_row("total", nil, profile.stats_total)
		end
	end
}

local CsvFormatter = Formatter:new {
	format_row = function(self, modname, instrument_name, statistics)
		self:print(
			"%q,%q,%d,%d,%d,%d,%d,%f,%f,%f",
			modname, instrument_name,
			statistics.samples,
			statistics.time_min,
			statistics.time_max,
			statistics:get_time_avg(),
			statistics.time_all,
			statistics.part_min,
			statistics.part_max,
			statistics:get_part_avg()
		)
	end,
	format = function(self, filter)
		self:print(
			"%q,%q,%q,%q,%q,%q,%q,%q,%q,%q",
			"modname", "instrumentation",
			"samples",
			"time min µs",
			"time max µs",
			"time avg µs",
			"time all µs",
			"part min %",
			"part max %",
			"part avg %"
		)
		for modname, mod_stats in pairs(self.profile.stats) do
			if filter_matches(filter, modname) then
				self:format_row(modname, "*", mod_stats)

				if mod_stats.instruments ~= nil then
					for instrument_name, instrument_stats in pairs(mod_stats.instruments) do
						self:format_row(modname, instrument_name, instrument_stats)
					end
				end
			end
		end
	end
}

local function format_statistics(profile, format, filter, enable_translation)
	local formatter
	if format == "csv" then
		formatter = CsvFormatter:new {
			profile = profile
		}
	else
		formatter = TxtFormatter:new {
			profile = profile
		}
	end
	formatter:format(filter)
	local out = formatter:flush()
	if not enable_translation then
		out = core.strip_escapes(out)
	end
	return out
end

---
-- Format the profile ready for display and
-- @return string to be printed to the console
--
function reporter.print(profile, filter, enable_translation)
	if filter == "" then filter = nil end
	return format_statistics(profile, "txt", filter, enable_translation)
end

---
-- Serialize the profile data and
-- @return serialized data to be saved to a file
--
local function serialize_profile(profile, format, filter)
	if format == "lua" or format == "json" or format == "json_pretty" then
		local stats = filter and {} or profile.stats
		if filter then
			for modname, mod_stats in pairs(profile.stats) do
				if filter_matches(filter, modname) then
					stats[modname] = mod_stats
				end
			end
		end
		if format == "lua" then
			return core.serialize(stats)
		elseif format == "json" then
			return core.write_json(stats)
		elseif format == "json_pretty" then
			return core.write_json(stats, true)
		end
	end
	-- Fall back to textual formats.
	return format_statistics(profile, format, filter, false)
end

local worldpath = core.get_worldpath()
local function get_save_path(format, filter)
	local report_path = settings:get("profiler.report_path") or ""
	if report_path ~= "" then
		core.mkdir(sprintf("%s%s%s", worldpath, DIR_DELIM, report_path))
	end
	return (sprintf(
		"%s/%s/profile-%s%s.%s",
		worldpath,
		report_path,
		os.date("%Y%m%dT%H%M%S"),
		filter and ("-" .. filter) or "",
		format
	):gsub("[/\\]+", DIR_DELIM))-- Clean up delims
end

---
-- Save the profile to the world path.
-- @return success, log message
--
function reporter.save(profile, format, filter)
	if not format or format == "" then
		format = settings:get("profiler.default_report_format") or "txt"
	end
	if filter == "" then
		filter = nil
	end

	local path = get_save_path(format, filter)

	local output, io_err = io.open(path, "w")
	if not output then
		return false, S("Saving of profile failed: @1", io_err)
	end
	local content, err = serialize_profile(profile, format, filter)
	if not content then
		output:close()
		return false, S("Saving of profile failed: @1", err)
	end
	output:write(content)
	output:close()

	core.log("action", "Profile saved to " .. path)
	return true, S("Profile saved to @1", path)
end

return reporter
