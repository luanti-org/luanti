-- Luanti
-- Copyright (C) 2022 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

local path = core.get_builtin_path() .. "common" .. DIR_DELIM .. "settings" .. DIR_DELIM

dofile(path .. "settingtypes.lua")
dofile(path .. "dlg_change_mapgen_flags.lua")
dofile(path .. "dlg_settings.lua")

-- Uncomment to generate 'minetest.conf.example' and 'settings_translation_file.cpp'.
-- For RUN_IN_PLACE the generated files may appear in the 'bin' folder.
-- See comment and alternative line at the end of 'generate_from_settingtypes.lua'.

-- dofile(path .. DIR_DELIM .. "generate_from_settingtypes.lua")
