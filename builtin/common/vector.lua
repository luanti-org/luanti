--[[
Vector helpers
Note: The vector.*-functions must be able to accept old vectors that had no metatables
]]

-- localize functions
local setmetatable = setmetatable
local math = math

vector = {}

local metatable = {}
vector.metatable = metatable

local xyz = {"x", "y", "z"}

-- only called when rawget(v, key) returns nil
function metatable.__index(v, key)
	return rawget(v, xyz[key]) or vector[key]
end

-- only called when rawget(v, key) returns nil
function metatable.__newindex(v, key, value)
	rawset(v, xyz[key] or key, value)
end

-- constructors

local function fast_new(x, y, z)
	return setmetatable({x = x, y = y, z = z}, metatable)
end

function vector.new(a, b, c)
	if a and b and c then
		return fast_new(a, b, c)
	end

	-- deprecated, use vector.copy and vector.zero directly
	if type(a) == "table" then
		return vector.copy(a)
	else
		assert(not a, "Invalid arguments for vector.new()")
		return vector.zero()
	end
end

function vector.zero()
	return fast_new(0, 0, 0)
end

function vector.copy(v)
	assert(v.x and v.y and v.z, "Invalid vector passed to vector.copy()")
	return fast_new(v.x, v.y, v.z)
end

function vector.from_string(s, init)
	local x, y, z, np = string.match(s, "^%s*%(%s*([^%s,]+)%s*[,%s]%s*([^%s,]+)%s*[,%s]" ..
			"%s*([^%s,]+)%s*[,%s]?%s*%)()", init)
	x = tonumber(x)
	y = tonumber(y)
	z = tonumber(z)
	if not (x and y and z) then
		return nil
	end
	return fast_new(x, y, z), np
end

function vector.to_string(v)
	return string.format("(%g, %g, %g)", v.x, v.y, v.z)
end
metatable.__tostring = vector.to_string

function vector.equals(a, b)
	return a.x == b.x and
	       a.y == b.y and
	       a.z == b.z
end
metatable.__eq = vector.equals

-- unary operations

function vector.length(v)
	return math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
end
-- Note: we cannot use __len because it is already used for primitive table length

function vector.normalize(v)
	local len = vector.length(v)
	if len == 0 then
		return fast_new(0, 0, 0)
	else
		return vector.divide(v, len)
	end
end

function vector.floor(v)
	return vector.apply(v, math.floor)
end

function vector.round(v)
	return vector.apply(v, math.round)
end

function vector.ceil(v)
	return vector.apply(v, math.ceil)
end

function vector.sign(v, tolerance)
	return vector.apply(v, math.sign, tolerance)
end

function vector.abs(v)
	return vector.apply(v, math.abs)
end

function vector.apply(v, func, ...)
	return fast_new(
		func(v.x, ...),
		func(v.y, ...),
		func(v.z, ...)
	)
end

function vector.combine(a, b, func)
	return fast_new(
		func(a.x, b.x),
		func(a.y, b.y),
		func(a.z, b.z)
	)
end

function vector.distance(a, b)
	local x = a.x - b.x
	local y = a.y - b.y
	local z = a.z - b.z
	return math.sqrt(x * x + y * y + z * z)
end

function vector.direction(pos1, pos2)
	return vector.subtract(pos2, pos1):normalize()
end

function vector.angle(a, b)
	local dotp = vector.dot(a, b)
	local cp = vector.cross(a, b)
	local crossplen = vector.length(cp)
	return math.atan2(crossplen, dotp)
end

function vector.dot(a, b)
	return a.x * b.x + a.y * b.y + a.z * b.z
end

function vector.cross(a, b)
	return fast_new(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x
	)
end

function metatable.__unm(v)
	return fast_new(-v.x, -v.y, -v.z)
end

-- add, sub, mul, div operations

function vector.add(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x + b.x,
			a.y + b.y,
			a.z + b.z
		)
	else
		return fast_new(
			a.x + b,
			a.y + b,
			a.z + b
		)
	end
end
function metatable.__add(a, b)
	return fast_new(
		a.x + b.x,
		a.y + b.y,
		a.z + b.z
	)
end

function vector.subtract(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x - b.x,
			a.y - b.y,
			a.z - b.z
		)
	else
		return fast_new(
			a.x - b,
			a.y - b,
			a.z - b
		)
	end
end
function metatable.__sub(a, b)
	return fast_new(
		a.x - b.x,
		a.y - b.y,
		a.z - b.z
	)
end

function vector.multiply(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x * b.x,
			a.y * b.y,
			a.z * b.z
		)
	else
		return fast_new(
			a.x * b,
			a.y * b,
			a.z * b
		)
	end
end
function metatable.__mul(a, b)
	if type(a) == "table" then
		return fast_new(
			a.x * b,
			a.y * b,
			a.z * b
		)
	else
		return fast_new(
			a * b.x,
			a * b.y,
			a * b.z
		)
	end
end

function vector.divide(a, b)
	if type(b) == "table" then
		return fast_new(
			a.x / b.x,
			a.y / b.y,
			a.z / b.z
		)
	else
		return fast_new(
			a.x / b,
			a.y / b,
			a.z / b
		)
	end
end
function metatable.__div(a, b)
	-- scalar/vector makes no sense
	return fast_new(
		a.x / b,
		a.y / b,
		a.z / b
	)
end

-- misc stuff

