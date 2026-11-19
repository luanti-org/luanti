-- Runtime media probe for the WebAssembly client.
--
-- Asks the page for a pair of tiny test Oggs (client/web/media/test/), sends
-- them to the player with core.dynamic_add_media, gives the files back, and
-- plays the tone. util/wasm/test_runtime_media_browser.py drives it and reads
-- the log lines. It does nothing unless `web_media_probe = true` is set in
-- minetest.conf, and nothing at all outside the WebAssembly client, where
-- core.web_media_request does not exist.

if not core.settings:get_bool("web_media_probe", false) then
	return
end
if type(core.web_media_request) ~= "function" then
	core.log("warning", "web_media_probe: core.web_media_request is not available here")
	return
end

local MODPATH = core.get_modpath(core.get_current_modname())
local ID = "test/tone"
local FILES = {"web_media_probe_tone_l.ogg", "web_media_probe_tone_r.ogg"}

local function log(message)
	core.log("action", "web_media_probe: " .. message)
end

local function play(name)
	local count = 0
	for _, file in ipairs(FILES) do
		local handle = core.sound_play({name = file:gsub("%.ogg$", "")},
				{to_player = name, gain = 0.2, loop = true})
		if handle then
			count = count + 1
		end
	end
	log(("playing %d source(s) for %s"):format(count, name))
end

local function send(name)
	local waiting = #FILES
	for _, file in ipairs(FILES) do
		local ok, accepted = pcall(core.dynamic_add_media, {
			filepath = MODPATH .. "/fetched/" .. file,
			to_player = name, client_cache = false,
		}, function()
			waiting = waiting - 1
			if waiting == 0 then
				log("media delivered to " .. name)
				core.web_media_release(ID)
				play(name)
			end
		end)
		if not (ok and accepted) then
			log("dynamic_add_media refused " .. file)
			return
		end
	end
end

local function poll(name, tries)
	if not core.get_player_by_name(name) or tries > 120 then
		return
	end
	local state = core.web_media_request(ID)
	if state == "ready" then
		log("page has " .. ID)
		send(name)
	elseif state == "loading" or state == "busy" then
		core.after(1, poll, name, tries + 1)
	else
		log("page answered " .. state)
	end
end

core.register_on_joinplayer(function(player)
	local name = player:get_player_name()
	core.after(2, poll, name, 0)
end)
