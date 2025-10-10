--------------------------------------------------------------------------------
-- Localize functions to avoid table lookups (better performance).
local string_sub, string_find, string_rep = string.sub, string.find, string.rep
local math = math

--------------------------------------------------------------------------------
local function basic_dump(o)
	local tp = type(o)
	if tp == "number" then
		local s = tostring(o)
		if tonumber(s) == o then
			return s
		end
		-- Prefer an exact representation over a compact representation.
		-- e.g. basic_dump(0.3) == "0.3",
		-- but basic_dump(0.1 + 0.2) == "0.30000000000000004"
		-- so the user can see that 0.1 + 0.2 ~= 0.3
		return string.format("%.17g", o)
	elseif tp == "string" then
		return string.format("%q", o)
	elseif tp == "boolean" then
		return tostring(o)
	elseif tp == "nil" then
		return "nil"
	elseif tp == "userdata" then
		return tostring(o)
	else
		return string.format("<%s>", tp)
	end
end

local keywords = {
	["and"] = true,
	["break"] = true,
	["do"] = true,
	["else"] = true,
	["elseif"] = true,
	["end"] = true,
	["false"] = true,
	["for"] = true,
	["function"] = true,
	["goto"] = true,  -- Lua 5.2
	["if"] = true,
	["in"] = true,
	["local"] = true,
	["nil"] = true,
	["not"] = true,
	["or"] = true,
	["repeat"] = true,
	["return"] = true,
	["then"] = true,
	["true"] = true,
	["until"] = true,
	["while"] = true,
}
local function is_valid_identifier(str)
	if not str:find("^[a-zA-Z_][a-zA-Z0-9_]*$") or keywords[str] then
		return false
	end
	return true
end

--------------------------------------------------------------------------------
-- Dumps values in a line-per-value format.
-- For example, {test = {"Testing..."}} becomes:
--   _["test"] = {}
--   _["test"][1] = "Testing..."
-- This handles tables as keys and circular references properly.
-- It also handles multiple references well, writing the table only once.
-- The dumped argument is internal-only.

