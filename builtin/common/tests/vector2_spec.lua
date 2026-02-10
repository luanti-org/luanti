_G.vector = {}
_G.vector2 = {}
dofile("builtin/common/math.lua")
dofile("builtin/common/vector.lua")
dofile("builtin/common/vector2.lua")

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
		assert.equal(vector2.new(0, 1), vector2.direction(a, b))
		assert.equal(vector2.new(0, 1), a:direction(b))
	end)

	it("distance()", function()
		local a = vector2.new(1, 0)
		local b = vector2.new(4, 4)
		assert.equal(5, vector2.distance(a, b))
		assert.equal(5, a:distance(b))
		assert.equal(0, vector2.distance(a, a))
		assert.equal(0, b:distance(b))
	end)

	it("length()", function()
		local a = vector2.new(3, 4)
		assert.equal(0, vector2.length(vector2.zero()))
		assert.equal(5, vector2.length(a))
		assert.equal(5, a:length())
	end)

	it("normalize()", function()
		local a = vector2.new(0, -5)
		assert.equal(vector2.new(0, -1), vector2.normalize(a))
		assert.equal(vector2.new(0, -1), a:normalize())
		assert.equal(vector2.zero(), vector2.normalize(vector2.zero()))
	end)

	it("floor()", function()
		local a = vector2.new(0.1, 0.9)
		assert.equal(vector2.new(0, 0), vector2.floor(a))
		assert.equal(vector2.new(0, 0), a:floor())
	end)

	it("round()", function()
		local a = vector2.new(0.1, 0.9)
		assert.equal(vector2.new(0, 1), vector2.round(a))
		assert.equal(vector2.new(0, 1), a:round())
	end)

	it("ceil()", function()
		local a = vector2.new(0.1, 0.9)
		assert.equal(vector2.new(1, 1), vector2.ceil(a))
		assert.equal(vector2.new(1, 1), a:ceil())
	end)

	it("sign()", function()
		local a = vector2.new(-120.3, 231.5)
		assert.equal(vector2.new(-1, 1), vector2.sign(a))
		assert.equal(vector2.new(-1, 1), a:sign())
		assert.equal(vector2.new(0, 1), vector2.sign(a, 200))
		assert.equal(vector2.new(0, 1), a:sign(200))
	end)

	it("abs()", function()
		local a = vector2.new(-123.456, 13)
		assert.equal(vector2.new(123.456, 13), vector2.abs(a))
		assert.equal(vector2.new(123.456, 13), a:abs())
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
		assert.equal(vector2.new(1, 1), vector2.apply(a, math.ceil))
		assert.equal(vector2.new(1, 1), a:apply(math.ceil))
		assert.equal(vector2.new(0.1, 0.9), vector2.apply(a, math.abs))
		assert.equal(vector2.new(0.1, 0.9), a:apply(math.abs))
		assert.equal(vector2.new(1.1, 2.9), vector2.apply(a, f))
		assert.equal(vector2.new(3.1, 4.9), a:apply(f))
		local b = vector2.new(1, 2)
		assert.equal(vector2.new(3, 4), vector2.apply(b, f2, 1, 1))
		assert.equal(vector2.new(3, 4), b:apply(f2, 1, 1))
	end)

	it("combine()", function()
		local a = vector2.new(1, 2)
		local b = vector2.new(3, 2)
		assert.equal(vector2.add(a, b), vector2.combine(a, b, function(x, y) return x + y end))
		assert.equal(vector2.new(3, 2), vector2.combine(a, b, math.max))
		assert.equal(vector2.new(1, 2), vector2.combine(a, b, math.min))
	end)

	it("equals()", function()
		local function assertE(a, b)
			assert.is_true(vector2.equals(a, b))
		end
		local function assertNE(a, b)
			assert.is_false(vector2.equals(a, b))
		end

		assertE({x = 0, y = 0}, {x = 0, y = 0})
		assertE({x = -1, y = 0}, {x = -1, y = 0})
		assertE({x = -1, y = 0}, vector2.new(-1, 0))
		local a = {x = 2, y = 4}
		assertE(a, a)
		assertNE({x = -1, y = 0}, a)

		assert.equal(vector2.new(1, 2), vector2.new(1, 2))
		assert.is_true(vector2.new(1, 2):equals(vector2.new(1, 2)))
		assert.not_equal(vector2.new(1, 2), vector2.new(1, 3))
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
		assert.equal(math.pi, vector2.angle(vector2.new(-1, -2), vector2.new(1, 2)))
		assert.is_true(math.abs(math.pi/2 - vector2.new(0, 1):angle(vector2.new(1, 0))) < 0.000001)
	end)

	it("dot()", function()
		assert.equal(-5, vector2.dot(vector2.new(-1, -2), vector2.new(1, 2)))
		assert.equal(0, vector2.zero():dot(vector2.new(1, 2)))
	end)

	it("offset()", function()
		assert.same({x = 41, y = 52}, vector2.offset(vector2.new(1, 2), 40, 50))
		assert.equal(vector2.new(41, 52), vector2.offset(vector2.new(1, 2), 40, 50))
		assert.equal(vector2.new(41, 52), vector2.new(1, 2):offset(40, 50))
	end)

	it("is()", function()
		local some_table1 = {foo = 13, [42] = 1, "bar", 2}
		local some_table2 = {1, 2}
		local some_table3 = {x = 1, 2}
		local some_table4 = {1, y = 3}
		local old = {x = 1, y = 2}
		local real = vector2.new(1, 2)

		assert.is_false(vector2.check(nil))
		assert.is_false(vector2.check(1))
		assert.is_false(vector2.check(true))
		assert.is_false(vector2.check("foo"))
		assert.is_false(vector2.check(some_table1))
		assert.is_false(vector2.check(some_table2))
		assert.is_false(vector2.check(some_table3))
		assert.is_false(vector2.check(some_table4))
		assert.is_false(vector2.check(old))
		assert.is_true(vector2.check(real))
		assert.is_true(real:check())
	end)

	it("global pairs", function()
		local out = {}
		local vec = vector2.new(10, 20)
		for k, v in pairs(vec) do
			out[k] = v
		end
		assert.same({x = 10, y = 20}, out)
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
		assert.equal(c, vector2.add(a, {x = 1, y = 4}))
		assert.equal(c, vector2.add(a, b))
		assert.equal(c, a:add(b))
		assert.equal(c, a + b)
		assert.equal(c, b + a)
	end)

	it("subtract()", function()
		local a = vector2.new(1, 2)
		local b = vector2.new(2, 4)
		local c = vector2.new(-1, -2)
		assert.equal(c, vector2.subtract(a, {x = 2, y = 4}))
		assert.equal(c, vector2.subtract(a, b))
		assert.equal(c, a:subtract(b))
		assert.equal(c, a - b)
		assert.equal(c, -b + a)
	end)

	it("multiply()", function()
		local a = vector2.new(1, 2)
		local s = 2
		local d = vector2.new(2, 4)
		assert.equal(d, vector2.multiply(a, s))
		assert.equal(d, a:multiply(s))
		assert.equal(d, a * s)
		assert.equal(d, s * a)
		assert.equal(-a, -1 * a)
	end)

	it("divide()", function()
		local a = vector2.new(1, 2)
		local s = 2
		local d = vector2.new(0.5, 1)
		assert.equal(d, vector2.divide(a, s))
		assert.equal(d, a:divide(s))
		assert.equal(d, a / s)
		assert.equal(d, 1/s * a)
		assert.equal(-a, a / -1)
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
		assert.same(nil, vector2.from_string("nothing"))
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
