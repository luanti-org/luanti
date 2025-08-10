-- Tests of generic and specific metadata functionality

local compare_meta = ItemStack("unittests:iron_lump"):get_meta()
compare_meta:from_table({
	fields = {
		a = "1",
		b = "2",
		c = "3",
		d = "4",
		e = "e",
		["0.3"] = "0.29999999999999999",
		["0.1+0.2"] = "0.30000000000000004",
	},
})

local function test_metadata(meta)
	meta:from_table({fields = {a = 2, b = "2"}})
	meta:set_string("a", 1)
	meta:set_string("c", "3")
	meta:set_int("d", 4)
	meta:set_string("e", "e")

	meta:set_string("", "!")
	meta:set_string("", "")

	meta:set_float("0.3", 0.3)
	meta:set_float("0.1+0.2", 0.1 + 0.2)

	assert(meta:equals(compare_meta))

	local tab = meta:to_table()
	assert(tab.fields.a == "1")
	assert(tab.fields.b == "2")
	assert(tab.fields.c == "3")
	assert(tab.fields.d == "4")
	assert(tab.fields.e == "e")
	assert(tab.fields["0.3"] == "0.29999999999999999")
	assert(tab.fields["0.1+0.2"] == "0.30000000000000004")

	local keys = meta:get_keys()
	assert(table.indexof(keys, "a") > 0)
	assert(table.indexof(keys, "b") > 0)
	assert(table.indexof(keys, "c") > 0)
	assert(table.indexof(keys, "d") > 0)
	assert(table.indexof(keys, "e") > 0)
	assert(#keys == 7)

	assert(not meta:contains(""))
	assert(meta:contains("a"))
	assert(meta:contains("b"))
	assert(meta:contains("c"))
	assert(meta:contains("d"))
	assert(meta:contains("e"))

	assert(meta:get("") == nil)
	assert(meta:get_string("") == "")
	assert(meta:get_int("") == 0)
	assert(meta:get_float("") == 0.0)
	assert(meta:get("a") == "1")
	assert(meta:get_string("a") == "1")
	assert(meta:get_int("a") == 1)
	assert(meta:get_float("a") == 1.0)
	assert(meta:get_int("e") == 0)
	assert(meta:get_float("e") == 0.0)
	assert(meta:get_float("0.3") == 0.3)
	assert(meta:get_float("0.1+0.2") == 0.1 + 0.2)

	meta:set_float("f", 1.1)
	meta:set_string("g", "${f}")
	meta:set_string("h", "${g}")
	meta:set_string("i", "${h}")
	assert(meta:get_float("h") > 1)
	assert(meta:get_string("i") == "${f}")

	meta:set_float("j", 1.23456789)
	assert(meta:get_float("j") == 1.23456789)
	meta:set_float("j", -1 / 0)
	assert(meta:get_float("j") == -1 / 0)
	meta:set_float("j", 0 / 0)
	assert(core.is_nan(meta:get_float("j")))

	meta:from_table()
	assert(next(meta:to_table().fields) == nil)
	assert(#meta:get_keys() == 0)

	assert(not meta:equals(compare_meta))
end

local function test_metadata_compat(meta)
	-- key/value removal using set_string (undocumented, deprecated way)
	meta:set_string("key", "value")
	assert(meta:get_string("key") == "value")
	meta:set_string("key", nil) -- ignore warning
	assert(meta:to_table().fields["key"] == nil)

	-- undocumented but supported consequence of Lua's
	-- automatic string <--> number cast
	meta:set_string("key", 2)
	assert(meta:get_string("key") == "2")

	-- from_table with non-string keys (supported)
	local values = meta:to_table()
	values.fields["new"] = 420
	meta:from_table(values)
	assert(meta:get_int("new") == 420)
	values.fields["new"] = nil
	meta:from_table(values)
	assert(meta:get("new") == nil)
end


local storage_a = core.get_mod_storage()
local storage_b = core.get_mod_storage()
local function test_mod_storage()
	assert(rawequal(storage_a, storage_b))
	test_metadata(storage_a)
end
unittests.register("test_mod_storage", test_mod_storage)

local function test_item_metadata()
	local meta = ItemStack("unittest:coal_lump"):get_meta()
	test_metadata(meta)
	test_metadata_compat(meta)
end
unittests.register("test_item_metadata", test_item_metadata)

local function test_node_metadata(player, pos)
	test_metadata(core.get_meta(pos))
end
unittests.register("test_node_metadata", test_node_metadata, {map=true})
