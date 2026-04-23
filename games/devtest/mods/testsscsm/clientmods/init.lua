core.log("info", "SSCSM testsscsm:init.lua loaded")
core.display_chat_message("hello from SSCSM testsscsm")

local helper_msg = dofile("helpers.lua")
core.display_chat_message("helpers.lua returned: " .. tostring(helper_msg))

-- Mod-channel round trip: join, send on join_ok signal, log replies.
local CHANNEL = "testsscsm:hello"
core.mod_channel_join(CHANNEL)

core.register_on_modchannel_signal(function(channel, signal)
	if channel ~= CHANNEL then return end
	core.display_chat_message("[testsscsm sscsm] signal: " .. tostring(signal))
	-- signal 0 = JOIN_OK in the C++ enum; safe to write now.
	if signal == 0 then
		core.mod_channel_send_all(CHANNEL, "ping from sscsm")
	end
end)

core.register_on_modchannel_message(function(channel, sender, msg)
	if channel ~= CHANNEL then return end
	core.display_chat_message("[testsscsm sscsm] " .. sender .. ": " .. msg)
end)