function vector.offset(v, x, y, z)
	return fast_new(
		v.x + x,
		v.y + y,
		v.z + z
	)
end

function vector.sort(a, b)
	return fast_new(math.min(a.x, b.x), math.min(a.y, b.y), math.min(a.z, b.z)),
		fast_new(math.max(a.x, b.x), math.max(a.y, b.y), math.max(a.z, b.z))
end

function vector.check(v)
	return getmetatable(v) == metatable
end

local function sin(x)
	if x % math.pi == 0 then
		return 0
	else
		return math.sin(x)
	end
end

local function cos(x)
	if x % math.pi == math.pi / 2 then
		return 0
	else
		return math.cos(x)
	end
end

function vector.rotate_around_axis(v, axis, angle)
	local cosangle = cos(angle)
	local sinangle = sin(angle)
	axis = vector.normalize(axis)
	-- https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
	local dot_axis = vector.multiply(axis, vector.dot(axis, v))
	local cross = vector.cross(v, axis)
	return vector.new(
		cross.x * sinangle + (v.x - dot_axis.x) * cosangle + dot_axis.x,
		cross.y * sinangle + (v.y - dot_axis.y) * cosangle + dot_axis.y,
		cross.z * sinangle + (v.z - dot_axis.z) * cosangle + dot_axis.z
	)
end

function vector.rotate(v, rot)
	local sinpitch = sin(-rot.x)
	local sinyaw = sin(-rot.y)
	local sinroll = sin(-rot.z)
	local cospitch = cos(rot.x)
	local cosyaw = cos(rot.y)
	local cosroll = math.cos(rot.z)
	-- Rotation matrix that applies yaw, pitch and roll
	local matrix = {
		{
			sinyaw * sinpitch * sinroll + cosyaw * cosroll,
			sinyaw * sinpitch * cosroll - cosyaw * sinroll,
			sinyaw * cospitch,
		},
		{
			cospitch * sinroll,
			cospitch * cosroll,
			-sinpitch,
		},
		{
			cosyaw * sinpitch * sinroll - sinyaw * cosroll,
			cosyaw * sinpitch * cosroll + sinyaw * sinroll,
			cosyaw * cospitch,
		},
	}
	-- Compute matrix multiplication: `matrix` * `v`
	return vector.new(
		matrix[1][1] * v.x + matrix[1][2] * v.y + matrix[1][3] * v.z,
		matrix[2][1] * v.x + matrix[2][2] * v.y + matrix[2][3] * v.z,
		matrix[3][1] * v.x + matrix[3][2] * v.y + matrix[3][3] * v.z
	)
end

function vector.dir_to_rotation(forward, up)
	forward = vector.normalize(forward)
	local rot = vector.new(math.asin(forward.y), -math.atan2(forward.x, forward.z), 0)
	if not up then
		return rot
	end
	assert(vector.dot(forward, up) < 0.000001,
			"Invalid vectors passed to vector.dir_to_rotation().")
	up = vector.normalize(up)
	-- Calculate vector pointing up with roll = 0, just based on forward vector.
	local forwup = vector.rotate(vector.new(0, 1, 0), rot)
	-- 'forwup' and 'up' are now in a plane with 'forward' as normal.
	-- The angle between them is the absolute of the roll value we're looking for.
	rot.z = vector.angle(forwup, up)

	-- Since vector.angle never returns a negative value or a value greater
	-- than math.pi, rot.z has to be inverted sometimes.
	-- To determine whether this is the case, we rotate the up vector back around
	-- the forward vector and check if it worked out.
	local back = vector.rotate_around_axis(up, forward, -rot.z)

	-- We don't use vector.equals for this because of floating point imprecision.
	if (back.x - forwup.x) * (back.x - forwup.x) +
			(back.y - forwup.y) * (back.y - forwup.y) +
			(back.z - forwup.z) * (back.z - forwup.z) > 0.0000001 then
		rot.z = -rot.z
	end
	return rot
end

function vector.in_area(pos, min, max)
	return (pos.x >= min.x) and (pos.x <= max.x) and
		(pos.y >= min.y) and (pos.y <= max.y) and
		(pos.z >= min.z) and (pos.z <= max.z)
end

function vector.random_direction()
	-- Generate a random direction of unit length, via rejection sampling
	local x, y, z, l2
	repeat -- expected less than two attempts on average (volume sphere vs. cube)
		x, y, z = math.random() * 2 - 1, math.random() * 2 - 1, math.random() * 2 - 1
		l2 = x*x + y*y + z*z
	until l2 <= 1 and l2 >= 1e-6
	-- normalize
	local l = math.sqrt(l2)
	return fast_new(x/l, y/l, z/l)
end

function vector.random_in_area(min, max)
	return fast_new(
		math.random(min.x, max.x),
		math.random(min.y, max.y),
		math.random(min.z, max.z)
	)
end

if rawget(_G, "core") and core.set_read_vector and core.set_push_vector then
	local function read_vector(v)
		return v.x, v.y, v.z
	end
	core.set_read_vector(read_vector)
	core.set_read_vector = nil

	if rawget(_G, "jit") then
		-- This is necessary to prevent trace aborts.
		local function push_vector(x, y, z)
			return (fast_new(x, y, z))
		end
		core.set_push_vector(push_vector)
	else
		core.set_push_vector(fast_new)
	end
	core.set_push_vector = nil
end
