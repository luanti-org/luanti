-- Server-side companion for the SSCSM test mod.
-- Uses the new clientmodchannel API (server-mod ↔ SSCSM RPC) so the
-- ping does NOT leak to other connected players.

local CHANNEL = "testsscsm:hello"
local ch = core.clientmodchannel_open(CHANNEL)

core.register_on_clientmodchannel_message(function(channel, sender, msg)
	if channel ~= CHANNEL then return end
	core.log("action", "[testsscsm server] got '" .. msg ..
			"' from " .. sender .. ", echoing")
	-- Send back to just the sender, not all subscribers.
	ch:send_to_player(sender, "echo: " .. msg)
end)
