local builtin_shared = ...

local chatcommand_msg_time_threshold =
	tonumber(core.settings:get("chatcommand_msg_time_threshold")) or 0.1
core.register_on_sending_chat_message(function(message)
	if message:sub(1,2) == ".." then
		return false
	end

	local first_char = message:sub(1,1)
	if first_char == "/" or first_char == "." then
		core.display_chat_message(core.gettext("Issued command: ") .. message)
	end

	return builtin_shared.try_handle_chatcommand(nil, message, chatcommand_msg_time_threshold)
end)

core.register_chatcommand("list_players", {
	description = core.gettext("List online players"),
	func = function(param)
		local player_names = core.get_player_names()
		if not player_names then
			return false, core.gettext("This command is disabled by server.")
		end

		local players = table.concat(player_names, ", ")
		return true, core.gettext("Online players: ") .. players
	end
})

core.register_chatcommand("disconnect", {
	description = core.gettext("Exit to main menu"),
	func = function(param)
		core.disconnect()
	end,
})

core.register_chatcommand("clear_chat_queue", {
	description = core.gettext("Clear the out chat queue"),
	func = function(param)
		core.clear_out_chat_queue()
		return true, core.gettext("The out chat queue is now empty.")
	end,
})

function core.run_server_chatcommand(cmd, param)
	core.send_chat_message("/" .. cmd .. " " .. param)
end
