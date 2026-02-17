_G.vector = {}
_G.vector2 = {}
dofile("builtin/common/math.lua")
dofile("builtin/common/vector.lua")
dofile("builtin/common/vector2.lua")

-- Custom assertion for comparing floating-point numbers with tolerance
local function number_close(state, arguments)
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

-- Custom assertion for comparing vectors with tolerance
-- Uses component-wise comparison to be self-contained
local function vector2_close(state, arguments)
	if #arguments < 2 then
		return false
	end

	local expected = arguments[1]
	local actual = arguments[2]
	local tolerance = arguments[3] or 0.000001

	if type(expected) == "table" and type(actual) == "table" then
		return math.abs(expected.x - actual.x) < tolerance and
		       math.abs(expected.y - actual.y) < tolerance
	end


	return false
end

assert:register("assertion", "number_close", number_close)
assert:register("assertion", "vector2_close", vector2_close)

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
		assert.number_close(0, dir1.x)
		assert.number_close(1, dir1.y)
		local dir2 = a:direction(b)
		assert.number_close(0, dir2.x)
		assert.number_close(1, dir2.y)
	end)

	it("distance()", function()
		local a = vector2.new(1, 0)
		local b = vector2.new(4, 4)
		assert.number_close(5, vector2.distance(a, b))
		assert.number_close(5, a:distance(b))
		assert.number_close(0, vector2.distance(a, a))
		assert.number_close(0, b:distance(b))
	end)

	it("length()", function()
		local a = vector2.new(3, 4)
		assert.number_close(0, vector2.length(vector2.zero()))
		assert.number_close(5, vector2.length(a))
		assert.number_close(5, a:length())
	end)

	it("normalize()", function()
		local a = vector2.new(0, -5)
		local norm1 = vector2.normalize(a)
		assert.number_close(0, norm1.x)
		assert.number_close(-1, norm1.y)
		local norm2 = a:normalize()
		assert.number_close(0, norm2.x)
		assert.number_close(-1, norm2.y)
		local norm3 = vector2.normalize(vector2.zero())
		assert.number_close(0, norm3.x)
		assert.number_close(0, norm3.y)
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
		local f = function(x)
			return x * 2
		end
		local f2 = function(x, opt1, opt2)
			return x + opt1 + opt2
		end
		local a = vector2.new(0.1, 0.9)
		assert.same(vector2.new(1, 1), vector2.apply(a, math.ceil))
		assert.same(vector2.new(1, 1), a:apply(math.ceil))
		assert.same(vector2.new(0.1, 0.9), vector2.apply(a, math.abs))
		assert.same(vector2.new(0.1, 0.9), a:apply(math.abs))
		assert.same(vector2.new(0.2, 1.8), vector2.apply(a, f))
		assert.same(vector2.new(0.2, 1.8), a:apply(f))
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
		assert.number_close(math.pi, vector2.angle(vector2.new(-1, -2), vector2.new(1, 2)))
		assert.number_close(math.pi/2, vector2.new(0, 1):angle(vector2.new(1, 0)))
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
		local str1 = vector2.to_string(v)
		local str2 = v:to_string()
		local str3 = tostring(v)

		-- All should produce the same string
		assert.same(str1, str2)
		assert.same(str1, str3)

		-- Verify the string format
		assert.same("(1, 2)", str1)

		-- Test edge cases for %g format
		assert.same("(0, 0)", vector2.to_string(vector2.new(0, 0)))
		assert.same("(-1, -2)", vector2.to_string(vector2.new(-1, -2)))
		assert.same("(0.0001, 1e+10)", vector2.to_string(vector2.new(0.0001, 1e10)))
		assert.same("(3.14159, 1.41421)", vector2.to_string(vector2.new(math.pi, math.sqrt(2))))
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

	describe("from_angle()", function()
		it("creates unit vector from angle", function()
			assert.vector2_close(vector2.new(1, 0), vector2.from_angle(0))
			assert.vector2_close(vector2.new(0, 1), vector2.from_angle(math.pi / 2))
			assert.vector2_close(vector2.new(-1, 0), vector2.from_angle(math.pi))
		end)

		it("throws on invalid input", function()
			assert.has.errors(function()
				vector2.from_angle()
			end)
		end)
	end)

	describe("to_angle()", function()
		it("returns angle of vector", function()
			assert.number_close(0, vector2.to_angle(vector2.new(1, 0)))
			assert.number_close(math.pi / 2, vector2.to_angle(vector2.new(0, 1)))
			assert.number_close(math.pi / 4, vector2.to_angle(vector2.new(1, 1)))
		end)

		it("is inverse of from_angle", function()
			local angle = math.pi / 3
			local v = vector2.from_angle(angle)
			assert.number_close(angle, vector2.to_angle(v))
		end)
	end)

	describe("rotate()", function()
		it("rotates vector by angle in radians", function()
			assert.vector2_close(vector2.new(0, 1), vector2.rotate(vector2.new(1, 0), math.pi / 2))
			assert.vector2_close(vector2.new(-1, 0), vector2.rotate(vector2.new(1, 0), math.pi))
			assert.vector2_close(vector2.new(0, 1), vector2.new(1, 0):rotate(math.pi / 2))
		end)

		it("preserves length", function()
			local v = vector2.new(3, 4)
			local rotated = vector2.rotate(v, math.pi / 3)
			assert.number_close(vector2.length(v), vector2.length(rotated))
		end)
	end)

	it("in_area()", function()
		assert.is_true(vector2.in_area(vector2.zero(), vector2.new(-10, -10), vector2.new(10, 10)))
		assert.is_true(vector2.in_area(vector2.new(-2, 5), vector2.new(-10, -10), vector2.new(10, 10)))
		assert.is_true(vector2.in_area(vector2.new(-10, -10), vector2.new(-10, -10), vector2.new(10, 10)))
		assert.is_false(vector2.in_area(vector2.new(-11, -10), vector2.new(-10, -10), vector2.new(10, 10)))
	end)
end)
