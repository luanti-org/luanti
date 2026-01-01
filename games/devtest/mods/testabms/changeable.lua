local abm_first_run = false

core.register_chatcommand("changeableABM", {
	params = "enable | disable | interval_1 | interval_5",
	description = "Active/Deactivate changeable ABM.",
	func = function(name, param)
		if param == "enable" then
			minetest.override_abm("testchangeableabm:abm", {
					interval = 30.0,
					chance = 1,
				})
			abm_first_run = true
			return true, "ABM has been enabled."
		elseif param == "interval_1" then
			minetest.override_abm("testchangeableabm:abm", {
					interval = 1.0,
					chance = 1,
				})
			abm_first_run = true
				return true, "ABM has been enabled with interval 1."
		elseif param == "interval_5" then
			minetest.override_abm("testchangeableabm:abm", {
					interval = 5.0,
					chance = 1,
				})
			abm_first_run = true
				return true, "ABM has been enabled with interval 5."
		elseif param == "disable" then
			minetest.override_abm("testchangeableabm:abm", {
					chance = 0,
				})
			return true, "ABM has been disabled."
		else
			return false, "Check /help changeableABB for allowed parameters."
		end
	end,
})

core.register_abm({
	name = "testchangeableabm:abm",
	nodenames = "basenodes:stone",
	interval = 30.0,
	chance = 0, -- as default, ABM is disabled
	action = function(pos)
		if abm_first_run then
			core.chat_send_all("Changeable ABM runs first time after enable.")
			abm_first_run = false
		end
	end,
})
