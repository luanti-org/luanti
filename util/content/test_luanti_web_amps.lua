#!/usr/bin/env lua
-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2026 The Luanti Contributors

-- Headless checks for the stage amps. Stubs `core` far enough to load lw_amps
-- whole, then plays a song and watches what is asked of the sound API: two
-- mono sources started in one step, in time for whoever walks in later, and
-- silence (never an error) when there is no sound at all.
--
--   lua util/content/test_luanti_web_amps.lua
--   (also invoked by util/content/test_luanti_web_amps.py)

local SCRIPT = arg[0]
local ROOT = SCRIPT:match("^(.*)/util/content/") or "."
local MODPATH = ROOT .. "/games/luanti_web/mods/lw_amps"

local failures = 0

local function fail(message)
	failures = failures + 1
	io.stderr:write("FAIL: " .. message .. "\n")
end

local function assert_true(cond, message)
	if not cond then
		fail(message)
	end
end

local function assert_eq(got, expected, message)
	if got ~= expected then
		fail(("%s: got %s, expected %s"):format(message, tostring(got),
			tostring(expected)))
	end
end

local function assert_near(got, expected, slack, message)
	if type(got) ~= "number" or math.abs(got - expected) > slack then
		fail(("%s: got %s, expected %s +/- %s"):format(message, tostring(got),
			tostring(expected), tostring(slack)))
	end
end

--------------------------------------------------------------------------
-- Engine bits the mod expects to exist
--------------------------------------------------------------------------

DIR_DELIM = "/"

function string.trim(s)
	return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

