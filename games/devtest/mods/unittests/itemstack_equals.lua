local function test_itemstack_equals_non_stack()
	local i1 = ItemStack("basenodes:stone")
	local i2 = { foo = "bar" }

	assert(not i1:equals(i2))
	assert(i1 ~= i2)
	assert(i2 ~= i1)
end

unittests.register("test_itemstack_equals_non_stack", test_itemstack_equals_non_stack)

local function test_itemstack_equals_name()
	local i1 = ItemStack("basenodes:stone")
	local i2 = ItemStack("basenodes:desert_stone")

	assert(not i1:equals(i2))
	assert(i1 ~= i2)
end

unittests.register("test_itemstack_equals_name", test_itemstack_equals_name)

local function test_itemstack_equals_count()
	local i1 = ItemStack("basenodes:stone")
	local i2 = ItemStack("basenodes:stone 2")

	assert(not i1:equals(i2))
	assert(i1 ~= i2)
end

unittests.register("test_itemstack_equals_count", test_itemstack_equals_count)

local function test_itemstack_equals_wear()
	local i1 = ItemStack("basetools:axe_stone")
	local i2 = ItemStack("basetools:axe_stone")

	i2:add_wear(1)

	assert(not i1:equals(i2))
	assert(i1 ~= i2)
end

unittests.register("test_itemstack_equals_wear", test_itemstack_equals_wear)

local function test_itemstack_equals_metadata()
	local i1 = ItemStack("basenodes:stone")
	local i2 = ItemStack("basenodes:stone")
	local i3 = ItemStack("basenodes:stone")

	local m1 = i1:get_meta()
	local m2 = i2:get_meta()
	local m3 = i3:get_meta()

	local keys = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p"}
	local values = {}

	for i, key in pairs(keys) do
		m1:set_int(key, i)
		m3:set_int(key, i)
		values[key] = i
	end

	m3:set_int("a", 999)

	for key, i in pairs(values) do
		m2:set_int(key, i)
	end

	assert(i1:equals(i2))
	assert(i1 == i2)
	assert(not i1:equals(i3))
	assert(i1 ~= i3)
end

unittests.register("test_itemstack_equals_metadata", test_itemstack_equals_metadata)

local function test_itemstack_serialization_preservation()
	local i = ItemStack("basenodes:stone 20 1000")
	assert(i:equals(core.deserialize(core.serialize(i))))
end

unittests.register("test_itemstack_serialization_preservation", test_itemstack_serialization_preservation)
