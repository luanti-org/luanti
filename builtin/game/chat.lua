local S = core.get_translator("__builtin")

-- Helper function that implements search and replace without pattern matching
-- Returns the string and a boolean indicating whether or not the string was modified
local function safe_gsub(s, replace, with)
	local i1, i2 = s:find(replace, 1, true)
	if not i1 then
		return s, false
	end

	return s:sub(1, i1 - 1) .. with .. s:sub(i2 + 1), true
end

--
-- Chat message formatter
--

-- Implemented in Lua to allow redefinition
function core.format_chat_message(name, message)
	local error_str = "Invalid chat message format - missing %s"
	local str = core.settings:get("chat_message_format")
	local replaced

	-- Name
	str, replaced = safe_gsub(str, "@name", name)
	if not replaced then
		error(error_str:format("@name"), 2)
	end

	-- Timestamp
	str = safe_gsub(str, "@timestamp", os.date("%H:%M:%S", os.time()))

	-- Insert the message into the string only after finishing all other processing
	str, replaced = safe_gsub(str, "@message", message)
	if not replaced then
		error(error_str:format("@message"), 2)
	end

	return str
end

--
-- Chat command handler
--

core.chatcommands = core.registered_chatcommands -- BACKWARDS COMPATIBILITY

local msg_time_threshold =
	tonumber(core.settings:get("chatcommand_msg_time_threshold")) or 0.1
core.register_on_chat_message(function(name, message)
	if message:sub(1,1) ~= "/" then
		return
	end

	local cmd, param = string.match(message, "^/([^ ]+) *(.*)")
	if not cmd then
		core.chat_send_player(name, "-!- "..S("Empty command."))
		return true
	end

	param = param or ""

	core.log("verbose", string.format("Handling chat command %q with params %q", cmd, param))

	-- Run core.registered_on_chatcommands callbacks.
	if core.run_callbacks(core.registered_on_chatcommands, 5, name, cmd, param) then
		return true
	end

	local cmd_def = core.registered_chatcommands[cmd]
	if not cmd_def then
		core.chat_send_player(name, "-!- "..S("Invalid command: @1", cmd))
		return true
	end
	local has_privs, missing_privs = core.check_player_privs(name, cmd_def.privs)
	if has_privs then
		core.set_last_run_mod(cmd_def.mod_origin)
		local t_before = core.get_us_time()
		local success, result = cmd_def.func(name, param)
		local delay = (core.get_us_time() - t_before) / 1000000
		if success == false and result == nil then
			core.chat_send_player(name, "-!- "..S("Invalid command usage."))
			local help_def = core.registered_chatcommands["help"]
			if help_def then
				local _, helpmsg = help_def.func(name, cmd)
				if helpmsg then
					core.chat_send_player(name, helpmsg)
				end
			end
		else
			if delay > msg_time_threshold then
				-- Show how much time it took to execute the command
				if result then
					result = result .. core.colorize("#f3d2ff", S(" (@1 s)",
						string.format("%.5f", delay)))
				else
					result = core.colorize("#f3d2ff", S(
						"Command execution took @1 s",
						string.format("%.5f", delay)))
				end
			end
			if result then
				core.chat_send_player(name, result)
			end
		end
	else
		core.chat_send_player(name,
				S("You don't have permission to run this command "
				.. "(missing privileges: @1).",
				table.concat(missing_privs, ", ")))
	end
	return true  -- Handled chat message
end)

if core.settings:get_bool("profiler.load") then
	-- Run after register_chatcommand and its register_on_chat_message
	-- Before any chatcommands that should be profiled
	profiler.init_chatcommand()
end

-- Parses a "range" string in the format of "here (number)" or
-- "(x1, y1, z1) (x2, y2, z2)", returning two position vectors
local function parse_range_str(player_name, str)
	local p1, p2
	local args = str:split(" ")

	if args[1] == "here" then
		p1, p2 = core.get_player_radius_area(player_name, tonumber(args[2]))
		if p1 == nil then
			return false, S("Unable to get position of player @1.", player_name)
		end
	else
		local player = core.get_player_by_name(player_name)
		local relpos
		if player then
			relpos = player:get_pos()
		end
		p1, p2 = core.string_to_area(str, relpos)
		if p1 == nil or p2 == nil then
			return false, S("Incorrect area format. "
				.. "Expected: (x1,y1,z1) (x2,y2,z2)")
		end
	end

	return p1, p2
end

--
-- Chat commands
--
core.register_chatcommand("me", {
	params = S("<action>"),
	description = S("Show chat action (e.g., '/me orders a pizza' "
		.. "displays '<player name> orders a pizza')"),
	privs = {shout=true},
	func = function(name, param)
		core.chat_send_all("* " .. name .. " " .. param)
		return true
	end,
})

core.register_chatcommand("admin", {
	description = S("Show the name of the server owner"),
	func = function(name)
		local admin = core.settings:get("name")
		if admin then
			return true, S("The administrator of this server is @1.", admin)
		else
			return false, S("There's no administrator named "
				.. "in the config file.")
		end
	end,
})

local function privileges_of(name, privs)
	if not privs then
		privs = core.get_player_privs(name)
	end
	local privstr = core.privs_to_string(privs, ", ")
	if privstr == "" then
		return S("@1 does not have any privileges.", name)
	else
		return S("Privileges of @1: @2", name, privstr)
	end
end

