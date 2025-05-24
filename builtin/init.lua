--
-- This file contains built-in stuff in Luanti implemented in Lua.
--
-- It is always loaded and executed after registration of the C API,
-- before loading and running any mods.
--

-- Initialize some very basic things
function core.error_handler(err, level)
	return debug.traceback(tostring(err), level)
end
do
	local function concat_args(...)
		local n, t = select("#", ...), {...}
		for i = 1, n do
			t[i] = tostring(t[i])
		end
		return table.concat(t, "\t")
	end
	function core.debug(...) core.log(concat_args(...)) end
	if core.print then
		local core_print = core.print
		-- Override native print and use
		-- terminal if that's turned on
		function print(...) core_print(concat_args(...)) end
		core.print = nil -- don't pollute our namespace
	end
end

do
	-- Note that PUC Lua just calls srand() which is already initialized by C++,
	-- but we don't want to rely on this implementation detail.
	local seed = 1048576 * (os.time() % 1048576)
	seed = seed + core.get_us_time() % 1048576
	math.randomseed(seed)
end

minetest = core

-- Load other files
local scriptdir = core.get_builtin_path()
local commonpath = scriptdir .. "common" .. DIR_DELIM
local asyncpath = scriptdir .. "async" .. DIR_DELIM

dofile(commonpath .. "math.lua")
dofile(commonpath .. "vector.lua")
dofile(commonpath .. "strict.lua")
dofile(commonpath .. "serialize.lua")
dofile(commonpath .. "misc_helpers.lua")

if INIT == "game" then
	local function mod_loader(module_name)
		local parts = module_name:split(".")
		local modname = parts[1]
		local modpath = core.get_modpath(modname)
		if not modpath then
			return "no mod called " .. modname
		end
		parts[1] = modpath
		local base_path = table.concat(parts, "/")
		local errors = {}
		for _, suffix in ipairs({"/init.lua", #parts > 1 and ".lua" or nil}) do
			local source_path = base_path .. suffix
			local f, err = io.open(source_path, "r")
			if f then
				local source = f:read("*all")
				local chunk, load_err = loadstring(source, source_path)
				f:close()
				assert(chunk, load_err)
				return chunk
			else
				table.insert(errors, err)
			end
		end
		return table.concat(errors, "\n")
	end

	local function preprocess_module_name(module_name)
		if module_name:sub(1, 1) == "." then
			module_name = assert(core.get_current_modname()) .. module_name
		end
		assert(module_name:find("^[A-Za-z0-9_%.]+$") and
				module_name:find("%.%.") == nil and
				module_name:sub(-1) ~= ".",
				"invalid module name")
		return module_name
	end

	local loaded = {} -- [module_name] = function() return module end
	package = {
		loaders = {mod_loader},
		unload = function(module_name)
			loaded[preprocess_module_name(module_name)] = nil
		end,
		set = function(module_name, module)
			loaded[preprocess_module_name(module_name)] = function() return module end
		end,
	}
	function require(module_name)
		module_name = preprocess_module_name(module_name)
		local module_func = loaded[module_name]
		if module_func then
			return module_func()
		end
		local errors = {}
		for _, loader in ipairs(package.loaders) do
			local res = loader(module_name)
			if type(res) == "function" then
				local module = res()
				package.set(module_name, module)
				return module
			end
			table.insert(errors, res)
		end
		error("failed to load module '" .. module_name .. '":\n' .. table.concat(errors, "\n"))
	end

	dofile(scriptdir .. "game" .. DIR_DELIM .. "init.lua")
	assert(not core.get_http_api)
elseif INIT == "mainmenu" then
	local mm_script = core.settings:get("main_menu_script")
	local custom_loaded = false
	if mm_script and mm_script ~= "" then
		local testfile = io.open(mm_script, "r")
		if testfile then
			testfile:close()
			dofile(mm_script)
			custom_loaded = true
			core.log("info", "Loaded custom main menu script: "..mm_script)
		else
			core.log("error", "Failed to load custom main menu script: "..mm_script)
			core.log("info", "Falling back to default main menu script")
		end
	end
	if not custom_loaded then
		dofile(core.get_mainmenu_path() .. DIR_DELIM .. "init.lua")
	end
elseif INIT == "async"  then
	dofile(asyncpath .. "mainmenu.lua")
elseif INIT == "async_game" then
	dofile(commonpath .. "metatable.lua")
	dofile(asyncpath .. "game.lua")
elseif INIT == "client" then
	dofile(scriptdir .. "client" .. DIR_DELIM .. "init.lua")
elseif INIT == "emerge" then
	dofile(scriptdir .. "emerge" .. DIR_DELIM .. "init.lua")
elseif INIT == "pause_menu" then
	dofile(scriptdir .. "pause_menu" .. DIR_DELIM .. "init.lua")
else
	error(("Unrecognized builtin initialization type %s!"):format(tostring(INIT)))
end
