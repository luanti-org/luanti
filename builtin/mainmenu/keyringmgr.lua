keyringmgr = {
    keyring = nil
}

local function get_keyring_path()
    return core.get_user_path() .. DIR_DELIM .. "client" .. DIR_DELIM .. core.settings:get("keyring_file")
end

local function save_keyring(keyring)
    core.safe_file_write(get_keyring_path(), core.write_json(keyring))
end

local function read_keyring()
    local path = get_keyring_path()

    local file = io.open(path, "r")
    if file then
        local json = file:read("*all")
        file:close()
        return core.parse_json(json)
    end
end

function keyringmgr.set_login(address, port, playername, password)
	local url = address .. ":" .. port
	local keyring = read_keyring() or {}
	local entry = keyring[url] or {playernames = {}}
	entry.last_playername = playername
	entry.playernames[playername] = 1
	keyring[url] = entry
	-- TODO error handling
	core.keychain_set_password(url, playername, password)
	save_keyring(keyring)
end

function keyringmgr.get_last_login(address, port)
	local url = address .. ":" .. port
	local keyring = read_keyring()
	local entry = keyring[url]
	if entry == nil then return nil end
	local got_password,password_or_error = core.keychain_get_password(url, entry.last_playername)
	-- TODO error handling
	return {
		playername = entry.last_playername,
		password = password_or_error,
	}
end
