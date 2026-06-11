core.register_sscsm({
	init_path = "client/init.lua",
	paths = {
		client = true,
		common = true,
	},
})

assert(assert(loadfile(core.get_modpath("sscsm") .. "/common/foo.lua"))() == "foo")
