--- Lua module to serialize values as Lua code.
-- From: https://github.com/appgurueu/modlib/blob/master/luon.lua
-- License: MIT

local next, rawget, pairs, pcall, error, type, setfenv, loadstring
	= next, rawget, pairs, pcall, error, type, setfenv, loadstring

local table_concat, string_dump, string_format, string_match, math_huge
	= table.concat, string.dump, string.format, string.match, math.huge

local function pack_args(...)
	return {n = select("#", ...), ...}
end

-- Recursively
-- (1) reads metatables from tables;
-- (2) counts occurrences of objects (non-primitives including strings) in a table.
local function prepare_objects(value)
	local counts = {}
	local type_lookup = {}
	if value == nil then
		-- Early return for nil; tables can't contain nil
		return counts, type_lookup
	end
	local function count_values(val, recount)
		local type_ = type(val)
		if type_ == "boolean" or type_ == "number" then
			return
		end
		local count = counts[val]
		if not recount then
			counts[val] = (count or 0) + 1
		end
		local mt = (not count) and (type_ == "table" or type_ == "userdata") and getmetatable(val)
		if mt and core.serializable_metatables[mt] then
			local args = pack_args(core.known_metatables[mt], core.serializable_metatables[mt](val))
			type_lookup[val] = args
			for _, v in ipairs(args) do
				count_values(v, rawequal(v, val))
			end
		elseif type_ == "table" then
			if recount or not count then
				for k, v in pairs(val) do
					count_values(k)
					count_values(v)
				end
			end
		elseif type_ ~= "string" and type_ ~= "function" then
			error("unsupported type: " .. type_)
		end
	end
	count_values(value)
	return counts, type_lookup
end

-- Build a "set" of Lua keywords. These can't be used as short key names.
-- See https://www.lua.org/manual/5.1/manual.html#2.1
local keywords = {}
for _, keyword in pairs({
	"and", "break", "do", "else", "elseif",
	"end", "false", "for", "function", "if",
	"in", "local", "nil", "not", "or",
	"repeat", "return", "then", "true", "until", "while",
	"goto" -- LuaJIT, Lua 5.2+
}) do
	keywords[keyword] = true
end

local function quote(string)
	return string_format("%q", string)
end

local function dump_func(func)
	return string_format("loadstring(%q)", string_dump(func))
end

