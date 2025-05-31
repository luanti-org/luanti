local function assert_close(expected, got)
	local tolerance = 1e-4
	if not expected:equals(got, tolerance) then
		error("expected " .. tostring(expected) .. " +- " .. tolerance .. " got " .. tostring(got))
	end
end

local mat1 = Matrix4.new(
	1, 2, 3, 4,
	5, 6, 7, 8,
	9, 10, 11, 12,
	13, 14, 15, 16
)

it("identity", function()
	assert.equals(Matrix4.new(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	), Matrix4.identity())
end)

describe("getters & setters", function()
	it("get & set", function()
		local mat = Matrix4.all(0)
		local i = 0
		for row = 1, 4 do
			for col = 1, 4 do
				i = i + 1
				assert.equals(0, mat:get(row, col))
				mat:set(row, col, i)
				assert.equals(i, mat:get(row, col))
			end
		end
		assert.equals(mat1, mat)
	end)
	it("get_row & set_row", function()
		local mat = mat1:copy()
		assert.same({5, 6, 7, 8}, {mat:get_row(2)})
		mat:set_row(2, 1, 2, 3, 4)
		assert.same({1, 2, 3, 4}, {mat:get_row(2)})
	end)
	it("get_column & set_column", function()
		local mat = mat1:copy()
		assert.same({3, 7, 11, 15}, {mat:get_column(3)})
		mat:set_column(3, 1, 2, 3, 4)
		assert.same({1, 2, 3, 4}, {mat:get_column(3)})
		assert.same({3, 7, 11, 15}, {mat1:get_column(3)})
	end)
end)

it("copy", function()
	assert.equals(mat1, mat1:copy())
end)

it("unpack", function()
	assert.equals(mat1, Matrix4.new(mat1:unpack()))
end)

describe("transform", function()
	it("4d", function()
		assert.same({
			1 * 1 + 2 * 5 + 3 * 9 + 4 * 13,
			1 * 2 + 2 * 6 + 3 * 10 + 4 * 14,
			1 * 3 + 2 * 7 + 3 * 11 + 4 * 15,
			1 * 4 + 2 * 8 + 3 * 12 + 4 * 16,
		}, {mat1:transform_4d(1, 2, 3, 4)})
	end)
	it("position", function()
		assert.equals(vector.new(
			1 * 1 + 2 * 5 + 3 * 9,
			1 * 2 + 2 * 6 + 3 * 10,
			1 * 3 + 2 * 7 + 3 * 11
		):offset(13, 14, 15), mat1:transform_position(vector.new(1, 2, 3)))
	end)
	it("direction", function()
		assert.equals(vector.new(
			1 * 1 + 2 * 5 + 3 * 9,
			1 * 2 + 2 * 6 + 3 * 10,
			1 * 3 + 2 * 7 + 3 * 11
		), mat1:transform_direction(vector.new(1, 2, 3)))
	end)
end)


local mat2 = Matrix4.new(
	16, 15, 14, 13,
	12, 11, 10, 9,
	8, 7, 6, 5,
	4, 3, 2, 1
)

describe("composition", function()
	it("identity for empty argument list", function()
		assert(Matrix4.identity():equals(Matrix4.compose()))
	end)
	it("same matrix for single argument", function()
		local mat = Matrix4.new(
			1, 2, 3, 4,
			5, 6, 7, 8,
			9, 10, 11, 12,
			13, 14, 15, 16
		)
		assert(mat:equals(mat:compose()))
	end)
	it("matrix multiplication for two arguments", function()
		local composition = mat1:compose(mat2)
		assert.equals(Matrix4.new(
			386, 444, 502, 560,
			274, 316, 358, 400,
			162, 188, 214, 240,
			50, 60, 70, 80
		), composition)
		assert.same({mat1:transform_4d(mat2:transform_4d(1, 2, 3, 4))},
				{composition:transform_4d(1, 2, 3, 4)})
	end)
	it("supports multiple arguments", function()
		local fib = Matrix4.new(
			0, 1, 0, 0, -- x' = y
			1, 1, 0, 0, -- y' = x + y
			0, 0, 1, 0,
			0, 0, 0, 1
		)
		local function rep(v, n)
			if n == 0 then
				return
			end
			return v, rep(v, n - 1)
		end
		local result = Matrix4.compose(rep(fib, 10))
		assert.equals(55, result:get(2, 1))
	end)
end)

