local function my_dofile(rel_path)
	return assert(loadfile(core.get_modpath("test_sscsm") .. rel_path))()
end

assert(my_dofile("common/foo_renamed.lua") == "foo")
assert(my_dofile("client/bar.lua") == "bar")
core.after(0, function()
	print("test_sscsm: hello world!")
end)