core.register_chatcommand("privs", {
	params = S("[<name>]"),
	description = S("Show privileges of yourself or another player"),
	func = function(caller, param)
		param = param:trim()
		local name = (param ~= "" and param or caller)
		if not core.player_exists(name) then
			return false, S("Player @1 does not exist.", name)
		end
		return true, privileges_of(name)
	end,
})

core.register_chatcommand("haspriv", {
	params = S("<privilege>"),
	description = S("Return list of all online players with privilege"),
	privs = {basic_privs = true},
	func = function(caller, param)
		param = param:trim()
		if param == "" then
			return false, S("Invalid parameters (see /help haspriv).")
		end
		if not core.registered_privileges[param] then
			return false, S("Unknown privilege!")
		end
		local privs = core.string_to_privs(param)
		local players_with_priv = {}
		for _, player in pairs(core.get_connected_players()) do
			local player_name = player:get_player_name()
			if core.check_player_privs(player_name, privs) then
				table.insert(players_with_priv, player_name)
			end
		end
		if #players_with_priv == 0 then
			return true, S("No online player has the \"@1\" privilege.",
					param)
		else
			table.sort(players_with_priv)
			return true, S("Players online with the \"@1\" privilege: @2",
					param,
					table.concat(players_with_priv, ", "))
		end
	end
})

local function handle_grant_command(caller, grantname, grantprivstr)
	local caller_privs = core.get_player_privs(caller)
	if not (caller_privs.privs or caller_privs.basic_privs) then
		return false, S("Your privileges are insufficient.")
	end

	if not core.get_auth_handler().get_auth(grantname) then
		return false, S("Player @1 does not exist.", grantname)
	end
	local grantprivs = core.string_to_privs(grantprivstr)
	if grantprivstr == "all" then
		grantprivs = core.registered_privileges
	end
	local privs = core.get_player_privs(grantname)
	local privs_unknown = ""
	local basic_privs =
		core.string_to_privs(core.settings:get("basic_privs") or "interact,shout")
	for priv, _ in pairs(grantprivs) do
		if not basic_privs[priv] and not caller_privs.privs then
			return false, S("Your privileges are insufficient. "..
					"'@1' only allows you to grant: @2",
					"basic_privs",
					core.privs_to_string(basic_privs, ', '))
		end
		if not core.registered_privileges[priv] then
			privs_unknown = privs_unknown .. S("Unknown privilege: @1", priv) .. "\n"
		end
		privs[priv] = true
	end
	if privs_unknown ~= "" then
		return false, privs_unknown
	end
	core.set_player_privs(grantname, privs)
	for priv, _ in pairs(grantprivs) do
		-- call the on_grant callbacks
		core.run_priv_callbacks(grantname, priv, caller, "grant")
	end
	core.log("action", caller..' granted ('..core.privs_to_string(grantprivs, ', ')..') privileges to '..grantname)
	if grantname ~= caller then
		core.chat_send_player(grantname,
				S("@1 granted you privileges: @2", caller,
				core.privs_to_string(grantprivs, ', ')))
	end
	return true, privileges_of(grantname)
end

core.register_chatcommand("grant", {
	params = S("<name> (<privilege> [, <privilege2> [<...>]] | all)"),
	description = S("Give privileges to player"),
	func = function(name, param)
		local grantname, grantprivstr = string.match(param, "([^ ]+) (.+)")
		if not grantname or not grantprivstr then
			return false, S("Invalid parameters (see /help grant).")
		end
		return handle_grant_command(name, grantname, grantprivstr)
	end,
})

core.register_chatcommand("grantme", {
	params = S("<privilege> [, <privilege2> [<...>]] | all"),
	description = S("Grant privileges to yourself"),
	func = function(name, param)
		if param == "" then
			return false, S("Invalid parameters (see /help grantme).")
		end
		return handle_grant_command(name, name, param)
	end,
})

local function handle_revoke_command(caller, revokename, revokeprivstr)
	local caller_privs = core.get_player_privs(caller)
	if not (caller_privs.privs or caller_privs.basic_privs) then
		return false, S("Your privileges are insufficient.")
	end

	if not core.get_auth_handler().get_auth(revokename) then
		return false, S("Player @1 does not exist.", revokename)
	end

	local privs = core.get_player_privs(revokename)

	local revokeprivs = core.string_to_privs(revokeprivstr)
	local is_singleplayer = core.is_singleplayer()
	local is_admin = not is_singleplayer
			and revokename == core.settings:get("name")
			and revokename ~= ""
	if revokeprivstr == "all" then
		revokeprivs = table.copy(privs)
	end

	local privs_unknown = ""
	local basic_privs =
		core.string_to_privs(core.settings:get("basic_privs") or "interact,shout")
	local irrevokable = {}
	local has_irrevokable_priv = false
	for priv, _ in pairs(revokeprivs) do
		if not basic_privs[priv] and not caller_privs.privs then
			return false, S("Your privileges are insufficient. "..
					"'@1' only allows you to revoke: @2",
					"basic_privs",
					core.privs_to_string(basic_privs, ', '))
		end
		local def = core.registered_privileges[priv]
		if not def then
			-- Old/removed privileges might still be granted to certain players
			if not privs[priv] then
				privs_unknown = privs_unknown .. S("Unknown privilege: @1", priv) .. "\n"
			end
		elseif is_singleplayer and def.give_to_singleplayer then
			irrevokable[priv] = true
		elseif is_admin and def.give_to_admin then
			irrevokable[priv] = true
		end
	end
	for priv, _ in pairs(irrevokable) do
		revokeprivs[priv] = nil
		has_irrevokable_priv = true
	end
	if privs_unknown ~= "" then
		return false, privs_unknown
	end
	if has_irrevokable_priv then
		if is_singleplayer then
			core.chat_send_player(caller,
					S("Note: Cannot revoke in singleplayer: @1",
					core.privs_to_string(irrevokable, ', ')))
		elseif is_admin then
			core.chat_send_player(caller,
					S("Note: Cannot revoke from admin: @1",
					core.privs_to_string(irrevokable, ', ')))
		end
	end

	local revokecount = 0
	for priv, _ in pairs(revokeprivs) do
		privs[priv] = nil
		revokecount = revokecount + 1
	end

	if revokecount == 0 then
		return false, S("No privileges were revoked.")
	end

	core.set_player_privs(revokename, privs)
	for priv, _ in pairs(revokeprivs) do
		-- call the on_revoke callbacks
		core.run_priv_callbacks(revokename, priv, caller, "revoke")
	end
	local new_privs = core.get_player_privs(revokename)

	core.log("action", caller..' revoked ('
			..core.privs_to_string(revokeprivs, ', ')
			..') privileges from '..revokename)
	if revokename ~= caller then
		core.chat_send_player(revokename,
			S("@1 revoked privileges from you: @2", caller,
			core.privs_to_string(revokeprivs, ', ')))
	end
	return true, privileges_of(revokename, new_privs)
