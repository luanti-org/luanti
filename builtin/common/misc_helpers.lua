-- Minetest: builtin/misc_helpers.lua

--------------------------------------------------------------------------------
-- Localize functions to avoid table lookups (better performance).
local string_sub, string_find = string.sub, string.find

--------------------------------------------------------------------------------
local function basic_dump(o)
	local tp = type(o)
	if tp == "number" then
		return tostring(o)
	elseif tp == "string" then
		return string.format("%q", o)
	elseif tp == "boolean" then
		return tostring(o)
	elseif tp == "nil" then
		return "nil"
	-- Uncomment for full function dumping support.
	-- Not currently enabled because bytecode isn't very human-readable and
	-- dump's output is intended for humans.
	--elseif tp == "function" then
	--	return string.format("loadstring(%q)", string.dump(o))
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

--------------------------------------------------------------------------------
-- This dumps values in a one-statement format.
-- For example, {test = {"Testing..."}} becomes:
-- [[{
-- 	test = {
-- 		"Testing..."
-- 	}
-- }]]
-- This supports tables as keys, but not circular references.
-- It performs poorly with multiple references as it writes out the full
-- table each time.
-- The indent field specifies a indentation string, it defaults to a tab.
-- Use the empty string to disable indentation.
-- The dumped and level arguments are internal-only.

