-- Luanti
-- Copyright (C) 2016 T4im
-- SPDX-License-Identifier: LGPL-2.1-or-later
local setmetatable = setmetatable
local pairs, format = pairs, string.format
local min, max, huge = math.min, math.max, math.huge
local core = core

local profiler = ...
-- Split sampler and profile up, to possibly allow for rotation later.
local sampler = {}
local profile
local stats_total
local logged_time, logged_data

local _stat_mt = {
	get_time_avg = function(self)
		return self.time_all/self.samples
	end,
	get_part_avg = function(self)
		if not self.part_all then
			return 100 -- Extra handling for "total"
		end
		return self.part_all/self.samples
	end,
}
_stat_mt.__index = _stat_mt

function sampler.reset()
	-- Accumulated logged time since last sample.
	-- This helps determining, the relative time a mod used up.
	logged_time = 0
	-- The measurements taken through instrumentation since last sample.
	logged_data = {}

	profile = {
		-- Current mod statistics (max/min over the entire mod lifespan)
		-- Mod specific instrumentation statistics are nested within.
		stats = {},
		-- Current stats over all mods.
		stats_total = setmetatable({
			samples = 0,
			time_min = huge,
			time_max = 0,
			time_all = 0,
			part_min = 100,
			part_max = 100
		}, _stat_mt)
	}
	stats_total = profile.stats_total

	-- Provide access to the most recent profile.
	sampler.profile = profile
end

---
-- Log a measurement for the sampler to pick up later.
-- Keep `log` and its often called functions lean.
-- It will directly add to the instrumentation overhead.
--
function sampler.log(modname, instrument_name, time_diff)
	if time_diff <= 0 then
		if time_diff < 0 then
			-- This **might** have happened on a semi-regular basis with huge mods,
			-- resulting in negative statistics (perhaps midnight time jumps or ntp corrections?).
			core.log("warning", format(
					"Time travel of %s::%s by %dµs.",
					modname, instrument_name, time_diff
			))
		end
		-- Throwing these away is better, than having them mess with the overall result.
		return
	end

	local mod_data = logged_data[modname]
	if mod_data == nil then
		mod_data = {}
		logged_data[modname] = mod_data
	end

	mod_data[instrument_name] = (mod_data[instrument_name] or 0) + time_diff
	-- Update logged time since last sample.
	logged_time = logged_time + time_diff
end

---
-- Return a requested statistic.
-- Initialize if necessary.
--
local function get_statistic(stats_table, name)
	local statistic = stats_table[name]
	if statistic == nil then
		statistic = setmetatable({
			samples = 0,
			time_min = huge,
			time_max = 0,
			time_all = 0,
			part_min = 100,
			part_max = 0,
			part_all = 0,
		}, _stat_mt)
		stats_table[name] = statistic
	end
	return statistic
end

---
-- Update a statistic table
--
local function update_statistic(stats_table, time)
	stats_table.samples = stats_table.samples + 1

	-- Update absolute time (µs) spend by the subject
	stats_table.time_min = min(stats_table.time_min, time)
	stats_table.time_max = max(stats_table.time_max, time)
	stats_table.time_all = stats_table.time_all + time

	-- Update relative time (%) of this sample spend by the subject
	local current_part = (time/logged_time) * 100
	stats_table.part_min = min(stats_table.part_min, current_part)
	stats_table.part_max = max(stats_table.part_max, current_part)
	stats_table.part_all = stats_table.part_all + current_part
end

---
-- Sample all logged measurements each server step.
-- Like any globalstep function, this should not be too heavy,
-- but does not add to the instrumentation overhead.
--
local function sample(dtime)
	-- Rare, but happens and is currently of no informational value.
	if logged_time == 0 then
		return
	end

	for modname, instruments in pairs(logged_data) do
		local mod_stats = get_statistic(profile.stats, modname)
		if mod_stats.instruments == nil then
			-- Current statistics for each instrumentation component
			mod_stats.instruments = {}
		end

		local mod_time = 0
		for instrument_name, time in pairs(instruments) do
			if time > 0 then
				mod_time = mod_time + time
				local instrument_stats = get_statistic(mod_stats.instruments, instrument_name)

				-- Update time of this sample spend by the instrumented function.
				update_statistic(instrument_stats, time)
				-- Reset logged data for the next sample.
				instruments[instrument_name] = 0
			end
		end

		-- Update time of this sample spend by this mod.
		update_statistic(mod_stats, mod_time)
	end

	-- Update the total time spend over all mods.
	stats_total.time_min = min(stats_total.time_min, logged_time)
	stats_total.time_max = max(stats_total.time_max, logged_time)
	stats_total.time_all = stats_total.time_all + logged_time

	stats_total.samples = stats_total.samples + 1
	logged_time = 0
end

---
-- Setup empty profile and register the sampling function
--
function sampler.init()
	sampler.reset()

	if core.settings:get_bool("instrument.profiler") then
		core.register_globalstep(function()
			if logged_time == 0 then
				return
			end
			return profiler.empty_instrument()
		end)
		core.register_globalstep(profiler.instrument {
			func = sample,
			mod = "*profiler*",
			class = "Sampler (update stats)",
			label = false,
		})
	else
		core.register_globalstep(sample)
	end
end

return sampler
