--
-- HP Change Reasons
--
local expect = nil
core.register_on_player_hpchange(function(player, hp_change, reason)
	if expect == nil then
		return
	end

	for key, value in pairs(reason) do
		assert(expect[key] == value)
	end
	for key, value in pairs(expect) do
		assert(reason[key] == value)
	end

	expect = nil
end)

local function run_hpchangereason_tests(player)
	local old_hp = player:get_hp()

	player:set_hp(20)
	expect = { type = "set_hp", from = "mod" }
	player:set_hp(3)
	assert(expect == nil)

	expect = { a = 234, type = "set_hp", from = "mod" }
	player:set_hp(7, { a= 234 })
	assert(expect == nil)

	expect = { df = 3458973454, type = "fall", from = "mod" }
	player:set_hp(10, { type = "fall", df = 3458973454 })
	assert(expect == nil)

	player:set_hp(old_hp)
end
unittests.register("test_hpchangereason", run_hpchangereason_tests, {player=true})

--
-- HP differences
--

local expected_diff = nil
local hpchange_counter = 0
local die_counter = 0
core.register_on_player_hpchange(function(player, hp_change, reason)
	if expected_diff then
		assert(hp_change == expected_diff)
		hpchange_counter = hpchange_counter + 1
	end
end)
core.register_on_dieplayer(function()
	die_counter = die_counter + 1
end)

local function hp_diference_test(player, hp_max)
	assert(hp_max >= 22)

	local old_hp = player:get_hp()
	local old_hp_max = player:get_properties().hp_max

	hpchange_counter = 0
	die_counter = 0

	expected_diff = nil
	player:set_properties({hp_max = hp_max})
	player:set_hp(22)
	assert(player:get_hp() == 22)
	assert(hpchange_counter == 0)
	assert(die_counter == 0)

	-- HP difference is not clamped
	expected_diff = -25
	player:set_hp(-3)
	-- actual final HP value is clamped to >= 0
	assert(player:get_hp() == 0)
	assert(hpchange_counter == 1)
	assert(die_counter == 1)

	expected_diff = 22
	player:set_hp(22)
	assert(player:get_hp() == 22)
	assert(hpchange_counter == 2)
	assert(die_counter == 1)

	-- Integer overflow is prevented
	-- so result is S32_MIN, not S32_MIN - 22
	expected_diff = -2147483648
	player:set_hp(-2147483648)
	-- actual final HP value is clamped to >= 0
	assert(player:get_hp() == 0)
	assert(hpchange_counter == 3)
	assert(die_counter == 2)

	-- Damage is ignored if player is already dead (hp == 0)
	expected_diff = "never equal"
	player:set_hp(-11)
	assert(player:get_hp() == 0)
	-- no on_player_hpchange or on_dieplayer call expected
	assert(hpchange_counter == 3)
	assert(die_counter == 2)

	expected_diff = 11
	player:set_hp(11)
	assert(player:get_hp() == 11)
	assert(hpchange_counter == 4)
	assert(die_counter == 2)

	-- HP difference is not clamped
	expected_diff = 1000000 - 11
	player:set_hp(1000000)
	-- actual final HP value is clamped to <= hp_max
	assert(player:get_hp() == hp_max)
	assert(hpchange_counter == 5)
	assert(die_counter == 2)

	-- "Healing" is not ignored when hp == hp_max
	expected_diff = 80000 - hp_max
	player:set_hp(80000)
	assert(player:get_hp() == hp_max)
	-- on_player_hpchange_call expected
	assert(hpchange_counter == 6)
	assert(die_counter == 2)

	expected_diff = nil
	player:set_properties({hp_max = old_hp_max})
	player:set_hp(old_hp)
	core.close_formspec(player:get_player_name(), "") -- hide death screen
end
local function run_hp_difference_tests(player)
	hp_diference_test(player, 22)
	hp_diference_test(player, 30)
	hp_diference_test(player, 65535) -- U16_MAX
end
unittests.register("test_hp_difference", run_hp_difference_tests, {player=true})

--
-- Player meta
--
local function run_player_meta_tests(player)
	local meta = player:get_meta()
	meta:set_string("foo", "bar")
	assert(meta:contains("foo"))
	assert(meta:get_string("foo") == "bar")
	assert(meta:get("foo") == "bar")

	local meta2 = player:get_meta()
	assert(meta2:get_string("foo") == "bar")
	assert(meta2:get("foo") == "bar")
	assert(meta:equals(meta2))

	meta:set_string("bob", "dillan")
	assert(meta:get_string("foo") == "bar")
	assert(meta:get_string("bob") == "dillan")
	assert(meta:get("bob") == "dillan")
	assert(meta2:get_string("foo") == "bar")
	assert(meta2:get_string("bob") == "dillan")
	assert(meta2:get("bob") == "dillan")
	assert(meta:equals(meta2))

	meta:set_string("foo", "")
	assert(not meta:contains("foo"))
	assert(meta:get("foo") == nil)
	assert(meta:get_string("foo") == "")
	assert(meta:equals(meta2))
end
unittests.register("test_player_meta", run_player_meta_tests, {player=true})

--
-- Player add pos
--
local function run_player_add_pos_tests(player)
	local pos = player:get_pos()
	player:add_pos(vector.new(0, 1000, 0))
	local newpos = player:get_pos()
	player:add_pos(vector.new(0, -1000, 0))
	local backpos = player:get_pos()
	local newdist = vector.distance(pos, newpos)
	assert(math.abs(newdist - 1000) <= 1)
	assert(vector.distance(pos, backpos) <= 1)
end
unittests.register("test_player_add_pos", run_player_add_pos_tests, {player=true})

--
-- Hotbar selection clamp
--
local function run_player_hotbar_clamp_tests(player)
	local inv = player:get_inventory()
	local old_inv_size = inv:get_size("main")
	local old_inv_list = inv:get_list("main") -- Avoid accidentally removing item
	local old_bar_size = player:hud_get_hotbar_itemcount()

	inv:set_size("main", 5)

	player:hud_set_hotbar_itemcount(2)
	assert(player:hud_get_hotbar_itemcount() == 2)

	player:hud_set_hotbar_itemcount(6)
	assert(player:hud_get_hotbar_itemcount() == 5)

	inv:set_size("main", old_inv_size)
	inv:set_list("main", old_inv_list)
	player:hud_set_hotbar_itemcount(old_bar_size)
end
unittests.register("test_player_hotbar_clamp", run_player_hotbar_clamp_tests, {player=true})