end

core.register_chatcommand("revoke", {
	params = S("<name> (<privilege> [, <privilege2> [<...>]] | all)"),
	description = S("Remove privileges from player"),
	privs = {},
	func = function(name, param)
		local revokename, revokeprivstr = string.match(param, "([^ ]+) (.+)")
		if not revokename or not revokeprivstr then
			return false, S("Invalid parameters (see /help revoke).")
		end
		return handle_revoke_command(name, revokename, revokeprivstr)
	end,
})

core.register_chatcommand("revokeme", {
	params = S("<privilege> [, <privilege2> [<...>]] | all"),
	description = S("Revoke privileges from yourself"),
	privs = {},
	func = function(name, param)
		if param == "" then
			return false, S("Invalid parameters (see /help revokeme).")
		end
		return handle_revoke_command(name, name, param)
	end,
})

core.register_chatcommand("setpassword", {
	params = S("<name> <password>"),
	description = S("Set player's password (sent unencrypted, thus insecure)"),
	privs = {password=true},
	func = function(name, param)
		local toname, raw_password = string.match(param, "^([^ ]+) +(.+)$")
		if not toname then
			toname = param:match("^([^ ]+) *$")
			raw_password = nil
		end

		if not toname then
			return false, S("Name field required.")
		end

		local msg_chat, msg_log, msg_ret
		if not raw_password then
			core.set_player_password(toname, "")
			msg_chat = S("Your password was cleared by @1.", name)
			msg_log = name .. " clears password of " .. toname .. "."
			msg_ret = S("Password of player \"@1\" cleared.", toname)
		else
			core.set_player_password(toname,
					core.get_password_hash(toname,
							raw_password))
			msg_chat = S("Your password was set by @1.", name)
			msg_log = name .. " sets password of " .. toname .. "."
			msg_ret = S("Password of player \"@1\" set.", toname)
		end

		if toname ~= name then
			core.chat_send_player(toname, msg_chat)
		end

		core.log("action", msg_log)

		return true, msg_ret
	end,
})

core.register_chatcommand("clearpassword", {
	params = S("<name>"),
	description = S("Set empty password for a player"),
	privs = {password=true},
	func = function(name, param)
		local toname = param
		if toname == "" then
			return false, S("Name field required.")
		end
		core.set_player_password(toname, '')

		core.log("action", name .. " clears password of " .. toname .. ".")

		return true, S("Password of player \"@1\" cleared.", toname)
	end,
})

core.register_chatcommand("auth_reload", {
	params = "",
	description = S("Reload authentication data"),
	privs = {server=true},
	func = function(name, param)
		local done = core.auth_reload()
		return done, (done and S("Done.") or S("Failed."))
	end,
})

core.register_chatcommand("remove_player", {
	params = S("<name>"),
	description = S("Remove a player's data"),
	privs = {server=true},
	func = function(name, param)
		local toname = param
		if toname == "" then
			return false, S("Name field required.")
		end

		local rc = core.remove_player(toname)

		if rc == 0 then
			core.log("action", name .. " removed player data of " .. toname .. ".")
			return true, S("Player \"@1\" removed.", toname)
		elseif rc == 1 then
			return true, S("No such player \"@1\" to remove.", toname)
		elseif rc == 2 then
			return true, S("Player \"@1\" is connected, cannot remove.", toname)
		end

		return false, S("Unhandled remove_player return code @1.", tostring(rc))
	end,
})


-- pos may be a non-integer position
local function find_free_position_near(pos)
	local tries = {
		vector.new( 1, 0,  0),
		vector.new(-1, 0,  0),
		vector.new( 0, 0,  1),
		vector.new( 0, 0, -1),
	}
	for _, d in ipairs(tries) do
		local p = vector.add(pos, d)
		local n = core.get_node_or_nil(p)
		if n then
			local def = core.registered_nodes[n.name]
			if def and not def.walkable then
				return p
			end
		end
	end
	return pos
end

