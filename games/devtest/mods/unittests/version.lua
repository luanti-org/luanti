unittests.register("test_get_version", function()
	local version = core.get_version()
	assert(type(version) == "table")
	assert(type(version.project) == "string")
	assert(type(version.string) == "string")
	assert(type(version.proto_min) == "number")
	assert(type(version.proto_max) == "number")
	assert(version.proto_max >= version.proto_min)
	assert(type(version.is_dev) == "boolean")
	if version.is_dev then
		assert(type(version.hash) == "string")
	else
		assert(version.hash == nil)
	end
end)

unittests.register("test_protocol_version", function(player)
	local info = core.get_player_information(player:get_player_name())

	local maxver = 0
	for _, v in pairs(core.protocol_versions) do
		maxver = math.max(maxver, v)
	end
	assert(maxver > 0) -- table must contain something valid

	-- If the client is older than a known version then it's pointless.
	if info.protocol_version < maxver then
		core.log("warning", "test_protocol_version: client is outdated, skipping test!")
		return
	end
	local info_server = core.get_version()
	if info.version_string ~= (info_server.hash or info_server.string) then
		core.log("warning", "test_protocol_version: client is not the same version. False-positive possible.")
	end

	-- The protocol version the client and server agreed on must exist in the table.
	local match = table.key_value_swap(core.protocol_versions)[info.protocol_version]
	print(string.format("client proto matched: %s sent: %s", match, info.version_string))
	assert(match ~= nil)
end, {player = true})
