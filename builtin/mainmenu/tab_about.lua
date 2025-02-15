--Luanti
--Copyright (C) 2013 sapier
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


local function prepare_credits(dest, source)
    local credit_string = table.concat(source, "\n") .. "\n"
    credit_string = core.hypertext_escape(credit_string)
    credit_string = credit_string:gsub("%[.-%]", "<gray>%1</gray>")
    table.insert(dest, credit_string)
end

local function get_credits()
    local credits_file = core.get_mainmenu_path() .. "/credits.json"
    local file, err = io.open(credits_file, "r")
    if not file then
        core.log("error", "Could not open credits file: " .. err)
        return {}
    end
    local content = file:read("*all")
    file:close()
    return core.parse_json(content) or {}
end

local function get_renderer_info()
    local ret = {}

    -- OpenGL version, stripped to just the important part
    local renderer = core.get_active_renderer()
    if renderer:sub(1, 7) == "OpenGL " then
        renderer = renderer:sub(8)
    end
    local version_match = renderer:match("^[%d.]+") or renderer:match("^ES [%d.]+")
    table.insert(ret, version_match or renderer)
    -- video driver
    table.insert(ret, core.get_active_driver():lower())
    -- irrlicht device
    table.insert(ret, core.get_active_irrlicht_device():upper())

    return table.concat(ret, " / ")
end

return {
    name = "about",
    caption = fgettext("About"),

    cbf_formspec = function(tabview, name, tabdata)
        local logofile = defaulttexturedir .. "logo.png"
        local version = core.get_version()

        local hypertext = {
            "<tag name=heading color=#ff0>",
            "<tag name=gray color=#aaa>",
        }

        local credits = get_credits()

        table.insert_all(hypertext, {
            "<heading>", fgettext_ne("Core Developers"), "</heading>\n",
        })
        prepare_credits(hypertext, credits.core_developers or {})
        table.insert_all(hypertext, {
            "\n",
            "<heading>", fgettext_ne("Core Team"), "</heading>\n",
        })
        prepare_credits(hypertext, credits.core_team or {})
        table.insert_all(hypertext, {
            "\n",
            "<heading>", fgettext_ne("Active Contributors"), "</heading>\n",
        })
        prepare_credits(hypertext, credits.contributors or {})
        table.insert_all(hypertext, {
            "\n",
            "<heading>", fgettext_ne("Previous Core Developers"), "</heading>\n",
        })
        prepare_credits(hypertext, credits.previous_core_developers or {})
        table.insert_all(hypertext, {
            "\n",
            "<heading>", fgettext_ne("Previous Contributors"), "</heading>\n",
        })
        prepare_credits(hypertext, credits.previous_contributors or {})

        hypertext = table.concat(hypertext):sub(1, -2)

        local fs = [[
            image[1.5,0.6;2.5,2.5;]] .. core.formspec_escape(logofile) .. [[]
            style[label_button;border=false]
            button[0.1,3.4;5.3,0.5;label_button;]] .. core.formspec_escape(version.project .. " " .. version.string) .. [[]
            button_url[1.5,4.1;2.5,0.8;homepage;luanti.org;https://www.luanti.org/]
            hypertext[5.5,0.25;9.75,6.6;credits;]] .. core.formspec_escape(hypertext) .. [[]
        ]]

        local active_renderer_info = fgettext("Active renderer:") .. "\n" .. core.formspec_escape(get_renderer_info())
        fs = fs .. [[
            style[label_button2;border=false]
            button[0.1,6;5.3,1;label_button2;]] .. active_renderer_info .. [[]
            tooltip[label_button2;]] .. active_renderer_info .. [[]
        ]]

        if PLATFORM == "Android" then
            fs = fs .. "button[0.5,5.1;4.5,0.8;share_debug;" .. fgettext("Share debug log") .. "]"
        else
            fs = fs .. [[
                tooltip[userdata;]] .. fgettext("Opens the directory that contains user-provided worlds, games, mods,\n" ..
                        "and texture packs in a file manager / explorer.") .. [[]
                button[0.5,5.1;4.5,0.8;userdata;]] .. fgettext("Open User Data Directory") .. [[]
            ]]
        end

        return fs
    end,

    cbf_button_handler = function(this, fields, name, tabdata)
        if fields.share_debug then
            local path = core.get_user_path() .. DIR_DELIM .. "debug.txt"
            core.share_file(path)
        end

        if fields.userdata then
            core.open_dir(core.get_user_path())
        end
    end,

    on_change = function(type)
        if type == "ENTER" then
            mm_game_theme.set_engine()
        end
    end,
}