function dump2(o, name, dumped)
	name = name or "_"
	-- "dumped" is used to keep track of serialized tables to handle
	-- multiple references and circular tables properly.
	-- It only contains tables as keys.  The value is the name that
	-- the table has in the dump, eg:
	-- {x = {"y"}} -> dumped[{"y"}] = '_["x"]'
	dumped = dumped or {}
	if type(o) ~= "table" then
		return string.format("%s = %s\n", name, basic_dump(o))
	end
	if dumped[o] then
		return string.format("%s = %s\n", name, dumped[o])
	end
	dumped[o] = name
	-- This contains a list of strings to be concatenated later (because
	-- Lua is slow at individual concatenation).
	local t = {}
	for k, v in pairs(o) do
		local keyStr
		if type(k) == "table" then
			if dumped[k] then
				keyStr = dumped[k]
			else
				-- Key tables don't have a name, so use one of
				-- the form _G["table: 0xFFFFFFF"]
				keyStr = string.format("_G[%q]", tostring(k))
				-- Dump key table
				t[#t + 1] = dump2(k, keyStr, dumped)
			end
		else
			keyStr = basic_dump(k)
		end
		local vname = string.format("%s[%s]", name, keyStr)
		t[#t + 1] = dump2(v, vname, dumped)
	end
	return string.format("%s = {}\n%s", name, table.concat(t))
end


-- This dumps values in a human-readable expression format.
-- If possible, the resulting string should evaluate to an equivalent value if loaded and executed.
-- For example, {test = {"Testing..."}} becomes:
-- [[{
-- 	test = {
-- 		"Testing..."
-- 	}
-- }]]
function dump(value, indent)
	indent = indent or "\t"
	local newline = indent == "" and "" or "\n"

	local rope = {}
	local write
	do
		-- Keeping the length of the table as a local variable is *much*
		-- faster than invoking the length operator.
		-- See https://gitspartv.github.io/LuaJIT-Benchmarks/#test12.
		local i = 0
		function write(str)
			i = i + 1
			rope[i] = str
		end
	end

	local n_refs = {}
	local function count_refs(val)
		if type(val) ~= "table" then
			return
		end
		local tbl = val
		if n_refs[tbl] then
			n_refs[tbl] = n_refs[tbl] + 1
			return
		end
		n_refs[tbl] = 1
		for k, v in pairs(tbl) do
			count_refs(k)
			count_refs(v)
		end
	end
	count_refs(value)

	local refs = {}
	local cur_ref = 1
	local function write_value(val, level)
		if type(val) ~= "table" then
			write(basic_dump(val))
			return
		end

		local tbl = val
		if refs[tbl] then
			write(refs[tbl])
			return
		end

		if n_refs[val] > 1 then
			refs[val] = ("getref(%d)"):format(cur_ref)
			write(("setref(%d)"):format(cur_ref))
			cur_ref = cur_ref + 1
		end
		write("{")
		if next(tbl) == nil then
			write("}")
			return
		end
		write(newline)

		local function write_entry(k, v)
			write(string_rep(indent, level))
			write("[")
			write_value(k, level + 1)
			write("] = ")
			write_value(v, level + 1)
			write(",")
			write(newline)
		end

		local keys = {string = {}, number = {}}
		for k in pairs(tbl) do
			local t = type(k)
			if keys[t] then
				table.insert(keys[t], k)
			end
		end

		-- Write string-keyed entries
		table.sort(keys.string)
		for _, k in ipairs(keys.string) do
			local v = val[k]
			if is_valid_identifier(k) then
				write(string_rep(indent, level))
				write(k)
				write(" = ")
				write_value(v, level + 1)
				write(",")
				write(newline)
			else
				write_entry(k, v)
			end
		end

		-- Write number-keyed entries
		local len = 0
		for i in ipairs(tbl) do
			len = i
		end
		if #keys.number == len then -- table is a list
			for _, v in ipairs(tbl) do
				write(string_rep(indent, level))
				write_value(v, level + 1)
				write(",")
				write(newline)
			end
		else -- table harbors arbitrary number keys
			table.sort(keys.number)
			for _, k in ipairs(keys.number) do
				write_entry(k, tbl[k])
			end
		end

		-- Write all remaining entries
		for k, v in pairs(val) do
			if not keys[type(k)] then
				write_entry(k, v)
			end
		end

		write(string_rep(indent, level - 1))
		write("}")
	end
	write_value(value, 1)
	return table.concat(rope)
end

--------------------------------------------------------------------------------
function string.split(str, delim, include_empty, max_splits, sep_is_pattern)
	delim = delim or ","
	if delim == "" then
		error("string.split separator is empty", 2)
	end
	max_splits = max_splits or -2
	local items = {}
	local pos, len = 1, #str
	local plain = not sep_is_pattern
	max_splits = max_splits + 1
	repeat
		local np, npe = string_find(str, delim, pos, plain)
		np, npe = (np or (len+1)), (npe or (len+1))
		if (not np) or (max_splits == 1) then
			np = len + 1
			npe = np
		end
		local s = string_sub(str, pos, np - 1)
		if include_empty or (s ~= "") then
			max_splits = max_splits - 1
			items[#items + 1] = s
		end
		pos = npe + 1
	until (max_splits == 0) or (pos > (len + 1))
	return items
end

--------------------------------------------------------------------------------
function table.indexof(list, val)
	for i, v in ipairs(list) do
		if v == val then
			return i
		end
	end
	return -1
end

--------------------------------------------------------------------------------
function table.keyof(tb, val)
	for k, v in pairs(tb) do
		if v == val then
			return k
		end
	end
	return nil
end

--------------------------------------------------------------------------------
function string:trim()
	return self:match("^%s*(.-)%s*$")
end

local formspec_escapes = {
	["\\"] = "\\\\",
	["["] = "\\[",
	["]"] = "\\]",
	[";"] = "\\;",
	[","] = "\\,",
	["$"] = "\\$",
}
function core.formspec_escape(text)
	-- Use explicit character set instead of dot here because it doubles the performance
	return text and string.gsub(text, "[\\%[%];,$]", formspec_escapes)
end


local hypertext_escapes = {
	["\\"] = "\\\\",
	["<"] = "\\<",
	[">"] = "\\>",
}
function core.hypertext_escape(text)
	return text and text:gsub("[\\<>]", hypertext_escapes)
end


function core.wrap_text(text, max_length, as_table)
	local result = {}
	local line = {}
	if #text <= max_length then
		return as_table and {text} or text
	end

	local line_length = 0
	for word in text:gmatch("%S+") do
		if line_length > 0 and line_length + #word + 1 >= max_length then
			-- word wouldn't fit on current line, move to next line
			table.insert(result, table.concat(line, " "))
			line = {word}
			line_length = #word
		else
			table.insert(line, word)
			line_length = line_length + 1 + #word
		end
	end

	table.insert(result, table.concat(line, " "))
	return as_table and result or table.concat(result, "\n")
end

--------------------------------------------------------------------------------

if INIT == "game" then
	local dirs1 = {9, 18, 7, 12}
	local dirs2 = {20, 23, 22, 21}

	function core.rotate_and_place(itemstack, placer, pointed_thing,
			infinitestacks, orient_flags, prevent_after_place)
		orient_flags = orient_flags or {}

		local unode = core.get_node_or_nil(pointed_thing.under)
		if not unode then
			return
		end
		local undef = core.registered_nodes[unode.name]
		local sneaking = placer and placer:get_player_control().sneak
		if undef and undef.on_rightclick and not sneaking then
			return undef.on_rightclick(pointed_thing.under, unode, placer,
					itemstack, pointed_thing)
		end
		local fdir = placer and core.dir_to_facedir(placer:get_look_dir()) or 0

		local above = pointed_thing.above
		local under = pointed_thing.under
		local iswall = (above.y == under.y)
		local isceiling = not iswall and (above.y < under.y)

		if undef and undef.buildable_to then
			iswall = false
		end

		if orient_flags.force_floor then
			iswall = false
			isceiling = false
		elseif orient_flags.force_ceiling then
			iswall = false
			isceiling = true
		elseif orient_flags.force_wall then
			iswall = true
			isceiling = false
		elseif orient_flags.invert_wall then
			iswall = not iswall
		end

		local param2 = fdir
		if iswall then
			param2 = dirs1[fdir + 1]
		elseif isceiling then
			if orient_flags.force_facedir then
				param2 = 20
			else
				param2 = dirs2[fdir + 1]
			end
		else -- place right side up
			if orient_flags.force_facedir then
				param2 = 0
			end
		end

		local old_itemstack = ItemStack(itemstack)
		local new_itemstack = core.item_place_node(itemstack, placer,
				pointed_thing, param2, prevent_after_place)
		return infinitestacks and old_itemstack or new_itemstack
	end


--------------------------------------------------------------------------------
--Wrapper for rotate_and_place() to check for sneak and assume Creative mode
--implies infinite stacks when performing a 6d rotation.
--------------------------------------------------------------------------------
	core.rotate_node = function(itemstack, placer, pointed_thing)
		local name = placer and placer:get_player_name() or ""
		local invert_wall = placer and placer:get_player_control().sneak or false
		return core.rotate_and_place(itemstack, placer, pointed_thing,
			core.is_creative_enabled(name),
			{invert_wall = invert_wall}, true)
	end
end

--------------------------------------------------------------------------------
function core.explode_table_event(evt)
	if evt ~= nil then
		local parts = evt:split(":")
		if #parts == 3 then
			local t = parts[1]:trim()
			local r = tonumber(parts[2]:trim())
			local c = tonumber(parts[3]:trim())
			if type(r) == "number" and type(c) == "number"
					and t ~= "INV" then
				return {type=t, row=r, column=c}
			end
		end
	end
	return {type="INV", row=0, column=0}
end

--------------------------------------------------------------------------------
function core.explode_textlist_event(evt)
	if evt ~= nil then
		local parts = evt:split(":")
		if #parts == 2 then
			local t = parts[1]:trim()
			local r = tonumber(parts[2]:trim())
			if type(r) == "number" and t ~= "INV" then
				return {type=t, index=r}
			end
		end
	end
	return {type="INV", index=0}
end

--------------------------------------------------------------------------------
function core.explode_scrollbar_event(evt)
	local retval = core.explode_textlist_event(evt)

	retval.value = retval.index
	retval.index = nil

	return retval
end

--------------------------------------------------------------------------------
function core.rgba(r, g, b, a)
	return a and string.format("#%02X%02X%02X%02X", r, g, b, a) or
			string.format("#%02X%02X%02X", r, g, b)
end

--------------------------------------------------------------------------------
function core.pos_to_string(pos, decimal_places)
	local x = pos.x
	local y = pos.y
	local z = pos.z
	if decimal_places ~= nil then
		x = string.format("%." .. decimal_places .. "f", x)
		y = string.format("%." .. decimal_places .. "f", y)
		z = string.format("%." .. decimal_places .. "f", z)
	end
	return "(" .. x .. "," .. y .. "," .. z .. ")"
end

--------------------------------------------------------------------------------
function core.string_to_pos(value)
	if value == nil then
		return nil
	end

	value = value:match("^%((.-)%)$") or value -- strip parentheses

	local x, y, z = value:trim():match("^([%d.-]+)[,%s]%s*([%d.-]+)[,%s]%s*([%d.-]+)$")
	if x and y and z then
		x = tonumber(x)
		y = tonumber(y)
		z = tonumber(z)
		return vector.new(x, y, z)
	end

	return nil
end


--------------------------------------------------------------------------------

do
	local rel_num_cap = "(~?-?%d*%.?%d*)" -- may be overly permissive as this will be tonumber'ed anyways
	local num_delim = "[,%s]%s*"
	local pattern = "^" .. table.concat({rel_num_cap, rel_num_cap, rel_num_cap}, num_delim) .. "$"

	local function parse_area_string(pos, relative_to)
		local pp = {}
		pp.x, pp.y, pp.z = pos:trim():match(pattern)
		return core.parse_coordinates(pp.x, pp.y, pp.z, relative_to)
	end

	function core.string_to_area(value, relative_to)
		local p1, p2 = value:match("^%((.-)%)%s*%((.-)%)$")
		if not p1 then
			return
		end

		p1 = parse_area_string(p1, relative_to)
		p2 = parse_area_string(p2, relative_to)

		if p1 == nil or p2 == nil then
			return
		end

		return p1, p2
	end
end


local function table_copy(value, preserve_metatables)
	local seen = {}
	local function copy(val)
		if type(val) ~= "table" then
			return val
		end
		local t = val
		if seen[t] then
			return seen[t]
		end
		local res = {}
		seen[t] = res
		for k, v in pairs(t) do
			res[copy(k)] = copy(v)
		end
		if preserve_metatables then
			setmetatable(res, getmetatable(t))
		end
		return res
	end
	return copy(value)
end

function table.copy(value)
	return table_copy(value, false)
end

function table.copy_with_metatables(value)
	return table_copy(value, true)
end

function table.insert_all(t, other)
	if table.move then -- LuaJIT
		return table.move(other, 1, #other, #t + 1, t)
	end
	for i=1, #other do
		t[#t + 1] = other[i]
	end
	return t
end


function table.key_value_swap(t)
	local ti = {}
	for k,v in pairs(t) do
		ti[v] = k
	end
	return ti
end


function table.shuffle(t, from, to, random)
	from = from or 1
	to = to or #t
	random = random or math.random
	local n = to - from + 1
	while n > 1 do
		local r = from + n-1
		local l = from + random(0, n-1)
		t[l], t[r] = t[r], t[l]
		n = n-1
	end
end


--------------------------------------------------------------------------------
-- mainmenu only functions
--------------------------------------------------------------------------------
if core.gettext then -- for client and mainmenu
	function fgettext_ne(text, ...)
		text = core.gettext(text)
		local arg = {n=select('#', ...), ...}
		if arg.n >= 1 then
			-- Insert positional parameters ($1, $2, ...)
			local result = ''
			local pos = 1
			while pos <= text:len() do
				local newpos = text:find('[$]', pos)
				if newpos == nil then
					result = result .. text:sub(pos)
					pos = text:len() + 1
				else
					local paramindex =
						tonumber(text:sub(newpos+1, newpos+1))
					result = result .. text:sub(pos, newpos-1)
						.. tostring(arg[paramindex])
					pos = newpos + 2
				end
			end
			text = result
		end
		return text
	end

	function fgettext(text, ...)
		return core.formspec_escape(fgettext_ne(text, ...))
	end

	function hgettext(text, ...)
		return core.hypertext_escape(fgettext_ne(text, ...))
	end
end

local ESCAPE_CHAR = string.char(0x1b)

function core.get_color_escape_sequence(color)
	return ESCAPE_CHAR .. "(c@" .. color .. ")"
end

function core.get_background_escape_sequence(color)
	return ESCAPE_CHAR .. "(b@" .. color .. ")"
end

function core.colorize(color, message)
	local lines = tostring(message):split("\n", true)
	local color_code = core.get_color_escape_sequence(color)

	for i, line in ipairs(lines) do
		lines[i] = color_code .. line
	end

	return table.concat(lines, "\n") .. core.get_color_escape_sequence("#fff")
end


function core.strip_foreground_colors(str)
	return (str:gsub(ESCAPE_CHAR .. "%(c@[^)]+%)", ""))
end

function core.strip_background_colors(str)
	return (str:gsub(ESCAPE_CHAR .. "%(b@[^)]+%)", ""))
end

function core.strip_colors(str)
	return (str:gsub(ESCAPE_CHAR .. "%([bc]@[^)]+%)", ""))
end


local function translate(textdomain, str, num, ...)
	local start_seq
	if textdomain == "" and num == "" then
		start_seq = ESCAPE_CHAR .. "T"
	elseif num == "" then
		start_seq = ESCAPE_CHAR .. "(T@" .. textdomain .. ")"
	else
		start_seq = ESCAPE_CHAR .. "(T@" .. textdomain .. "@" .. num .. ")"
	end
	local arg = {n=select('#', ...), ...}
	local end_seq = ESCAPE_CHAR .. "E"
	local arg_index = 1
	local translated = str:gsub("@(.)", function(matched)
		local c = string.byte(matched)
		if string.byte("1") <= c and c <= string.byte("9") then
			local a = c - string.byte("0")
			if a ~= arg_index then
				error("Escape sequences in string given to core.translate " ..
					"are not in the correct order: got @" .. matched ..
					"but expected @" .. tostring(arg_index))
			end
			if a > arg.n then
				error("Not enough arguments provided to core.translate")
			end
			arg_index = arg_index + 1
			return ESCAPE_CHAR .. "F" .. arg[a] .. ESCAPE_CHAR .. "E"
		elseif matched == "n" then
			return "\n"
		else
			return matched
		end
	end)
	if arg_index < arg.n + 1 then
		error("Too many arguments provided to core.translate")
	end
	return start_seq .. translated .. end_seq
end

function core.translate(textdomain, str, ...)
	return translate(textdomain, str, "", ...)
end

function core.translate_n(textdomain, str, str_plural, n, ...)
	assert (type(n) == "number")
	assert (n >= 0)
	assert (math.floor(n) == n)

	-- Truncate n if too large
	local max = 1000000
	if n >= 2 * max then
		n = n % max + max
	end
	if n == 1 then
		return translate(textdomain, str, "1", ...)
	else
		return translate(textdomain, str_plural, tostring(n), ...)
	end
end

function core.get_translator(textdomain)
	return
		(function(str, ...) return core.translate(textdomain or "", str, ...) end),
		(function(str, str_plural, n, ...) return core.translate_n(textdomain or "", str, str_plural, n, ...) end)
end

--------------------------------------------------------------------------------
-- Returns the exact coordinate of a pointed surface
--------------------------------------------------------------------------------
function core.pointed_thing_to_face_pos(placer, pointed_thing)
	-- Avoid crash in some situations when player is inside a node, causing
	-- 'above' to equal 'under'.
	if vector.equals(pointed_thing.above, pointed_thing.under) then
		return pointed_thing.under
	end

	local eye_height = placer:get_properties().eye_height
	local eye_offset_first = placer:get_eye_offset()
	local node_pos = pointed_thing.under
	local camera_pos = placer:get_pos()
	local pos_off = vector.multiply(
			vector.subtract(pointed_thing.above, node_pos), 0.5)
	local look_dir = placer:get_look_dir()
	local offset, nc
	local oc = {}

	for c, v in pairs(pos_off) do
		if nc or v == 0 then
			oc[#oc + 1] = c
		else
			offset = v
			nc = c
		end
	end

	local fine_pos = {[nc] = node_pos[nc] + offset}
	camera_pos.y = camera_pos.y + eye_height + eye_offset_first.y / 10
	local f = (node_pos[nc] + offset - camera_pos[nc]) / look_dir[nc]

	for i = 1, #oc do
		fine_pos[oc[i]] = camera_pos[oc[i]] + look_dir[oc[i]] * f
	end
	return fine_pos
end

function core.string_to_privs(str, delim)
	assert(type(str) == "string")
	delim = delim or ','
	local privs = {}
	for _, priv in pairs(string.split(str, delim)) do
		privs[priv:trim()] = true
	end
	return privs
end

function core.privs_to_string(privs, delim)
	assert(type(privs) == "table")
	delim = delim or ','
	local list = {}
	for priv, bool in pairs(privs) do
		if bool then
			list[#list + 1] = priv
		end
	end
	table.sort(list)
	return table.concat(list, delim)
end

function core.is_nan(number)
	return number ~= number
end

--[[ Helper function for parsing an optionally relative number
of a chat command parameter, using the chat command tilde notation.

Parameters:
* arg: String snippet containing the number; possible values:
    * "<number>": return as number
    * "~<number>": return relative_to + <number>
    * "~": return relative_to
    * Anything else will return `nil`
* relative_to: Number to which the `arg` number might be relative to

Returns:
A number or `nil`, depending on `arg.

Examples:
* `core.parse_relative_number("5", 10)` returns 5
* `core.parse_relative_number("~5", 10)` returns 15
* `core.parse_relative_number("~", 10)` returns 10
]]
function core.parse_relative_number(arg, relative_to)
	if not arg then
		return nil
	elseif arg == "~" then
		return relative_to
	elseif string.sub(arg, 1, 1) == "~" then
		local number = tonumber(string.sub(arg, 2))
		if not number then
			return nil
		end
		if core.is_nan(number) or number == math.huge or number == -math.huge then
			return nil
		end
		return relative_to + number
	else
		local number = tonumber(arg)
		if core.is_nan(number) or number == math.huge or number == -math.huge then
			return nil
		end
		return number
	end
end

--[[ Helper function to parse coordinates that might be relative
to another position; supports chat command tilde notation.
Intended to be used in chat command parameter parsing.

Parameters:
* x, y, z: Parsed x, y, and z coordinates as strings
* relative_to: Optional position vector as reference point

Syntax of x, y and z:
* "<number>": use as number
* "~<number>": use <number> + reference point on this axis
* "~": use reference point on this axis

Returns: a vector or nil for invalid input
]]
function core.parse_coordinates(x, y, z, relative_to)
	if not relative_to then
		x, y, z = tonumber(x), tonumber(y), tonumber(z)
		return x and y and z and vector.new(x, y, z)
	end
	local rx = core.parse_relative_number(x, relative_to.x)
	local ry = core.parse_relative_number(y, relative_to.y)
	local rz = core.parse_relative_number(z, relative_to.z)
	return rx and ry and rz and vector.new(rx, ry, rz)
end
