--Luanti
--Copyright (C) 2022 rubenwardy
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

local path = core.get_mainmenu_path() .. DIR_DELIM .. "settings"

dofile(path .. DIR_DELIM .. "settingtypes.lua")
dofile(path .. DIR_DELIM .. "dlg_change_mapgen_flags.lua")
dofile(path .. DIR_DELIM .. "dlg_settings.lua")

-- Uncomment to generate 'minetest.conf.example' and 'settings_translation_file.cpp'.
-- For RUN_IN_PLACE the generated files may appear in the 'bin' folder.
-- See comment and alternative line at the end of 'generate_from_settingtypes.lua'.

-- dofile(path .. DIR_DELIM .. "generate_from_settingtypes.lua")