-- Teleports player <name> to <p> if possible
local function teleport_to_pos(name, p)
	local lm = 31007 -- equals MAX_MAP_GENERATION_LIMIT in C++
	if p.x < -lm or p.x > lm or p.y < -lm or p.y > lm
			or p.z < -lm or p.z > lm then
		return false, S("Cannot teleport out of map bounds!")
	end
	local teleportee = core.get_player_by_name(name)
	if not teleportee then
		return false, S("Cannot get player with name @1.", name)
	end
	if teleportee:get_attach() then
		return false, S("Cannot teleport, @1 " ..
			"is attached to an object!", name)
	end
	teleportee:set_pos(p)
	return true, S("Teleporting @1 to @2.", name, core.pos_to_string(p, 1))
end

-- Teleports player <name> next to player <target_name> if possible
local function teleport_to_player(name, target_name)
	if name == target_name then
		return false, S("One does not teleport to oneself.")
	end
	local teleportee = core.get_player_by_name(name)
	if not teleportee then
		return false, S("Cannot get teleportee with name @1.", name)
	end
	if teleportee:get_attach() then
		return false, S("Cannot teleport, @1 " ..
			"is attached to an object!", name)
	end
	local target = core.get_player_by_name(target_name)
	if not target then
		return false, S("Cannot get target player with name @1.", target_name)
	end
	local p = find_free_position_near(target:get_pos())
	teleportee:set_pos(p)
	return true, S("Teleporting @1 to @2 at @3.", name, target_name,
		core.pos_to_string(p, 1))
end

core.register_chatcommand("teleport", {
	params = S("<X>,<Y>,<Z> | <to_name> | <name> <X>,<Y>,<Z> | <name> <to_name>"),
	description = S("Teleport to position or player"),
	privs = {teleport=true},
	func = function(name, param)
		local player = core.get_player_by_name(name)
		local relpos
		if player then
			relpos = player:get_pos()
		end
		local p = {}
		p.x, p.y, p.z = string.match(param, "^([%d.~-]+)[, ] *([%d.~-]+)[, ] *([%d.~-]+)$")
		p = core.parse_coordinates(p.x, p.y, p.z, relpos)
		if p and p.x and p.y and p.z then
			return teleport_to_pos(name, p)
		end

		local target_name = param:match("^([^ ]+)$")
		if target_name then
			return teleport_to_player(name, target_name)
		end

		local has_bring_priv = core.check_player_privs(name, {bring=true})
		local missing_bring_msg = S("You don't have permission to teleport " ..
			"other players (missing privilege: @1).", "bring")

		local teleportee_name
		p = {}
		teleportee_name, p.x, p.y, p.z = param:match(
				"^([^ ]+) +([%d.~-]+)[, ] *([%d.~-]+)[, ] *([%d.~-]+)$")
		if teleportee_name then
			local teleportee = core.get_player_by_name(teleportee_name)
			if not teleportee then
				return
			end
			relpos = teleportee:get_pos()
			p = core.parse_coordinates(p.x, p.y, p.z, relpos)
		end
		p = vector.apply(p, tonumber)

		if teleportee_name and p.x and p.y and p.z then
			if not has_bring_priv then
				return false, missing_bring_msg
			end
			return teleport_to_pos(teleportee_name, p)
		end

		teleportee_name, target_name = string.match(param, "^([^ ]+) +([^ ]+)$")
		if teleportee_name and target_name then
			if not has_bring_priv then
				return false, missing_bring_msg
			end
			return teleport_to_player(teleportee_name, target_name)
		end

		return false
	end,
})

core.register_chatcommand("set", {
	params = S("([-n] <name> <value>) | <name>"),
	description = S("Set or read server configuration setting"),
	privs = {server=true},
	func = function(name, param)
		local arg, setname, setvalue = string.match(param, "(-[n]) ([^ ]+) (.+)")
		if arg and arg == "-n" and setname and setvalue then
			core.settings:set(setname, setvalue)
			return true, setname .. " = " .. setvalue
		end

		setname, setvalue = string.match(param, "([^ ]+) (.+)")
		if setname and setvalue then
			if setname:sub(1, 7) == "secure." then
				return false, S("Failed. Cannot modify secure settings. "
					.. "Edit the settings file manually.")
			end
			if not core.settings:get(setname) then
				return false, S("Failed. Use '/set -n <name> <value>' "
					.. "to create a new setting.")
			end
			core.settings:set(setname, setvalue)
			return true, S("@1 = @2", setname, setvalue)
		end

		setname = string.match(param, "([^ ]+)")
		if setname then
			setvalue = core.settings:get(setname)
			if not setvalue then
				setvalue = S("<not set>")
			end
			return true, S("@1 = @2", setname, setvalue)
		end

		return false, S("Invalid parameters (see /help set).")
	end,
})

local function emergeblocks_callback(pos, action, num_calls_remaining, ctx)
	if ctx.total_blocks == 0 then
		ctx.total_blocks   = num_calls_remaining + 1
		ctx.current_blocks = 0
	end
	ctx.current_blocks = ctx.current_blocks + 1

	if ctx.current_blocks == ctx.total_blocks then
		core.chat_send_player(ctx.requestor_name,
			S("Finished emerging @1 blocks in @2ms.",
				ctx.total_blocks,
				string.format("%.2f", (os.clock() - ctx.start_time) * 1000)))
	end
end