-- Serializes Lua nil, booleans, numbers, strings, tables and even functions
-- Tables are referenced by reference, strings are referenced by value. Supports circular tables.
local function serialize(value, write)
	local reference, refnum = "1", 1
	-- [object] = reference
	local references = {}
	-- Circular tables that must be filled using `table[key] = value` statements
	local to_fill = {}
	local counts, typeinfo = prepare_objects(value)
	if next(typeinfo) then
		write [[
		if not (core and core.serializable_metatables) then
			core = { known_metatables = {}, serializable_metatables = {}}
		end;
		]]
	end
	for object, count in pairs(counts) do
		local type_ = type(object)
		-- Object must appear more than once. If it is a string, the reference has to be shorter than the string.
		if count >= 2 and (type_ ~= "string" or #reference + 5 < #object) then
			if refnum == 1 then
				write"local _={};" -- initialize reference table
			end
			write"_["
			write(reference)
			write("]=")
			if type_ == "table" then
				write("{}")
			elseif type_ == "function" then
				write(dump_func(object))
			elseif type_ == "string" then
				write(quote(object))
			end
			write(";")
			references[object] = reference
			if type_ == "table" then
				to_fill[object] = reference
			end
			refnum = refnum + 1
			reference = ("%d"):format(refnum)
		end
	end
	-- Used to decide whether we should do "key=..."
	local function use_short_key(key)
		return not references[key] and type(key) == "string" and (not keywords[key]) and string_match(key, "^[%a_][%a%d_]*$")
	end
	local dump
	local function dump_serialized(value)
		local serialized = assert(typeinfo[value])
		write "(core.serializable_metatables["
		dump(serialized[1])
		write "])("
		for k = 2, serialized.n do
			if k ~= 2 then
				write ","
			end
			local v = serialized[k]
			dump(v, rawequal(v, value))
		end
		write ")"
	end
	dump = function(value, skip_mt)
		-- Primitive types
		if value == nil then
			return write("nil")
		end
		if value == true then
			return write("true")
		end
		if value == false then
			return write("false")
		end
		local type_ = type(value)
		if type_ == "number" then
			if value ~= value then -- nan
				return write"0/0"
			elseif value == math_huge then
				return write"1/0"
			elseif value == -math_huge then
				return write"-1/0"
			else
				return write(string_format("%.17g", value))
			end
		end
		-- Reference types: table, function and string
		local ref = references[value]
		if ref then
			write"_["
			write(ref)
			return write"]"
		end
		if (not skip_mt) and typeinfo[value] then
			dump_serialized(value)
			return
		end
		if type_ == "string" then
			return write(quote(value))
		end
		if type_ == "function" then
			return write(dump_func(value))
		end
		if type_ == "table" then
			write("{")
			-- First write list keys:
			-- Don't use the table length #value here as it may horribly fail
			-- for tables which use large integers as keys in the hash part;
			-- stop at the first "hole" (nil value) instead
			local len = 0
			local first = true -- whether this is the first entry, which may not have a leading comma
			while true do
				local v = rawget(value, len + 1) -- use rawget to avoid metatables like the vector metatable
				if v == nil then break end
				if first then first = false else write(",") end
				dump(v)
				len = len + 1
			end
			-- Now write map keys ([key] = value)
			for k, v in next, value do
				-- We have written all non-float keys in [1, len] already
				if type(k) ~= "number" or k % 1 ~= 0 or k < 1 or k > len then
					if first then first = false else write(",") end
					if use_short_key(k) then
						write(k)
					else
						write("[")
						dump(k)
						write("]")
					end
					write("=")
					dump(v)
				end
			end
			write("}")
			return
		end
	end
	-- Write the statements to fill circular tables
	for tbl, ref in pairs(to_fill) do
		for k, v in pairs(tbl) do
			write("_[")
			write(ref)
			write("]")
			if use_short_key(k) then
				write(".")
				write(k)
			else
				write("[")
				dump(k)
				write("]")
			end
			write("=")
			dump(v)
			write(";")
		end
		if typeinfo[tbl] then
			write("_[")
			write(ref)
			write("]=")
			dump_serialized(tbl)
			write(";")
		end
	end
	write("return ")
	dump(value)
end

function core.serialize(value)
	local rope = {}
	serialize(value, function(text)
		 -- Faster than table.insert(rope, text) on PUC Lua 5.1
		rope[#rope + 1] = text
	end)
	return table_concat(rope)
end

local function dummy_func() end

function core.deserialize(str, safe)
	-- Backwards compatibility
	if str == nil then
		core.log("deprecated", "core.deserialize called with nil (expected string).")
		return nil, "Invalid type: Expected a string, got nil"
	end
	local t = type(str)
	if t ~= "string" then
		error(("core.deserialize called with %s (expected string)."):format(t))
	end

	local func, err = loadstring(str)
	if not func then return nil, err end

	-- math.huge was serialized to inf and NaNs to nan by Lua in engine version 5.6, so we have to support this here
	local env = {
		inf = math_huge,
		nan = 0/0,
		core = {
			known_metatables = core.known_metatables,
			serializable_metatables = core.serializable_metatables,
		},
	}
	if safe then
		env.loadstring = dummy_func
	else
		env.loadstring = function(str, ...)
			local func, err = loadstring(str, ...)
			if func then
				setfenv(func, env)
				return func
			end
			return nil, err
		end
	end
	setfenv(func, env)
	local success, value_or_err = pcall(func)
	if success then
		return value_or_err
	end
	return nil, value_or_err
end

