unused_args = false
allow_defined_top = true
max_string_line_length = false
max_line_length = false

ignore = {
	"131", -- Unused global variable
	"211", -- Unused local variable
	"231", -- Local variable never accessed
	"311", -- Value assigned to a local variable is unused
	"412", -- Redefining an argument
	"421", -- Shadowing a local variable
	"431", -- Shadowing an upvalue
	"432", -- Shadowing an upvalue argument
	"611", -- Line contains only whitespace
}

read_globals = {
	"ItemStack",
	"INIT",
	"DIR_DELIM",
	"dump", "dump2",
	"fgettext", "fgettext_ne",
	"vector",
	"VoxelArea",
	"VoxelManip",
	"profiler",
	"Settings",
	"check",
	"PseudoRandom",
	"PcgRandom",

	string = {fields = {"split", "trim"}},
	table  = {fields = {"copy", "getn", "indexof", "insert_all", "key_value_swap"}},
	math   = {fields = {"hypot", "round"}},
}

globals = {
	"aborted",
	"minetest",
	"core",
	os = { fields = { "tempfolder" } },
	"_",
}

