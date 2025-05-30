-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2023 v-rob, Vincent Robinson <robinsonvincent89@gmail.com>

local next_id = 0

function ui.new_id()
	-- Just increment a monotonic counter and return it as hex. Even at
	-- unreasonably fast ID generation rates, it would take years for this
	-- counter to hit the 2^53 limit and start generating duplicates.
	next_id = next_id + 1
	return string.format("_%X", next_id)
end

ui._ID_CHARS = "a-zA-Z0-9_%-%:"

function ui.is_id(str)
	return type(str) == "string" and str == str:match("^[" .. ui._ID_CHARS .. "]+$")
end

function ui._is_reserved_id(str)
	return ui.is_id(str) and str:match("^[_-]")
end

-- This coordinate size calculation copies the one for fixed-size formspec
-- coordinates in guiFormSpecMenu.cpp.
function ui.get_coord_size()
	return math.floor(0.5555 * 96)
end

function ui._apply_bool(add, prop)
	if add ~= nil then
		return add
	end
	return prop
end

ui._encode = core.encode_network
ui._decode = core.decode_network

function ui._encode_array(format, arr)
	local formatted = {}
	for _, val in ipairs(arr) do
		table.insert(formatted, ui._encode(format, val))
	end

	return ui._encode("IZ", #formatted, table.concat(formatted))
end

function ui._pack_flags(...)
	local flags = 0
	for _, flag in ipairs({...}) do
		flags = bit.bor(bit.lshift(flags, 1), flag and 1 or 0)
	end
	return flags
end

function ui._make_flags()
	return {flags = 0, num_flags = 0, data = {}}
end

function ui._shift_flag(fl, flag)
	-- OR the LSB with the condition, and then right rotate it to the MSB.
	fl.flags = bit.ror(bit.bor(fl.flags, flag and 1 or 0), 1)
	fl.num_flags = fl.num_flags + 1

	return flag
end

function ui._shift_flag_bool(fl, flag)
	if ui._shift_flag(fl, flag ~= nil) then
		ui._shift_flag(fl, flag)
	else
		ui._shift_flag(fl, false)
	end
end

function ui._encode_flag(fl, ...)
	table.insert(fl.data, ui._encode(...))
end

function ui._encode_flags(fl)
	-- We've been shifting into the right the entire time, so flags are in the
	-- upper bits; however, the protocol expects them to be in the lower bits.
	-- So, shift them the appropriate amount into the lower bits.
	local adjusted = bit.rshift(fl.flags, 32 - fl.num_flags)
	return ui._encode("I", adjusted) .. table.concat(fl.data)
end
