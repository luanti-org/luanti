local callback_test = 0

local out = function(player, formname, fields, number)
	local snum = ""
	if number then
		snum = " "..number
	end
	local msg = "Formspec callback"..snum..": player="..player:get_player_name()..", formname=\""..tostring(formname).."\", fields="..dump(fields)
	core.chat_send_player(player:get_player_name(), msg)
	core.log("action", msg)
end

core.register_on_player_receive_fields(function(player, formname, fields)
	if callback_test == 1 then
		out(player, formname, fields)
	elseif callback_test == 2 then
		out(player, formname, fields, 1)
	end
end)
core.register_on_player_receive_fields(function(player, formname, fields)
	if callback_test == 2 then
		out(player, formname, fields, 2)
		return true -- Disable the first callback
	end
end)
core.register_on_player_receive_fields(function(player, formname, fields)
	if callback_test == 2 then
		out(player, formname, fields, 3)
	end
end)

core.register_chatcommand("test_formspec_callbacks", {
	params = "[ 0 | 1 | 2 ]",
	description = "Test: Change formspec callbacks testing mode",
	func = function(name, param)
		local mode = tonumber(param)
		if not mode then
			callback_test = (callback_test + 1 % 3)
		else
			callback_test = mode
		end
		if callback_test == 1 then
			core.chat_send_player(name, "Formspec callback test mode 1 enabled: Logging only")
		elseif callback_test == 2 then
			core.chat_send_player(name, "Formspec callback test mode 2 enabled: Three callbacks, disable pre-registered callbacks")
		else
			callback_test = 0
			core.chat_send_player(name, "Formspec callback test disabled!")
		end
	end
})
