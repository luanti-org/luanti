local function describe(_, func)
	func()
end

local function it(section_name, func)
	print("Running test: " .. section_name)
	func()
end

local function assert_close(expected, actual)
	assert(expected:angle_to(actual) < 1e-4)
end

describe("constructors", function()
	it("identity", function()
		local rot = Rotation.identity()
		assert.same({0, 0, 0, 1}, {rot:to_quaternion()})
	end)
	it("quaternion", function()
		local rot = Rotation.quaternion(0, 0, 0, 1)
		assert_close(rot, Rotation.identity())
	end)
	it("axis angle", function() end)
	it("axis-angle shorthands", function()

	end)
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

	end)
end)

local function random_quaternion()
	local x = math.random()
	local y = math.random()
	local z = math.random()
	local w = math.random()
	return Rotation.quaternion(x, y, z, w)
end

describe("inversion", function()
	it("random quaternions", function()
		for _ = 1, 100 do
			local rot = random_quaternion()
			assert_close(Rotation.identity(), rot:inverse():compose(rot))
			assert_close(Rotation.identity(), rot:compose(rot:inverse()))
		end
	end)
	it("inverts the angle", function()
		for _ = 1, 100 do
			local rot = random_quaternion()
			local axis, angle = rot:axis_angle()
			local inv_axis, inv_angle = rot:inverse():axis_angle()
			assert(axis:distance(inv_axis) < 1e-4)
			assert(math.abs(angle + inv_angle) < 1e-4)
		end
	end)
end)