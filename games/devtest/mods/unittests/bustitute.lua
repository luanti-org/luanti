-- A simple substitute for a busted-like unit test interface

local bustitute = {}

local test_env = setmetatable({}, {__index = _G})

test_env.assert = setmetatable({}, {__call = function(_, ...)
	return assert(...)
end})

function test_env.assert.equals(expected, got)
	if expected ~= got then
		error("expected " .. dump(expected) .. ", got " .. dump(got))
	end
end

local function same(a, b)
	if a == b then
		return true
	end
	if type(a) ~= "table" or type(b) ~= "table" then
		return false
	end
	for k, v in pairs(a) do
		if not same(b[k], v) then
			return false
		end
	end
	for k, v in pairs(b) do
		if a[k] == nil then -- if a[k] is present, we already compared them above
			return false
		end
	end
	return true
end

function test_env.assert.same(expected, got)
	if not same(expected, got) then
		error("expected " .. dump(expected) .. ", got " .. dump(got))
	end
end

local full_test_name = {}

function test_env.describe(name, func)
	table.insert(full_test_name, name)
	func()
	table.remove(full_test_name)
end

function test_env.it(name, func)
	table.insert(full_test_name, name)
	unittests.register(table.concat(full_test_name, " "), func, {})
	table.remove(full_test_name)
end

function bustitute.register(name)
	local modpath = core.get_modpath(core.get_current_modname())
	local chunk = assert(loadfile(modpath .. "/" .. name .. ".lua"))
	setfenv(chunk, test_env)
	test_env.describe(name, chunk)
end

return bustitute