local function emergeblocks_progress_update(ctx)
	if ctx.current_blocks ~= ctx.total_blocks then
		core.chat_send_player(ctx.requestor_name,
			S("emergeblocks update: @1/@2 blocks emerged (@3%)",
			ctx.current_blocks, ctx.total_blocks,
			string.format("%.1f", (ctx.current_blocks / ctx.total_blocks) * 100)))

		core.after(2, emergeblocks_progress_update, ctx)
	end
end

core.register_chatcommand("emergeblocks", {
	params = S("(here [<radius>]) | (<pos1> <pos2>)"),
	description = S("Load (or, if nonexistent, generate) map blocks contained in "
		.. "area pos1 to pos2 (<pos1> and <pos2> must be in parentheses)"),
	privs = {server=true},
	func = function(name, param)
		local p1, p2 = parse_range_str(name, param)
		if p1 == false then
			return false, p2
		end

		local context = {
			current_blocks = 0,
			total_blocks   = 0,
			start_time     = os.clock(),
			requestor_name = name
		}

		core.emerge_area(p1, p2, emergeblocks_callback, context)
		core.after(2, emergeblocks_progress_update, context)

		return true, S("Started emerge of area ranging from @1 to @2.",
			core.pos_to_string(p1, 1), core.pos_to_string(p2, 1))
	end,
})

core.register_chatcommand("deleteblocks", {
	params = S("(here [<radius>]) | (<pos1> <pos2>)"),
	description = S("Delete map blocks contained in area pos1 to pos2 "
		.. "(<pos1> and <pos2> must be in parentheses)"),
	privs = {server=true},
	func = function(name, param)
		local p1, p2 = parse_range_str(name, param)
		if p1 == false then
			return false, p2
		end

		if core.delete_area(p1, p2) then
			return true, S("Successfully cleared area "
				.. "ranging from @1 to @2.",
				core.pos_to_string(p1, 1), core.pos_to_string(p2, 1))
		else
			return false, S("Failed to clear one or more "
				.. "blocks in area.")
		end
	end,
})

core.register_chatcommand("fixlight", {
	params = S("(here [<radius>]) | (<pos1> <pos2>)"),
	description = S("Resets lighting in the area between pos1 and pos2 "
		.. "(<pos1> and <pos2> must be in parentheses)"),
	privs = {server = true},
	func = function(name, param)
		local p1, p2 = parse_range_str(name, param)
		if p1 == false then
			return false, p2
		end

		if core.fix_light(p1, p2) then
			return true, S("Successfully reset light in the area "
				.. "ranging from @1 to @2.",
				core.pos_to_string(p1, 1), core.pos_to_string(p2, 1))
		else
			return false, S("Failed to load one or more blocks in area.")
		end
	end,
})

core.register_chatcommand("mods", {
	params = "",
	description = S("List mods installed on the server"),
	privs = {},
	func = function(name, param)
		local mods = core.get_modnames()
		if #mods == 0 then
			return true, S("No mods installed.")
		else
			return true, table.concat(core.get_modnames(), ", ")
		end
	end,
})

local function handle_give_command(cmd, giver, receiver, stackstring)
	core.log("action", giver .. " invoked " .. cmd
			.. ', stackstring="' .. stackstring .. '"')
	local itemstack = ItemStack(stackstring)
	if itemstack:is_empty() then
		return false, S("Cannot give an empty item.")
	elseif (not itemstack:is_known()) or (itemstack:get_name() == "unknown") then
		return false, S("Cannot give an unknown item.")
	-- Forbid giving 'ignore' due to unwanted side effects
	elseif itemstack:get_name() == "ignore" then
		return false, S("Giving 'ignore' is not allowed.")
	end
	local receiverref = core.get_player_by_name(receiver)
	if receiverref == nil then
		return false, S("@1 is not a known player.", receiver)
	end
	local leftover = receiverref:get_inventory():add_item("main", itemstack)
	local partiality
	if leftover:is_empty() then
		partiality = nil
	elseif leftover:get_count() == itemstack:get_count() then
		partiality = false
	else
		partiality = true
	end
	-- The actual item stack string may be different from what the "giver"
	-- entered (e.g. big numbers are always interpreted as 2^16-1).
	stackstring = itemstack:to_string()
	local msg
	if partiality == true then
		msg = S("@1 partially added to inventory.", stackstring)
	elseif partiality == false then
		msg = S("@1 could not be added to inventory.", stackstring)
	else
		msg = S("@1 added to inventory.", stackstring)
	end
	if giver == receiver then
		return true, msg
	else
		core.chat_send_player(receiver, msg)
		local msg_other
		if partiality == true then
			msg_other = S("@1 partially added to inventory of @2.",
					stackstring, receiver)
		elseif partiality == false then
			msg_other = S("@1 could not be added to inventory of @2.",
					stackstring, receiver)
		else
			msg_other = S("@1 added to inventory of @2.",
					stackstring, receiver)
		end
		return true, msg_other
	end
end

core.register_chatcommand("give", {
	params = S("<name> <ItemString> [<count> [<wear>]]"),
	description = S("Give item to player"),
	privs = {give=true},
	func = function(name, param)
		local toname, itemstring = string.match(param, "^([^ ]+) +(.+)$")
		if not toname or not itemstring then
			return false, S("Name and ItemString required.")
		end
		return handle_give_command("/give", name, toname, itemstring)
	end,
})