function dump(o, indent, nested, level)
	local t = type(o)
	if not level and t == "userdata" then
		-- when userdata (e.g. player) is passed directly, print its metatable:
		return "userdata metatable: " .. dump(getmetatable(o))
	end
	if t ~= "table" then
		return basic_dump(o)
	end

	-- Contains table -> true/nil of currently nested tables
	nested = nested or {}
	if nested[o] then
		return "<circular reference>"
	end
	nested[o] = true
	indent = indent or "\t"
	level = level or 1

	local ret = {}
	local dumped_indexes = {}
	for i, v in ipairs(o) do
		ret[#ret + 1] = dump(v, indent, nested, level + 1)
		dumped_indexes[i] = true
	end
	for k, v in pairs(o) do
		if not dumped_indexes[k] then
			if type(k) ~= "string" or not is_valid_identifier(k) then
				k = "["..dump(k, indent, nested, level + 1).."]"
			end
			v = dump(v, indent, nested, level + 1)
			ret[#ret + 1] = k.." = "..v
		end
	end
	nested[o] = nil
	if indent ~= "" then
		local indent_str = "\n"..string.rep(indent, level)
		local end_indent_str = "\n"..string.rep(indent, level - 1)
		return string.format("{%s%s%s}",
				indent_str,
				table.concat(ret, ","..indent_str),
				end_indent_str)
	end
	return "{"..table.concat(ret, ", ").."}"
end

--------------------------------------------------------------------------------
function string.split(str, delim, include_empty, max_splits, sep_is_pattern)
	delim = delim or ","
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
function string:trim()
	return (self:gsub("^%s*(.-)%s*$", "%1"))
end

--------------------------------------------------------------------------------
function math.hypot(x, y)
	local t
	x = math.abs(x)
	y = math.abs(y)
	t = math.min(x, y)
	x = math.max(x, y)
	if x == 0 then return 0 end
	t = t / x
	return x * math.sqrt(1 + t * t)
end

--------------------------------------------------------------------------------
function math.sign(x, tolerance)
	tolerance = tolerance or 0
	if x > tolerance then
		return 1
	elseif x < -tolerance then
		return -1
	end
	return 0
end

--------------------------------------------------------------------------------
function math.factorial(x)
	assert(x % 1 == 0 and x >= 0, "factorial expects a non-negative integer")
	if x >= 171 then
		-- 171! is greater than the biggest double, no need to calculate
		return math.huge
	end
	local v = 1
	for k = 2, x do
		v = v * k
	end
	return v
end

function core.formspec_escape(text)
	if text ~= nil then
		text = string.gsub(text,"\\","\\\\")
		text = string.gsub(text,"%]","\\]")
		text = string.gsub(text,"%[","\\[")
		text = string.gsub(text,";","\\;")
		text = string.gsub(text,",","\\,")
	end
	return text
end


function core.wrap_text(text, max_length, as_table)
	local result = {}
	local line = {}
	if #text <= max_length then
		return as_table and {text} or text
	end

	for word in text:gmatch('%S+') do
		local cur_length = #table.concat(line, ' ')
		if cur_length > 0 and cur_length + #word + 1 >= max_length then
			-- word wouldn't fit on current line, move to next line
			table.insert(result, table.concat(line, ' '))
			line = {}
		end
		table.insert(line, word)
	end

	table.insert(result, table.concat(line, ' '))
	return as_table and result or table.concat(result, '\n')
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

	local p = {}
	p.x, p.y, p.z = string.match(value, "^([%d.-]+)[, ] *([%d.-]+)[, ] *([%d.-]+)$")
	if p.x and p.y and p.z then
		p.x = tonumber(p.x)
		p.y = tonumber(p.y)
		p.z = tonumber(p.z)
		return p
	end
	p = {}
	p.x, p.y, p.z = string.match(value, "^%( *([%d.-]+)[, ] *([%d.-]+)[, ] *([%d.-]+) *%)$")
	if p.x and p.y and p.z then
		p.x = tonumber(p.x)
		p.y = tonumber(p.y)
		p.z = tonumber(p.z)
		return p
	end
	return nil
end


--------------------------------------------------------------------------------
function core.string_to_area(value)
	local p1, p2 = unpack(value:split(") ("))
	if p1 == nil or p2 == nil then
		return nil
	end

	p1 = core.string_to_pos(p1 .. ")")
	p2 = core.string_to_pos("(" .. p2)
	if p1 == nil or p2 == nil then
		return nil
	end

	return p1, p2
end

local function test_string_to_area()
	local p1, p2 = core.string_to_area("(10.0, 5, -2) (  30.2,   4, -12.53)")
	assert(p1.x == 10.0 and p1.y == 5 and p1.z == -2)
	assert(p2.x == 30.2 and p2.y == 4 and p2.z == -12.53)

	p1, p2 = core.string_to_area("(10.0, 5, -2  30.2,   4, -12.53")
	assert(p1 == nil and p2 == nil)

	p1, p2 = core.string_to_area("(10.0, 5,) -2  fgdf2,   4, -12.53")
	assert(p1 == nil and p2 == nil)
end

test_string_to_area()

--------------------------------------------------------------------------------
function table.copy(t, seen)
	local n = {}
	seen = seen or {}
	seen[t] = n
	for k, v in pairs(t) do
		n[(type(k) == "table" and (seen[k] or table.copy(k, seen))) or k] =
			(type(v) == "table" and (seen[v] or table.copy(v, seen))) or v
	end
	return n
end


function table.insert_all(t, other)
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


function table.find(t, e, i, j)
    i = i or 1
    j = j or #t
    
    local mt = {__eq = function(e1, e2)
            if type(e2) ~= "table" then
                return false
            end
             
            local is_elem_exists = true
            for i1, v1 in pairs(e1) do
                if v1 ~= e2[i1] then
                    is_elem_exists = false
                    break
                end  
            end
                    
            return is_elem_exists
    end}
  
    if type(e) == "table" then
        setmetatable(table.copy(e), mt)
    end
    
    local iter = 0
    local findings = {}
    for k, v in pairs(t) do
        iter = iter + 1
        
        if type(v) == "table" then
            setmetatable(table.copy(v), mt)
        end
        if not(iter < i or iter > j) and e == v then
            table.insert(findings, k)
        end
    end
    
    return findings
end


function table.find_if(t, func, i, j)
    i = i or 1
    j = j or #t
    
    local iter = 0
    local satisf_keys = {}
    for k, v in pairs(t) do
        iter = iter + 1
        
        if not(iter < i or iter > j) and func(v) then
            table.insert(satisf_keys, v)
        end
    end
    
    return satisf_keys
end

--------------------------------------------------------------------------------
-- mainmenu only functions
--------------------------------------------------------------------------------
if INIT == "mainmenu" then
	function core.get_game(index)
		local games = core.get_games()

		if index > 0 and index <= #games then
			return games[index]
		end

		return nil
	end
end

if INIT == "client" or INIT == "mainmenu" then
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

	return table.concat(lines, "\n") .. core.get_color_escape_sequence("#ffffff")
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

function core.translate(textdomain, str, ...)
	local start_seq
	if textdomain == "" then
		start_seq = ESCAPE_CHAR .. "T"
	else
		start_seq = ESCAPE_CHAR .. "(T@" .. textdomain .. ")"
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

function core.get_translator(textdomain)
	return function(str, ...) return core.translate(textdomain or "", str, ...) end
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
	return table.concat(list, delim)
end
