-- Server-side companion for the SSCSM test mod.
-- Joins a mod channel and echoes whatever the client sends.

local CHANNEL = "testsscsm:hello"

core.mod_channel_join(CHANNEL)

core.register_on_modchannel_message(function(channel, sender, msg)
	if channel ~= CHANNEL then return end
	core.log("action", "[testsscsm server] got '" .. msg ..
			"' from " .. sender .. ", echoing")
	local ch = core.mod_channel_join(CHANNEL)
	if ch and ch:is_writeable() then
		ch:send_all("echo: " .. msg)
	end
end)