core.register_chatcommand("giveme", {
	params = S("<ItemString> [<count> [<wear>]]"),
	description = S("Give item to yourself"),
	privs = {give=true},
	func = function(name, param)
		local itemstring = string.match(param, "(.+)$")
		if not itemstring then
			return false, S("ItemString required.")
		end
		return handle_give_command("/giveme", name, name, itemstring)
	end,
})

core.register_chatcommand("spawnentity", {
	params = S("<EntityName> [<X>,<Y>,<Z>]"),
	description = S("Spawn entity at given (or your) position"),
	privs = {give=true, interact=true},
	func = function(name, param)
		local entityname, pstr = string.match(param, "^([^ ]+) *(.*)$")
		if not entityname then
			return false, S("EntityName required.")
		end
		core.log("action", ("%s invokes /spawnentity, entityname=%q")
				:format(name, entityname))
		local player = core.get_player_by_name(name)
		if player == nil then
			core.log("error", "Unable to spawn entity, player is nil")
			return false, S("Unable to spawn entity, player is nil.")
		end
		if not core.registered_entities[entityname] then
			return false, S("Cannot spawn an unknown entity.")
		end
		local p
		if pstr == "" then
			p = player:get_pos()
		else
			p = {}
			p.x, p.y, p.z = string.match(pstr, "^([%d.~-]+)[, ] *([%d.~-]+)[, ] *([%d.~-]+)$")
			local relpos = player:get_pos()
			p = core.parse_coordinates(p.x, p.y, p.z, relpos)
			if not (p and p.x and p.y and p.z) then
				return false, S("Invalid parameters (@1).", param)
			end
		end
		p.y = p.y + 1
		local obj = core.add_entity(p, entityname)
		if obj then
			return true, S("@1 spawned.", entityname)
		else
			return true, S("@1 failed to spawn.", entityname)
		end
	end,
})

core.register_chatcommand("pulverize", {
	params = "",
	description = S("Destroy item in hand"),
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			core.log("error", "Unable to pulverize, no player.")
			return false, S("Unable to pulverize, no player.")
		end
		local wielded_item = player:get_wielded_item()
		if wielded_item:is_empty() then
			return false, S("Unable to pulverize, no item in hand.")
		end
		core.log("action", name .. " pulverized \"" ..
			wielded_item:get_name() .. " " .. wielded_item:get_count() .. "\"")
		player:set_wielded_item(nil)
		return true, S("An item was pulverized.")
	end,
})

-- Key = player name
core.rollback_punch_callbacks = {}

core.register_on_punchnode(function(pos, node, puncher)
	local name = puncher and puncher:get_player_name()
	if name and core.rollback_punch_callbacks[name] then
		core.rollback_punch_callbacks[name](pos, node, puncher)
		core.rollback_punch_callbacks[name] = nil
	end
end)

core.register_chatcommand("rollback_check", {
	params = S("[<range>] [<seconds>] [<limit>]"),
	description = S("Check who last touched a node or a node near it "
		.. "within the time specified by <seconds>. "
		.. "Default: range = 0, seconds = 86400 = 24h, limit = 5. "
		.. "Set <seconds> to inf for no time limit"),
	privs = {rollback=true},
	func = function(name, param)
		if not core.settings:get_bool("enable_rollback_recording") then
			return false, S("Rollback functions are disabled.")
		end
		local range, seconds, limit =
			param:match("(%d+) *(%d*) *(%d*)")
		range = tonumber(range) or 0
		seconds = tonumber(seconds) or 86400
		limit = tonumber(limit) or 5
		if limit > 100 then
			return false, S("That limit is too high!")
		end

		core.rollback_punch_callbacks[name] = function(pos, node, puncher)
			local name = puncher:get_player_name()
			core.chat_send_player(name, S("Checking @1 ...", core.pos_to_string(pos)))
			local actions = core.rollback_get_node_actions(pos, range, seconds, limit)
			if not actions then
				core.chat_send_player(name, S("Rollback functions are disabled."))
				return
			end
			local num_actions = #actions
			if num_actions == 0 then
				core.chat_send_player(name,
						S("Nobody has touched the specified "
						.. "location in @1 seconds.",
						seconds))
				return
			end
			local time = os.time()
			for i = num_actions, 1, -1 do
				local action = actions[i]
				core.chat_send_player(name,
					S("@1 @2 @3 -> @4 @5 seconds ago.",
							core.pos_to_string(action.pos),
							action.actor,
							action.oldnode.name,
							action.newnode.name,
							time - action.time))
			end
		end

		return true, S("Punch a node (range=@1, seconds=@2, limit=@3).",
				range, seconds, limit)
	end,
})

core.register_chatcommand("rollback", {
	params = S("(<name> [<seconds>]) | (:<actor> [<seconds>])"),
	description = S("Revert actions of a player. "
		.. "Default for <seconds> is 60. "
		.. "Set <seconds> to inf for no time limit"),
	privs = {rollback=true},
	func = function(name, param)
		if not core.settings:get_bool("enable_rollback_recording") then
			return false, S("Rollback functions are disabled.")
		end
		local target_name, seconds = string.match(param, ":([^ ]+) *(%d*)")
		local rev_msg
		if not target_name then
			local player_name
			player_name, seconds = string.match(param, "([^ ]+) *(%d*)")
			if not player_name then
				return false, S("Invalid parameters. "
					.. "See /help rollback and "
					.. "/help rollback_check.")
			end
			seconds = tonumber(seconds) or 60
			target_name = "player:"..player_name
			rev_msg = S("Reverting actions of player '@1' since @2 seconds.",
				player_name, seconds)
		else
			seconds = tonumber(seconds) or 60
			rev_msg = S("Reverting actions of @1 since @2 seconds.",
				target_name, seconds)
		end
		core.chat_send_player(name, rev_msg)
		local success, log = core.rollback_revert_actions_by(
				target_name, seconds)
		local response = ""
		if #log > 100 then
			response = S("(log is too long to show)").."\n"
		else
			for _, line in pairs(log) do
				response = response .. line .. "\n"
			end
		end
		if success then
			response = response .. S("Reverting actions succeeded.")
		else
			response = response .. S("Reverting actions FAILED.")
		end
		return success, response
	end,
})

