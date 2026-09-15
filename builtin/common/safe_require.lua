-- Safe require with a custom loader

local is_main_mod_env = INIT == "game"

-- If mod security is disabled, there may be an existing package table.
package = package or {}
package.loaders = package.loaders or {}
package.loaded = {} -- [module_name] = module

local function mod_loader(module_name)
	local parts = module_name:split(".")
	local modname = parts[1]
	local modpath = core.get_modpath(modname)
	if not modpath then
		return "no mod called " .. modname
	end
	if is_main_mod_env and (modname ~= core.get_current_modname() and package.loaded[modname] == nil) then
		-- loadMod() in C++ always sets package.loaded[modname].
		-- If it has not been set, it has not been loaded yet.
		-- This does not apply to async / emerge envs, where package.loaded starts out empty,
		-- and we might still want to require some pure library mods.
		return "missing dependency on mod " .. modname .. " in mod.conf"
	end
	local base_path = table.concat({modpath, unpack(parts, 2)}, "/")
	local suffixes = {"/init.lua"}
	if #parts >= 2 then -- also try modpath/.../module.lua
		table.insert(suffixes, ".lua")
	end
	local errors = {}
	for _, suffix in ipairs(suffixes) do
		local file_path = base_path .. suffix
		local file, err = io.open(file_path, "r")
		if file then
			local contents, read_err = file:read("*a")
			file:close()
			if not contents then
				return read_err
			end
			if contents:sub(1, 1) == "#" then
				-- skip shebang, keep line ending
				--- c.f. ScriptApiSecurity::safeLoadFileContent()
				contents = contents:match("^[^\r\n]+(.*)")
			end
			local chunk, load_err = loadstring(contents, "@" .. file_path)
			-- If loading the file succeeded, return the chunk as module loader;
			-- if loading the *existing* file failed, return the error,
			-- don't silence syntax errors by trying other files
			-- (this is why we prefer not to use loadfile() here: it conflates the two kinds of errors)
			return chunk or load_err
		else
			table.insert(errors, err)
		end
	end
	return table.concat(errors, "\n")
end

table.insert(package.loaders, 1, mod_loader)

local function is_valid_module_name(module_name)
	return module_name:find("^[A-Za-z0-9_%.]+$") and
			module_name:find("%.%.") == nil and
			module_name:sub(1, 1) ~= "." and
			module_name:sub(-1) ~= "."
end

local loading = {} -- unique object to mark module as loading

function require(module_name)
	assert(is_valid_module_name(module_name),
			"module names must consist of alphanumeric names separated by dots")
	-- c.f. ll_require() in loadlib.c (PUC Lua)
	do
		local module = package.loaded[module_name]
		if module == loading then
			error("failed to load module '" .. module_name .. "': cyclic require()")
		end
		if module ~= nil then
			return module
		end
	end

	local errors = {}
	for _, loader in ipairs(package.loaders) do
		local res = loader(module_name)
		if type(res) == "function" then
			package.loaded[module_name] = loading
			local module = res()
			if module == nil then
				module = true
			end
			package.loaded[module_name] = module
			return module
		end
		-- Loaders MUST return a function, string or nil per the reference manual.
		-- It is useful to produce a proper error message on invalid types:
		-- A loader may mistakenly be directly returning a module table.
		assert(type(res) == "string" or res == nil, "loaders must return function, string or nil")
		table.insert(errors, res)
	end
	error("failed to load module '" .. module_name .. "':\n" .. table.concat(errors, '\n'))
end
