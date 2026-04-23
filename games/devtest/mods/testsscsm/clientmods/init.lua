core.log("info", "SSCSM testsscsm:init.lua loaded")
core.display_chat_message("hello from SSCSM testsscsm")

local helper_msg = dofile("helpers.lua")
core.display_chat_message("helpers.lua returned: " .. tostring(helper_msg))

-- Clientmod-channel round trip via the new server-mod ↔ SSCSM primitive.
local CHANNEL = "testsscsm:hello"

core.register_on_clientmodchannel_signal(function(channel, signal)
	if channel ~= CHANNEL then return end
	core.display_chat_message("[testsscsm sscsm] signal: " .. tostring(signal))
	if signal == 0 then -- JOIN_OK
		core.clientmodchannel_send(CHANNEL, "ping from sscsm")
	end
end)

core.register_on_clientmodchannel_message(function(channel, msg)
	if channel ~= CHANNEL then return end
	-- Note: no `sender` arg — server is the only correspondent.
	core.display_chat_message("[testsscsm sscsm] server: " .. msg)
end)

-- Userdata-style: returns a channel object on JOIN_OK, nil otherwise.
core.clientmodchannel_join(CHANNEL)

-- Verify content-id channel delivery. core.registered_content_ids is
-- populated asynchronously by *server_builtin* via the engine-reserved
-- *core:content_defs* channel after on_joinplayer fires server-side,
-- so we can't read it during init — defer with core.after.
core.after(2, function()
	local n = 0
	for _ in pairs(core.registered_content_ids) do n = n + 1 end
	core.display_chat_message("[testsscsm] content_id(air) = " ..
			tostring(core.get_content_id("air")) ..
			" (" .. n .. " ids known)")
end)
