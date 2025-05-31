local function assert_close(expected, got)
	if expected:angle_to(got) > 1e-3 then
		error("expected +-" .. tostring(expected) .. " got " .. tostring(got))
	end
end

local function assert_close_vec(expected, got)
	if expected:distance(got) > 1e-4 then
		error("expected " .. tostring(expected) .. " got " .. tostring(got))
	end
end

local function srandom(n)
	if n == 0 then
		return
	end
	return 2 * math.random() - 1, srandom(n - 1)
end

local function random_rotation()
	return Rotation.quaternion(srandom(4))
end

describe("constructors", function()
	it("identity", function()
		local rot = Rotation.identity()
		assert.same({0, 0, 0, 1}, {rot:to_quaternion()})
	end)
	it("quaternion", function()
		assert_close(Rotation.identity(), Rotation.quaternion(0, 0, 0, 1))
	end)
	it("axis-angle", function()
		assert_close(Rotation.quaternion(1, 1, 1, 0),
				Rotation.axis_angle(vector.new(1, 1, 1), math.pi))
	end)
	it("axis-angle shorthands", function()
		local angle = math.pi
		assert_close(Rotation.quaternion(1, 0, 0, 0), Rotation.x(angle))
		assert_close(Rotation.quaternion(0, 1, 0, 0), Rotation.y(angle))
		assert_close(Rotation.quaternion(0, 0, 1, 0), Rotation.z(angle))
	end)
	it("euler angles", function()
		local pitch, yaw, roll = 1, 2, 3
		assert_close(Rotation.compose(Rotation.z(roll), Rotation.y(yaw), Rotation.x(pitch)),
				Rotation.euler_angles(pitch, yaw, roll))
	end)
end)

describe("conversions", function()
	local function test_roundtrip(name)
		it(name, function()
			for _ = 1, 100 do
				local rot = random_rotation()
				assert_close(rot, Rotation[name](rot["to_" .. name](rot)))
			end
		end)
	end
	test_roundtrip("quaternion")
	test_roundtrip("axis_angle")
	test_roundtrip("euler_angles")
end)

describe("composition", function()
	it("is the identity for an empty argument list", function()
		assert_close(Rotation.identity(), Rotation.compose())
	end)
	it("is the same rotation for a single argument", function()
		local rot = Rotation.x(math.pi / 2)
		assert_close(rot, rot:compose())
	end)
	it("is consistent with application", function()
		for _ = 1, 100 do
			local r1, r2 = random_rotation(), random_rotation()
			local v = vector.new(srandom(3))
			assert_close_vec(r1:apply(r2:apply(v)), r1:compose(r2):apply(v))
		end
	end)
end)

it("application", function()
	assert_close_vec(vector.new(-2, 1, 3), Rotation.z(math.pi / 2):apply(vector.new(1, 2, 3)))
end)

it("inversion", function()
	assert_close(Rotation.x(-math.pi / 2), Rotation.x(math.pi / 2):invert())
end)

it("slerp", function()
	local from, to = Rotation.identity(), Rotation.x(2)
	assert_close(Rotation.identity(), from:slerp(to, 0))
	assert_close(Rotation.x(1), from:slerp(to, 0.5))
	assert_close(Rotation.x(2), from:slerp(to, 1))
end)

it("tostring", function()
	assert(type(tostring(Rotation.identity())) == "string")
end)
