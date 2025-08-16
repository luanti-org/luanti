unused_args = false
allow_defined_top = true

ignore = {
	"131", -- Unused global variable
	"431", -- Shadowing an upvalue
	"432", -- Shadowing an upvalue argument
}

read_globals = {
	"ItemStack",
	"INIT",
	"PLATFORM",
	"DIR_DELIM",
	"dump", "dump2",
	"fgettext", "fgettext_ne",
	"vector",
	"VoxelArea",
	"VoxelManip",
	"profiler",
	"Settings",
	"ValueNoise", "ValueNoiseMap",
	"tracy",

	string = {fields = {"split", "trim"}},
	table  = {fields = {"copy", "copy_with_metatables", "getn", "indexof", "keyof", "insert_all"}},
	math   = {fields = {"hypot", "round"}},
}

globals = {
	"core",
	"gamedata",
	os = { fields = { "tempfolder" } },
	"_",
}

stds.menu_common = {
	globals = {
		"mt_color_grey", "mt_color_blue", "mt_color_lightblue", "mt_color_green",
		"mt_color_dark_green", "mt_color_orange", "mt_color_red",
	},
}

files["builtin/client/register.lua"] = {
	globals = {
		debug = {fields={"getinfo"}},
	}
}

files["builtin/common/math.lua"] = {
	globals = {
		"math",
	},
}

files["builtin/common/misc_helpers.lua"] = {
	globals = {
		"dump", "dump2", "table", "math", "string",
		"fgettext", "fgettext_ne", "basic_dump", "game", -- ???
		"file_exists", "get_last_folder", "cleanup_path", -- ???
	},
}

files["builtin/common/vector.lua"] = {
	globals = { "vector", "math" },
}

files["builtin/game/voxelarea.lua"] = {
	globals = { "VoxelArea" },
}

files["builtin/game/init.lua"] = {
	globals = { "profiler" },
}

files["builtin/common/filterlist.lua"] = {
	globals = {
		"filterlist",
		"compare_worlds", "sort_worlds_alphabetic", "sort_mod_list", -- ???
	},
}

files["builtin/mainmenu"] = {
	std = "+menu_common",
	globals = {
		"gamedata",
	},
}

files["builtin/common/settings"] = {
	std = "+menu_common",
}

files["builtin/common/tests"] = {
	read_globals = {
		"describe",
		"it",
		"assert",
	},
}
