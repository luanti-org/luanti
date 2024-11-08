--Luanti
--Copyright (C) 2022 rubenwardy
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

local make = {}


-- This file defines various component constructors, of the form:
--
--     make.component(setting)
--
-- `setting` is a table representing the settingtype.
--
-- A component is a table with the following:
--
-- * `full_width`: (Optional) true if the component shouldn't reserve space for info / reset.
-- * `info_text`: (Optional) string, informational text shown in an info icon.
-- * `setting`: (Optional) the setting.
-- * `max_w`: (Optional) maximum width, `avail_w` will never exceed this.
-- * `resettable`: (Optional) if this is true, a reset button is shown.
-- * `get_formspec = function(self, avail_w)`:
--     * `avail_w` is the available width for the component.
--     * Returns `fs, used_height`.
--     * `fs` is a string for the formspec.
--       Components should be relative to `0,0`, and not exceed `avail_w` or the returned `used_height`.
--     * `used_height` is the space used by components in `fs`.
-- * `on_submit = function(self, fields, parent)`:
--     * `fields`: submitted formspec fields
--     * `parent`: the fstk element for the settings UI, use to show dialogs
--     * Return true if the event was handled, to prevent future components receiving it.


local function get_label(setting)
	local show_technical_names = core.settings:get_bool("show_technical_names")
	if not show_technical_names and setting.readable_name then
		return fgettext(setting.readable_name)
	end
	return setting.name
end


local function is_valid_number(value)
	return type(value) == "number" and not (value ~= value or value >= math.huge or value <= -math.huge)
end


function make.heading(text)
	return {
		full_width = true,
		get_formspec = function(self, avail_w)
			return ("label[0,0.6;%s]box[0,0.9;%f,0.05;#ccc6]"):format(core.formspec_escape(text), avail_w), 1.2
		end,
	}
end


function make.note(text)
	return {
		full_width = true,
		get_formspec = function(self, avail_w)
			-- Assuming label height 0.4:
			-- Position at y=0 to eat 0.2 of the padding above, leave 0.05.
			-- The returned used_height doesn't include padding.
			return ("label[0,0;%s]"):format(core.colorize("#bbb", core.formspec_escape(text))), 0.2
		end,
	}
end


--- Used for string and numeric style fields
---
--- @param converter Function to coerce values from strings.
--- @param validator Validator function, optional. Returns true when valid.
--- @param stringifier Function to convert values to strings, optional.
local function make_field(converter, validator, stringifier)
	return function(setting)
		return {
			info_text = setting.comment,
			setting = setting,

			get_formspec = function(self, avail_w)
				local value = core.settings:get(setting.name) or setting.default
				self.resettable = core.settings:has(setting.name)

				local fs = ("field[0,0.3;%f,0.8;%s;%s;%s]"):format(
					avail_w - 1.5, setting.name, get_label(setting), core.formspec_escape(value))
				fs = fs .. ("field_enter_after_edit[%s;true]"):format(setting.name)
				fs = fs .. ("button[%f,0.3;1.5,0.8;%s;%s]"):format(avail_w - 1.5, "set_" .. setting.name, fgettext("Set"))

				return fs, 1.1
			end,

			on_submit = function(self, fields)
				if fields["set_" .. setting.name] or fields.key_enter_field == setting.name then
					local value = converter(fields[setting.name])
					if value == nil or (validator and not validator(value)) then
						return true
					end

					if setting.min then
						value = math.max(value, setting.min)
					end
					if setting.max then
						value = math.min(value, setting.max)
					end
					core.settings:set(setting.name, (stringifier or tostring)(value))
					return true
				end
			end,
		}
	end
end


make.float = make_field(tonumber, is_valid_number, function(x)
	local str = tostring(x)
	if str:match("^[+-]?%d+$") then
		str = str .. ".0"
	end
	return str
end)
make.int = make_field(function(x)
	local value = tonumber(x)
	return value and math.floor(value)
end, is_valid_number)
make.string = make_field(tostring, nil)


