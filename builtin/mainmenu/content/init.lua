-- Luanti
-- Copyright (C) 2023 rubenwardy
-- SPDX-License-Identifier: LGPL-2.1-or-later

local path = core.get_mainmenu_path() .. DIR_DELIM .. "content"

dofile(path .. DIR_DELIM .. "pkgmgr.lua")
dofile(path .. DIR_DELIM .. "contentdb.lua")
dofile(path .. DIR_DELIM .. "update_detector.lua")
dofile(path .. DIR_DELIM .. "screenshots.lua")
dofile(path .. DIR_DELIM .. "dlg_install.lua")
dofile(path .. DIR_DELIM .. "dlg_overwrite.lua")
dofile(path .. DIR_DELIM .. "dlg_package.lua")
dofile(path .. DIR_DELIM .. "dlg_contentdb.lua")
