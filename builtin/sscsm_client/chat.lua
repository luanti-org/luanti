local builtin_shared = ...

local chatcommand_msg_time_threshold = 0.1 -- FIXME: take value from server or client
function core.handle_sending_chat_message(message)
	if builtin_shared.try_handle_chatcommand(nil, message, chatcommand_msg_time_threshold) then
		return
	end

	core.send_chat_message_raw(message)
end
