core.register_sscsm({
	init_path = "client/init.lua",
	paths = {
		client = true,
		["common/foo_renamed.lua"] = "common/foo.lua",
	},
})

assert(assert(loadfile(core.get_modpath("test_sscsm") .. "/common/foo.lua"))() == "foo")
