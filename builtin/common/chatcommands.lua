local builtin_shared = ...

-- For server-side translations (if INIT == "game")
-- Otherwise, use core.gettext
local S = core.get_translator("__builtin")

local CMD_MARKER = INIT == "client" and "."
		or (INIT == "game" or INIT == "sscsm") and "/"
		or error("unexpected INIT: " .. INIT)

local function call_with_name(func_with, func_without, name, ...)
	if name then
		return func_with(name, ...)
	else
		return func_without(...)
	end
end

local function call_same_with_name(func, name, ...)
	return call_with_name(func, func, name, ...)
end

core.registered_chatcommands = {}

-- Interpret the parameters of a command, separating options and arguments.
-- Input: command, param
--   command: name of command
--   param: parameters of command
-- Returns: opts, args
--   opts is a string of option letters, or false on error
--   args is an array with the non-option arguments in order, or an error message
-- Example: for this command line:
--      /command a b -cd e f -g
-- the function would receive:
--      a b -cd e f -g
-- and it would return:
--	"cdg", {"a", "b", "e", "f"}
-- Negative numbers are taken as arguments. Long options (--option) are
-- currently rejected as reserved.
local function getopts(command, param)
	local opts = ""
	local args = {}
	for match in param:gmatch("%S+") do
		if match:byte(1) == 45 then -- 45 = '-'
			local second = match:byte(2)
			if second == 45 then
				return false, S("Invalid parameters (see /help @1).", command)
			elseif second and (second < 48 or second > 57) then -- 48 = '0', 57 = '9'
				opts = opts .. match:sub(2)
			else
				-- numeric, add it to args
				args[#args + 1] = match
			end
		else
			args[#args + 1] = match
		end
	end
	return opts, args
end

function core.register_chatcommand(cmd, def)
	def = def or {}
	def.params = def.params or ""
	def.description = def.description or ""
	def.privs = def.privs or {}
	def.mod_origin = core.get_current_modname() or "??"
	core.registered_chatcommands[cmd] = def
end

function core.unregister_chatcommand(name)
	if core.registered_chatcommands[name] then
		core.registered_chatcommands[name] = nil
	else
		core.log("warning", "Not unregistering chatcommand " ..name..
			" because it doesn't exist.")
	end
end

function core.override_chatcommand(name, redefinition)
	local chatcommand = core.registered_chatcommands[name]
	assert(chatcommand, "Attempt to override non-existent chatcommand "..name)
	for k, v in pairs(redefinition) do
		rawset(chatcommand, k, v)
	end
	core.registered_chatcommands[name] = chatcommand
end

local function format_help_line(cmd, def)
	local msg = core.colorize("#00ffff", CMD_MARKER .. cmd)
	if def.params and def.params ~= "" then
		msg = msg .. " " .. def.params
	end
	if def.description and def.description ~= "" then
		msg = msg .. ": " .. def.description
	end
	return msg
end

local function do_help_cmd(name, param)
	local opts, args = getopts("help", param)
	if not opts then
		return false, args
	end
	if #args > 1 then
		return false, S("Too many arguments, try using just /help <command>")
	end
	local use_gui = INIT == "client" or core.get_player_by_name(name)
	use_gui = use_gui and not opts:find("t")

	if #args == 0 and not use_gui then
		local cmds = {}
		for cmd, def in pairs(core.registered_chatcommands) do
			if INIT == "client" or core.check_player_privs(name, def.privs) then
				cmds[#cmds + 1] = cmd
			end
		end
		table.sort(cmds)
		local msg
		if INIT == "game" then
			msg = S("Available commands: @1",
				table.concat(cmds, " ")) .. "\n"
				.. S("Use '/help <cmd>' to get more "
				.. "information, or '/help all' to list "
				.. "everything.")
		else
			msg = core.gettext("Available commands: ")
				.. table.concat(cmds, " ") .. "\n"
				-- TRANSLATORS: 'help' and 'all' mustn't be translated. cmd = command
				.. core.gettext("Use '.help <cmd>' to get more "
				.. "information, or '.help all' to list "
				.. "everything.")
		end
		return true, msg
	elseif #args == 0 or (args[1] == "all" and use_gui) then
		core.show_general_help_formspec(name)
		return true
	elseif args[1] == "all" then
		local cmds = {}
		for cmd, def in pairs(core.registered_chatcommands) do
			if INIT == "client" or core.check_player_privs(name, def.privs) then
				cmds[#cmds + 1] = format_help_line(cmd, def)
			end
		end
		table.sort(cmds)
		local msg
		if INIT == "game" then
			msg = S("Available commands:")
		else
			msg = core.gettext("Available commands:")
		end
		return true, msg.."\n"..table.concat(cmds, "\n")
	elseif INIT == "game" and args[1] == "privs" then
		if use_gui then
			core.show_privs_help_formspec(name)
			return true
		end
		local privs = {}
		for priv, def in pairs(core.registered_privileges) do
			privs[#privs + 1] = priv .. ": " .. def.description
		end
		table.sort(privs)
		return true, S("Available privileges:").."\n"..table.concat(privs, "\n")
	else
		local cmd = args[1]
		local def = core.registered_chatcommands[cmd]
		if not def then
			local msg
			if INIT == "game" then
				msg = S("Command not available: @1", cmd)
			else
				msg = core.gettext("Command not available: ") .. cmd
			end
			return false, msg
		else
			return true, format_help_line(cmd, def)
		end
	end
end

if INIT == "client" then
	core.register_chatcommand("help", {
		-- TRANSLATORS: Syntax of 'help' command. Don't translate anything except 'cmd' (=command)
		params = core.gettext("[all | <cmd>] [-t]"),
		description = core.gettext("Get help for commands (-t: output in chat)"),
		func = function(param)
			return do_help_cmd(nil, param)
		end,
	})
elseif INIT == "game" then
	core.register_chatcommand("help", {
		params = S("[all | privs | <cmd>] [-t]"),
		description = S("Get help for commands or list privileges (-t: output in chat)"),
		func = do_help_cmd,
	})
elseif INIT == "sscsm" then -- luacheck: ignore
	-- FIXME: implement SSCSM /help that combines SSCSM and SSM
end

function builtin_shared.try_handle_chatcommand(name, message, chatcommand_msg_time_threshold)
	if message:sub(1,1) ~= CMD_MARKER then
		return false
	end

	local function display_msg(msg)
		return call_with_name(core.chat_send_player, core.display_chat_message, name, msg)
	end

	local cmd, param = string.match(message, string.format("^%%%s([^ ]+) *(.*)", CMD_MARKER))
	if not cmd then
		display_msg("-!- "..S("Empty command."))
		return true
	end

	param = param or ""

	core.log("verbose", string.format("Handling chat command %q with params %q", cmd, param))

	-- Run core.registered_on_chatcommands callbacks.
	if name then
		if core.run_callbacks(core.registered_on_chatcommands, 5, name, cmd, param) then
			return true
		end
	else
		if core.run_callbacks(core.registered_on_chatcommands, 5, cmd, param) then
			return true
		end
	end

	local cmd_def = core.registered_chatcommands[cmd]
	if not cmd_def then
		if INIT == "sscsm" then
			return false -- send msg to server
		else
			display_msg("-!- "..S("Invalid command: @1", cmd))
			return true
		end
	end
	local has_privs, missing_privs = call_with_name(
			core.check_player_privs, core.check_local_player_privs, name, cmd_def.privs)
	if has_privs then
		core.set_last_run_mod(cmd_def.mod_origin)
		local t_before = core.get_us_time()
		local success, result = call_same_with_name(cmd_def.func, name, param)
		local delay = (core.get_us_time() - t_before) / 1000000
		if success == false and result == nil then
			display_msg("-!- "..S("Invalid command usage."))
			local help_def = core.registered_chatcommands["help"]
			if help_def then
				local _, helpmsg = call_same_with_name(help_def.func, name, cmd)
				if helpmsg then
					display_msg(helpmsg)
				end
			end
		else
			if delay > chatcommand_msg_time_threshold then
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
				display_msg(result)
			end
		end
	else
		display_msg(S("You don't have permission to run this command "
				.. "(missing privileges: @1).",
				table.concat(missing_privs, ", ")))
	end
	return true  -- Handled chat message
end
