local SIZE_TAG = "size[11,5.5,true]"

local function avoid_noid()
	return "label[1,1;Avoid the Noid!]"
end

local function menu_formspec(simple_singleplayer_mode, is_touchscreen, address)
	local ypos = simple_singleplayer_mode and 0.7 or 0.1
	local control_text = ""
	
	if is_touchscreen then
		control_text = fgettext([[Controls:
No menu open:
- slide finger: look around
- tap: place/punch/use (default)
- long tap: dig/use (default)
Menu/inventory open:
- double tap (outside):
 --> close
- touch stack, touch slot:
 --> move stack
- touch&drag, tap 2nd finger
 --> place single item to slot
]])
	end
	
	local fs = {
		"formspec_version[1]",
		SIZE_TAG,
		("button_exit[4,%f;3,0.5;btn_continue;%s]"):format(ypos, fgettext("Continue"))
	}
	ypos = ypos + 1
	
	if not simple_singleplayer_mode then
		fs[#fs + 1] = ("button_exit[4,%f;3,0.5;btn_change_password;%s]"):format(
			ypos, fgettext("Change Password"))
	else
		fs[#fs + 1] = "field[4.95,0;5,1.5;;" .. fgettext("Game paused") .. ";]"
	end
	
	fs[#fs + 1] = ("button_exit[4,%f;3,0.5;btn_key_config;%s]"):format(
		ypos, fgettext("Controls"))
	ypos = ypos + 1
	fs[#fs + 1] = ("button_exit[4,%f;3,0.5;btn_settings;%s]"):format(
		ypos, fgettext("Settings"))
	ypos = ypos + 1
	fs[#fs + 1] = ("button_exit[4,%f;3,0.5;btn_exit_menu;%s]"):format(
		ypos, fgettext("Exit to Menu"))
	ypos = ypos + 1
	fs[#fs + 1] = ("button_exit[4,%f;3,0.5;btn_exit_os;%s]"):format(
		ypos, fgettext("Exit to OS"))
	ypos = ypos + 1
	
	-- Controls
	if control_text ~= "" then
		fs[#fs + 1] = ("textarea[7.5,0.25;3.9,6.25;;%s;]"):format(control_text)
	end
	
	-- Server info
	local info = minetest.get_version()
	fs[#fs + 1] = ("textarea[0.4,0.25;3.9,6.25;;%s %s\n\n%s\n"):format(
		info.project, info.hash or info.string, fgettext("Game info:"))
	
	fs[#fs + 1] = "- Mode: " .. (simple_singleplayer_mode and "Singleplayer" or
		((not address) and "Hosting server" or "Remote server"))
	
	if not address then
		local enable_damage = minetest.settings:get_bool("enable_damage")
		local enable_pvp = minetest.settings:get_bool("enable_pvp")
		local server_announce = minetest.settings:get_bool("server_announce")
		local server_name = minetest.settings:get("server_name")
		table.insert_all(fs, {
			"\n",
			enable_damage and
				("- PvP: " .. (enable_pvp and "On" or "Off")) or "",
			"\n",
			"- Public: " .. (server_announce and "On" or "Off"),
			"\n",
			(server_announce and server_name) and  
				("- Server Name: " .. minetest.formspec_escape(server_name)) or ""
		})
	end
		
	fs[#fs + 1] = ";]"
	
	
	return table.concat(fs, "")
end

function core.show_pause_menu(is_singleplayer, is_touchscreen, address)
	minetest.show_formspec("MT_PAUSE_MENU", menu_formspec(is_singleplayer, is_touchscreen, address))
end

local scriptpath = core.get_builtin_path()
local path = scriptpath.."mainmenu"..DIR_DELIM.."settings"

function core.get_mainmenu_path()
	return scriptpath.."mainmenu"
end

defaulttexturedir = ""
dofile(path .. DIR_DELIM .. "settingtypes.lua")
dofile(path .. DIR_DELIM .. "dlg_change_mapgen_flags.lua")
dofile(path .. DIR_DELIM .. "dlg_settings.lua")

function dialog_create(name, spec, buttonhandler, eventhandler)
	minetest.show_formspec(name, spec({}))
end

function core.show_settings()
	load(true, false)
	show_settings_client_formspec()
end