core.register_chatcommand("status", {
	description = S("Show server status"),
	func = function(name, param)
		local status = core.get_server_status(name, false)
		if status and status ~= "" then
			return true, status
		end
		return false, S("This command was disabled by a mod or game.")
	end,
})

local function get_time(timeofday)
	local time = math.floor(timeofday * 1440)
	local minute = time % 60
	local hour = (time - minute) / 60
	return time, hour, minute
end

core.register_chatcommand("time", {
	params = S("[<0..23>:<0..59> | <0..24000>]"),
	description = S("Show or set time of day"),
	privs = {},
	func = function(name, param)
		if param == "" then
			local current_time = math.floor(core.get_timeofday() * 1440)
			local minutes = current_time % 60
			local hour = (current_time - minutes) / 60
			return true, S("Current time is @1:@2.",
					string.format("%d", hour),
					string.format("%02d", minutes))
		end
		local player_privs = core.get_player_privs(name)
		if not player_privs.settime then
			return false, S("You don't have permission to run "
				.. "this command (missing privilege: @1).", "settime")
		end
		local relative, negative, hour, minute = param:match("^(~?)(%-?)(%d+):(%d+)$")
		if not relative then -- checking the first capture against nil suffices
			local new_time = core.parse_relative_number(param, core.get_timeofday() * 24000)
			if not new_time then
				new_time = tonumber(param) or -1
			else
				new_time = new_time % 24000
			end
			if new_time ~= new_time or new_time < 0 or new_time > 24000 then
				return false, S("Invalid time (must be between 0 and 24000).")
			end
			core.set_timeofday(new_time / 24000)
			core.log("action", name .. " sets time to " .. new_time)
			return true, S("Time of day changed.")
		end
		local new_time
		hour = tonumber(hour)
		minute = tonumber(minute)
		if relative == "" then
			if hour < 0 or hour > 23 then
				return false, S("Invalid hour (must be between 0 and 23 inclusive).")
			elseif minute < 0 or minute > 59 then
				return false, S("Invalid minute (must be between 0 and 59 inclusive).")
			end
			new_time = (hour * 60 + minute) / 1440
		else
			if minute < 0 or minute > 59 then
				return false, S("Invalid minute (must be between 0 and 59 inclusive).")
			end
			local current_time = core.get_timeofday()
			if negative == "-" then -- negative time
				hour, minute = -hour, -minute
			end
			new_time = (current_time + (hour * 60 + minute) / 1440) % 1
			local _
			_, hour, minute = get_time(new_time)
		end
		core.set_timeofday(new_time)
		core.log("action", ("%s sets time to %d:%02d"):format(name, hour, minute))
		return true, S("Time of day changed.")
	end,
})

core.register_chatcommand("days", {
	description = S("Show day count since world creation"),
	func = function(name, param)
		return true, S("Current day is @1.", core.get_day_count())
	end
})

local function parse_shutdown_param(param)
	local delay, reconnect, message
	local one, two, three
	one, two, three = param:match("^(%S+) +(%-r) +(.*)")
	if one and two and three then
		-- 3 arguments: delay, reconnect and message
		return one, two, three
	end
	-- 2 arguments
	one, two = param:match("^(%S+) +(.*)")
	if one and two then
		if tonumber(one) then
			delay = one
			if two == "-r" then
				reconnect = two
			else
				message = two
			end
		elseif one == "-r" then
			reconnect, message = one, two
		end
		return delay, reconnect, message
	end
	-- 1 argument
	one = param:match("(.*)")
	if tonumber(one) then
		delay = one
	elseif one == "-r" then
		reconnect = one
	else
		message = one
	end
	return delay, reconnect, message
end

core.register_chatcommand("shutdown", {
	params = S("[<delay_in_seconds> | -1] [-r] [<message>]"),
	description = S("Shutdown server (-1 cancels a delayed shutdown, -r allows players to reconnect)"),
	privs = {server=true},
	func = function(name, param)
		local delay, reconnect, message = parse_shutdown_param(param)
		local bool_reconnect = reconnect == "-r"
		if not message then
			message = ""
		end
		delay = tonumber(delay) or 0

		if delay == 0 then
			core.log("action", name .. " shuts down server")
			core.chat_send_all("*** "..S("Server shutting down (operator request)."))
		end
		core.request_shutdown(message:trim(), bool_reconnect, delay)
		return true
	end,
})

