_G.vector = {}
_G.vector2 = {}
dofile("builtin/common/math.lua")
dofile("builtin/common/vector.lua")
dofile("builtin/common/vector2.lua")

-- Custom assertion for comparing floating-point numbers with tolerance
local function close(state, arguments)
	if #arguments < 2 then
		return false
	end
	
	local expected = arguments[1]
	local actual = arguments[2]
	local tolerance = arguments[3] or 0.000001
	
	if type(expected) == "number" and type(actual) == "number" then
		return math.abs(expected - actual) < tolerance
	end
	
	return false
end

assert:register("assertion", "close", close)

describe("vector2", function()
	describe("new()", function()
		it("constructs", function()
			assert.same({x = 1, y = 2}, vector2.new(1, 2))

			assert.is_true(vector2.check(vector2.new(1, 2)))
		end)

		it("throws on invalid input", function()
			assert.has.errors(function()
				vector2.new()
			end)

			assert.has.errors(function()
				vector2.new({ x = 3, y = 2 })
			end)

			assert.has.errors(function()
				vector2.new({ x = 3 })
			end)

			assert.has.errors(function()
				vector2.new({ d = 3 })
			end)
		end)
	end)

	it("zero()", function()
		assert.same({x = 0, y = 0}, vector2.zero())
		assert.is_true(vector2.check(vector2.zero()))
	end)

	it("copy()", function()
		local v = vector2.new(1, 2)
		assert.same(v, vector2.copy(v))
		assert.is_true(vector2.check(vector2.copy(v)))
	end)

	it("indexes", function()
		local some_vector = vector2.new(24, 42)
		assert.equal(24, some_vector[1])
		assert.equal(24, some_vector.x)
		assert.equal(42, some_vector[2])
		assert.equal(42, some_vector.y)

		some_vector[1] = 100
		assert.equal(100, some_vector.x)
		some_vector.x = 101
		assert.equal(101, some_vector[1])

		some_vector[2] = 100
		assert.equal(100, some_vector.y)
		some_vector.y = 102
		assert.equal(102, some_vector[2])
	end)

	it("direction()", function()
		local a = vector2.new(1, 0)
		local b = vector2.new(1, 42)
		local dir1 = vector2.direction(a, b)
		assert.close(0, dir1.x)
		assert.close(1, dir1.y)
		local dir2 = a:direction(b)
		assert.close(0, dir2.x)
		assert.close(1, dir2.y)
	end)

	it("distance()", function()
		local a = vector2.new(1, 0)
		local b = vector2.new(4, 4)
		assert.close(5, vector2.distance(a, b))
		assert.close(5, a:distance(b))
		assert.close(0, vector2.distance(a, a))
		assert.close(0, b:distance(b))
	end)

	it("length()", function()
		local a = vector2.new(3, 4)
		assert.close(0, vector2.length(vector2.zero()))
		assert.close(5, vector2.length(a))
		assert.close(5, a:length())
	end)

	it("normalize()", function()
		local a = vector2.new(0, -5)
		local norm1 = vector2.normalize(a)
		assert.close(0, norm1.x)
		assert.close(-1, norm1.y)
		local norm2 = a:normalize()
		assert.close(0, norm2.x)
		assert.close(-1, norm2.y)
		local norm3 = vector2.normalize(vector2.zero())
		assert.close(0, norm3.x)
		assert.close(0, norm3.y)
	end)

	it("floor()", function()
		local a = vector2.new(0.1, 0.9)
		assert.same(vector2.new(0, 0), vector2.floor(a))
		assert.same(vector2.new(0, 0), a:floor())
	end)

	it("round()", function()
		local a = vector2.new(0.1, 0.9)
		assert.same(vector2.new(0, 1), vector2.round(a))
		assert.same(vector2.new(0, 1), a:round())
	end)

	it("ceil()", function()
		local a = vector2.new(0.1, 0.9)
		assert.same(vector2.new(1, 1), vector2.ceil(a))
		assert.same(vector2.new(1, 1), a:ceil())
	end)

	it("sign()", function()
		local a = vector2.new(-120.3, 231.5)
		assert.same(vector2.new(-1, 1), vector2.sign(a))
		assert.same(vector2.new(-1, 1), a:sign())
		assert.same(vector2.new(0, 1), vector2.sign(a, 200))
		assert.same(vector2.new(0, 1), a:sign(200))
	end)

	it("abs()", function()
		local a = vector2.new(-123.456, 13)
		assert.same(vector2.new(123.456, 13), vector2.abs(a))
		assert.same(vector2.new(123.456, 13), a:abs())
	end)

	it("apply()", function()
		local i = 0
		local f = function(x)
			i = i + 1
			return x + i
		end
		local f2 = function(x, opt1, opt2)
			return x + opt1 + opt2
		end
		local a = vector2.new(0.1, 0.9)
		assert.same(vector2.new(1, 1), vector2.apply(a, math.ceil))
		assert.same(vector2.new(1, 1), a:apply(math.ceil))
		assert.same(vector2.new(0.1, 0.9), vector2.apply(a, math.abs))
		assert.same(vector2.new(0.1, 0.9), a:apply(math.abs))
		assert.same(vector2.new(1.1, 2.9), vector2.apply(a, f))
		assert.same(vector2.new(3.1, 4.9), a:apply(f))
		local b = vector2.new(1, 2)
		assert.same(vector2.new(3, 4), vector2.apply(b, f2, 1, 1))
		assert.same(vector2.new(3, 4), b:apply(f2, 1, 1))
	end)

	it("combine()", function()
		local a = vector2.new(1, 2)
		local b = vector2.new(3, 2)
		assert.same(vector2.add(a, b), vector2.combine(a, b, function(x, y) return x + y end))
		assert.same(vector2.new(3, 2), vector2.combine(a, b, math.max))
		assert.same(vector2.new(1, 2), vector2.combine(a, b, math.min))
	end)

	it("equals()", function()
		assert.is_true(vector2.equals({x = 0, y = 0}, {x = 0, y = 0}))
		assert.is_true(vector2.equals({x = -1, y = 0}, vector2.new(-1, 0)))
		assert.is_false(vector2.equals({x = 1, y = 2}, {x = 1, y = 3}))
		local a = vector2.new(1, 2)
		assert.is_true(a:equals(a))
		assert.is_true(vector2.new(1, 2) == vector2.new(1, 2))
		assert.is_false(vector2.new(1, 2) == vector2.new(1, 3))
	end)

	it("metatable is same", function()
		local a = vector2.zero()
		local b = vector2.new(1, 2)

		assert.equal(true, vector2.check(a))
		assert.equal(true, vector2.check(b))

		assert.equal(vector2.metatable, getmetatable(a))
		assert.equal(vector2.metatable, getmetatable(b))
		assert.equal(vector2.metatable, a.metatable)
	end)

	it("sort()", function()
		local a = vector2.new(1, 2)
		local b = vector2.new(0.5, 232)
		local sorted = {vector2.new(0.5, 2), vector2.new(1, 232)}
		assert.same(sorted, {vector2.sort(a, b)})
		assert.same(sorted, {a:sort(b)})
	end)

	it("angle()", function()
		assert.close(math.pi, vector2.angle(vector2.new(-1, -2), vector2.new(1, 2)))
		assert.close(math.pi/2, vector2.new(0, 1):angle(vector2.new(1, 0)))
	end)

	it("dot()", function()
		assert.equal(-5, vector2.dot(vector2.new(-1, -2), vector2.new(1, 2)))
		assert.equal(0, vector2.zero():dot(vector2.new(1, 2)))
	end)

	it("offset()", function()
		assert.same({x = 41, y = 52}, vector2.offset(vector2.new(1, 2), 40, 50))
		assert.same(vector2.new(41, 52), vector2.offset(vector2.new(1, 2), 40, 50))
		assert.same(vector2.new(41, 52), vector2.new(1, 2):offset(40, 50))
	end)

	it("check()", function()
		assert.is_false(vector2.check(nil))
		assert.is_false(vector2.check(1))
		assert.is_false(vector2.check({x = 1, y = 2}))
		local real = vector2.new(1, 2)
		assert.is_true(vector2.check(real))
		assert.is_true(real:check())
	end)

	it("abusing works", function()
		local v = vector2.new(1, 2)
		v.a = 1
		assert.equal(1, v.a)

		local a_is_there = false
		for key, value in pairs(v) do
			if key == "a" then
				a_is_there = true
				assert.equal(value, 1)
				break
			end
		end
		assert.is_true(a_is_there)
	end)

	it("add()", function()
		local a = vector2.new(1, 2)
		local b = vector2.new(1, 4)
		local c = vector2.new(2, 6)
		assert.same(c, vector2.add(a, {x = 1, y = 4}))
		assert.same(c, vector2.add(a, b))
		assert.same(c, a:add(b))
		assert.same(c, a + b)
		assert.same(c, b + a)
	end)

	it("subtract()", function()
		local a = vector2.new(1, 2)
		local b = vector2.new(2, 4)
		local c = vector2.new(-1, -2)
		assert.same(c, vector2.subtract(a, {x = 2, y = 4}))
		assert.same(c, vector2.subtract(a, b))
		assert.same(c, a:subtract(b))
		assert.same(c, a - b)
		assert.same(c, -b + a)
	end)

	it("multiply()", function()
		local a = vector2.new(1, 2)
		local s = 2
		local d = vector2.new(2, 4)
		assert.same(d, vector2.multiply(a, s))
		assert.same(d, a:multiply(s))
		assert.same(d, a * s)
		assert.same(d, s * a)
		assert.same(-a, -1 * a)
	end)

	it("divide()", function()
		local a = vector2.new(1, 2)
		local s = 2
		local d = vector2.new(0.5, 1)
		assert.same(d, vector2.divide(a, s))
		assert.same(d, a:divide(s))
		assert.same(d, a / s)
		assert.same(d, 1/s * a)
		assert.same(-a, a / -1)
	end)

	it("to_string()", function()
		local v = vector2.new(1, 2)
		assert.same("(1, 2)", vector2.to_string(v))
		assert.same("(1, 2)", v:to_string())
		assert.same("(1, 2)", tostring(v))
	end)

	it("from_string()", function()
		local v = vector2.new(1, 2)
		assert.is_true(vector2.check(vector2.from_string("(1, 2)")))
		assert.same({v, 7}, {vector2.from_string("(1, 2)")})
		assert.same({v, 7}, {vector2.from_string("(1,2 )")})
		assert.same({v, 7}, {vector2.from_string("(1,2,)")})
		assert.same({v, 6}, {vector2.from_string("(1 2)")})
		assert.same({v, 9}, {vector2.from_string("( 1, 2 )")})
		assert.same({v, 9}, {vector2.from_string(" ( 1, 2) ")})
		assert.same({vector2.zero(), 6}, {vector2.from_string("(0,0) ( 1, 2) ")})
		assert.same({v, 14}, {vector2.from_string("(0,0) ( 1, 2) ", 6)})
		assert.same({v, 14}, {vector2.from_string("(0,0) ( 1, 2) ", 7)})
		assert.is_nil(vector2.from_string("nothing"))
	end)

	-- This function is needed because of floating point imprecision.
	local function almost_equal(a, b)
		if type(a) == "number" then
			return math.abs(a - b) < 0.00000000001
		end
		return vector2.distance(a, b) < 0.000000000001
	end

	describe("rotate()", function()
		it("rotates", function()
			assert.is_true(almost_equal({x = -1, y = 0},
				vector2.rotate({x = 1, y = 0}, math.pi)))
			assert.is_true(almost_equal({x = 0, y = 1},
				vector2.rotate({x = 1, y = 0}, math.pi / 2)))
		end)
		it("rotates back", function()
			local v = {x = 1, y = 3}
			local angle = math.pi / 13
			local rotated = vector2.rotate(v, angle)
			rotated = vector2.rotate(rotated, -angle)
			assert.is_true(almost_equal(v, rotated))
		end)
	end)

	describe("from_polar()", function()
		it("creates vector from polar coordinates", function()
			-- East (positive x-axis): angle = 0
			local v1 = vector2.from_polar(1, 0)
			assert.is_true(almost_equal(vector2.new(1, 0), v1))

			-- North (positive y-axis): angle = pi/2
			local v2 = vector2.from_polar(1, math.pi / 2)
			assert.is_true(almost_equal(vector2.new(0, 1), v2))

			-- West (negative x-axis): angle = pi
			local v3 = vector2.from_polar(1, math.pi)
			assert.is_true(almost_equal(vector2.new(-1, 0), v3))

			-- South (negative y-axis): angle = -pi/2
			local v4 = vector2.from_polar(1, -math.pi / 2)
			assert.is_true(almost_equal(vector2.new(0, -1), v4))

			-- Test with different radius
			local v5 = vector2.from_polar(5, math.pi / 4)
			assert.is_true(almost_equal(vector2.new(5 * math.cos(math.pi / 4), 5 * math.sin(math.pi / 4)), v5))
		end)

		it("is inverse of to_polar", function()
			local v = vector2.new(3, 4)
			local r, theta = vector2.to_polar(v)
			local v2 = vector2.from_polar(r, theta)
			assert.is_true(almost_equal(v, v2))
		end)

		it("throws on invalid input", function()
			assert.has.errors(function()
				vector2.from_polar()
			end)
			assert.has.errors(function()
				vector2.from_polar(1)
			end)
		end)
	end)

	describe("to_polar()", function()
		it("converts vector to polar coordinates", function()
			-- East (positive x-axis): angle = 0
			local r1, theta1 = vector2.to_polar(vector2.new(5, 0))
			assert.equal(5, r1)
			assert.is_true(almost_equal(0, theta1))

			-- North (positive y-axis): angle = pi/2
			local r2, theta2 = vector2.to_polar(vector2.new(0, 3))
			assert.equal(3, r2)
			assert.is_true(almost_equal(math.pi / 2, theta2))

			-- West (negative x-axis): angle = pi or -pi
			local r3, theta3 = vector2.to_polar(vector2.new(-2, 0))
			assert.equal(2, r3)
			assert.is_true(almost_equal(math.pi, math.abs(theta3)))

			-- Test with diagonal vector (3, 4, 5 triangle)
			local r4, theta4 = vector2.to_polar(vector2.new(3, 4))
			assert.equal(5, r4)
			assert.is_true(almost_equal(math.atan2(4, 3), theta4))
		end)

		it("handles zero vector", function()
			local r, theta = vector2.to_polar(vector2.zero())
			assert.equal(0, r)
			assert.equal(0, theta)
		end)

		it("is inverse of from_polar", function()
			local r, theta = 7, math.pi / 3
			local v = vector2.from_polar(r, theta)
			local r2, theta2 = vector2.to_polar(v)
			assert.is_true(almost_equal(r, r2))
			assert.is_true(almost_equal(theta, theta2))
		end)
	end)

	describe("signed_angle()", function()
		it("returns signed angle from first to second vector", function()
			-- From east to north: positive pi/2 (counterclockwise)
			local a1 = vector2.signed_angle(vector2.new(1, 0), vector2.new(0, 1))
			assert.is_true(almost_equal(math.pi / 2, a1))

			-- From north to east: negative pi/2 (clockwise)
			local a2 = vector2.signed_angle(vector2.new(0, 1), vector2.new(1, 0))
			assert.is_true(almost_equal(-math.pi / 2, a2))

			-- From east to west: pi
			local a3 = vector2.signed_angle(vector2.new(1, 0), vector2.new(-1, 0))
			assert.is_true(almost_equal(math.pi, math.abs(a3)))

			-- Same direction: 0
			local a4 = vector2.signed_angle(vector2.new(1, 1), vector2.new(2, 2))
			assert.is_true(almost_equal(0, a4))
		end)

		it("works with method call syntax", function()
			local a = vector2.signed_angle(vector2.new(1, 0), vector2.new(0, 1))
			local b = vector2.new(1, 0):signed_angle(vector2.new(0, 1))
			assert.is_true(almost_equal(a, b))
		end)

		it("is opposite of reversed angle", function()
			local v1 = vector2.new(1, 2)
			local v2 = vector2.new(3, -1)
			local a1 = vector2.signed_angle(v1, v2)
			local a2 = vector2.signed_angle(v2, v1)
			assert.is_true(almost_equal(a1, -a2))
		end)

		it("normalizes result to (-pi, pi]", function()
			-- Test case where angle difference crosses pi boundary
			-- Vector near -pi (slightly clockwise from negative x-axis)
			local v1 = vector2.new(-1, -0.1)
			-- Vector near +pi (slightly counterclockwise from negative x-axis)
			local v2 = vector2.new(-1, 0.1)
			local a = vector2.signed_angle(v1, v2)
			-- Should be normalized to a small angle, not near 2*pi
			assert.is_true(a > -math.pi and a <= math.pi)
			-- The angle should be small (around 0.2 radians for this case)
			assert.is_true(math.abs(a) < 1)  -- Much smaller than 2*pi or pi
		end)
	end)

	it("in_area()", function()
		assert.is_true(vector2.in_area(vector2.zero(), vector2.new(-10, -10), vector2.new(10, 10)))
		assert.is_true(vector2.in_area(vector2.new(-2, 5), vector2.new(-10, -10), vector2.new(10, 10)))
		assert.is_true(vector2.in_area(vector2.new(-10, -10), vector2.new(-10, -10), vector2.new(10, 10)))
		assert.is_false(vector2.in_area(vector2.new(-11, -10), vector2.new(-10, -10), vector2.new(10, 10)))
	end)

	it("random_in_area()", function()
		local min = vector2.new(-100, -100)
		local max = vector2.new(100, 100)
		for i = 1, 1000 do
			local random = vector2.random_in_area(min, max)
			assert.is_true(vector2.in_area(random, min, max))
		end
	end)
end)
