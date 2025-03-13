local F = core.formspec_escape

-- hashed node pos -> sound handle
local played_sounds = {}

-- all of these can be set via the formspec
local meta_keys = {
	-- SimpleSoundSpec
	"sss.name",
	"sss.gain",
	"sss.pitch",
	"sss.fade",
	-- sound parameters
	"sparam.gain",
	"sparam.pitch",
	"sparam.fade",
	"sparam.start_time",
	"sparam.loop",
	"sparam.pos",
	"sparam.object",
	"sparam.to_player",
	"sparam.exclude_player",
	"sparam.max_hear_distance",
	-- fade
	"fade.step",
	"fade.gain",
	-- other
	"ephemeral",
}

local function get_all_metadata(meta)
	return {
		sss = {
			name  = meta:get_string("sss.name"),
			gain  = meta:get_string("sss.gain"),
			pitch = meta:get_string("sss.pitch"),
			fade  = meta:get_string("sss.fade"),
		},
		sparam = {
			gain              = meta:get_string("sparam.gain"),
			pitch             = meta:get_string("sparam.pitch"),
			fade              = meta:get_string("sparam.fade"),
			start_time       = meta:get_string("sparam.start_time"),
			loop              = meta:get_string("sparam.loop"),
			pos               = meta:get_string("sparam.pos"),
			object            = meta:get_string("sparam.object"),
			to_player         = meta:get_string("sparam.to_player"),
			exclude_player    = meta:get_string("sparam.exclude_player"),
			max_hear_distance = meta:get_string("sparam.max_hear_distance"),
		},
		fade = {
			gain = meta:get_string("fade.gain"),
			step = meta:get_string("fade.step"),
		},
		ephemeral = meta:get_string("ephemeral"),
	}
end

local function log_msg(msg)
	core.log("action", msg)
	core.chat_send_all(msg)
end

local function try_call(f, ...)
	local function log_on_err(success, errmsg, ...)
		if not success then
			log_msg("[soundstuff:jukebox] Call failed: "..errmsg)
		else
			return errmsg, ...
		end
	end

	return log_on_err(pcall(f, ...))
end

