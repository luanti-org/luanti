-- Server-provided SSCSM builtin.
-- Runs in the SSCSM sandbox after *client_builtin* and before clientmods.

-- Subscribe to engine-reserved clientmod channels and translate their
-- payloads into the corresponding `core.*` Lua tables. The server-side
-- half lives in builtin/game/sscsm_bridge.lua.

local CONTENT_DEFS_CHANNEL = "*core:content_defs*"

core.register_on_clientmodchannel_message(function(channel, msg)
	if channel ~= CONTENT_DEFS_CHANNEL then return end

	-- Format: "name=id;name=id;...". Reset and rebuild — the server
	-- is the authoritative source, so we replace rather than merge.
	local ids, names = {}, {}
	for entry in string.gmatch(msg, "([^;]+)") do
		local sep = string.find(entry, "=", 1, true)
		if sep then
			local name = string.sub(entry, 1, sep - 1)
			local id = tonumber(string.sub(entry, sep + 1))
			if name and id then
				ids[name] = id
				names[id] = name
			end
		end
	end
	core.registered_content_ids = ids
	core.registered_content_names = names
end)

-- Ask for content defs once the server has acked our JOIN. Pull rather
-- than push so the server doesn't race against subscription state.
core.register_on_clientmodchannel_signal(function(channel, signal)
	if channel == CONTENT_DEFS_CHANNEL and signal == 0 then  -- JOIN_OK
		core.clientmodchannel_send(CONTENT_DEFS_CHANNEL, "ready")
	end
end)

core.clientmodchannel_join(CONTENT_DEFS_CHANNEL)
core.log("info", "SSCSM server_builtin loaded; subscribed to "
		.. CONTENT_DEFS_CHANNEL)