core.register_chatcommand("ban", {
	params = S("[<name>]"),
	description = S("Ban the IP of a player or show the ban list"),
	privs = {ban=true},
	func = function(name, param)
		if param == "" then
			local ban_list = core.get_ban_list()
			if ban_list == "" then
				return true, S("The ban list is empty.")
			else
				return true, S("Ban list: @1", ban_list)
			end
		end
		if core.is_singleplayer() then
			return false, S("You cannot ban players in singleplayer!")
		end
		if not core.get_player_by_name(param) then
			return false, S("Player is not online.")
		end
		if not core.ban_player(param) then
			return false, S("Failed to ban player.")
		end
		local desc = core.get_ban_description(param)
		core.log("action", name .. " bans " .. desc .. ".")
		return true, S("Banned @1.", desc)
	end,
})

core.register_chatcommand("unban", {
	params = S("<name> | <IP_address>"),
	description = S("Remove IP ban belonging to a player/IP"),
	privs = {ban=true},
	func = function(name, param)
		if not core.unban_player_or_ip(param) then
			return false, S("Failed to unban player/IP.")
		end
		core.log("action", name .. " unbans " .. param)
		return true, S("Unbanned @1.", param)
	end,
})

core.register_chatcommand("kick", {
	params = S("<name> [<reason>]"),
	description = S("Kick a player"),
	privs = {kick=true},
	func = function(name, param)
		local tokick, reason = param:match("([^ ]+) (.+)")
		tokick = tokick or param
		if not core.kick_player(tokick, reason) then
			return false, S("Failed to kick player @1.", tokick)
		end
		local log_reason = ""
		if reason then
			log_reason = " with reason \"" .. reason .. "\""
		end
		core.log("action", name .. " kicks " .. tokick .. log_reason)
		return true, S("Kicked @1.", tokick)
	end,
})

core.register_chatcommand("clearobjects", {
	params = S("[full | quick]"),
	description = S("Clear all objects in world"),
	privs = {server=true},
	func = function(name, param)
		local options = {}
		if param == "" or param == "quick" then
			options.mode = "quick"
		elseif param == "full" then
			options.mode = "full"
		else
			return false, S("Invalid usage, see /help clearobjects.")
		end

		core.log("action", name .. " clears objects ("
				.. options.mode .. " mode).")
		if options.mode == "full" then
			core.chat_send_all(S("Clearing all objects. This may take a long time. "
				.. "You may experience a timeout. (by @1)", name))
		end
		core.clear_objects(options)
		core.log("action", "Object clearing done.")
		core.chat_send_all("*** "..S("Cleared all objects."))
		return true
	end,
})

core.register_chatcommand("msg", {
	params = S("<name> <message>"),
	description = S("Send a direct message to a player"),
	privs = {shout=true},
	func = function(name, param)
		local sendto, message = param:match("^(%S+)%s(.+)$")
		if not sendto then
			return false, S("Invalid usage, see /help msg.")
		end
		if not core.get_player_by_name(sendto) then
			return false, S("The player @1 is not online.", sendto)
		end
		core.log("action", "DM from " .. name .. " to " .. sendto
				.. ": " .. message)
		core.chat_send_player(sendto, S("DM from @1: @2", name, message))
		return true, S("DM sent to @1: @2", sendto, message)
	end,
})

core.register_chatcommand("last-login", {
	params = S("[<name>]"),
	description = S("Get the last login time of a player or yourself"),
	func = function(name, param)
		if param == "" then
			param = name
		end
		local pauth = core.get_auth_handler().get_auth(param)
		if pauth and pauth.last_login and pauth.last_login ~= -1 then
			-- Time in UTC, ISO 8601 format
			return true, S("@1's last login time was @2.",
				param,
				os.date("!%Y-%m-%dT%H:%M:%SZ", pauth.last_login))
		end
		return false, S("@1's last login time is unknown.", param)
	end,
})

core.register_chatcommand("clearinv", {
	params = S("[<name>]"),
	description = S("Clear the inventory of yourself or another player"),
	func = function(name, param)
		local player
		if param and param ~= "" and param ~= name then
			if not core.check_player_privs(name, {server=true}) then
				return false, S("You don't have permission to "
					.. "clear another player's inventory "
					.. "(missing privilege: @1).", "server")
			end
			player = core.get_player_by_name(param)
			core.chat_send_player(param, S("@1 cleared your inventory.", name))
		else
			player = core.get_player_by_name(name)
		end

		if player then
			player:get_inventory():set_list("main", {})
			player:get_inventory():set_list("craft", {})
			player:get_inventory():set_list("craftpreview", {})
			core.log("action", name.." clears "..player:get_player_name().."'s inventory")
			return true, S("Cleared @1's inventory.", player:get_player_name())
		else
			return false, S("Player must be online to clear inventory!")
		end
	end,
})

local function handle_kill_command(killer, victim)
	if core.settings:get_bool("enable_damage") == false then
		return false, S("Players can't be killed, damage has been disabled.")
	end
	local victimref = core.get_player_by_name(victim)
	if victimref == nil then
		return false, S("Player @1 is not online.", victim)
	elseif victimref:get_hp() <= 0 then
		if killer == victim then
			return false, S("You are already dead.")
		else
			return false, S("@1 is already dead.", victim)
		end
	end
	if killer ~= victim then
		core.log("action", string.format("%s killed %s", killer, victim))
	end
	-- Kill victim
	victimref:set_hp(0)
	return true, S("@1 has been killed.", victim)
end

core.register_chatcommand("kill", {
	params = S("[<name>]"),
	description = S("Kill player or yourself"),
	privs = {server=true},
	func = function(name, param)
		return handle_kill_command(name, param == "" and name or param)
	end,
})
