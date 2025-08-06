local log = {}

local function insert_log(...)
	log[#log+1] = string.format(...)
end

local function objref_str(self, ref)
	if ref and ref:is_player() then
		return "player"
	end
	return self.object == ref and "self" or tostring(ref)
end

core.register_entity("unittests:callbacks", {
	initial_properties = {
		hp_max = 5,
		visual = "upright_sprite",
		textures = { "unittests_callback.png" },
		static_save = false,
	},

	on_activate = function(self, staticdata, dtime_s)
		self.object:set_armor_groups({test = 100})
		assert(self.object:get_hp() == self.initial_properties.hp_max)
		insert_log("on_activate(%d)", #staticdata)
	end,
	on_deactivate = function(self, removal)
		insert_log("on_deactivate(%s)", tostring(removal))
	end,
	on_punch = function(self, puncher, time_from_last_punch, tool_capabilities, dir, damage)
		insert_log("on_punch(%s, %.1f, %d)", objref_str(self, puncher),
			time_from_last_punch, damage)
	end,
	on_death = function(self, killer)
		assert(self.object:get_hp() == 0)
		insert_log("on_death(%s)", objref_str(self, killer))
	end,
	on_rightclick = function(self, clicker)
		insert_log("on_rightclick(%s)", objref_str(self, clicker))
	end,
	on_attach_child = function(self, child)
		insert_log("on_attach_child(%s)", objref_str(self, child))
		assert(child:get_attach() == self.object)
		local ok = false
		for _, obj in ipairs(self.object:get_children()) do
			if obj == child then
				ok = true
			end
		end
		assert(ok, "Child not found in get_children")
	end,
	on_detach_child = function(self, child)
		insert_log("on_detach_child(%s)", objref_str(self, child))
		assert(child:get_attach() == nil)
		local ok = true
		for _, obj in ipairs(self.object:get_children()) do
			if obj == child then
				ok = false
			end
		end
		assert(ok, "Former child found in get_children")
	end,
	on_detach = function(self, parent)
		insert_log("on_detach(%s)", objref_str(self, parent))
		assert(self.object:get_attach() == nil)
		local ok = true
		for _, obj in ipairs(parent:get_children()) do
			if obj == self.object then
				ok = false
			end
		end
		assert(ok, "Former child found in get_children")
	end,
	get_staticdata = function(self)
		assert(false)
	end,
})

--

local function check_log(expect)
	if #expect ~= #log then
		error("Log mismatch: " .. core.write_json(log))
	end
	for i, s in ipairs(expect) do
		if log[i] ~= s then
			error("Log mismatch at " .. i .. ": " .. core.write_json(log))
		end
	end
	log = {} -- clear it for next time
end

local function test_entity_lifecycle(_, pos)
	log = {}

	-- with binary in staticdata
	local obj = core.add_entity(pos, "unittests:callbacks", "abc\000def")
	assert(obj and obj:is_valid())
	check_log({"on_activate(7)"})

	obj:set_hp(0)
	check_log({"on_death(nil)", "on_deactivate(true)"})

	assert(not obj:is_valid())
end
unittests.register("test_entity_lifecycle", test_entity_lifecycle, {map=true})

local function test_entity_interact(_, pos)
	log = {}

	local obj = core.add_entity(pos, "unittests:callbacks")
	check_log({"on_activate(0)"})

	-- rightclick
	obj:right_click(obj)
	check_log({"on_rightclick(self)"})

	-- useless punch
	obj:punch(obj, 0.5, {})
	check_log({"on_punch(self, 0.5, 0)"})

	-- fatal punch
	obj:punch(obj, 1.9, {
		full_punch_interval = 1.0,
		damage_groups = { test = 10 },
	})
	check_log({
		-- does 10 damage even though we only have 5 hp
		"on_punch(self, 1.9, 10)",
		"on_death(self)",
		"on_deactivate(true)"
	})
end
unittests.register("test_entity_interact", test_entity_interact, {map=true})

local function test_entity_attach(player, pos)
	log = {}

	local obj = core.add_entity(pos, "unittests:callbacks")
	check_log({"on_activate(0)"})

	-- attach player to entity
	player:set_attach(obj)
	check_log({"on_attach_child(player)"})
	assert(player:get_attach() == obj)
	player:set_detach()
	check_log({"on_detach_child(player)"})
	assert(player:get_attach() == nil)

	-- attach entity to player
	obj:set_attach(player)
	check_log({})
	assert(obj:get_attach() == player)
	obj:set_detach()
	check_log({"on_detach(player)"})
	assert(obj:get_attach() == nil)

	obj:remove()
end
unittests.register("test_entity_attach", test_entity_attach, {player=true, map=true})

---------

core.register_entity("unittests:dummy", {
	initial_properties = {
		hp_max = 1,
		visual = "upright_sprite",
		textures = { "no_texture.png" },
		static_save = false,
	},
})

local function test_entity_raycast(_, pos)
	local obj1 = core.add_entity(pos, "unittests:dummy")
	local obj2 = core.add_entity(pos:offset(1, 0, 0), "unittests:dummy")
	local raycast = core.raycast(pos:offset(-1, 0, 0), pos:offset(2, 0, 0), true, false)
	for pt in raycast do
		if pt.type == "object" then
			assert(pt.ref == obj1)
			obj1:remove()
			obj2:remove()
			obj1 = nil -- object should be hit exactly one
		end
	end
	assert(obj1 == nil)
end
unittests.register("test_entity_raycast", test_entity_raycast, {map=true})

local function test_object_iterator(pos, make_iterator)
	local obj1 = core.add_entity(pos, "unittests:dummy")
	local obj2 = core.add_entity(pos, "unittests:dummy")
	assert(obj1 and obj2)
	local found = false
	-- As soon as we find one of the objects, we remove both, invalidating the other.
	for obj in make_iterator() do
		assert(obj:is_valid())
		if obj == obj1 or obj == obj2 then
			obj1:remove()
			obj2:remove()
			found = true
		end
	end
	assert(found)
end

unittests.register("test_objects_inside_radius", function(_, pos)
	test_object_iterator(pos, function()
		return core.objects_inside_radius(pos, 1)
	end)
end, {map=true})

unittests.register("test_objects_in_area", function(_, pos)
	test_object_iterator(pos, function()
		return core.objects_in_area(pos:offset(-1, -1, -1), pos:offset(1, 1, 1))
	end)
end, {map=true})

-- Tests that bone rotation euler angles are preserved (see #14992)
local function test_get_bone_rot(_, pos)
	local obj = core.add_entity(pos, "unittests:dummy")
	for _ = 1, 100 do
		local function assert_similar(euler_angles)
			local _, rot = obj:get_bone_position("bonename")
			assert(euler_angles:distance(rot) < 1e-3)
			local override = obj:get_bone_override("bonename")
			assert(euler_angles:distance(override.rotation.vec:apply(math.deg)) < 1e-3)
		end
		local deg = 1e3 * vector.new(math.random(), math.random(), math.random())
		obj:set_bone_position("bonename", vector.zero(), deg)
		assert_similar(deg)
		local rad = 3 * math.pi * vector.new(math.random(), math.random(), math.random())
		obj:set_bone_override("bonename", {rotation = {vec = rad}})
		assert_similar(rad:apply(math.deg))
	end
end
unittests.register("test_get_bone_rot", test_get_bone_rot, {map=true})

---------

-- Spawn an entity from an ItemStack
local function test_item_drop(_, pos)
	local itemstack_src, itemstack_ret, obj

	-- Try to place something that does not exist (placement fails)
	itemstack_src = ItemStack("n_np_solution 1")
	itemstack_ret, obj = core.item_drop(itemstack_src, nil, pos)
	assert(obj == nil)
	assert(itemstack_ret == nil)

	-- Test known item (placement successful)
	itemstack_src = ItemStack("testnodes:normal 69")
	itemstack_ret, obj = core.item_drop(itemstack_src, nil, pos)
	assert(obj:get_hp() ~= nil)
	assert(itemstack_ret and itemstack_ret:is_empty())
	assert(itemstack_ret:equals(itemstack_src))
end
unittests.register("test_item_drop", test_item_drop, {map=true})

local function test_entity_guid(_, pos)
	local obj0 = core.add_entity(pos, "unittests:dummy")
	local obj1 = core.add_entity(pos, "unittests:dummy")

	assert(obj0 ~= obj1)
	assert(obj0:get_guid() ~= obj1:get_guid())
	assert(core.objects_by_guid[obj0:get_guid()] == obj0)
	assert(core.objects_by_guid[obj1:get_guid()] == obj1)

	obj0:remove()
	obj1:remove()
end
unittests.register("test_entity_guid", test_entity_guid, {map=true})
