-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later
-- Copyright (C) 2026 The Luanti Contributors

-- Stage amps: a song played out of two cabinets.
--
-- Luanti only places a sound in the world if it is a single-channel Ogg. A
-- stereo file is played with no position at all (and the engine warns about
-- it), so a stereo mix cannot come out of a pair of cabs. Each song therefore
-- ships as two mono files cut from the original left and right channels:
--
--   lw_amps_<song>_l.ogg   the original left channel  -> audience-left cab
--   lw_amps_<song>_r.ogg   the original right channel -> audience-right cab
--
-- `_l` / `_r` rather than `.1` / `.2`, because `<name>.<digit>.ogg` is a random
-- sound group and the engine would play only one of the two.
--
-- The pair is started together, in the same server step, with the same
-- `start_time`, so the stereo image survives: a visitor standing between the
-- cabs hears the mix the way it was recorded. A visitor who walks in later is
-- given the pair at `start_time = elapsed % length` and lands in time with
-- everyone else. That clock is also what the dance director (lw_dance) reads,
-- which is how the band on stage stays on the beat of the song coming out of
-- the cabs.
--
-- The files are not in `sounds/`, on purpose. Everything under a mod's
-- `sounds/` is hashed by the server and announced to every client on join,
-- which for these is about 26 MiB on every start of the showcase, songs or
-- not. They are sent with core.dynamic_add_media, one pair at a time, to a
-- player who is actually in earshot of a song that is playing — in this pack
-- only on the concert arena (lw_maps), so nothing else pays.
--
-- In the web build the Around songs are not even in the game: they are runtime
-- media (client/web/media/songs/), and the first listener to come into range
-- asks the page for the pair (core.web_media_request) and waits for it. See
-- "Songs that are not join-time media" below. A native install puts the files
-- in this mod's `songs/` folder (see songs/README.md) and nothing is fetched.
-- The cabaret's songs use the same `media_dir` path (below).

lw_amps = {}

local storage = core.get_mod_storage()
local MODPATH = core.get_modpath("lw_amps")

--------------------------------------------------------------------------
-- Tunables
--------------------------------------------------------------------------

-- `max_hear_distance` only admits players who are inside it when a playback
-- *starts*; a player outside it then hears nothing until a new playback is
-- started for them. That is why the sweep below starts the pair per player as
-- they come into range rather than once for everybody.
lw_amps.DEFAULT_HEAR = 80

-- Positional gain is the level at three nodes from the source. It does not get
-- louder inside one node, and OpenAL clamps the final gain at 1, so a gain
-- above 1 does not make the cab louder close up — it carries full volume
-- further out. 3 keeps the floor in front of the stage at full level.
lw_amps.DEFAULT_GAIN = 3

-- How often listeners are checked: who has walked into range and needs the
-- pair, who has walked well out of it and can give their sources back.
local SWEEP_INTERVAL = 1.0

-- A listener is released this far past the hearing distance, not at it, so a
-- visitor pacing along the edge does not restart the song every second.
local RELEASE_MARGIN = 16

-- How far `/amp left` / `/amp right` look for the cab being pointed at.
local POINT_RANGE = 10

--------------------------------------------------------------------------
-- Songs
--------------------------------------------------------------------------

-- name -> {name, title, length, bpm, sounds = {left, right}}
lw_amps.songs = {}
lw_amps.song_order = {}
lw_amps.default_song = nil

local function file_exists(path)
	local file = io.open(path, "rb")
	if file then
		file:close()
		return true
	end
	return false
end