local function show_formspec(pos, player)
	local meta = core.get_meta(pos)

	local md = get_all_metadata(meta)

	local pos_hash = core.hash_node_position(pos)
	local sound_handle = played_sounds[pos_hash]

	local fs = {}
	local function fs_add(str)
		table.insert(fs, str)
	end

	fs_add([[
		formspec_version[6]
		size[14,12]
	]])

	-- SimpleSoundSpec
	fs_add(string.format([[
		container[0.5,0.5]
		box[-0.1,-0.1;4.2,3.2;#EBEBEB20]
		style[*;font=mono,bold]
		label[0,0.25;SimpleSoundSpec]
		style[*;font=mono]
		field[0.00,1.00;4,0.75;sss.name;name;%s]
		field[0.00,2.25;1,0.75;sss.gain;gain;%s]
		field[1.25,2.25;1,0.75;sss.pitch;pitch;%s]
		field[2.50,2.25;1,0.75;sss.fade;fade;%s]
		container_end[]
		field_close_on_enter[sss.name;false]
		field_close_on_enter[sss.gain;false]
		field_close_on_enter[sss.pitch;false]
		field_close_on_enter[sss.fade;false]
	]], F(md.sss.name), F(md.sss.gain), F(md.sss.pitch), F(md.sss.fade)))

	-- sound parameter table
	fs_add(string.format([[
		container[5.5,0.5]
		box[-0.1,-0.1;4.2,10.7;#EBEBEB20]
		style[*;font=mono,bold]
		label[0,0.25;sound parameter table]
		style[*;font=mono]
		field[0.00,1;1,0.75;sparam.gain;gain;%s]
		field[1.25,1;1,0.75;sparam.pitch;pitch;%s]
		field[2.50,1;1,0.75;sparam.fade;fade;%s]
		field[0,2.25;4,0.75;sparam.start_time;start_time;%s]
		field[0,3.50;4,0.75;sparam.loop;loop;%s]
		field[0,4.75;4,0.75;sparam.pos;pos;%s]
		field[0,6.00;4,0.75;sparam.object;object;%s]
		field[0,7.25;4,0.75;sparam.to_player;to_player;%s]
		field[0,8.50;4,0.75;sparam.exclude_player;exclude_player;%s]
		field[0,9.75;4,0.75;sparam.max_hear_distance;max_hear_distance;%s]
		container_end[]
		field_close_on_enter[sparam.gain;false]
		field_close_on_enter[sparam.pitch;false]
		field_close_on_enter[sparam.fade;false]
		field_close_on_enter[sparam.start_time;false]
		field_close_on_enter[sparam.loop;false]
		field_close_on_enter[sparam.pos;false]
		field_close_on_enter[sparam.object;false]
		field_close_on_enter[sparam.to_player;false]
		field_close_on_enter[sparam.exclude_player;false]
		field_close_on_enter[sparam.max_hear_distance;false]
		tooltip[sparam.object;Get a name with the Branding Iron.]
	]], F(md.sparam.gain), F(md.sparam.pitch), F(md.sparam.fade),
			F(md.sparam.start_time), F(md.sparam.loop), F(md.sparam.pos),
			F(md.sparam.object), F(md.sparam.to_player), F(md.sparam.exclude_player),
			F(md.sparam.max_hear_distance)))

	-- fade
	fs_add(string.format([[
		container[10.75,3]
		box[-0.1,-0.1;3.2,3.2;#EBEBEB20]
		style[*;font=mono,bold]
		label[0,0.25;fade]
		style[*;font=mono]
		field[0.00,1;1,0.75;fade.step;step;%s]
		field[1.25,1;1,0.75;fade.gain;gain;%s]
	]], F(md.fade.step), F(md.fade.gain)))
	if not sound_handle then
		fs_add([[
			box[0,2;3,0.75;#363636FF]
			label[0.25,2.375;no sound]
		]])
	else
		fs_add([[
			button[0,2;3,0.75;btn_fade;Fade]
		]])
	end
	fs_add([[
		container_end[]
		field_close_on_enter[fade.step;false]
		field_close_on_enter[fade.gain;false]
	]])

	-- ephemeral
	fs_add(string.format([[
		checkbox[0.5,5;ephemeral;ephemeral;%s]
	]], md.ephemeral))

	-- play/stop and release buttons
	if not sound_handle then
		fs_add([[
			container[10.75,0.5]
			button[0,0;3,0.75;btn_play;Play]
			container_end[]
		]])
	else
		fs_add([[
			container[10.75,0.5]
			button[0,0;3,0.75;btn_stop;Stop]
			button[0,1;3,0.75;btn_release;Release]
			container_end[]
		]])
	end

	-- save and quit button
	fs_add([[
		button_exit[10.75,11;3,0.75;btn_save_quit;Save & Quit]
	]])

	core.show_formspec(player:get_player_name(), "soundstuff:jukebox@"..pos:to_string(),
			table.concat(fs))
end

core.register_node("soundstuff:jukebox", {
	description = "Jukebox\nAllows to play arbitrary sounds.",
	tiles = {"soundstuff_jukebox.png"},
	groups = {dig_immediate = 2},

	on_construct = function(pos)
		local meta = core.get_meta(pos)
		-- SimpleSoundSpec
		meta:set_string("sss.name", "")
		meta:set_string("sss.gain", "")
		meta:set_string("sss.pitch", "")
		meta:set_string("sss.fade", "")
		-- sound parameters
		meta:set_string("sparam.gain", "")
		meta:set_string("sparam.pitch", "")
		meta:set_string("sparam.fade", "")
		meta:set_string("sparam.start_time", "")
		meta:set_string("sparam.loop", "")
		meta:set_string("sparam.pos", pos:to_string())
		meta:set_string("sparam.object", "")
		meta:set_string("sparam.to_player", "")
		meta:set_string("sparam.exclude_player", "")
		meta:set_string("sparam.max_hear_distance", "")
		-- fade
		meta:set_string("fade.gain", "")
		meta:set_string("fade.step", "")
		-- other
		meta:set_string("ephemeral", "")

		meta:mark_as_private(meta_keys)
	end,

	on_rightclick = function(pos, _node, clicker, _itemstack, _pointed_thing)
		show_formspec(pos, clicker)
	end,
})

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname:sub(1, 19) ~= "soundstuff:jukebox@" then
		return false
	end

	local pos = vector.from_string(formname, 20)
	if not pos or pos ~= pos:round() then
		core.log("error", "[soundstuff:jukebox] Invalid formname.")
		return true
	end

	local meta = core.get_meta(pos)

	for _, k in ipairs(meta_keys) do
		if fields[k] then
			meta:set_string(k, fields[k])
		end
	end
	meta:mark_as_private(meta_keys)

	local pos_hash = core.hash_node_position(pos)
	local sound_handle = played_sounds[pos_hash]

	if not sound_handle then
		if fields.btn_play then
			local md = get_all_metadata(meta)

			local sss = {
				name = md.sss.name,
				gain  = tonumber(md.sss.gain),
				pitch = tonumber(md.sss.pitch),
				fade  = tonumber(md.sss.fade),
			}
			local sparam = {
				gain  = tonumber(md.sparam.gain),
				pitch = tonumber(md.sparam.pitch),
				fade  = tonumber(md.sparam.fade),
				start_time = tonumber(md.sparam.start_time),
				loop = core.is_yes(md.sparam.loop),
				pos = vector.from_string(md.sparam.pos),
				object = testtools.get_branded_object(md.sparam.object),
				to_player = md.sparam.to_player,
				exclude_player = md.sparam.exclude_player,
				max_hear_distance = tonumber(md.sparam.max_hear_distance),
			}
			local ephemeral = core.is_yes(md.ephemeral)

			log_msg(string.format(
					"[soundstuff:jukebox] Playing sound: core.sound_play(%s, %s, %s)",
					string.format("{name=\"%s\", gain=%s, pitch=%s, fade=%s}",
							sss.name, sss.gain, sss.pitch, sss.fade),
					string.format("{gain=%s, pitch=%s, fade=%s, start_time=%s, loop=%s, pos=%s, "
						.."object=%s, to_player=\"%s\", exclude_player=\"%s\", max_hear_distance=%s}",
							sparam.gain, sparam.pitch, sparam.fade, sparam.start_time,
							sparam.loop, sparam.pos, sparam.object and "<objref>",
							sparam.to_player, sparam.exclude_player,
							sparam.max_hear_distance),
					tostring(ephemeral)))

			sound_handle = try_call(core.sound_play, sss, sparam, ephemeral)

			played_sounds[pos_hash] = sound_handle
			show_formspec(pos, player)
		end

	else
		if fields.btn_stop then
			log_msg("[soundstuff:jukebox] Stopping sound: core.sound_stop(<handle>)")

			try_call(core.sound_stop, sound_handle)

		elseif fields.btn_release then
			log_msg("[soundstuff:jukebox] Releasing handle.")

			played_sounds[pos_hash] = nil
			show_formspec(pos, player)

		elseif fields.btn_fade then
			local md = get_all_metadata(meta)

			local step = tonumber(md.fade.step)
			local gain = tonumber(md.fade.gain)

			log_msg(string.format(
					"[soundstuff:jukebox] Fading sound: core.sound_fade(<handle>, %s, %s)",
					step, gain))

			try_call(core.sound_fade, sound_handle, step, gain)
		end
	end

	return true
end)
