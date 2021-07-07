--- Lua module to serialize values as Lua code.
-- From: https://github.com/appgurueu/modlib/blob/master/luon.lua
-- License: MIT

local assert, next, pairs, pcall, error, type, setfenv, loadstring
    = assert, next, pairs, pcall, error, type, setfenv, loadstring

local table_insert, table_concat, string_dump, string_format, string_match, math_huge
    = table.insert, table.concat, string.dump, string.format, string.match, math.huge

-- Recursively counts occurences of objects (non-primitives including strings) in a table.
local function count_objects(value)
    local counts = {}
	if value == nil then
		-- Early return for nil
		return counts
	end
    local function count_values(val)
		local type_ = type(val)
		if type_ == "boolean" or type_ == "number" then return end
        local count = counts[val]
        counts[val] = (count or 0) + 1
        if not count and type_ == "table" then
            for k, v in pairs(val) do
                count_values(k)
                count_values(v)
            end
        end
    end
    count_values(value)
    return counts
end

-- Build a table with the succeeding character from A-Z
local succ = {}
for char = ("A"):byte(), ("Z"):byte() - 1 do
    succ[string.char(char)] = string.char(char + 1)
end

local function quote(string)
    return string_format("%q", string)
end

local function serialize(value, write)
    local reference = {"A"}
    local function increment_reference(place)
        if not reference[place] then
            reference[place] = "B"
        elseif reference[place] == "Z" then
            reference[place] = "A"
            return increment_reference(place + 1)
        else
            reference[place] = succ[reference[place]]
        end
    end
    local references = {}
    local to_fill = {}
    for object, count in pairs(count_objects(value)) do
        local type_ = type(object)
        if count >= 2 and (type_ ~= "string" or #reference + 2 >= #object) then
            local ref = table_concat(reference)
            write(ref)
            write"="
            write(type_ == "table" and "{}" or quote(object))
            write";"
            references[object] = ref
            if type_ == "table" then
                to_fill[object] = ref
            end
            increment_reference(1)
        end
    end
    local function is_short_key(key)
        return not references[key] and type(key) == "string" and string_match(key, "^[%a_][%a%d_]*$")
    end
    local function dump(value)
        -- Primitive types
        if value == nil then
            return write"nil"
        end
        if value == true then
            return write"true"
        end
        if value == false then
            return write"false"
        end
        local type_ = type(value)
        if type_ == "number" then
            return write(string_format("%.17g", value))
        end
		if type_ == "function" then
			return write(string_format("loadstring(%q)", string_dump(value)))
		end
        -- Reference types: table and string
        local ref = references[value]
        if ref then
            return write(ref)
        elseif type_ == "string" then
            return write(quote(value))
        elseif type_ == "table" then
            local first = true
            write"{"
            local len = #value
            for i = 1, len do
                if not first then write";" end
                dump(value[i])
                first = false
            end
            for k, v in next, value do
                if type(k) ~= "number" or k % 1 ~= 0 or k < 1 or k > len then
                    if not first then write";" end
                    if is_short_key(k) then
                        write(k)
                    else
                        write"["
                        dump(k)
                        write"]"
                    end
                    write"="
                    dump(v)
                    first = false
                end
            end
            write"}"
        else
            error("unsupported type: " .. type_)
        end
    end
    for table, ref in pairs(to_fill) do
        for k, v in pairs(table) do
            write(ref)
            if is_short_key(k) then
                write"."
                write(k)
            else
                write"["
                dump(k)
                write"]"
            end
            write"="
            dump(v)
            write";"
        end
    end
    write"return "
    dump(value)
end

function core.serialize(value)
    local rope = {}
    serialize(value, function(text)
        table_insert(rope, text)
    end)
    return table_concat(rope)
end

local function dummy_func() end

local function deserialize(func, safe)
	-- math.huge is serialized to inf, 0/0 is serialized to -nan
	local env = {inf = math_huge, nan = 0/0}
	if safe then
		env.loadstring = dummy_func
	else
		env.loadstring = function(str, ...)
			if str:byte(1) == 0x1B then
				return nil, "Bytecode prohibited"
			end
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

function core.deserialize(str, safe)
	if str:byte(1) == 0x1B then
		return nil, "Bytecode prohibited"
	end
    return deserialize(assert(loadstring(str)), safe)
end