-- `length` is the duration of the files in seconds, measured from the files
-- themselves (ffprobe) — it is what `start_time` wraps on, so a guess would
-- drift a late arriver out of time by the error on every loop.
--
-- Two optional fields keep a song out of the media every client downloads
-- at join (see "Songs that are not join-time media" below):
--
-- * `media_dir`: the pair lives in lw_amps/<media_dir>/ instead of sounds/,
--   and is pushed to each listener with core.dynamic_add_media the first time
--   they come into range. A song whose two files are not both there is not
--   registered, so a stage waiting for its songs stays quiet instead of
--   playing silence.
-- * `lazy_media`: the files may not exist yet when the song is registered.
--   The web build streams the Around songs in after the world is up (see
--   client/web/packages.js), so the check moves from registration to the
--   moment a listener needs the pair: until both files are there that
--   listener is simply not started, and the sweep tries again every second.
-- * `web_media`: the id of this song's files as runtime media ("songs/<song>"),
--   which the WebAssembly build fetches on demand when a listener first needs
--   them (core.web_media_request, a no-op everywhere else). Only meaningful
--   with `lazy_media`; where that function does not exist the pair is read
--   from `media_dir` and a missing pair means a silent stage.
-- * `playlist`: the stage it belongs to (`lw_amps.playlist`), so a map can say
--   "play my songs" without naming files that may not have been added yet.
function lw_amps.register_song(name, def)
	assert(type(name) == "string" and name:match("^[a-z0-9_]+$"),
			"lw_amps.register_song: name has to be lowercase letters, digits and _")
	assert(type(def) == "table" and type(def.length) == "number" and def.length > 0,
			"lw_amps.register_song: " .. name .. " needs a positive length")
	def = table.copy(def)
	def.name = name
	def.title = def.title or name
	def.sounds = {
		left = "lw_amps_" .. name .. "_l",
		right = "lw_amps_" .. name .. "_r",
	}
	assert(def.web_media == nil or (type(def.web_media) == "string" and
			def.web_media:match("^[a-z0-9_]+/[a-z0-9_]+$")),
			"lw_amps.register_song: " .. name .. ": web_media has to look like set/id")
	if def.media_dir then
		local dir = MODPATH .. DIR_DELIM .. def.media_dir .. DIR_DELIM
		def.files = {
			left = dir .. def.sounds.left .. ".ogg",
			right = dir .. def.sounds.right .. ".ogg",
		}
		if not def.lazy_media and
				not (file_exists(def.files.left) and file_exists(def.files.right)) then
			core.log("warning", ("lw_amps: %s is registered but %s or %s is " ..
					"missing; leaving it out"):format(name, def.files.left, def.files.right))
			return nil
		end
	end
	if not lw_amps.songs[name] then
		lw_amps.song_order[#lw_amps.song_order + 1] = name
	end
	lw_amps.songs[name] = def
	lw_amps.default_song = lw_amps.default_song or name
	return def
end

function lw_amps.get_song(name)
	return name and lw_amps.songs[name] or nil
end

-- The songs registered with `playlist = name`, in registration order. Empty,
-- not nil, when none are: a stage whose songs have not been added yet.
function lw_amps.playlist(name)
	local list = {}
	for _, song in ipairs(lw_amps.song_order) do
		if name and lw_amps.songs[song].playlist == name then
			list[#list + 1] = song
		end
	end
	return list
end

-- Lace Waterfall, "Around": the happy-hard mix and its six alternate takes.
-- Every pair was checked to have the same duration on both channels.
local AROUND = {
	{"around_happy_hard",   159.021474, "Around (happy hard)"},
	{"around_happy_hard_1", 199.328413, "Around (happy hard, take 1)"},
	{"around_happy_hard_2", 152.229637, "Around (happy hard, take 2)"},
	{"around_happy_hard_3", 162.835351, "Around (happy hard, take 3)"},
	{"around_happy_hard_4", 152.934943, "Around (happy hard, take 4)"},
	{"around_happy_hard_5", 155.521066, "Around (happy hard, take 5)"},
	{"around_happy_hard_6", 160.458209, "Around (happy hard, take 6)"},
}
for _, take in ipairs(AROUND) do
	lw_amps.register_song(take[1], {
		title = take[3],
		artist = "Lace Waterfall",
		length = take[2],
		bpm = 175,
		-- songs/, not sounds/: see the note at the top of this file.
		media_dir = "songs",
		-- The files are not in the game. The web build fetches them when a
		-- listener first needs them (client/web/media/songs/).
		lazy_media = true,
		web_media = "songs/" .. take[1],
	})
end

-- The cabaret club's songs (lw_maps' `cabaret` map). This is the slot: the
-- folder ships with no audio and a songs.lua whose register calls are
-- commented out. See cabaret/README.md for where the Ogg pairs go and the one
-- line each song needs.
do
	local songs = MODPATH .. DIR_DELIM .. "cabaret" .. DIR_DELIM .. "songs.lua"
	if file_exists(songs) then
		dofile(songs)
	end
end

--------------------------------------------------------------------------
-- Stages
--------------------------------------------------------------------------

-- A stage is a pair of cabs that knows what it plays: `song`, or the first
-- song in its `playlist`. Maps register theirs (lw_maps does it for every map
-- with `amps`), and punching one of those cabs plays that stage's song from
-- that stage, wherever the amps were pointed before. Cabs nobody registered
-- behave as they always have: /amp left, /amp right, and the default song.
lw_amps.stages = {}

local function same_pos(a, b)
	return a ~= nil and b ~= nil and a.x == b.x and a.y == b.y and a.z == b.z
end

function lw_amps.register_stage(def)
	assert(def.left and def.right, "lw_amps.register_stage needs left and right")
	def = table.copy(def)
	def.left, def.right = vector.copy(def.left), vector.copy(def.right)
	lw_amps.stages[#lw_amps.stages + 1] = def
	return def
end

-- The stage whose cab is at `cab` (the node, not the sound source above it).
function lw_amps.stage_at_cab(cab)
	local source = vector.new(cab.x, cab.y + 1, cab.z)
	for _, stage in ipairs(lw_amps.stages) do
		if same_pos(stage.left, source) or same_pos(stage.right, source) then
			return stage
		end
	end
	return nil
end

-- The stage a playlist song belongs to. Only playlist songs have one: a song
-- like "Around" can be played from any pair of cabs a visitor sets up.
function lw_amps.stage_for_song(name)
	local def = lw_amps.get_song(name)
	if not (def and def.playlist) then
		return nil
	end
	for _, stage in ipairs(lw_amps.stages) do
		if stage.playlist == def.playlist then
			return stage
		end
	end
	return nil
end

-- What a stage (or a map's `amps` table) plays: its `song` if that is
-- registered, else the first song in its playlist, else nil.
function lw_amps.stage_song(stage)
	if stage.song and lw_amps.get_song(stage.song) then
		return stage.song
	end
	return lw_amps.playlist(stage.playlist)[1]
end

--------------------------------------------------------------------------
-- The cabs
--------------------------------------------------------------------------

-- Where each side's sound comes from: one node above the cab, so the source is
-- at ear height for the floor rather than inside the box.
local amps = {left = nil, right = nil}

local settings = {
	hear = lw_amps.DEFAULT_HEAR,
	gain = lw_amps.DEFAULT_GAIN,
}

local function load_state()
	for _, side in ipairs({"left", "right"}) do
		local stored = storage:get_string(side)
		if stored ~= "" then
			amps[side] = core.string_to_pos(stored)
		end
	end
	local hear = tonumber(storage:get_string("hear"))
	if hear and hear > 0 then
		settings.hear = hear
	end
	local gain = tonumber(storage:get_string("gain"))
	if gain and gain > 0 then
		settings.gain = gain
	end
end
load_state()

function lw_amps.get_amps()
	return amps.left and vector.copy(amps.left) or nil,
			amps.right and vector.copy(amps.right) or nil
end

-- `pos` is the sound source itself. Code that builds a stage (the companion
-- arena) calls this directly; a visitor uses /amp left and /amp right.
function lw_amps.set_amp(side, pos)
	assert(side == "left" or side == "right", "lw_amps.set_amp: side is left or right")
	amps[side] = pos and vector.copy(pos) or nil
	storage:set_string(side, pos and core.pos_to_string(amps[side]) or "")
end

function lw_amps.set_amps(left, right)
	lw_amps.set_amp("left", left)
	lw_amps.set_amp("right", right)
end

function lw_amps.get_settings()
	return {hear = settings.hear, gain = settings.gain}
end

function lw_amps.set_hear(distance)
	if type(distance) ~= "number" or distance <= 0 then
		return false
	end
	settings.hear = distance
	storage:set_string("hear", tostring(distance))
	return true
end

function lw_amps.set_gain(gain)
	if type(gain) ~= "number" or gain <= 0 then
		return false
	end
	settings.gain = gain
	storage:set_string("gain", tostring(gain))
	return true
end

--------------------------------------------------------------------------
-- Listeners
--------------------------------------------------------------------------

-- fn(event, song_name, info), event "start" or "stop". `info` carries the
-- song's def, how far into it the clock is, and the two source positions,
-- which is everything a stage needs to dance to it.
local on_song = {}

function lw_amps.register_on_song(fn)
	on_song[#on_song + 1] = fn
end

local function notify(event, name, info)
	for _, fn in ipairs(on_song) do
		local ok, err = pcall(fn, event, name, info)
		if not ok then
			core.log("error", ("lw_amps: a %s listener for %s failed: %s")
					:format(event, name, tostring(err)))
		end
	end
end

--------------------------------------------------------------------------
-- Playback
--------------------------------------------------------------------------

-- The one song that is playing, or nil. `elapsed` is accumulated from server
-- steps rather than read off the wall clock: in singleplayer the pause menu
-- stops the server and pauses every sound, and a wall-clock song would come
-- back from the menu ahead of what is actually coming out of the cabs.
local playing = nil

function lw_amps.now_playing()
	if not playing then
		return nil
	end
	return playing.name, playing.elapsed, playing.def
end

local function info_for(state)
	return {
		song = state.def,
		elapsed = state.elapsed,
		length = state.def.length,
		bpm = state.def.bpm,
		left = vector.copy(state.left),
		right = vector.copy(state.right),
	}
end

local function nearest_cab(state, pos)
	return math.min(vector.distance(pos, state.left), vector.distance(pos, state.right))
end

--------------------------------------------------------------------------
-- Songs that are not join-time media
--------------------------------------------------------------------------

-- A song with `media_dir` is not in sounds/, so the engine does not put it in
-- the media list every client downloads and hashes before the world loads.
-- Instead each listener is sent the pair, once, when they first come into
-- range of it, with core.dynamic_add_media. Until both files have arrived the
-- listener is simply not started; the sweep below tries again every second
-- and starts them in time with the song once their client has the pair.
-- Nobody who never goes near the stage downloads its songs.
--
-- In the web build the pair is not on disk either. `web_media` names it for the
-- page (client/web/runtime_media.js), which downloads both files, checks them
-- and writes them where `files` points. The page does one download at a time:
-- while it is busy with another song, or the files are still coming, the
-- answer is "loading"/"busy" and the listener waits for the next sweep, so a
-- listener never has a second multi-megabyte transfer going. A song the page
-- cannot get ("missing", "failed") leaves the stage silent and nothing throws.
-- Once the engine has taken the bytes the files are given back.
local pushed = {}  -- player name -> {[song] = "pending" | true}
local web_waiting = {}  -- song -> {[player] = true}: pushes the files are needed for
local web_logged = {}  -- song -> the last web media state logged
local web_gave_up = {}  -- song -> true: the page said "missing"/"invalid"/"unavailable"

local function web_media_for(def)
	if def.web_media and type(core.web_media_request) == "function" then
		return def.web_media
	end
end

-- One line per change of state, so a silent stage says why.
local function web_note(def, state)
	if web_logged[def.name] == state then
		return
	end
	web_logged[def.name] = state
	local level = (state == "ready" or state == "loading" or state == "busy")
			and "action" or "warning"
	core.log(level, ("lw_amps: %s: web media %s"):format(def.name, state))
end

-- Gives a song's files back once no pending push needs them. Whoever comes
-- into range later asks again (the browser caches the download).
local function web_release_if_idle(song)
	local def = lw_amps.songs[song]
	local waiting = web_waiting[song]
	if not def or (waiting and next(waiting) ~= nil) then
		return
	end
	web_waiting[song] = nil
	-- Asked even when this never saw "ready": the page may have finished
	-- between two sweeps. It knows whether it holds anything.
	if def.web_media and type(core.web_media_release) == "function" then
		web_logged[song] = nil
		pcall(core.web_media_release, def.web_media)
	end
end

local function web_done(song, who)
	local waiting = web_waiting[song]
	if waiting then
		waiting[who] = nil
	end
	web_release_if_idle(song)
end

local function both_files(def)
	return file_exists(def.files.left) and file_exists(def.files.right)
end

local function media_ready(def, name)
	if not def.files then
		return true
	end
	local mine = pushed[name] or {}
	pushed[name] = mine
	if mine[def.name] == true then
		return true
	end
	if type(core.dynamic_add_media) ~= "function" then
		-- An engine without it (or a test double): nothing to send, and a
		-- sound_play for a file the client lacks is just silence.
		return true
	end
	if mine[def.name] == "pending" then
		return false
	end
	if def.lazy_media then
		-- Not necessarily there yet. In the web build ask the page for it,
		-- every time: it answers at once, and it is also what starts a fetch
		-- that was not running. Anywhere else the files were installed by
		-- hand or not at all, and a stage without them stays quiet. The clock
		-- and the dancers run either way.
		local id = web_media_for(def)
		if id and not web_gave_up[def.name] then
			local ok, state = pcall(core.web_media_request, id)
			state = ok and state or "failed"
			web_note(def, state)
			if state == "missing" or state == "invalid" or state == "unavailable" then
				web_gave_up[def.name] = true
			end
		end
		if not both_files(def) then
			return false
		end
	end
	mine[def.name] = "pending"
	web_waiting[def.name] = web_waiting[def.name] or {}
	web_waiting[def.name][name] = true
	local waiting = 2
	for _, side in ipairs({"left", "right"}) do
		-- Not ephemeral, so the same file can go to every listener in turn;
		-- not cached, so a browser does not keep a second copy in IndexedDB.
		local ok, accepted = pcall(core.dynamic_add_media, {
			filepath = def.files[side], to_player = name, client_cache = false,
		}, function(who)
			waiting = waiting - 1
			if waiting == 0 then
				if pushed[who] then
					pushed[who][def.name] = true
				end
				web_done(def.name, name)
			end
		end)
		if not (ok and accepted) then
			mine[def.name] = nil
			web_done(def.name, name)
			return false
		end
	end
	return false
end

local function stop_for(state, name)
	local handles = state.listeners[name]
	state.listeners[name] = nil
	if not handles or type(core.sound_stop) ~= "function" then
		return
	end
	for _, handle in ipairs(handles) do
		pcall(core.sound_stop, handle)
	end
end

-- Both cabs, one player, one step. Silence is a normal outcome — no OpenAL in
-- this build, a file that is missing, a sound_play that throws — and nothing
-- else here depends on the sound having started: the clock and the dancers
-- run either way.
local function start_for(state, name)
	if state.listeners[name] or type(core.sound_play) ~= "function" then
		return false
	end
	if not media_ready(state.def, name) then
		return false
	end
	local offset = state.elapsed % state.def.length
	local handles = {}
	for _, side in ipairs({"left", "right"}) do
		local ok, handle = pcall(core.sound_play, {name = state.def.sounds[side]}, {
			pos = state[side],
			gain = settings.gain,
			max_hear_distance = settings.hear,
			loop = true,
			start_time = offset,
			to_player = name,
		}, false)
		if ok and handle then
			handles[#handles + 1] = handle
		end
	end
	state.listeners[name] = handles
	return true
end

function lw_amps.play(name)
	name = name or lw_amps.default_song
	local def = lw_amps.get_song(name)
	if not def then
		return false, ("No such song: %s. /amp list shows them."):format(tostring(name))
	end
	if not (amps.left and amps.right) then
		return false, "Point at the audience-left cab and use /amp left, then " ..
				"the audience-right one and /amp right."
	end
	if playing then
		lw_amps.stop()
	end

	playing = {
		name = name,
		def = def,
		elapsed = 0,
		sweep = 0,
		left = vector.copy(amps.left),
		right = vector.copy(amps.right),
		listeners = {},
	}
	for _, player in ipairs(core.get_connected_players()) do
		if nearest_cab(playing, player:get_pos()) <= settings.hear then
			start_for(playing, player:get_player_name())
		end
	end
	notify("start", name, info_for(playing))
	return true, ("Playing %s."):format(def.title)
end

function lw_amps.stop()
	local state = playing
	if not state then
		return false, "Nothing is playing."
	end
	playing = nil
	for listener in pairs(state.listeners) do
		stop_for(state, listener)
	end
	-- A pair fetched for a listener who never got to use it.
	web_release_if_idle(state.name)
	notify("stop", state.name, info_for(state))
	return true, ("Stopped %s."):format(state.def.title)
end

function lw_amps.toggle(name)
	if playing then
		return lw_amps.stop()
	end
	return lw_amps.play(name)
end

function lw_amps.status()
	local left, right = lw_amps.get_amps()
	local lines = {
		("Left cab: %s · right cab: %s · hear %d · gain %.2f"):format(
			left and core.pos_to_string(left) or "not set",
			right and core.pos_to_string(right) or "not set",
			settings.hear, settings.gain),
	}
	if playing then
		local count = 0
		for _ in pairs(playing.listeners) do
			count = count + 1
		end
		lines[#lines + 1] = ("Playing %s: %.1fs of %.1fs, %d listener(s)."):format(
			playing.def.title, playing.elapsed % playing.def.length,
			playing.def.length, count)
	else
		lines[#lines + 1] = "Nothing is playing."
	end
	return table.concat(lines, "\n")
end

core.register_globalstep(function(dtime)
	local state = playing
	if not state then
		return
	end
	state.elapsed = state.elapsed + dtime
	state.sweep = state.sweep + dtime
	if state.sweep < SWEEP_INTERVAL then
		return
	end
	state.sweep = 0

	local online = {}
	for _, player in ipairs(core.get_connected_players()) do
		local name = player:get_player_name()
		online[name] = true
		local away = nearest_cab(state, player:get_pos())
		if not state.listeners[name] and away <= settings.hear then
			start_for(state, name)
		elseif state.listeners[name] and away > settings.hear + RELEASE_MARGIN then
			-- Out of earshot: give the two sources back. A browser tab has a
			-- small OpenAL budget, and walking back in restarts the pair in
			-- time anyway.
			stop_for(state, name)
		end
	end
	for name in pairs(state.listeners) do
		if not online[name] then
			state.listeners[name] = nil
		end
	end
end)

core.register_on_leaveplayer(function(player)
	local name = player:get_player_name()
	-- Dynamic media goes with the client, so a rejoin is sent it again.
	pushed[name] = nil
	-- A push that will never be confirmed no longer holds a song's files.
	for song in pairs(web_waiting) do
		web_done(song, name)
	end
	if playing then
		-- The client's sounds went with it; only the handles are left to drop.
		playing.listeners[name] = nil
	end
end)

core.register_on_shutdown(function()
	if playing then
		lw_amps.stop()
	end
end)

--------------------------------------------------------------------------
-- The cab node
--------------------------------------------------------------------------

-- Punching a cab: stop whatever is playing, or play. A registered stage's cab
-- plays that stage's song from that stage; any other cab plays the default
-- song from wherever /amp left and /amp right point.
local function punch_cab(pos)
	if playing then
		return lw_amps.stop()
	end
	local stage = lw_amps.stage_at_cab(pos)
	if not stage then
		return lw_amps.play()
	end
	local song = lw_amps.stage_song(stage)
	if not song then
		return false, stage.empty or "This stage has no songs yet."
	end
	lw_amps.set_amps(stage.left, stage.right)
	return lw_amps.play(song)
end

core.register_node("lw_amps:cab", {
	description = "Amp Cabinet (punch to play / stop the song)",
	tiles = {
		"lw_amps_cab_top.png", "lw_amps_cab_side.png",
		"lw_amps_cab_side.png", "lw_amps_cab_side.png",
		"lw_amps_cab_side.png", "lw_amps_cab_front.png",
	},
	paramtype = "light",
	paramtype2 = "facedir",
	is_ground_content = false,
	-- Not in lw_palette: that group fills the material library's pedestals,
	-- which are full. The cab is in the inventory like every described node.
	groups = {cracky = 2, oddly_breakable_by_hand = 2, lw_amp = 1},
	-- The in-world trigger: the song starts when a cab is punched, and the
	-- director starts the band on that same event.
	on_punch = function(pos, node, puncher)
		if not puncher or not puncher:is_player() then
			return
		end
		local _, message = punch_cab(pos)
		core.chat_send_player(puncher:get_player_name(), message)
	end,
	on_rightclick = function(pos, node, clicker)
		if not clicker or not clicker:is_player() then
			return
		end
		local _, message = punch_cab(pos)
		core.chat_send_player(clicker:get_player_name(), message)
	end,
})

--------------------------------------------------------------------------
-- /amp
--------------------------------------------------------------------------

-- The node the player is looking at, if it is an amp.
local function pointed_amp(player)
	local props = player:get_properties()
	local eye = vector.add(player:get_pos(),
		vector.new(0, (props and props.eye_height) or 1.625, 0))
	local reach = vector.add(eye, vector.multiply(player:get_look_dir(), POINT_RANGE))
	for pointed in core.raycast(eye, reach, false, false) do
		if pointed.type == "node" then
			local name = core.get_node(pointed.under).name
			if core.get_item_group(name, "lw_amp") > 0 then
				return pointed.under
			end
			return nil, name
		end
	end
	return nil
end

local function set_from_look(player, side)
	local cab, other = pointed_amp(player)
	if not cab then
		return false, other and ("That is %s, not an amp cab."):format(other)
				or "Look at an amp cab first."
	end
	local source = vector.new(cab.x, cab.y + 1, cab.z)
	lw_amps.set_amp(side, source)
	return true, ("%s cab set: the %s channel plays from %s."):format(
		side == "left" and "Audience-left" or "Audience-right", side,
		core.pos_to_string(source))
end

local function list_songs()
	local lines = {"Songs:"}
	for _, name in ipairs(lw_amps.song_order) do
		local def = lw_amps.songs[name]
		lines[#lines + 1] = ("  %s — %s, %.0fs%s%s"):format(name, def.title,
			def.length, def.bpm and (", " .. def.bpm .. " bpm") or "",
			(playing and playing.name == name) and " [playing]" or "")
	end
	lines[#lines + 1] = "/amp left · right · play [<song>] · stop · hear <n> · gain <g> · status"
	return table.concat(lines, "\n")
end

core.register_chatcommand("amp", {
	params = "[left | right | play [<song>] | stop | hear <nodes> | gain <g> | list | status]",
	description = "Set up the stage amps and play a song through them",
	func = function(name, param)
		local player = core.get_player_by_name(name)
		if not player then
			return false, "You have to be in the world."
		end
		local words = (param or ""):trim():split(" ")
		local verb = words[1] or ""

		-- Moving the cabs or changing how far they carry changes the show for
		-- everybody on the server; starting and stopping a song does not.
		local function can_configure()
			return core.check_player_privs(name, {server = true})
		end

		if verb == "" or verb == "status" then
			return true, lw_amps.status()
		elseif verb == "list" then
			return true, list_songs()
		elseif verb == "left" or verb == "right" then
			if not can_configure() then
				return false, "Setting the cabs needs the server privilege."
			end
			return set_from_look(player, verb)
		elseif verb == "hear" then
			if not can_configure() then
				return false, "Changing the hearing distance needs the server privilege."
			end
			if not lw_amps.set_hear(tonumber(words[2])) then
				return false, "Usage: /amp hear <nodes>, a positive number."
			end
			return true, ("The cabs carry %d nodes from the next play."):format(settings.hear)
		elseif verb == "gain" then
			if not can_configure() then
				return false, "Changing the gain needs the server privilege."
			end
			if not lw_amps.set_gain(tonumber(words[2])) then
				return false, "Usage: /amp gain <g>, a positive number."
			end
			return true, ("Gain is %.2f from the next play."):format(settings.gain)
		elseif verb == "play" then
			return lw_amps.play(words[2])
		elseif verb == "stop" then
			return lw_amps.stop()
		end
		return false, ("I do not know /amp %s. Try /amp list."):format(verb)
	end,
})