function string.split(s, sep)
	local out = {}
	for piece in s:gmatch("([^" .. (sep or " ") .. "]+)") do
		out[#out + 1] = piece
	end
	return out
end

function table.copy(t)
	if type(t) ~= "table" then
		return t
	end
	local out = {}
	for k, v in pairs(t) do
		out[table.copy(k)] = table.copy(v)
	end
	return out
end

vector = {}
function vector.new(x, y, z)
	if type(x) == "table" then
		return {x = x.x, y = x.y, z = x.z}
	end
	return {x = x or 0, y = y or 0, z = z or 0}
end
function vector.copy(v) return vector.new(v) end
function vector.add(a, b)
	return vector.new(a.x + b.x, a.y + b.y, a.z + b.z)
end
function vector.multiply(v, k)
	return vector.new(v.x * k, v.y * k, v.z * k)
end
function vector.distance(a, b)
	local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
	return math.sqrt(dx * dx + dy * dy + dz * dz)
end

--------------------------------------------------------------------------
-- Stub engine
--------------------------------------------------------------------------

local world = {}
local players = {}
local globalsteps = {}
local leaveplayer = {}
local privs = {}
local stored = {}
local played = {}
local stopped = {}
local next_handle = 0

local function key(pos)
	return math.floor(pos.x) .. "," .. math.floor(pos.y) .. "," .. math.floor(pos.z)
end

local function sound_play(spec, params, ephemeral)
	next_handle = next_handle + 1
	played[#played + 1] = {spec = spec, params = params, ephemeral = ephemeral,
		handle = next_handle, step = 0}
	return next_handle
end

local current_step = 0

core = {
	registered_nodes = {},
	registered_chatcommands = {},
	get_modpath = function() return MODPATH end,
	get_mod_storage = function()
		return {
			get_string = function(_, k) return stored[k] or "" end,
			set_string = function(_, k, v) stored[k] = v end,
		}
	end,
	pos_to_string = function(pos)
		return ("(%s,%s,%s)"):format(pos.x, pos.y, pos.z)
	end,
	string_to_pos = function(s)
		local x, y, z = s:match("^%((%-?[%d.]+),(%-?[%d.]+),(%-?[%d.]+)%)$")
		if not x then
			return nil
		end
		return vector.new(tonumber(x), tonumber(y), tonumber(z))
	end,
	get_node = function(pos)
		return world[key(pos)] or {name = "air"}
	end,
	get_item_group = function(name, group)
		local def = core.registered_nodes[name]
		return def and def.groups and def.groups[group] or 0
	end,
	get_connected_players = function()
		local out = {}
		for _, player in pairs(players) do
			out[#out + 1] = player
		end
		return out
	end,
	get_player_by_name = function(name) return players[name] end,
	check_player_privs = function(name, want)
		for priv in pairs(want) do
			if not (privs[name] and privs[name][priv]) then
				return false
			end
		end
		return true
	end,
	chat_send_player = function() end,
	sound_play = sound_play,
	sound_stop = function(handle) stopped[#stopped + 1] = handle end,
	-- The ray only ever meets the node the test says the player is looking at.
	raycast = function()
		local done = false
		return function()
			if done then
				return nil
			end
			done = true
			if core.looking_at then
				return {type = "node", under = core.looking_at,
					above = vector.add(core.looking_at, {x = 0, y = 1, z = 0})}
			end
			return nil
		end
	end,
	log = function() end,
	register_node = function(name, def)
		def.name = name
		core.registered_nodes[name] = def
	end,
	register_chatcommand = function(name, def)
		core.registered_chatcommands[name] = def
	end,
	register_globalstep = function(fn) globalsteps[#globalsteps + 1] = fn end,
	register_on_leaveplayer = function(fn) leaveplayer[#leaveplayer + 1] = fn end,
	register_on_shutdown = function() end,
}

local function step(dtime)
	current_step = current_step + 1
	for _, fn in ipairs(globalsteps) do
		fn(dtime)
	end
end

local function FakePlayer(name, pos)
	local player = {
		is_player = function() return true end,
		get_player_name = function() return name end,
		get_pos = function() return vector.new(pos) end,
		set_pos = function(_, p) pos = vector.new(p) end,
		get_properties = function() return {eye_height = 1.625} end,
		get_look_dir = function() return vector.new(0, 0, 1) end,
	}
	players[name] = player
	return player
end

local function amp(name, param)
	return core.registered_chatcommands["amp"].func(name, param)
end

dofile(MODPATH .. "/init.lua")

--------------------------------------------------------------------------
-- The song table
--------------------------------------------------------------------------

assert_eq(#lw_amps.song_order, 7, "the main mix and six takes are registered")
assert_eq(lw_amps.default_song, "around_happy_hard", "the main mix is the default")
local around = lw_amps.get_song("around_happy_hard")
assert_near(around.length, 159.021474, 1e-6, "length is the measured duration")
assert_eq(around.bpm, 175, "Around is 175 bpm")
assert_eq(around.sounds.left, "lw_amps_around_happy_hard_l", "left file name")
assert_eq(around.sounds.right, "lw_amps_around_happy_hard_r", "right file name")
for _, name in ipairs(lw_amps.song_order) do
	assert_true(not lw_amps.songs[name].sounds.left:find("%.%d$"),
		name .. " is not named like a random sound group")
end

--------------------------------------------------------------------------
-- Setting up the cabs
--------------------------------------------------------------------------

local host = FakePlayer("host", {x = 3, y = 10, z = -6})
privs.host = {server = true}
FakePlayer("guest_no_priv", {x = 3, y = 10, z = -6})

local ok, message = lw_amps.play()
assert_true(not ok, "a song does not play before the cabs are set")
assert_true(message:find("/amp left") ~= nil, "and the message says how to set them")

core.set_node = nil
world[key({x = 0, y = 10, z = 0})] = {name = "lw_amps:cab"}
world[key({x = 6, y = 10, z = 0})] = {name = "lw_amps:cab"}
world[key({x = 3, y = 9, z = 3})] = {name = "lw_nodes:stone"}

core.looking_at = {x = 0, y = 10, z = 0}
ok = amp("guest_no_priv", "left")
assert_true(not ok, "setting a cab needs the server privilege")
ok, message = amp("host", "left")
assert_true(ok, "/amp left sets the cab being looked at: " .. tostring(message))
local left = lw_amps.get_amps()
assert_true(left ~= nil and left.x == 0 and left.y == 11 and left.z == 0,
	"the left source is one node above the cab")
assert_eq(stored.left, "(0,11,0)", "and is remembered across restarts")

core.looking_at = {x = 3, y = 9, z = 3}
ok = amp("host", "right")
assert_true(not ok, "pointing at something that is not a cab is refused")
core.looking_at = {x = 6, y = 10, z = 0}
ok = amp("host", "right")
assert_true(ok, "/amp right sets the other cab")

--------------------------------------------------------------------------
-- Playing: two sources, one step, one clock
--------------------------------------------------------------------------

local events = {}
lw_amps.register_on_song(function(event, song, info)
	events[#events + 1] = {event = event, song = song, info = info}
end)

ok = lw_amps.play("around_happy_hard")
assert_true(ok, "the song plays once both cabs are set")
-- Two players are in range (the host and a guest), two sources each.
assert_eq(#played, 4, "one source per cab per listener")
local mine = {}
for _, call in ipairs(played) do
	if call.params.to_player == "host" then
		mine[#mine + 1] = call
	end
end
assert_eq(#mine, 2, "the host gets one source per cab")
local l, r = mine[1], mine[2]
assert_eq(l.spec.name, "lw_amps_around_happy_hard_l", "the left file goes to...")
assert_eq(l.params.pos.x, 0, "...the left cab")
assert_eq(r.spec.name, "lw_amps_around_happy_hard_r", "the right file goes to...")
assert_eq(r.params.pos.x, 6, "...the right cab")
assert_eq(l.params.start_time, r.params.start_time, "both start at the same point")
assert_eq(l.params.start_time, 0, "from the top")
assert_true(l.params.loop and r.params.loop, "both loop")
assert_eq(l.params.to_player, "host", "each listener gets their own pair")
assert_eq(l.params.gain, 3, "default gain")
assert_eq(l.params.max_hear_distance, 80, "default hearing distance")
assert_eq(l.ephemeral, false, "the pair keeps handles so it can be stopped")
assert_eq(#events, 1, "listeners are told the song started")
assert_eq(events[1].event, "start", "a start event")
assert_eq(events[1].info.elapsed, 0, "at the top of the song")
assert_eq(events[1].info.left.x, 0, "with the cab positions")

local before = #played
for _ = 1, 105 do
	step(0.1)
end
local name, elapsed = lw_amps.now_playing()
assert_eq(name, "around_happy_hard", "now_playing names the song")
assert_near(elapsed, 10.5, 1e-6, "the clock counts server steps")

-- A late arrival: in range at the next sweep, given the pair in time.
FakePlayer("late", {x = 3, y = 10, z = 4})
before = #played
step(1.0)
local late = {}
for index = before + 1, #played do
	if played[index].params.to_player == "late" then
		late[#late + 1] = played[index]
	end
end
assert_eq(#late, 2, "a late arrival is given both sources")
assert_near(late[1].params.start_time, 11.5, 1e-6, "in time with the song")
assert_eq(late[1].params.start_time, late[2].params.start_time,
	"and the pair is in time with itself")

-- Somebody far away is not started at all, and walking away releases a pair.
FakePlayer("far", {x = 500, y = 10, z = 0})
before = #played
step(1.0)
for index = before + 1, #played do
	assert_true(played[index].params.to_player ~= "far",
		"nobody out of earshot is given the song")
end
players.late.set_pos(nil, {x = 300, y = 10, z = 0})
local stopped_before = #stopped
step(1.0)
assert_eq(#stopped - stopped_before, 2, "walking well out of range stops the pair")
players.late.set_pos(nil, {x = 3, y = 10, z = 4})
before = #played
step(1.0)
assert_eq(#played - before, 2, "walking back in starts it again")

-- The song loops; start_time wraps on its length.
for _ = 1, 160 do
	step(1.0)
end
players.far.set_pos(nil, {x = 3, y = 10, z = 5})
before = #played
step(1.0)
local wrapped = nil
for index = before + 1, #played do
	if played[index].params.to_player == "far" then
		wrapped = played[index].params.start_time
	end
end
local _, now = lw_amps.now_playing()
assert_near(wrapped, now % around.length, 1e-6, "start_time wraps on the length")
assert_true(wrapped < around.length, "and never runs past the end")

--------------------------------------------------------------------------
-- Stopping
--------------------------------------------------------------------------

stopped = {}
ok = lw_amps.stop()
assert_true(ok, "the song stops")
assert_true(#stopped >= 4, "every listener's pair is stopped")
assert_eq(lw_amps.now_playing(), nil, "and nothing is playing")
assert_eq(events[#events].event, "stop", "listeners are told it stopped")

-- The cab is the in-world button.
local cab = core.registered_nodes["lw_amps:cab"]
assert_true(cab.groups.lw_amp == 1, "the cab is in the amp group")
cab.on_punch({x = 0, y = 10, z = 0}, {name = "lw_amps:cab"}, host)
assert_eq(lw_amps.now_playing(), "around_happy_hard", "punching a cab plays the default song")
cab.on_punch({x = 0, y = 10, z = 0}, {name = "lw_amps:cab"}, host)
assert_eq(lw_amps.now_playing(), nil, "punching it again stops it")

--------------------------------------------------------------------------
-- Settings
--------------------------------------------------------------------------

ok = amp("guest_no_priv", "hear 200")
assert_true(not ok, "changing the range needs the server privilege")
ok = amp("host", "hear 120")
assert_true(ok, "/amp hear sets the range")
assert_eq(stored.hear, "120", "and remembers it")
ok = amp("host", "gain 0.5")
assert_true(ok, "/amp gain sets the gain")
played = {}
lw_amps.play("around_happy_hard_3")
assert_eq(played[1].params.max_hear_distance, 120, "the new range is used")
assert_eq(played[1].params.gain, 0.5, "and the new gain")
assert_eq(played[1].spec.name, "lw_amps_around_happy_hard_3_l", "an alternate take plays")
-- Playing another song replaces the first, it does not stack.
local stops_before = #events
lw_amps.play("around_happy_hard")
assert_eq(events[stops_before + 1].event, "stop", "the old song is stopped first")
assert_eq(events[stops_before + 2].event, "start", "then the new one starts")
lw_amps.stop()
local listed = select(2, amp("host", "list"))
assert_true(listed:find("around_happy_hard_6") ~= nil, "/amp list names every take")

--------------------------------------------------------------------------
-- Silence is not an error
--------------------------------------------------------------------------

-- A build without sound: no sound_play at all. The clock and the listeners
-- carry on, so the band still dances.
core.sound_play = nil
events = {}
ok = lw_amps.play()
assert_true(ok, "a song still 'plays' with no sound API")
assert_eq(events[1].event, "start", "and listeners still hear about it")
step(1.0)
assert_near(select(2, lw_amps.now_playing()), 1.0, 1e-6, "the clock still runs")
lw_amps.stop()

-- A sound_play that throws (a missing file, a broken OpenAL) is caught.
core.sound_play = function()
	error("no such sound")
end
ok = lw_amps.play()
assert_true(ok, "a throwing sound_play does not stop the show")
step(1.0)
lw_amps.stop()
core.sound_play = sound_play

--------------------------------------------------------------------------
-- Stages, playlists, and songs that are not join-time media
--------------------------------------------------------------------------

assert_eq(#lw_amps.playlist("cabaret"), 0, "the cabaret ships with no songs")
assert_eq(lw_amps.register_song("cabaret_missing", {length = 10,
	media_dir = "cabaret", playlist = "cabaret"}), nil,
	"a cabaret song whose files are missing is left out")
assert_eq(lw_amps.get_song("cabaret_missing"), nil, "and is not playable")

-- A pair dropped into the cabaret folder (made here, removed at the end,
-- and never touching a real pair of the same name).
local CABARET_DIR = MODPATH .. "/cabaret/"
local made = {}
for _, side in ipairs({"l", "r"}) do
	local path = CABARET_DIR .. "lw_amps_cabaret_test_" .. side .. ".ogg"
	local existing = io.open(path, "rb")
	if existing then
		existing:close()
	else
		local file = io.open(path, "wb")
		if file then
			file:write("OggS")
			file:close()
			made[#made + 1] = path
		end
	end
end
local cabaret_song = lw_amps.register_song("cabaret_test", {length = 20, bpm = 112,
	media_dir = "cabaret", playlist = "cabaret"})
assert_true(cabaret_song ~= nil, "a cabaret song with both files registers")
assert_eq(cabaret_song and cabaret_song.files.left,
	CABARET_DIR .. "lw_amps_cabaret_test_l.ogg", "its left file is in the cabaret folder")
assert_eq(lw_amps.playlist("cabaret")[1], "cabaret_test", "and is in the cabaret playlist")
assert_eq(lw_amps.default_song, "around_happy_hard", "the default song is still Around")

local stage = lw_amps.register_stage({left = {x = 100, y = 11, z = 0},
	right = {x = 106, y = 11, z = 0}, playlist = "cabaret"})
assert_eq(lw_amps.stage_at_cab({x = 100, y = 10, z = 0}), stage,
	"a stage knows its cab by the node under the source")
assert_eq(lw_amps.stage_at_cab({x = 0, y = 10, z = 0}), nil, "and not anyone else's")
assert_eq(lw_amps.stage_for_song("cabaret_test"), stage, "a playlist song has a home stage")
assert_eq(lw_amps.stage_for_song("around_happy_hard"), nil,
	"Around plays from whatever cabs a visitor sets up")
assert_eq(lw_amps.stage_song(stage), "cabaret_test", "the stage plays its first song")

-- Punching a stage cab plays its song from its own cabs; the pair is pushed
-- to a listener first and started once their client has both files.
local media = {}
core.dynamic_add_media = function(options, callback)
	media[#media + 1] = {options = options, callback = callback}
	return true
end
players.host.set_pos(nil, {x = 103, y = 10, z = 4})
played = {}
cab.on_punch({x = 106, y = 10, z = 0}, {name = "lw_amps:cab"}, host)
assert_eq(lw_amps.now_playing(), "cabaret_test", "punching a stage cab plays its song")
local stage_left = lw_amps.get_amps()
assert_eq(stage_left and stage_left.x, 100, "from that stage's cabs")
local function for_host(list, field)
	local out = {}
	for _, item in ipairs(list) do
		if item[field].to_player == "host" then
			out[#out + 1] = item
		end
	end
	return out
end
assert_eq(#for_host(played, "params"), 0, "nothing plays before the client has the files")
local pushes = for_host(media, "options")
assert_eq(#pushes, 2, "both files are pushed to the listener")
assert_eq(pushes[1] and pushes[1].options.client_cache, false, "without a cached copy")
assert_true(pushes[1] and not pushes[1].options.ephemeral,
	"not ephemeral, so the next listener can be sent the same file")
step(1.0)
assert_eq(#for_host(media, "options"), 2, "a pending push is not repeated every sweep")
assert_eq(#for_host(played, "params"), 0, "and still nothing plays")
for _, push in ipairs(pushes) do
	push.callback("host")
end
step(1.0)
local pair = for_host(played, "params")
assert_eq(#pair, 2, "once both have arrived, the sweep starts the pair")
assert_eq(pair[1] and pair[1].spec.name, "lw_amps_cabaret_test_l", "the left file...")
assert_eq(pair[1] and pair[1].params.pos.x, 100, "...from the stage's left cab")
assert_near(pair[1] and pair[1].params.start_time, 2.0, 1e-6, "in time with the song")
cab.on_punch({x = 106, y = 10, z = 0}, {name = "lw_amps:cab"}, host)
assert_eq(lw_amps.now_playing(), nil, "punching the stage cab again stops it")
lw_amps.play("cabaret_test")
step(1.0)
assert_eq(#for_host(media, "options"), 2, "a listener is sent a song once")
lw_amps.stop()

-- A stage with nothing in its playlist stays quiet when punched.
lw_amps.register_stage({left = {x = 200, y = 11, z = 0},
	right = {x = 206, y = 11, z = 0}, playlist = "nothing_yet"})
cab.on_punch({x = 200, y = 10, z = 0}, {name = "lw_amps:cab"}, host)
assert_eq(lw_amps.now_playing(), nil, "an empty stage plays nothing")

for _, path in ipairs(made) do
	os.remove(path)
end

--------------------------------------------------------------------------
-- Song media is sent on demand, not on join
--------------------------------------------------------------------------

-- The songs are not in sounds/, so the client only has a pair once lw_amps
-- has sent it. Nobody is started on a file they do not have; the sweep after
-- the client confirms it starts them, in time. The Around songs are not in
-- the game either (client/web/media/songs/): this part is the native path,
-- where somebody put the files in the mod's songs/ folder by hand.
local SONG_DIR = MODPATH .. "/songs/"

local function pair_paths(song)
	return {SONG_DIR .. "lw_amps_" .. song .. "_l.ogg", SONG_DIR .. "lw_amps_" .. song .. "_r.ogg"}
end

local function write_pair(song)
	for _, path in ipairs(pair_paths(song)) do
		local file = assert(io.open(path, "wb"))
		file:write("OggS")
		file:close()
	end
end

local function remove_pair(song)
	for _, path in ipairs(pair_paths(song)) do
		os.remove(path)
	end
end

for _, song in ipairs({"around_happy_hard_2", "lazy_test", "web_test", "gone_test"}) do
	for _, path in ipairs(pair_paths(song)) do
		assert_true(io.open(path, "rb") == nil, "no stray " .. path)
	end
end

local added, confirm = {}, {}
core.dynamic_add_media = function(options, callback)
	added[#added + 1] = options
	confirm[#confirm + 1] = function() callback(options.to_player) end
	return true
end
lw_amps.set_hear(80)

-- No files and no page to fetch them: a silent stage, and nothing throws.
local sent_before = #played
lw_amps.play("around_happy_hard_2")
step(1.0)
assert_eq(#added, 0, "nothing is sent for files that are not installed")
assert_eq(#played, sent_before, "and nothing plays")
lw_amps.stop()

write_pair("around_happy_hard_2")
lw_amps.play("around_happy_hard_2")
local sent = #added
assert_true(sent > 0 and sent % 2 == 0, "the song's pair is sent to each listener")
for _, options in ipairs(added) do
	assert_true(options.filepath:find("/lw_amps/songs/lw_amps_around_happy_hard_2_[lr]%.ogg$") ~= nil,
		"from lw_amps/songs: " .. tostring(options.filepath))
	assert_true(options.to_player ~= nil, "to that player only")
	assert_eq(options.client_cache, false, "and not cached by the browser")
end
assert_eq(#played, sent_before, "nothing plays before the client has the files")
step(1.0)
assert_eq(#added, sent, "a pending file is not sent twice")
for _, fn in ipairs(confirm) do
	fn()
end
step(1.0)
assert_true(#played > sent_before, "the pair starts once it has arrived")
assert_true(played[#played].params.start_time > 0, "late, but in step with the clock")
lw_amps.stop()
lw_amps.play("around_happy_hard_2")
assert_eq(#added, sent, "a file a client has is not sent again")
lw_amps.stop()
remove_pair("around_happy_hard_2")

-- A `lazy_media` song registers with no files and waits for them.
local lazy = lw_amps.register_song("lazy_test", {length = 10, media_dir = "songs",
	lazy_media = true})
assert_true(lazy ~= nil, "a lazy song registers before its files exist")
assert_eq(lw_amps.get_song("lazy_test") ~= nil, true, "and is playable")
local lazy_sent = #added
local lazy_played = #played
lw_amps.play("lazy_test")
step(1.0)
step(1.0)
assert_eq(#added, lazy_sent, "nothing is sent for files that have not arrived")
assert_eq(#played, lazy_played, "and nothing plays")
write_pair("lazy_test")
step(1.0)
assert_true(#added > lazy_sent, "once the files exist the pair is sent on the next sweep")
for i = lazy_sent + 1, #confirm do
	confirm[i]()
end
step(1.0)
assert_true(#played > lazy_played, "and the song starts in step with the clock")
lw_amps.stop()
remove_pair("lazy_test")

--------------------------------------------------------------------------
-- The web build: the page fetches the pair when a listener first needs it
--------------------------------------------------------------------------

assert_true(not pcall(lw_amps.register_song, "bad_web", {length = 1, web_media = "../x"}),
	"a web_media id has to be set/id")

-- A stand-in for the page (client/web/runtime_media.js): one download at a
-- time, answers at once, writes the files only when it says "ready".
local page = {state = "loading", requests = {}, released = {}}
local logs = {}
local real_log = core.log
core.log = function(level, message) logs[#logs + 1] = level .. ": " .. message end
local function page_fetch_done(song)
	write_pair(song)
	page.state = "ready"
end
core.web_media_request = function(id)
	page.requests[#page.requests + 1] = id
	if page.throw then
		error("the page is gone")
	end
	return page.state
end
core.web_media_release = function(id)
	page.released[#page.released + 1] = id
	remove_pair(id:match("/(.+)$"))
	return true
end
local function count_logs(pattern)
	local n = 0
	for _, line in ipairs(logs) do
		if line:find(pattern) then
			n = n + 1
		end
	end
	return n
end

local web = lw_amps.register_song("web_test", {length = 10, media_dir = "songs",
	lazy_media = true, web_media = "songs/web_test"})
assert_true(web ~= nil, "a web song registers before its files exist")
local web_sent, web_played = #added, #played
lw_amps.play("web_test")
step(1.0)
assert_true(#page.requests >= 1, "a listener in range asks the page for the pair")
assert_eq(page.requests[1], "songs/web_test", "by its id")
assert_eq(#added, web_sent, "nothing is sent while the page is still fetching")
assert_eq(count_logs("web media loading"), 1, "loading is logged once, not every sweep")
step(1.0)
page.state = "busy"
step(1.0)
assert_eq(#added, web_sent, "nothing is sent while the page is busy with another song")
assert_eq(#played, web_played, "and nothing plays")
assert_eq(count_logs("web media busy"), 1, "busy is logged")
page_fetch_done("web_test")
step(1.0)
assert_true(#added > web_sent, "once the page has the pair it is sent on the next sweep")
local requests_at_ready = #page.requests
step(1.0)
assert_eq(#page.requests, requests_at_ready, "a pending push does not ask the page again")
assert_eq(#page.released, 0, "the files stay until the engine has taken them")
local first, last = web_sent + 1, #confirm
for i = first, last do
	confirm[i]()
end
assert_eq(#page.released, 1, "the files are given back once the engine has them")
assert_eq(page.released[1], "songs/web_test", "the same id")
step(1.0)
assert_true(#played > web_played, "and the song starts in step with the clock")
lw_amps.stop()
assert_eq(#page.released, 2, "stopping a song asks once more, in case the page finished between sweeps")

-- The player leaves before the engine confirms: nobody is left holding it.
page.state = "loading"
page.released = {}
lw_amps.play("web_test")
step(1.0)
page_fetch_done("web_test")
-- The earlier listener's state is gone with them, as it is on a real rejoin.
for _, player in ipairs(core.get_connected_players()) do
	for _, fn in ipairs(leaveplayer) do
		fn(player)
	end
end
lw_amps.stop()
assert_true(#page.released >= 1, "files nobody is waiting for are released when the song stops")

-- A song the page cannot get leaves the stage silent, and is not asked for
-- every second.
local gone = lw_amps.register_song("gone_test", {length = 10, media_dir = "songs",
	lazy_media = true, web_media = "songs/gone_test"})
assert_true(gone ~= nil, "registers")
page.state = "missing"
page.requests = {}
logs = {}
local gone_sent, gone_played = #added, #played
lw_amps.play("gone_test")
step(1.0)
step(1.0)
step(1.0)
assert_eq(#added, gone_sent, "a song the page does not have is never sent")
assert_eq(#played, gone_played, "and the band is silent")
assert_eq(#page.requests, 1, "a missing song is asked for once")
assert_eq(count_logs("^warning: lw_amps: gone_test: web media missing"), 1,
	"and the reason is logged as a warning")
lw_amps.stop()

-- A page that throws is the same as a failure: silence, then a retry later.
local failing = lw_amps.register_song("web_fail", {length = 10, media_dir = "songs",
	lazy_media = true, web_media = "songs/web_fail"})
assert_true(failing ~= nil, "registers")
page.throw = true
page.requests = {}
lw_amps.play("web_fail")
step(1.0)
step(1.0)
assert_true(#page.requests >= 2, "a failing page is asked again on the next sweep")
assert_true(count_logs("web media failed") >= 1, "and the failure is logged")
page.throw = false
lw_amps.stop()

core.log = real_log
core.web_media_request = nil
core.web_media_release = nil
core.dynamic_add_media = nil

if failures > 0 then
	io.stderr:write(("%d check(s) failed\n"):format(failures))
	os.exit(1)
end

print("stage amp checks ok")