function make.bool(setting)
	return {
		info_text = setting.comment,
		setting = setting,

		get_formspec = function(self, avail_w)
			local value = core.settings:get_bool(setting.name, core.is_yes(setting.default))
			self.resettable = core.settings:has(setting.name)

			local fs = ("checkbox[0,0.25;%s;%s;%s]"):format(
				setting.name, get_label(setting), tostring(value))
			return fs, 0.5
		end,

		on_submit = function(self, fields)
			if fields[setting.name] == nil then
				return false
			end

			core.settings:set_bool(setting.name, core.is_yes(fields[setting.name]))
			return true
		end,
	}
end


function make.enum(setting)
	return {
		info_text = setting.comment,
		setting = setting,
		max_w = 4.5,

		get_formspec = function(self, avail_w)
			local value = core.settings:get(setting.name) or setting.default
			self.resettable = core.settings:has(setting.name)

			local labels = setting.option_labels or {}

			local items = {}
			for i, option in ipairs(setting.values) do
				items[i] = core.formspec_escape(labels[option] or option)
			end

			local selected_idx = table.indexof(setting.values, value)
			local fs = "label[0,0.1;" .. get_label(setting) .. "]"

			fs = fs .. ("dropdown[0,0.3;%f,0.8;%s;%s;%d;true]"):format(
				avail_w, setting.name, table.concat(items, ","), selected_idx, value)

			return fs, 1.1
		end,

		on_submit = function(self, fields)
			local old_value = core.settings:get(setting.name) or setting.default
			local idx = tonumber(fields[setting.name]) or 0
			local value = setting.values[idx]
			if value == nil or value == old_value then
				return false
			end

			core.settings:set(setting.name, value)
			return true
		end,
	}
end


local function make_path(setting)
	return {
		info_text = setting.comment,
		setting = setting,

		get_formspec = function(self, avail_w)
			local value = core.settings:get(setting.name) or setting.default
			self.resettable = core.settings:has(setting.name)

			local fs = ("field[0,0.3;%f,0.8;%s;%s;%s]"):format(
				avail_w - 3, setting.name, get_label(setting), core.formspec_escape(value))
			fs = fs .. ("button[%f,0.3;1.5,0.8;%s;%s]"):format(avail_w - 3, "pick_" .. setting.name, fgettext("Browse"))
			fs = fs .. ("button[%f,0.3;1.5,0.8;%s;%s]"):format(avail_w - 1.5, "set_" .. setting.name, fgettext("Set"))

			return fs, 1.1
		end,

		on_submit = function(self, fields)
			local dialog_name = "dlg_path_" .. setting.name
			if fields["pick_" .. setting.name] then
				local is_file = setting.type ~= "path"
				core.show_path_select_dialog(dialog_name,
					is_file and fgettext_ne("Select file") or fgettext_ne("Select directory"), is_file)
				return true
			end
			if fields[dialog_name .. "_accepted"] then
				local value = fields[dialog_name .. "_accepted"]
				if value ~= nil then
					core.settings:set(setting.name, value)
				end
				return true
			end
			if fields["set_" .. setting.name] or fields.key_enter_field == setting.name then
				local value = fields[setting.name]
				if value ~= nil then
					core.settings:set(setting.name, value)
				end
				return true
			end
		end,
	}
end

if PLATFORM == "Android" then
	-- The Irrlicht file picker doesn't work on Android.
	make.path = make.string
	make.filepath = make.string
else
	make.path = make_path
	make.filepath = make_path
end


function make.v3f(setting)
	return {
		info_text = setting.comment,
		setting = setting,

		get_formspec = function(self, avail_w)
			local value = vector.from_string(core.settings:get(setting.name) or setting.default)
			self.resettable = core.settings:has(setting.name)

			-- Allocate space for "Set" button
			avail_w = avail_w - 1

			local fs = "label[0,0.1;" .. get_label(setting) .. "]"

			local field_width = (avail_w - 3*0.25) / 3

			fs = fs .. ("field[%f,0.6;%f,0.8;%s;%s;%s]"):format(
				0, field_width, setting.name .. "_x", "X", value.x)
			fs = fs .. ("field[%f,0.6;%f,0.8;%s;%s;%s]"):format(
				field_width + 0.25, field_width, setting.name .. "_y", "Y", value.y)
			fs = fs .. ("field[%f,0.6;%f,0.8;%s;%s;%s]"):format(
				2 * (field_width + 0.25), field_width, setting.name .. "_z", "Z", value.z)

			fs = fs .. ("button[%f,0.6;1,0.8;%s;%s]"):format(avail_w, "set_" .. setting.name, fgettext("Set"))

			return fs, 1.4
		end,

		on_submit = function(self, fields)
			if fields["set_" .. setting.name]  or
					fields.key_enter_field == setting.name .. "_x" or
					fields.key_enter_field == setting.name .. "_y" or
					fields.key_enter_field == setting.name .. "_z" then
				local x = tonumber(fields[setting.name .. "_x"])
				local y = tonumber(fields[setting.name .. "_y"])
				local z = tonumber(fields[setting.name .. "_z"])
				if is_valid_number(x) and is_valid_number(y) and is_valid_number(z) then
					core.settings:set(setting.name, vector.new(x, y, z):to_string())
				else
					core.log("error", "Invalid vector: " .. dump({x, y, z}))
				end
				return true
			end
		end,
	}
