--
-- Builtin authentication handler
--

-- Make the auth object private, deny access to mods
local core_auth = core.auth
core.auth = nil

core.builtin_auth_handler = {
	get_auth = function(name)
		assert(type(name) == "string")
		local auth_entry = core_auth.read(name)
		-- If no such auth found, return nil
		if not auth_entry then
			return nil
		end
		-- Figure out what privileges the player should have.
		-- Take a copy of the privilege table
		local privileges = {}
		for priv, _ in pairs(auth_entry.privileges) do
			privileges[priv] = true
		end
		-- If singleplayer, give all privileges except those marked as give_to_singleplayer = false
		if core.is_singleplayer() then
			for priv, def in pairs(core.registered_privileges) do
				if def.give_to_singleplayer then
					privileges[priv] = true
				end
			end
		-- For the admin, give everything
		elseif name == core.settings:get("name") then
			for priv, def in pairs(core.registered_privileges) do
				if def.give_to_admin then
					privileges[priv] = true
				end
			end
		end
		-- All done
		return {
			password = auth_entry.password,
			privileges = privileges,
			last_login = auth_entry.last_login,
		}
	end,
	create_auth = function(name, password)
		assert(type(name) == "string")
		assert(type(password) == "string")
		core.log('info', "Built-in authentication handler adding player '"..name.."'")
		return core_auth.create({
			name = name,
			password = password,
			privileges = core.string_to_privs(core.settings:get("default_privs")),
			last_login = -1,  -- Defer login time calculation until record_login (called by on_joinplayer)
		})
	end,
	delete_auth = function(name)
		assert(type(name) == "string")
		local auth_entry = core_auth.read(name)
		if not auth_entry then
			return false
		end
		core.log('info', "Built-in authentication handler deleting player '"..name.."'")
		return core_auth.delete(name)
	end,
	set_password = function(name, password)
		assert(type(name) == "string")
		assert(type(password) == "string")
		local auth_entry = core_auth.read(name)
		if not auth_entry then
			core.builtin_auth_handler.create_auth(name, password)
		else
			core.log('info', "Built-in authentication handler setting password of player '"..name.."'")
			auth_entry.password = password
			core_auth.save(auth_entry)
		end
		return true
	end,
	set_privileges = function(name, privileges)
		assert(type(name) == "string")
		assert(type(privileges) == "table")
		local auth_entry = core_auth.read(name)
		if not auth_entry then
			auth_entry = core.builtin_auth_handler.create_auth(name,
				core.get_password_hash(name,
					core.settings:get("default_password")))
		end

		local prev_privs = auth_entry.privileges
		auth_entry.privileges = privileges

		core_auth.save(auth_entry)

		for priv, value in pairs(privileges) do
			-- Warnings for improper API usage
			if value == false then
				core.log('deprecated', "`false` value given to `core.set_player_privs`, "..
						"this is almost certainly a bug, "..
						"granting a privilege rather than revoking it")
			elseif value ~= true then
				core.log('deprecated', "non-`true` value given to `core.set_player_privs`")
			end
			-- Run grant callbacks
			if prev_privs[priv] == nil then
				core.run_priv_callbacks(name, priv, nil, "grant")
			end
		end

		-- Run revoke callbacks
		for priv, _ in pairs(prev_privs) do
			if privileges[priv] == nil then
				core.run_priv_callbacks(name, priv, nil, "revoke")
			end
		end
		core.notify_authentication_modified(name)
	end,
	reload = function()
		core_auth.reload()
		return true
	end,
	record_login = function(name)
		assert(type(name) == "string")
		local auth_entry = core_auth.read(name)
		assert(auth_entry)
		auth_entry.last_login = os.time()
		core_auth.save(auth_entry)
	end,
	iterate = function()
		local names = {}
		local nameslist = core_auth.list_names()
		for k,v in pairs(nameslist) do
			names[v] = true
		end
		return pairs(names)
	end,
}

core.register_on_prejoinplayer(function(name, ip)
	if core.registered_auth_handler ~= nil then
		return -- Don't do anything if custom auth handler registered
	end
	local auth_entry = core_auth.read(name)
	if auth_entry ~= nil then
		return
	end

	local name_lower = name:lower()
	for k in core.builtin_auth_handler.iterate() do
		if k:lower() == name_lower then
			return string.format("\nCannot create new player called '%s'. "..
					"Another account called '%s' is already registered. "..
					"Please check the spelling if it's your account "..
					"or use a different nickname.", name, k)
		end
	end
end)

--
-- Authentication API
--

function core.register_authentication_handler(handler)
	if core.registered_auth_handler then
		error("Add-on authentication handler already registered by "..core.registered_auth_handler_modname)
	end
	core.registered_auth_handler = handler
	core.registered_auth_handler_modname = core.get_current_modname()
	handler.mod_origin = core.registered_auth_handler_modname
end

function core.get_auth_handler()
	return core.registered_auth_handler or core.builtin_auth_handler
end

local function auth_pass(name)
	return function(...)
		local auth_handler = core.get_auth_handler()
		if auth_handler[name] then
			return auth_handler[name](...)
		end
		return false
	end
end

core.set_player_password = auth_pass("set_password")
core.set_player_privs    = auth_pass("set_privileges")
core.remove_player_auth  = auth_pass("delete_auth")
core.auth_reload         = auth_pass("reload")

function core.change_player_privs(name, changes)
	local privs = core.get_player_privs(name)
	for priv, change in pairs(changes) do
		if change == true then
			privs[priv] = true
		elseif change == false then
			privs[priv] = nil
		else
			error("non-bool value given to `core.change_player_privs`")
		end
	end
	core.set_player_privs(name, privs)
end

local record_login = auth_pass("record_login")
core.register_on_joinplayer(function(player)
	record_login(player:get_player_name())
end)
