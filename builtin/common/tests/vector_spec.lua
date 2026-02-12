_G.vector = {}
dofile("builtin/common/math.lua")
dofile("builtin/common/vector.lua")

describe("vector", function()
	describe("new()", function()
		it("constructs", function()
			assert.same({x = 0, y = 0, z = 0}, vector.new())
			assert.same({x = 1, y = 2, z = 3}, vector.new(1, 2, 3))
			assert.same({x = 3, y = 2, z = 1}, vector.new({x = 3, y = 2, z = 1}))

			assert.is_true(vector.check(vector.new()))
			assert.is_true(vector.check(vector.new(1, 2, 3)))
			assert.is_true(vector.check(vector.new({x = 3, y = 2, z = 1})))

			local input = vector.new({ x = 3, y = 2, z = 1 })
			local output = vector.new(input)
			assert.same(input, output)
			assert.equal(input, output)
			assert.is_false(rawequal(input, output))
			assert.equal(input, input:new())
		end)

		it("throws on invalid input", function()
			assert.has.errors(function()
				vector.new({ x = 3 })
			end)

			assert.has.errors(function()
				vector.new({ d = 3 })
			end)
		end)
	end)

	it("zero()", function()
		assert.same({x = 0, y = 0, z = 0}, vector.zero())
		assert.same(vector.new(), vector.zero())
		assert.equal(vector.new(), vector.zero())
		assert.is_true(vector.check(vector.zero()))
	end)

	it("copy()", function()
		local v = vector.new(1, 2, 3)
		assert.same(v, vector.copy(v))
		assert.same(vector.new(v), vector.copy(v))
		assert.equal(vector.new(v), vector.copy(v))
		assert.is_true(vector.check(vector.copy(v)))
	end)

	it("indexes", function()
		local some_vector = vector.new(24, 42, 13)
		assert.equal(24, some_vector[1])
		assert.equal(24, some_vector.x)
		assert.equal(42, some_vector[2])
		assert.equal(42, some_vector.y)
		assert.equal(13, some_vector[3])
		assert.equal(13, some_vector.z)

		some_vector[1] = 100
		assert.equal(100, some_vector.x)
		some_vector.x = 101
		assert.equal(101, some_vector[1])

		some_vector[2] = 100
		assert.equal(100, some_vector.y)
		some_vector.y = 102
		assert.equal(102, some_vector[2])

		some_vector[3] = 100
		assert.equal(100, some_vector.z)
		some_vector.z = 103
		assert.equal(103, some_vector[3])
	end)

	it("direction()", function()
		local a = vector.new(1, 0, 0)
		local b = vector.new(1, 42, 0)
		assert.equal(vector.new(0, 1, 0), vector.direction(a, b))
		assert.equal(vector.new(0, 1, 0), a:direction(b))
	end)

	it("distance()", function()
		local a = vector.new(1, 0, 0)
		local b = vector.new(3, 42, 9)
		assert.is_true(math.abs(43 - vector.distance(a, b)) < 1.0e-12)
		assert.is_true(math.abs(43 - a:distance(b)) < 1.0e-12)
		assert.equal(0, vector.distance(a, a))
		assert.equal(0, b:distance(b))
	end)

	it("length()", function()
		local a = vector.new(0, 0, -23)
		assert.equal(0, vector.length(vector.new()))
		assert.equal(23, vector.length(a))
		assert.equal(23, a:length())
	end)

	it("normalize()", function()
		local a = vector.new(0, 0, -23)
		assert.equal(vector.new(0, 0, -1), vector.normalize(a))
		assert.equal(vector.new(0, 0, -1), a:normalize())
		assert.equal(vector.new(), vector.normalize(vector.new()))
	end)

	it("floor()", function()
		local a = vector.new(0.1, 0.9, -0.5)
		assert.equal(vector.new(0, 0, -1), vector.floor(a))
		assert.equal(vector.new(0, 0, -1), a:floor())
	end)

	it("round()", function()
		local a = vector.new(0.1, 0.9, -0.5)
		assert.equal(vector.new(0, 1, -1), vector.round(a))
		assert.equal(vector.new(0, 1, -1), a:round())
	end)

	it("ceil()", function()
		local a = vector.new(0.1, 0.9, -0.5)
		assert.equal(vector.new(1, 1, 0), vector.ceil(a))
		assert.equal(vector.new(1, 1, 0), a:ceil())
	end)

	it("sign()", function()
		local a = vector.new(-120.3, 0, 231.5)
		assert.equal(vector.new(-1, 0, 1), vector.sign(a))
		assert.equal(vector.new(-1, 0, 1), a:sign())
		assert.equal(vector.new(0, 0, 1), vector.sign(a, 200))
		assert.equal(vector.new(0, 0, 1), a:sign(200))
	end)

	it("abs()", function()
		local a = vector.new(-123.456, 0, 13)
		assert.equal(vector.new(123.456, 0, 13), vector.abs(a))
		assert.equal(vector.new(123.456, 0, 13), a:abs())
	end)

	it("apply()", function()
		local i = 0
		local f = function(x)
			i = i + 1
			return x + i
		end
		local f2 = function(x, opt1, opt2, opt3)
			return x + opt1 + opt2 + opt3
		end
		local a = vector.new(0.1, 0.9, -0.5)
		assert.equal(vector.new(1, 1, 0), vector.apply(a, math.ceil))
		assert.equal(vector.new(1, 1, 0), a:apply(math.ceil))
		assert.equal(vector.new(0.1, 0.9, 0.5), vector.apply(a, math.abs))
		assert.equal(vector.new(0.1, 0.9, 0.5), a:apply(math.abs))
		assert.equal(vector.new(1.1, 2.9, 2.5), vector.apply(a, f))
		assert.equal(vector.new(4.1, 5.9, 5.5), a:apply(f))
		local b = vector.new(1, 2, 3)
		assert.equal(vector.new(4, 5, 6), vector.apply(b, f2, 1, 1, 1))
		assert.equal(vector.new(4, 5, 6), b:apply(f2, 1, 1, 1))
	end)

	it("combine()", function()
		local a = vector.new(1, 2, 3)
		local b = vector.new(3, 2, 1)
		assert.equal(vector.add(a, b), vector.combine(a, b, function(x, y) return x + y end))
		assert.equal(vector.new(3, 2, 3), vector.combine(a, b, math.max))
		assert.equal(vector.new(1, 2, 1), vector.combine(a, b, math.min))
	end)

	it("equals()", function()
		local function assertE(a, b)
			assert.is_true(vector.equals(a, b))
		end
		local function assertNE(a, b)
			assert.is_false(vector.equals(a, b))
		end

		assertE({x = 0, y = 0, z = 0}, {x = 0, y = 0, z = 0})
		assertE({x = -1, y = 0, z = 1}, {x = -1, y = 0, z = 1})
		assertE({x = -1, y = 0, z = 1}, vector.new(-1, 0, 1))
		local a = {x = 2, y = 4, z = -10}
		assertE(a, a)
		assertNE({x = -1, y = 0, z = 1}, a)

		assert.equal(vector.new(1, 2, 3), vector.new(1, 2, 3))
		assert.is_true(vector.new(1, 2, 3):equals(vector.new(1, 2, 3)))
		assert.not_equal(vector.new(1, 2, 3), vector.new(1, 2, 4))
		assert.is_true(vector.new(1, 2, 3) == vector.new(1, 2, 3))
		assert.is_false(vector.new(1, 2, 3) == vector.new(1, 3, 3))
	end)

	it("metatable is same", function()
		local a = vector.new()
		local b = vector.new(1, 2, 3)

		assert.equal(true, vector.check(a))
		assert.equal(true, vector.check(b))

		assert.equal(vector.metatable, getmetatable(a))
		assert.equal(vector.metatable, getmetatable(b))
		assert.equal(vector.metatable, a.metatable)
	end)

	it("sort()", function()
		local a = vector.new(1, 2, 3)
		local b = vector.new(0.5, 232, -2)
		local sorted = {vector.new(0.5, 2, -2), vector.new(1, 232, 3)}
		assert.same(sorted, {vector.sort(a, b)})
		assert.same(sorted, {a:sort(b)})
	end)

	it("angle()", function()
		assert.equal(math.pi, vector.angle(vector.new(-1, -2, -3), vector.new(1, 2, 3)))
		assert.equal(math.pi/2, vector.new(0, 1, 0):angle(vector.new(1, 0, 0)))
	end)

	it("dot()", function()
		assert.equal(-14, vector.dot(vector.new(-1, -2, -3), vector.new(1, 2, 3)))
		assert.equal(0, vector.new():dot(vector.new(1, 2, 3)))
	end)

	it("cross()", function()
		local a = vector.new(-1, -2, 0)
		local b = vector.new(1, 2, 3)
		assert.equal(vector.new(-6, 3, 0), vector.cross(a, b))
		assert.equal(vector.new(-6, 3, 0), a:cross(b))
	end)

	it("offset()", function()
		assert.same({x = 41, y = 52, z = 63}, vector.offset(vector.new(1, 2, 3), 40, 50, 60))
		assert.equal(vector.new(41, 52, 63), vector.offset(vector.new(1, 2, 3), 40, 50, 60))
		assert.equal(vector.new(41, 52, 63), vector.new(1, 2, 3):offset(40, 50, 60))
	end)

	it("is()", function()
		local some_table1 = {foo = 13, [42] = 1, "bar", 2}
		local some_table2 = {1, 2, 3}
		local some_table3 = {x = 1, 2, 3}
		local some_table4 = {1, 2, z = 3}
		local old = {x = 1, y = 2, z = 3}
		local real = vector.new(1, 2, 3)

		assert.is_false(vector.check(nil))
		assert.is_false(vector.check(1))
		assert.is_false(vector.check(true))
		assert.is_false(vector.check("foo"))
		assert.is_false(vector.check(some_table1))
		assert.is_false(vector.check(some_table2))
		assert.is_false(vector.check(some_table3))
		assert.is_false(vector.check(some_table4))
		assert.is_false(vector.check(old))
		assert.is_true(vector.check(real))
		assert.is_true(real:check())
	end)

	it("global pairs", function()
		local out = {}
		local vec = vector.new(10, 20, 30)
		for k, v in pairs(vec) do
			out[k] = v
		end
		assert.same({x = 10, y = 20, z = 30}, out)
	end)

	it("abusing works", function()
		local v = vector.new(1, 2, 3)
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
		local a = vector.new(1, 2, 3)
		local b = vector.new(1, 4, 3)
		local c = vector.new(2, 6, 6)
		assert.equal(c, vector.add(a, {x = 1, y = 4, z = 3}))
		assert.equal(c, vector.add(a, b))
		assert.equal(c, a:add(b))
		assert.equal(c, a + b)
		assert.equal(c, b + a)
	end)

	it("subtract()", function()
		local a = vector.new(1, 2, 3)
		local b = vector.new(2, 4, 3)
		local c = vector.new(-1, -2, 0)
		assert.equal(c, vector.subtract(a, {x = 2, y = 4, z = 3}))
		assert.equal(c, vector.subtract(a, b))
		assert.equal(c, a:subtract(b))
		assert.equal(c, a - b)
		assert.equal(c, -b + a)
	end)

	it("multiply()", function()
		local a = vector.new(1, 2, 3)
		local b = vector.new(2, 4, 3)
		local c = vector.new(2, 8, 9)
		local s = 2
		local d = vector.new(2, 4, 6)
		assert.equal(c, vector.multiply(a, {x = 2, y = 4, z = 3}))
		assert.equal(c, vector.multiply(a, b))
		assert.equal(d, vector.multiply(a, s))
		assert.equal(d, a:multiply(s))
		assert.equal(d, a * s)
		assert.equal(d, s * a)
		assert.equal(-a, -1 * a)
	end)

	it("divide()", function()
		local a = vector.new(1, 2, 3)
		local b = vector.new(2, 4, 3)
		local c = vector.new(0.5, 0.5, 1)
		local s = 2
		local d = vector.new(0.5, 1, 1.5)
		assert.equal(c, vector.divide(a, {x = 2, y = 4, z = 3}))
		assert.equal(c, vector.divide(a, b))
		assert.equal(d, vector.divide(a, s))
		assert.equal(d, a:divide(s))
		assert.equal(d, a / s)
		assert.equal(d, 1/s * a)
		assert.equal(-a, a / -1)
	end)

	it("to_string()", function()
		local v = vector.new(1, 2, 3.14)
		assert.same("(1, 2, 3.14)", vector.to_string(v))
		assert.same("(1, 2, 3.14)", v:to_string())
		assert.same("(1, 2, 3.14)", tostring(v))
	end)

	it("from_string()", function()
		local v = vector.new(1, 2, 3.14)
		assert.is_true(vector.check(vector.from_string("(1, 2, 3.14)")))
		assert.same({v, 13}, {vector.from_string("(1, 2, 3.14)")})
		assert.same({v, 12}, {vector.from_string("(1,2 ,3.14)")})
		assert.same({v, 12}, {vector.from_string("(1,2,3.14,)")})
		assert.same({v, 11}, {vector.from_string("(1 2 3.14)")})
		assert.same({v, 15}, {vector.from_string("( 1, 2, 3.14 )")})
		assert.same({v, 15}, {vector.from_string(" ( 1, 2, 3.14) ")})
		assert.same({vector.new(), 8}, {vector.from_string("(0,0,0) ( 1, 2, 3.14) ")})
		assert.same({v, 22}, {vector.from_string("(0,0,0) ( 1, 2, 3.14) ", 8)})
		assert.same({v, 22}, {vector.from_string("(0,0,0) ( 1, 2, 3.14) ", 9)})
		assert.same(nil, vector.from_string("nothing"))
	end)

	-- This function is needed because of floating point imprecision.
	local function almost_equal(a, b)
		if type(a) == "number" then
			return math.abs(a - b) < 0.00000000001
		end
		return vector.distance(a, b) < 0.000000000001
	end

	describe("rotate_around_axis()", function()
		it("rotates", function()
			assert.True(almost_equal({x = -1, y = 0, z = 0},
				vector.rotate_around_axis({x = 1, y = 0, z = 0}, {x = 0, y = 1, z = 0}, math.pi)))
			assert.True(almost_equal({x = 0, y = 1, z = 0},
				vector.rotate_around_axis({x = 0, y = 0, z = 1}, {x = 1, y = 0, z = 0}, math.pi / 2)))
			assert.True(almost_equal({x = 4, y = 1, z = 1},
				vector.rotate_around_axis({x = 4, y = 1, z = 1}, {x = 4, y = 1, z = 1}, math.pi / 6)))
		end)
		it("keeps distance to axis", function()
			local rotate1 = {x = 1, y = 3, z = 1}
			local axis1 = {x = 1, y = 3, z = 2}
			local rotated1 = vector.rotate_around_axis(rotate1, axis1, math.pi / 13)
			assert.True(almost_equal(vector.distance(axis1, rotate1), vector.distance(axis1, rotated1)))
			local rotate2 = {x = 1, y = 1, z = 3}
			local axis2 = {x = 2, y = 6, z = 100}
			local rotated2 = vector.rotate_around_axis(rotate2, axis2, math.pi / 23)
			assert.True(almost_equal(vector.distance(axis2, rotate2), vector.distance(axis2, rotated2)))
			local rotate3 = {x = 1, y = -1, z = 3}
			local axis3 = {x = 2, y = 6, z = 100}
			local rotated3 = vector.rotate_around_axis(rotate3, axis3, math.pi / 2)
			assert.True(almost_equal(vector.distance(axis3, rotate3), vector.distance(axis3, rotated3)))
		end)
		it("rotates back", function()
			local rotate1 = {x = 1, y = 3, z = 1}
			local axis1 = {x = 1, y = 3, z = 2}
			local rotated1 = vector.rotate_around_axis(rotate1, axis1, math.pi / 13)
			rotated1 = vector.rotate_around_axis(rotated1, axis1, -math.pi / 13)
			assert.True(almost_equal(rotate1, rotated1))
			local rotate2 = {x = 1, y = 1, z = 3}
			local axis2 = {x = 2, y = 6, z = 100}
			local rotated2 = vector.rotate_around_axis(rotate2, axis2, math.pi / 23)
			rotated2 = vector.rotate_around_axis(rotated2, axis2, -math.pi / 23)
			assert.True(almost_equal(rotate2, rotated2))
			local rotate3 = {x = 1, y = -1, z = 3}
			local axis3 = {x = 2, y = 6, z = 100}
			local rotated3 = vector.rotate_around_axis(rotate3, axis3, math.pi / 2)
			rotated3 = vector.rotate_around_axis(rotated3, axis3, -math.pi / 2)
			assert.True(almost_equal(rotate3, rotated3))
		end)
		it("is right handed", function()
			local v_before1 = {x = 0, y = 1, z = -1}
			local v_after1 = vector.rotate_around_axis(v_before1, {x = 1, y = 0, z = 0}, math.pi / 4)
			assert.True(almost_equal(vector.normalize(vector.cross(v_after1, v_before1)), {x = 1, y = 0, z = 0}))

			local v_before2 = {x = 0, y = 3, z = 4}
			local v_after2 = vector.rotate_around_axis(v_before2, {x = 1, y = 0, z = 0},  2 * math.pi / 5)
			assert.True(almost_equal(vector.normalize(vector.cross(v_after2, v_before2)), {x = 1, y = 0, z = 0}))

			local v_before3 = {x = 1, y = 0, z = -1}
			local v_after3 = vector.rotate_around_axis(v_before3, {x = 0, y = 1, z = 0}, math.pi / 4)
			assert.True(almost_equal(vector.normalize(vector.cross(v_after3, v_before3)), {x = 0, y = 1, z = 0}))

			local v_before4 = {x = 3, y = 0, z = 4}
			local v_after4 = vector.rotate_around_axis(v_before4, {x = 0, y = 1, z = 0}, 2 * math.pi / 5)
			assert.True(almost_equal(vector.normalize(vector.cross(v_after4, v_before4)), {x = 0, y = 1, z = 0}))

			local v_before5 = {x = 1, y = -1, z = 0}
			local v_after5 = vector.rotate_around_axis(v_before5, {x = 0, y = 0, z = 1}, math.pi / 4)
			assert.True(almost_equal(vector.normalize(vector.cross(v_after5, v_before5)), {x = 0, y = 0, z = 1}))

			local v_before6 = {x = 3, y = 4, z = 0}
			local v_after6 = vector.rotate_around_axis(v_before6, {x = 0, y = 0, z = 1}, 2 * math.pi / 5)
			assert.True(almost_equal(vector.normalize(vector.cross(v_after6, v_before6)), {x = 0, y = 0, z = 1}))
		end)
	end)

	describe("rotate()", function()
		it("rotates", function()
			assert.True(almost_equal({x = -1, y = 0, z = 0},
				vector.rotate({x = 1, y = 0, z = 0}, {x = 0, y = math.pi, z = 0})))
			assert.True(almost_equal({x = 0, y = -1, z = 0},
				vector.rotate({x = 1, y = 0, z = 0}, {x = 0, y = 0, z = math.pi / 2})))
			assert.True(almost_equal({x = 1, y = 0, z = 0},
				vector.rotate({x = 1, y = 0, z = 0}, {x = math.pi / 123, y = 0, z = 0})))
		end)
		it("rotation order is Z-X-Y", function()
			local r = vector.new(1, 2, 3)
			for _, v in ipairs({
				vector.new(1, 0, 0),
				vector.new(0, 1, 0),
				vector.new(0, 0, 1),
			}) do
				local expected = v:rotate(r)
				local function try(order)
					local rotated = v
					for axis in order:gmatch(".") do
						local r_axis = vector.zero()
						r_axis[axis] = r[axis]
						rotated = vector.rotate(rotated, r_axis)
					end
					return almost_equal(rotated, expected)
				end
				assert.False(try("xyz"))
				assert.False(try("xzy"))
				assert.False(try("yxz"))
				assert.False(try("yzx"))
				assert.True(try("zxy"))
				assert.False(try("zyx"))
			end
		end)
		it("is right handed", function()
			local v_before1 = {x = 0, y = 1, z = -1}
			local v_after1 = vector.rotate(v_before1, {x = math.pi / 4, y = 0, z = 0})
			assert.True(almost_equal(vector.normalize(vector.cross(v_after1, v_before1)), {x = 1, y = 0, z = 0}))

			local v_before2 = {x = 0, y = 3, z = 4}
			local v_after2 = vector.rotate(v_before2, {x = 2 * math.pi / 5, y = 0, z = 0})
			assert.True(almost_equal(vector.normalize(vector.cross(v_after2, v_before2)), {x = 1, y = 0, z = 0}))

			local v_before3 = {x = 1, y = 0, z = -1}
			local v_after3 = vector.rotate(v_before3, {x = 0, y = math.pi / 4, z = 0})
			assert.True(almost_equal(vector.normalize(vector.cross(v_after3, v_before3)), {x = 0, y = 1, z = 0}))

			local v_before4 = {x = 3, y = 0, z = 4}
			local v_after4 = vector.rotate(v_before4, {x = 0, y = 2 * math.pi / 5, z = 0})
			assert.True(almost_equal(vector.normalize(vector.cross(v_after4, v_before4)), {x = 0, y = 1, z = 0}))

			local v_before5 = {x = 1, y = -1, z = 0}
			local v_after5 = vector.rotate(v_before5, {x = 0, y = 0, z = math.pi / 4})
			assert.True(almost_equal(vector.normalize(vector.cross(v_after5, v_before5)), {x = 0, y = 0, z = 1}))

			local v_before6 = {x = 3, y = 4, z = 0}
			local v_after6 = vector.rotate(v_before6, {x = 0, y = 0, z = 2 * math.pi / 5})
			assert.True(almost_equal(vector.normalize(vector.cross(v_after6, v_before6)), {x = 0, y = 0, z = 1}))
		end)
	end)

	it("dir_to_rotation()", function()
		-- Comparing rotations (pitch, yaw, roll) is hard because of certain ambiguities,
		-- e.g. (pi, 0, pi) looks exactly the same as (0, pi, 0)
		-- So instead we convert the rotation back to vectors and compare these.
		local function forward_at_rot(rot)
			return vector.rotate(vector.new(0, 0, 1), rot)
		end
		local function up_at_rot(rot)
			return vector.rotate(vector.new(0, 1, 0), rot)
		end
		local rot1 = vector.dir_to_rotation({x = 1, y = 0, z = 0}, {x = 0, y = 1, z = 0})
		assert.True(almost_equal({x = 1, y = 0, z = 0}, forward_at_rot(rot1)))
		assert.True(almost_equal({x = 0, y = 1, z = 0}, up_at_rot(rot1)))
		local rot2 = vector.dir_to_rotation({x = 1, y = 1, z = 0}, {x = 0, y = 0, z = 1})
		assert.True(almost_equal({x = 1/math.sqrt(2), y = 1/math.sqrt(2), z = 0}, forward_at_rot(rot2)))
		assert.True(almost_equal({x = 0, y = 0, z = 1}, up_at_rot(rot2)))
		for i = 1, 1000 do
			local rand_vec = vector.new(math.random(), math.random(), math.random())
			if vector.length(rand_vec) ~= 0 then
				local rot_1 = vector.dir_to_rotation(rand_vec)
				local rot_2 = {
					x = math.atan2(rand_vec.y, math.sqrt(rand_vec.z * rand_vec.z + rand_vec.x * rand_vec.x)),
					y = -math.atan2(rand_vec.x, rand_vec.z),
					z = 0
				}
				assert.True(almost_equal(rot_1, rot_2))
			end
		end

	end)

	it("in_area()", function()
		assert.True(vector.in_area(vector.zero(), vector.new(-10, -10, -10), vector.new(10, 10, 10)))
		assert.True(vector.in_area(vector.new(-2, 5, -8), vector.new(-10, -10, -10), vector.new(10, 10, 10)))
		assert.True(vector.in_area(vector.new(-10, -10, -10), vector.new(-10, -10, -10), vector.new(10, 10, 10)))
		assert.False(vector.in_area(vector.new(-10, -10, -10), vector.new(10, 10, 10), vector.new(-11, -10, -10)))
	end)

	it("random_in_area()", function()
		local min = vector.new(-100, -100, -100)
		local max = vector.new(100, 100, 100)
		for i = 1, 1000 do
			local random = vector.random_in_area(min, max)
			assert.True(vector.in_area(random, min, max))
		end
	end)
end)
