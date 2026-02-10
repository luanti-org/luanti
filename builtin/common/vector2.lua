--[[
2D Vector helpers
Note: The vector2.*-functions must be able to accept old vectors that had no metatables
]]

-- localize functions
local setmetatable = setmetatable
local math = math

vector2 = {}

local metatable = {}
vector2.metatable = metatable

local xy = {"x", "y"}

-- only called when rawget(v, key) returns nil
function metatable.__index(v, key)
	return rawget(v, xy[key]) or vector2[key]
end

-- only called when rawget(v, key) returns nil
function metatable.__newindex(v, key, value)
	rawset(v, xy[key] or key, value)
end

-- constructors

local function fast_new(x, y)
	return setmetatable({x = x, y = y}, metatable)
end

function vector2.new(a, b)
	assert(a and b, "Invalid arguments for vector2.new()")
	return fast_new(a, b)
end

function vector2.zero()
	return fast_new(0, 0)
end

function vector2.copy(v)
	assert(v.x and v.y, "Invalid vector passed to vector2.copy()")
	return fast_new(v.x, v.y)
end

function vector2.from_string(s, init)
	local x, y, np = string.match(s, "^%s*%(%s*([^%s,]+)%s*[,%s]%s*([^%s,]+)%s*[,%s]?%s*%)()", init)
	x = tonumber(x)
	y = tonumber(y)
	if not (x and y) then
		return nil
	end
	return fast_new(x, y), np
end

function vector2.to_string(v)
	return string.format("(%g, %g)", v.x, v.y)
end
metatable.__tostring = vector2.to_string

function vector2.equals(a, b)
	return a.x == b.x and a.y == b.y
end
metatable.__eq = vector2.equals

-- unary operations

function vector2.length(v)
	return math.sqrt(v.x * v.x + v.y * v.y)
end

function vector2.normalize(v)
	local len = vector2.length(v)
	if len == 0 then
		return fast_new(0, 0)
	else
		return vector2.divide(v, len)
	end
end

function vector2.floor(v)
	return vector2.apply(v, math.floor)
end

function vector2.round(v)
	return vector2.apply(v, math.round)
end

function vector2.ceil(v)
	return vector2.apply(v, math.ceil)
end

function vector2.sign(v, tolerance)
	return vector2.apply(v, math.sign, tolerance)
end

function vector2.abs(v)
	return vector2.apply(v, math.abs)
end

function vector2.apply(v, func, ...)
	return fast_new(
		func(v.x, ...),
		func(v.y, ...)
	)
end

function vector2.combine(a, b, func)
	return fast_new(
		func(a.x, b.x),
		func(a.y, b.y)
	)
end

function vector2.distance(a, b)
	local x = a.x - b.x
	local y = a.y - b.y
	return math.sqrt(x * x + y * y)
end

function vector2.direction(pos1, pos2)
	return vector2.subtract(pos2, pos1):normalize()
end

function vector2.angle(a, b)
	local dotp = vector2.dot(a, b)
	local crossplen = math.abs(a.x * b.y - a.y * b.x)
	return math.atan2(crossplen, dotp)
end

function vector2.dot(a, b)
	return a.x * b.x + a.y * b.y
end

function metatable.__unm(v)
	return fast_new(-v.x, -v.y)
end

-- add, sub, mul, div operations

function vector2.add(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x + b.x,
			a.y + b.y
		)
	else
		return fast_new(
			a.x + b,
			a.y + b
		)
	end
end
function metatable.__add(a, b)
	return fast_new(
		a.x + b.x,
		a.y + b.y
	)
end

function vector2.subtract(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x - b.x,
			a.y - b.y
		)
	else
		return fast_new(
			a.x - b,
			a.y - b
		)
	end
end
function metatable.__sub(a, b)
	return fast_new(
		a.x - b.x,
		a.y - b.y
	)
end

function vector2.multiply(a, b)
	return fast_new(
		a.x * b,
		a.y * b
	)
end
function metatable.__mul(a, b)
	if type(a) == "table" then
		return fast_new(
			a.x * b,
			a.y * b
		)
	else
		return fast_new(
			a * b.x,
			a * b.y
		)
	end
end

function vector2.divide(a, b)
	return fast_new(
		a.x / b,
		a.y / b
	)
end
function metatable.__div(a, b)
	-- scalar/vector makes no sense
	return fast_new(
		a.x / b,
		a.y / b
	)
end

-- misc stuff

function vector2.offset(v, x, y)
	return fast_new(
		v.x + x,
		v.y + y
	)
end

function vector2.sort(a, b)
	return fast_new(math.min(a.x, b.x), math.min(a.y, b.y)),
		fast_new(math.max(a.x, b.x), math.max(a.y, b.y))
end

function vector2.check(v)
	return getmetatable(v) == metatable
end

function vector2.rotate(v, angle)
	local cosangle = math.cos(angle)
	local sinangle = math.sin(angle)
	return fast_new(
		v.x * cosangle - v.y * sinangle,
		v.x * sinangle + v.y * cosangle
	)
end

function vector2.in_area(pos, min, max)
	return (pos.x >= min.x) and (pos.x <= max.x) and
		(pos.y >= min.y) and (pos.y <= max.y)
end

function vector2.random_direction()
	-- Generate a random direction of unit length
	local angle = math.random() * 2 * math.pi
	return fast_new(math.cos(angle), math.sin(angle))
end

function vector2.random_in_area(min, max)
	return fast_new(
		math.random(min.x, max.x),
		math.random(min.y, max.y)
	)
end
