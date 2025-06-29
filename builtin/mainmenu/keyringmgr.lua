keyringmgr = {
  keyring = nil,

	general_login = false
}

--------------------------------------------------------------------------------
local function get_keyring_path()
	return core.get_user_path() .. DIR_DELIM .. "client" .. DIR_DELIM .. core.settings:get("keyring_file")
end

--------------------------------------------------------------------------------
local function save_keyring(keyring)
	core.safe_file_write(get_keyring_path(), core.write_json(keyring))
end

--------------------------------------------------------------------------------
local function read_keyring()
	local path = get_keyring_path()

	local file = io.open(path, "r")
	if file then
		local json = file:read("*all")
		file:close()
		return core.parse_json(json)
	end
end

--------------------------------------------------------------------------------
local function is_for_server(keys, server)
	return keys.address == server.address and keys.port == server.port
end

--------------------------------------------------------------------------------
local function delete_keys(keyring, server)
	for i=1, #keyring do
		local keys = keyring[i]

		if is_for_server(keys, server) then
			table.remove(keyring, i)
			return
		end
	end
end

--------------------------------------------------------------------------------
local function get_keys(keyring, server)
  for i=1, #keyring do
    local keys = keyring[i]

    if is_for_server(keys, server) then
      return keys
    end
  end
  -- If we don't find any existing keys, we make a blank set for the server
  local keys = {
    address = server.address,
    port = server.port,
    logins = {},
    last_login = false,
  }
  table.insert(keyring, 1, keys)
	save_keyring(keyring)
  return keys
end

--------------------------------------------------------------------------------
local function rewrite_keys(keyring, server, new_keys)
	delete_keys(keyring, server)
	table.insert(keyring, 1, new_keys)
  save_keyring(keyring)
end

--------------------------------------------------------------------------------
local function set_general_login(server, playername)
	keyringmgr.general_login = {
		playername = playername,
		password = keyringmgr.get_login(server, playername)
	}
end

--------------------------------------------------------------------------------
function keyringmgr.get_keyring()
	if keyringmgr.keyring then
		return keyringmgr.keyring
	end

	keyringmgr.keyring = {}

	-- Add keyring, removing duplicates
	local seen = {}
	for _, keys in ipairs(read_keyring() or {}) do
		local key = ("%s:%d"):format(keys.address:lower(), keys.port)
		if not seen[key] then
			seen[key] = true
			keyringmgr.keyring[#keyringmgr.keyring + 1] = keys
		end
	end

	return keyringmgr.keyring
end

--------------------------------------------------------------------------------
function keyringmgr.set_last_login(server, playername)
	local keyring = keyringmgr.get_keyring()
  local new_keys = get_keys(keyring, server)
  new_keys.last_login = playername

  rewrite_keys(keyring, server, new_keys)
	set_general_login(server, playername)
end

--------------------------------------------------------------------------------
function keyringmgr.set_login(server, playername, password)
	assert(type(server.port) == "number")

  local keyring = keyringmgr.get_keyring()
	local new_keys = get_keys(keyring, server);
	
	-- Check for existing entires for playername
	for _, login in ipairs(new_keys.logins or {}) do
		if login.playername and login.playername == playername then
      login.password = password
      break
		end
  end

  -- If not, we add a new key
	if not new_keys.logins then
		new_keys.logins = {}
	end
	table.insert(new_keys.logins, 1, {
    playername = playername,
    password = password
  })

	keyringmgr.set_last_login(server, playername)
			
  rewrite_keys(keyring, server, new_keys)
end

--------------------------------------------------------------------------------
function keyringmgr.get_login(server, playername)
	assert(type(server.port) == "number")

  local keyring = keyringmgr.get_keyring()
	local new_keys = get_keys(keyring, server);
	
	-- Check for existing entires for playername
	for _, login in ipairs(new_keys.logins) do
		if login.playername and login.playername == playername then
      return login
		end
  end
  error("No login found on " .. server.address .. ":" .. server.port .. " for player " .. playername)
end

--------------------------------------------------------------------------------
function keyringmgr.remove_login(server, playername)
	assert(type(server.port) == "number")

  local keyring = keyringmgr.get_keyring()
	local new_keys = get_keys(keyring, server);
  for i=1, #new_keys.logins do
    local login = new_keys.logins[i]

    if(login.playername == playername) then
      table.remove(new_keys.logins, i)
      break
    end
  end

  rewrite_keys(keyring, server, new_keys)
end

--------------------------------------------------------------------------------
function keyringmgr.delete_keys(server)
	local keyring = keyringmgr.get_keyring()
	delete_keys(keyring, server)
  save_keyring(keyring)
end

--------------------------------------------------------------------------------
function keyringmgr.get_last_login(server)
  local playername = get_keys(keyringmgr.get_keyring(), server).last_login
  if playername then
   return keyringmgr.get_login(server, playername)
  end
  return false
end

--------------------------------------------------------------------------------
function keyringmgr.get_last_playername()
	if keyringmgr.general_login then
		return keyringmgr.general_login.playername
	end
	return false
end

--------------------------------------------------------------------------------
function keyringmgr.get_last_password()
	if keyringmgr.general_login then
		return keyringmgr.general_login.password
	end
	return false
end