local function random_matrix4()
	local t = {}
	for i = 1, 16 do
		t[i] = math.random()
	end
	return Matrix4.new(unpack(t))
end

it("determinant", function()
	assert.equals(42, Matrix4.scale(vector.new(2, 3, 7)):determinant())
end)

describe("inversion", function()
	it("simple permutation", function()
		assert_close(Matrix4.new(
			0, 1, 0, 0,
			0, 0, 1, 0,
			1, 0, 0, 0,
			0, 0, 0, 1
		), Matrix4.new(
			0, 0, 1, 0,
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 0, 1
		):invert())
	end)
	it("random matrices", function()
		for _ = 1, 100 do
			local mat = random_matrix4()
			if math.abs(mat:determinant()) > 1e-3 then
				assert_close(Matrix4.identity(), mat:invert():compose(mat))
				assert_close(Matrix4.identity(), mat:compose(mat:invert()))
			end
		end
	end)
end)

it("transpose", function()
	assert.equals(Matrix4.new(
		1, 5, 9, 13,
		2, 6, 10, 14,
		3, 7, 11, 15,
		4, 8, 12, 16
	), mat1:transpose())
end)

describe("affine transform constructors", function()
	it("translation", function()
		assert.equals(Matrix4.new(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			1, 2, 3, 1
		), Matrix4.translation(vector.new(1, 2, 3)))
	end)
	it("scale", function()
		assert.equals(Matrix4.new(
			-1, 0, 0, 0,
			0, 2, 0, 0,
			0, 0, 3, 0,
			0, 0, 0, 1
		), Matrix4.scale(vector.new(-1, 2, 3)))
	end)
	it("rotation", function()
		assert_close(Matrix4.new(
			0, 1, 0, 0,
			-1, 0, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		), Matrix4.rotation(Rotation.z(math.pi / 2)))
	end)
	it("reflection", function()
		assert_close(Matrix4.identity() - 2/(1^2 + 2^2 + 3^2) * Matrix4.new(
			1, 2, 3, 0,
			2, 4, 6, 0,
			3, 6, 9, 0,
			0, 0, 0, 0
		), Matrix4.reflection(vector.new(1, 2, 3)))
	end)
end)

describe("affine transform methods", function()
	local t = vector.new(4, 5, 6)
	local r = Rotation.z(math.pi / 2)
	local s = vector.new(1, 2, 3)
	local trs = Matrix4.compose(
		Matrix4.translation(t),
		Matrix4.rotation(r),
		Matrix4.scale(s)
	)
	it("is affine", function()
		assert(trs:is_affine_transform())
		assert(not mat1:is_affine_transform())
	end)
	it("get translation", function()
		assert.equals(t, trs:get_translation())
	end)
	it("get rotation & scale", function()
		local rotation, scale = trs:get_rs()
		assert(r:angle_to(rotation) < 1e-3)
		assert(s:distance(scale) < 1e-3)
	end)
	it("set translation", function()
		local mat = trs:copy()
		local v = vector.new(1, 2, 3)
		mat:set_translation(v)
		assert.equals(v, mat:get_translation())
	end)
end)

describe("metamethods", function()
	it("addition", function()
		assert.equals(Matrix4.all(17), mat1 + mat2)
	end)
	it("subtraction", function()
		assert.equals(Matrix4.all(0), mat1 - mat1)
	end)
	it("unary minus", function()
		assert.equals(-1 * mat1, -mat1)
	end)
	it("scalar multiplication", function()
		assert.equals(2 * mat1, mat1 * 2)
		assert.equals(2 * mat1, mat1:compose(Matrix4.new(
			2, 0, 0, 0,
			0, 2, 0, 0,
			0, 0, 2, 0,
			0, 0, 0, 2
		)))
	end)
	it("equals", function()
		local mat1 = Matrix4.identity()
		local mat2 = Matrix4.identity()
		assert(mat1:equals(mat2))
		mat2:set(1, 1, 0)
		assert(not mat1:equals(mat2))
		mat2:set(1, 1, 0.999)
		assert(mat1:equals(mat2, 0.01))
	end)
	it("tostring", function()
		assert(tostring(Matrix4.scale(vector.new(12345, 0, 0))):find"12345")
	end)
end)
