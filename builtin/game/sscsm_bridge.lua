-- Push engine-known data to the SSCSM worker on each connecting client
-- via reserved-name clientmod channels. Server-side half of the
-- *server_builtin* SSCSM init's subscribers. See doc/sscsm_api.md.
--
-- Reserved channel names start with "*core:" and are documented as
-- engine-only. User-mods that join these channels still receive the
-- traffic (no enforcement), but the data is public anyway (it's the
-- node id mapping the client already has via TOCLIENT_NODEDEF).

local CONTENT_DEFS_CHANNEL = "*core:content_defs*"
local content_defs_ch = core.clientmodchannel_open(CONTENT_DEFS_CHANNEL)

-- Build the message body once per joinplayer rather than caching, so
-- nodes registered after server start are included.
local function build_content_defs_payload()
	-- Format: "name=id;name=id;..." with semicolon separator.
	-- Names are alphanumeric+underscore+colon; safe to embed.
	local parts = {}
	for name, _ in pairs(core.registered_nodes) do
		local ok, id = pcall(core.get_content_id, name)
		if ok then
			parts[#parts + 1] = name .. "=" .. id
		end
	end
	return table.concat(parts, ";")
end

-- Request-driven: SSCSM sends "ready" after its JOIN_OK signal, we
-- respond with the payload. Avoids the race where on_joinplayer would
-- fire before the SSCSM's JOIN packet had been processed server-side.
core.register_on_clientmodchannel_message(function(channel, sender, msg)
	if channel ~= CONTENT_DEFS_CHANNEL then return end
	if msg ~= "ready" then return end
	local payload = build_content_defs_payload()
	local ok = content_defs_ch:send_to_player(sender, payload)
	core.log("info", "sscsm_bridge: pushed " .. #payload ..
			" B of content defs to " .. sender ..
			" (ok=" .. tostring(ok) .. ")")
end)