end


function make.flags(setting)
	local checkboxes = {}

	return {
		info_text = setting.comment,
		setting = setting,

		get_formspec = function(self, avail_w)
			local fs = {
				"label[0,0.1;" .. get_label(setting) .. "]",
			}

			self.resettable = core.settings:has(setting.name)

			checkboxes = {}
			for _, name in ipairs(setting.possible) do
				checkboxes[name] = false
			end
			local function apply_flags(flag_string, what)
				local prefixed_flags = {}
				for _, name in ipairs(flag_string:split(",")) do
					prefixed_flags[name:trim()] = true
				end
				for _, name in ipairs(setting.possible) do
					local enabled = prefixed_flags[name]
					local disabled = prefixed_flags["no" .. name]
					if enabled and disabled then
						core.log("warning", "Flag " .. name .. " in " .. what .. " " ..
								setting.name .. " both enabled and disabled, ignoring")
					elseif enabled then
						checkboxes[name] = true
					elseif disabled then
						checkboxes[name] = false
					end
				end
			end
			-- First apply the default, which is necessary since flags
			-- which are not overridden may be missing from the value.
			apply_flags(setting.default, "default for setting")
			local value = core.settings:get(setting.name)
			if value then
				apply_flags(value, "setting")
			end

			local columns = math.max(math.floor(avail_w / 2.5), 1)
			local column_width = avail_w / columns
			local x = 0
			local y = 0.55

			for _, possible in ipairs(setting.possible) do
				if x >= avail_w then
					x = 0
					y = y + 0.5
				end

				local is_checked = checkboxes[possible]
				fs[#fs + 1] = ("checkbox[%f,%f;%s;%s;%s]"):format(
					x, y, setting.name .. "_" .. possible,
					core.formspec_escape(possible), tostring(is_checked))
				x = x + column_width
			end

			return table.concat(fs, ""), y + 0.25
		end,

		on_submit = function(self, fields)
			local changed = false
			for name, _ in pairs(checkboxes) do
				local value = fields[setting.name .. "_" .. name]
				if value ~= nil then
					checkboxes[name] = core.is_yes(value)
					changed = true
				end
			end

			if changed then
				local values = {}
				for _, name in ipairs(setting.possible) do
					if checkboxes[name] then
						table.insert(values, name)
					else
						table.insert(values, "no" .. name)
					end
				end

				core.settings:set(setting.name, table.concat(values, ","))
			end
			return changed
		end
	}
end


local function make_noise_params(setting)
	return {
		info_text = setting.comment,
		setting = setting,

		get_formspec = function(self, avail_w)
			-- The "defaults" noise parameter flag doesn't reset a noise
			-- setting to its default value, so we offer a regular reset button.
			self.resettable = core.settings:has(setting.name)

			local fs = "label[0,0.4;" .. get_label(setting) .. "]" ..
					("button[%f,0;2.5,0.8;%s;%s]"):format(avail_w - 2.5, "edit_" .. setting.name, fgettext("Edit"))
			return fs, 0.8
		end,

		on_submit = function(self, fields, tabview)
			if fields["edit_" .. setting.name] then
				local dlg = create_change_mapgen_flags_dlg(setting)
				dlg:set_parent(tabview)
				tabview:hide()
				dlg:show()

				return true
			end
		end,
	}
end

make.noise_params_2d = make_noise_params
make.noise_params_3d = make_noise_params


return make
