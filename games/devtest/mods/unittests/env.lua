-- Object Passing / Serialization

local abm_action1 = function ()
end
local abm_action2 = function ()
end

core.register_abm({
	name = "unittests:abm",
	nodenames = "testnodes:stone",
	interval = 1,
	chance = 0,
	min_y = -10,
	max_y = 20,
	action = abm_action1
})

local abm_index = #core.registered_abms

local function test_abm_override()
	core.override_abm("unittests:abm", {
			interval = 500,
			chance = 25,
			min_y = -100,
			max_y = 200,
			action = abm_action2
		})
	assert(core.registered_abms[abm_index].interval == 500)
	assert(core.registered_abms[abm_index].chance == 25)
	assert(core.registered_abms[abm_index].min_y == -100)
	assert(core.registered_abms[abm_index].max_y == 200)
	assert(core.registered_abms[abm_index].action == abm_action2)

	core.override_abm("unittests:abm", {
			interval = 1,
			chance = 0,
			min_y = -10,
			max_y = 20,
			action = abm_action1
		})
	assert(core.registered_abms[abm_index].interval == 1)
	assert(core.registered_abms[abm_index].chance == 0)
	assert(core.registered_abms[abm_index].min_y == -10)
	assert(core.registered_abms[abm_index].max_y == 20)
	assert(core.registered_abms[abm_index].action == abm_action1)
end
unittests.register("test_abm_override", test_abm_override)
